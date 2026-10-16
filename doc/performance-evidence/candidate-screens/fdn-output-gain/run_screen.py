#!/usr/bin/env python3
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path('/root/code/fluidsynth-analysis-20261015/fdn-output-gain')
EVIDENCE = Path('/root/code/fluidsynth-analysis-20261015/fdn-coefficient-tuning')
SF2 = Path('/root/code/fluidsynth-analysis-20261015/assets/MuseScore_General.sf2')
MIDI = Path('/root/code/symusic/tests/testcases/Multitrack_MIDIs/All The Small Things.mid')
PROBE = EVIDENCE / 'render-baseline'
LIBS = {
    'baseline': ROOT / 'build-baseline/src',
    'candidate': ROOT / 'build-candidate/src',
}
OUT = ROOT / 'renders'
SECONDS = 10
BLOCK = 64
REPS = 3

OUT.mkdir(exist_ok=True)
def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

cpus = sorted(os.sched_getaffinity(0)) if hasattr(os, 'sched_getaffinity') else []
cpu = cpus[0] if cpus else None
if cpu is not None:
    os.sched_setaffinity(0, {cpu})
env_common = dict(os.environ, OMP_NUM_THREADS='1', OMP_DYNAMIC='FALSE')

def run(arm, seconds, pair=None, warmup=False):
    env = dict(env_common, LD_LIBRARY_PATH=str(LIBS[arm]))
    pcm = OUT / (f'warmup-{arm}.f32' if warmup else f'pair-{pair}-{arm}.f32')
    command = [str(PROBE), str(SF2), str(MIDI), str(BLOCK), '1', str(seconds), '1', str(pcm)]
    result = subprocess.run(command, env=env, check=True, capture_output=True, text=True)
    row = json.loads(result.stdout)
    row.update({'arm': arm, 'pair': pair, 'warmup': warmup,
                'pcm_sha256': sha(pcm), 'library_sha256': sha(LIBS[arm] / 'libfluidsynth.so.3.6.1')})
    return row

manifest = {
    'baseline_commit': 'ae31c51454c4d2bab98c9d8ce65e9a5eafcf741c',
    'candidate_branch': 'perf/fdn-output-gain-20261016',
    'candidate_source_diff': subprocess.check_output(
        ['git', '-C', '/root/code/fluidsynth-worktrees/fdn-output-gain-20261016',
         'diff', '--', 'src/rvoice/fluid_rev_fdn.cpp'], text=True),
    'probe_sha256': sha(PROBE),
    'soundfont_sha256': sha(SF2),
    'midi_sha256': sha(MIDI),
    'libraries': {arm: sha(lib / 'libfluidsynth.so.3.6.1') for arm, lib in LIBS.items()},
    'settings': {'sample_rate': 44100, 'polyphony': 256, 'gain': 0.2,
                 'interpolation': 4, 'cores': 1, 'seconds': SECONDS,
                 'reverb_engine': 'fdn', 'reverb_active': True,
                 'chorus_active': False, 'block': BLOCK,
                 'OMP_NUM_THREADS': 1, 'OMP_DYNAMIC': False,
                 'cpu_affinity': cpu},
    'repetitions': REPS,
    'order': 'AB/BA alternation',
}
(ROOT / 'screen-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')

rows = [run(arm, 1, warmup=True) for arm in ('baseline', 'candidate')]
for pair in range(REPS):
    order = ('baseline', 'candidate') if pair % 2 == 0 else ('candidate', 'baseline')
    for arm in order:
        rows.append(run(arm, SECONDS, pair=pair))
        print(json.dumps(rows[-1], sort_keys=True), flush=True)
(ROOT / 'screen.jsonl').write_text(''.join(json.dumps(row, sort_keys=True) + '\n' for row in rows))
