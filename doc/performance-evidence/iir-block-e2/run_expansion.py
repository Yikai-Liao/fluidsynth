#!/usr/bin/env python3
"""Fixed-sample expanded E2 render guard matrix; timings exclude PCM verification."""
import datetime, hashlib, json, os, platform, statistics, subprocess, time
from pathlib import Path

ROOT = Path(__file__).resolve().parent
PROBE = ROOT / 'render_probe_extended'
ASSET_ROOT = Path(os.environ['FLUIDSYNTH_E2_ASSET_DIR']) if os.environ.get('FLUIDSYNTH_E2_ASSET_DIR') else None
MIDI_ROOT = Path(os.environ['FLUIDSYNTH_E2_MIDI_ROOT']) if os.environ.get('FLUIDSYNTH_E2_MIDI_ROOT') else None
if ASSET_ROOT is None or MIDI_ROOT is None:
    raise SystemExit('set FLUIDSYNTH_E2_ASSET_DIR and FLUIDSYNTH_E2_MIDI_ROOT to the external test assets')
SF2 = ASSET_ROOT / 'MuseScore_General.sf2'
MIDI = MIDI_ROOT / 'Multitrack_MIDIs/All The Small Things.mid'
MAESTRO = MIDI_ROOT / 'One_track_MIDIs/Maestro_1.mid'
POLY = ROOT / 'high_poly_96.mid'
BUILDS = {'baseline': Path(os.environ.get('FLUIDSYNTH_E2_BASELINE_BUILD', ROOT/'build-baseline')),
          'candidate': Path(os.environ.get('FLUIDSYNTH_E2_CANDIDATE_BUILD', ROOT/'build-double'))}
CPU = os.environ.get('FLUIDSYNTH_E2_CPU', '2')
OUT = ROOT / 'expansion'
OUT.mkdir(exist_ok=True)

CASES = [
    {'name':'all_b128', 'midi':MIDI, 'block':128, 'rate':44100, 'seconds':10, 'polyphony':256},
    {'name':'all_b256', 'midi':MIDI, 'block':256, 'rate':44100, 'seconds':10, 'polyphony':256},
    {'name':'all_48k', 'midi':MIDI, 'block':64, 'rate':48000, 'seconds':10, 'polyphony':256},
    {'name':'all_96k', 'midi':MIDI, 'block':64, 'rate':96000, 'seconds':10, 'polyphony':256},
    {'name':'poly96_sustain', 'midi':POLY, 'block':64, 'rate':44100, 'seconds':10, 'polyphony':512},
    {'name':'all_custom_lp', 'midi':MIDI, 'block':64, 'rate':44100, 'seconds':10, 'polyphony':256,
     'custom_type':1, 'custom_fc_cents':9000, 'custom_q':0.7071},
    {'name':'maestro_30s', 'midi':MAESTRO, 'block':64, 'rate':44100, 'seconds':30, 'polyphony':256},
]

def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as f:
        for b in iter(lambda:f.read(1024*1024),b''): h.update(b)
    return h.hexdigest()

def command(case, arm, pcm='-', latency='-'):
    custom_type=case.get('custom_type',0)
    fc=case.get('custom_fc_cents',0)
    q=case.get('custom_q',0)
    argv=[str(PROBE),str(SF2),str(case['midi']),str(case['block']),'1',str(case['seconds']),'1','0',
          str(pcm),str(case['rate']),str(case['polyphony']),str(custom_type),str(fc),str(q),str(latency)]
    libdir=BUILDS[arm]/'src'
    env={'PATH':'/usr/bin:/bin','OMP_NUM_THREADS':'1','LD_LIBRARY_PATH':str(libdir)}
    return ['taskset','-c',CPU,*argv],env

def run_one(case, arm, pair, order, warmup=False):
    pcm=OUT/f"pcm-{case['name']}-{arm}.f32" if pair == 0 and not warmup else '-'
    argv,env=command(case,arm,pcm)
    started=time.monotonic()
    cp=subprocess.run(argv,capture_output=True,text=True,env=env,check=True,timeout=300)
    process_wall=time.monotonic()-started
    if 'Failed to allocate a synthesis process' in cp.stderr:
        raise RuntimeError(f"voice allocation failure in {case['name']} {arm}: {cp.stderr[:300]}")
    v=json.loads(cp.stdout)
    return {'case':case['name'],'arm':arm,'pair':pair,'order':order,'warmup':warmup,
      'block':case['block'],'sample_rate':case['rate'],'duration_s':case['seconds'],
      'fx':1,'interpolation':'4th','polyphony_limit':case['polyphony'],
      'custom_filter_type':case.get('custom_type',0),'custom_fc_cents':case.get('custom_fc_cents',0),
      'custom_q':case.get('custom_q',0),'midi_path':str(case['midi']),
      'midi_sha256':sha(case['midi']),'sf2_sha256':sha(SF2),
      'library_sha256':sha(BUILDS[arm]/'src/libfluidsynth.so.3.6.1'),
      'render_wall_s':v['render_wall'],'render_cpu_s':v['render_cpu'],'setup_wall_s':v['setup_wall'],
      'process_wall_s':process_wall,'maxrss_kib':v['maxrss_kib'],'hash':v['hash'],'rms':v['rms'],'peak':v['peak'],
      'pcm_path':str(pcm) if pcm!='-' else None,'stdout':cp.stdout.strip(),'stderr':cp.stderr.strip(),
      'timestamp_utc':datetime.datetime.now(datetime.timezone.utc).isoformat()}

def main():
    subprocess.run(['python3',str(ROOT/'generate_high_poly_96.py')],check=True,capture_output=True,text=True)
    manifest={'created_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
      'base_sha':'6be67a906b9d679ea930c8841fd0cac0d8a0a25e','cpu_affinity':CPU,
      'cpu_model':subprocess.run(['bash','-lc',"lscpu | sed -n 's/^Model name:[[:space:]]*//p' | head -1"],capture_output=True,text=True).stdout.strip(),
      'compiler':'GCC 14.2 release build; production flags verified in formal-primary setup',
      'delta':0.02,'pairs_per_case':15,'repeats':1,
      'sf2':str(SF2),'sf2_sha256':sha(SF2),'probe_sha256':sha(PROBE),
      'cases':[{k:(str(v) if isinstance(v,Path) else v) for k,v in c.items()} for c in CASES]}
    (OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    for case in CASES:
        path=OUT/f"{case['name']}.jsonl"
        with path.open('w') as log:
            for arm in ('baseline','candidate'):
                row=run_one(case,arm,-1,[],True); log.write(json.dumps(row,sort_keys=True)+'\n'); log.flush()
                print('warmup',case['name'],arm,round(row['render_wall_s'],6),row['hash'],flush=True)
            for pair in range(15):
                order=['baseline','candidate'] if pair%2==0 else ['candidate','baseline']
                rows=[]
                for arm in order:
                    row=run_one(case,arm,pair,order)
                    log.write(json.dumps(row,sort_keys=True)+'\n'); log.flush(); rows.append(row)
                print('pair',case['name'],pair+1,'/15',[(r['arm'],round(r['render_wall_s'],6),r['hash']) for r in rows],flush=True)

if __name__=='__main__': main()
