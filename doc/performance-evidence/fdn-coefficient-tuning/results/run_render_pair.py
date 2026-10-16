#!/usr/bin/env python3
"""Run the FDN baseline/candidate probe with caller-supplied build and input paths."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--sf2', required=True, type=Path)
parser.add_argument('--midi', required=True, type=Path)
parser.add_argument('--baseline-probe', required=True, type=Path)
parser.add_argument('--candidate-probe', required=True, type=Path)
parser.add_argument('--baseline-lib-dir', required=True, type=Path)
parser.add_argument('--candidate-lib-dir', required=True, type=Path)
parser.add_argument('--out-dir', required=True, type=Path)
parser.add_argument('--block', type=int, default=64)
parser.add_argument('--cores', type=int, default=1)
parser.add_argument('--seconds', type=int, default=15)
args = parser.parse_args()
args.out_dir.mkdir(parents=True, exist_ok=True)

probes = {'baseline': args.baseline_probe, 'candidate': args.candidate_probe}
libs = {'baseline': args.baseline_lib_dir, 'candidate': args.candidate_lib_dir}
env_base = dict(os.environ, OMP_NUM_THREADS='1', OMP_DYNAMIC='FALSE')

def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

rows = []
for arm in ('baseline', 'candidate'):
    for fx in (0, 1):
        pcm = args.out_dir / f'piano-single-{arm}-fx{fx}.f32'
        env = dict(env_base, LD_LIBRARY_PATH=str(libs[arm]))
        command = [str(probes[arm]), str(args.sf2), str(args.midi), str(args.block),
                   str(args.cores), str(args.seconds), str(fx), str(pcm)]
        completed = subprocess.run(command, env=env, check=True, capture_output=True, text=True)
        library = libs[arm] / 'libfluidsynth.so.3.6.1'
        row = {
            'arm': arm,
            'fx': bool(fx),
            **json.loads(completed.stdout),
            'pcm_sha256': sha256(pcm),
            'library_sha256': sha256(library),
        }
        rows.append(row)
        print(json.dumps(row, sort_keys=True), flush=True)

(args.out_dir / 'render-pair.json').write_text(json.dumps(rows, indent=2) + '\n')
