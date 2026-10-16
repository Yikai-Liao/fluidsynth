#!/usr/bin/env python3
"""Checks that the double-only candidate does not change float render behavior/performance."""
import datetime, hashlib, json, math, os, statistics, subprocess, time
from pathlib import Path

ROOT=Path(__file__).resolve().parent
PROBE=ROOT/'render_probe_extended'
ASSET_ROOT=Path(os.environ['FLUIDSYNTH_E2_ASSET_DIR']) if os.environ.get('FLUIDSYNTH_E2_ASSET_DIR') else None
MIDI_ROOT=Path(os.environ['FLUIDSYNTH_E2_MIDI_ROOT']) if os.environ.get('FLUIDSYNTH_E2_MIDI_ROOT') else None
if ASSET_ROOT is None or MIDI_ROOT is None:
    raise SystemExit('set FLUIDSYNTH_E2_ASSET_DIR and FLUIDSYNTH_E2_MIDI_ROOT to the external test assets')
SF2=ASSET_ROOT/'MuseScore_General.sf2'
MIDI=MIDI_ROOT/'Multitrack_MIDIs/All The Small Things.mid'
BUILDS={'baseline':Path(os.environ.get('FLUIDSYNTH_E2_BASELINE_FLOAT_BUILD',ROOT/'build-baseline-float')),
        'candidate':Path(os.environ.get('FLUIDSYNTH_E2_CANDIDATE_FLOAT_BUILD',ROOT/'build-float'))}
CPU=os.environ.get('FLUIDSYNTH_E2_CPU','2')
OUT=ROOT/'float-regression'
OUT.mkdir(exist_ok=True)

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def run(arm,pair,order,warmup=False):
    pcm=OUT/f'pcm-{arm}.f32' if pair==0 and not warmup else '-'
    argv=[str(PROBE),str(SF2),str(MIDI),'64','1','10','1','0',str(pcm),'44100','256','0','0','0','-']
    env={'PATH':'/usr/bin:/bin','OMP_NUM_THREADS':'1','LD_LIBRARY_PATH':str(BUILDS[arm]/'src')}
    start=time.monotonic(); cp=subprocess.run(['taskset','-c',CPU,*argv],env=env,capture_output=True,text=True,check=True,timeout=300)
    if cp.stderr.strip(): raise RuntimeError(cp.stderr)
    v=json.loads(cp.stdout)
    return {'arm':arm,'pair':pair,'order':order,'warmup':warmup,'render_wall_s':v['render_wall'],
      'render_cpu_s':v['render_cpu'],'process_wall_s':time.monotonic()-start,'hash':v['hash'],
      'pcm_path':str(pcm) if pcm!='-' else None,'timestamp_utc':datetime.datetime.now(datetime.timezone.utc).isoformat()}

def main():
    p=OUT/'runs.jsonl'
    with p.open('w') as f:
      for arm in ('baseline','candidate'):
        r=run(arm,-1,[],True); f.write(json.dumps(r,sort_keys=True)+'\n'); f.flush()
      for pair in range(15):
        order=['baseline','candidate'] if pair%2==0 else ['candidate','baseline']
        for arm in order:
          r=run(arm,pair,order); f.write(json.dumps(r,sort_keys=True)+'\n'); f.flush()
        print('float pair',pair+1,'/15',[(r['arm'],round(r['render_wall_s'],6),r['hash']) for r in [json.loads(s) for s in p.read_text().splitlines() if json.loads(s).get('pair')==pair]],flush=True)
    rows=[json.loads(s) for s in p.read_text().splitlines()]
    groups={i:{r['arm']:r for r in rows if not r['warmup'] and r['pair']==i} for i in range(15)}
    ratios=[groups[i]['candidate']['render_wall_s']/groups[i]['baseline']['render_wall_s'] for i in range(15)]
    logs=list(map(math.log,ratios)); mean=statistics.mean(logs); half=2.1447866879169273*statistics.stdev(logs)/math.sqrt(15)
    result={'sample':'All The Small Things, 64 frames, 44.1 kHz, FX on, 10 sec','pairs':15,
      'render_wall_ratio_geomean':math.exp(mean),'render_wall_ratio_median':statistics.median(ratios),
      'render_wall_ratio_95ci':[math.exp(mean-half),math.exp(mean+half)],'pairs_candidate_faster':sum(x<1 for x in ratios),
      'all_hashes_equal':all(groups[i]['candidate']['hash']==groups[i]['baseline']['hash'] for i in groups),
      'pcm_bitwise_equal':(OUT/'pcm-baseline.f32').read_bytes()==(OUT/'pcm-candidate.f32').read_bytes(),
      'baseline_float_library_sha256':sha(BUILDS['baseline']/'src/libfluidsynth.so.3.6.1'),
      'candidate_float_library_sha256':sha(BUILDS['candidate']/'src/libfluidsynth.so.3.6.1'),
      'midi_sha256':sha(MIDI),'sf2_sha256':sha(SF2),'ratios':ratios}
    (OUT/'summary.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()
