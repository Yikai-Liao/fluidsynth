#!/usr/bin/env python3
import argparse, datetime, hashlib, json, os, platform, subprocess, time
from pathlib import Path

root = Path(__file__).resolve().parent
probe = root / 'render_probe_e2'
asset_root = Path(os.environ['FLUIDSYNTH_E2_ASSET_DIR']) if os.environ.get('FLUIDSYNTH_E2_ASSET_DIR') else None
midi_root = Path(os.environ['FLUIDSYNTH_E2_MIDI_ROOT']) if os.environ.get('FLUIDSYNTH_E2_MIDI_ROOT') else None
if asset_root is None or midi_root is None:
    raise SystemExit('set FLUIDSYNTH_E2_ASSET_DIR and FLUIDSYNTH_E2_MIDI_ROOT to the external test assets')
sf2 = asset_root / 'MuseScore_General.sf2'
midi = midi_root / 'Multitrack_MIDIs/All The Small Things.mid'
cpu = os.environ.get('FLUIDSYNTH_E2_CPU', '2')
paths = {
    'baseline': Path(os.environ.get('FLUIDSYNTH_E2_BASELINE_BUILD', root / 'build-baseline')),
    'candidate': Path(os.environ.get('FLUIDSYNTH_E2_CANDIDATE_BUILD', root / 'build-double')),
}


def sha256(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def run_one(label, build):
    libdir = build / 'src'
    env = {'PATH': '/usr/bin:/bin', 'OMP_NUM_THREADS': '1', 'LD_LIBRARY_PATH': str(libdir)}
    argv = ['taskset', '-c', cpu, str(probe), str(sf2), str(midi), '64', '1', '10', '1', '0']
    start = time.monotonic()
    completed = subprocess.run(argv, capture_output=True, text=True, env=env, check=True, timeout=120)
    process_wall = time.monotonic() - start
    result = json.loads(completed.stdout)
    return {
        'label': label,
        'setup_wall_s': result['setup_wall'],
        'render_wall_s': result['render_wall'],
        'setup_plus_render_wall_s': result['setup_wall'] + result['render_wall'],
        'render_cpu_s': result['render_cpu'],
        'process_wall_s': process_wall,
        'hash': result['hash'],
        'rms': result['rms'],
        'peak': result['peak'],
        'maxrss_kib': result['maxrss_kib'],
        'stdout': completed.stdout.strip(),
        'stderr': completed.stderr.strip(),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('mode', choices=['aa', 'candidate'])
    ap.add_argument('--batch', type=int, choices=[1, 2], default=1)
    ap.add_argument('--output-dir', type=Path, default=root / 'formal-primary')
    args = ap.parse_args()
    outdir = args.output_dir
    outdir.mkdir(parents=True, exist_ok=True)
    outfile = outdir / f"{args.mode}{'-batch'+str(args.batch) if args.mode == 'candidate' else ''}.jsonl"
    log = outfile.open('w')
    left_build = paths['baseline']
    right_build = paths['baseline'] if args.mode == 'aa' else paths['candidate']
    left_label = 'baseline'
    right_label = 'baseline_repeat' if args.mode == 'aa' else 'candidate'

    for label, build in ((left_label, left_build), (right_label, right_build)):
        row = run_one(label, build)
        row.update({'mode': args.mode, 'batch': args.batch, 'warmup': True, 'timestamp_utc': datetime.datetime.now(datetime.timezone.utc).isoformat()})
        log.write(json.dumps(row, sort_keys=True) + '\n'); log.flush()
        print('warmup', label, round(row['render_wall_s'], 6), row['hash'], flush=True)

    for pair in range(15):
        left_first = (pair % 2 == 0) if args.mode == 'aa' or args.batch == 1 else (pair % 2 == 1)
        order = [(left_label, left_build), (right_label, right_build)] if left_first else [(right_label, right_build), (left_label, left_build)]
        rows = []
        for label, build in order:
            row = run_one(label, build)
            row.update({'mode': args.mode, 'batch': args.batch, 'pair': pair, 'order': [x[0] for x in order], 'warmup': False,
                        'timestamp_utc': datetime.datetime.now(datetime.timezone.utc).isoformat()})
            log.write(json.dumps(row, sort_keys=True) + '\n'); log.flush()
            rows.append(row)
        print(f"pair {pair+1}/15", [r['label'] for r in rows],
              [(r['label'], round(r['render_wall_s'], 6), r['hash']) for r in rows], flush=True)
    log.close()

if __name__ == '__main__':
    main()
