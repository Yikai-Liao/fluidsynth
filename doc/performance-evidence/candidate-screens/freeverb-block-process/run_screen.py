#!/usr/bin/env python3
import hashlib
import json
import os
import platform
import statistics
import subprocess
from pathlib import Path

ROOT = Path('/root/code/fluidsynth-analysis-20261015')
OUT = ROOT / 'freeverb-block-process'
PROBE = OUT / 'render_probe_reverb'
SF2 = ROOT / 'assets/MuseScore_General.sf2'
MIDI = Path('/root/code/symusic/tests/testcases/Multitrack_MIDIs/All The Small Things.mid')
LIBS = {
    'baseline': ROOT / 'interpolation-block-safe-count/build-baseline/src',
    'candidate': OUT / 'build-candidate/src',
}
SECONDS = 10
BLOCK = 64
REPS = 3

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

allowed = sorted(os.sched_getaffinity(0)) if hasattr(os, 'sched_getaffinity') else []
cpu = allowed[0] if allowed else None
if cpu is not None:
    os.sched_setaffinity(0, {cpu})
env_base = dict(os.environ, OMP_NUM_THREADS='1', OMP_DYNAMIC='FALSE')

def run(arm, repetition, warmup=False):
    env = dict(env_base, LD_LIBRARY_PATH=str(LIBS[arm]))
    result = subprocess.run(
        [str(PROBE), str(SF2), str(MIDI), str(BLOCK), '1', str(SECONDS), '1', 'free', '0'],
        env=env, check=True, capture_output=True, text=True)
    row = json.loads(result.stdout)
    row.update({'arm': arm, 'repetition': repetition, 'warmup': warmup})
    return row

manifest = {
    'baseline_commit': 'ae31c51454c4d2bab98c9d8ce65e9a5eafcf741c',
    'candidate_branch': 'perf/freeverb-block-process-20261016',
    'candidate_source_diff': subprocess.check_output(
        ['git', '-C', '/root/code/fluidsynth-worktrees/freeverb-block-process-20261016',
         'diff', '--', 'src/rvoice/fluid_rev_freeverb.cpp'], text=True),
    'probe_sha256': sha(PROBE),
    'soundfont_sha256': sha(SF2),
    'midi_sha256': sha(MIDI),
    'libraries': {arm: sha(lib / 'libfluidsynth.so.3.6.1') for arm, lib in LIBS.items()},
    'settings': {'sample_rate': 44100, 'polyphony': 256, 'gain': 0.2,
                 'interpolation': 4, 'cores': 1, 'seconds': SECONDS,
                 'reverb_engine': 'free', 'reverb_active': True,
                 'chorus_active': False, 'block': BLOCK,
                 'OMP_NUM_THREADS': 1, 'OMP_DYNAMIC': False,
                 'cpu_affinity': cpu},
    'repetitions': REPS,
    'order': 'AB/BA alternation',
    'host': {'platform': platform.platform(), 'machine': platform.machine(),
             'compiler': subprocess.check_output(['/usr/bin/c++', '--version'],
                                                  text=True).splitlines()[0],
             'affinity_available': allowed},
}
(OUT / 'screen-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')

rows = []
with (OUT / 'screen.jsonl').open('w') as stream:
    for arm in ('baseline', 'candidate'):
        row = run(arm, -1, True)
        rows.append(row)
        stream.write(json.dumps(row) + '\n')
    for rep in range(REPS):
        arms = ['baseline', 'candidate'] if rep % 2 == 0 else ['candidate', 'baseline']
        for arm in arms:
            row = run(arm, rep)
            rows.append(row)
            stream.write(json.dumps(row) + '\n')
            stream.flush()

pairs = []
for rep in range(REPS):
    pair = {r['arm']: r for r in rows if r['repetition'] == rep}
    pairs.append(pair)
ratios = [p['candidate']['render_wall'] / p['baseline']['render_wall'] for p in pairs]
cpu_ratios = [p['candidate']['render_cpu'] / p['baseline']['render_cpu'] for p in pairs]
summary = {
    'wall_ratio_candidate_over_baseline': ratios,
    'wall_ratio_median': statistics.median(ratios),
    'wall_ratio_range': [min(ratios), max(ratios)],
    'cpu_ratio_candidate_over_baseline': cpu_ratios,
    'cpu_ratio_median': statistics.median(cpu_ratios),
    'pcm_equal': all(p['candidate']['hash'] == p['baseline']['hash'] for p in pairs),
    'hashes': {arm: sorted({r['hash'] for r in rows if r['arm'] == arm})
               for arm in ('baseline', 'candidate')},
    'pairs': pairs,
}
(OUT / 'screen-summary.json').write_text(json.dumps(summary, indent=2) + '\n')
print(json.dumps(summary, indent=2))
