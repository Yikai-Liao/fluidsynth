#!/usr/bin/env python3
import csv
import hashlib
import json
import platform
import statistics
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent
FINAL = ROOT / 'final'

def rows(path):
    with path.open(newline='') as f:
        return list(csv.DictReader(f))

quality = rows(FINAL / 'quality.csv')
dynamic = rows(FINAL / 'dynamic.csv')
bench = rows(FINAL / 'bench.csv')
result = {}
for precision in ('float', 'double'):
    result[precision] = {}
    for width in ('2', '4'):
        selected = [r for r in quality if r['precision'] == precision and r['width'] == width]
        active = [r for r in selected if r['reference_rms_dbfs'] and float(r['reference_rms_dbfs']) >= -60.0]
        quiet = [r for r in selected if r['reference_rms_dbfs'] and float(r['reference_rms_dbfs']) < -60.0]
        sine = [r for r in active if r['case'] == 'sine']
        worst_relative = max(active, key=lambda r: float(r['relative_error_db']))
        worst_gain = max(sine, key=lambda r: abs(float(r['gain_delta_db'])))
        worst_quiet = max(quiet, key=lambda r: float(r['max_abs_error_dbfs']))
        dyn = [r for r in dynamic if r['precision'] == precision and r['width'] == width
               and int(r['constant_run_length']) > 1]
        finite = all(r['finite'] == '1' for r in selected)
        worst_state = max(float(r['final_state_diff']) for r in selected)
        ratios = [float(r['block_over_scalar']) for r in bench
                  if r['precision'] == precision and r['width'] == width]
        relative_pass = float(worst_relative['relative_error_db']) <= -60.0
        gain_pass = abs(float(worst_gain['gain_delta_db'])) <= 0.1
        quiet_pass = float(worst_quiet['max_abs_error_dbfs']) <= -80.0
        dynamic_pass = all(float(r['relative_error_db']) <= -60.0 for r in dyn)
        result[precision][width] = {
            'quality_rows': len(selected),
            'active_rows': len(active),
            'quiet_rows': len(quiet),
            'worst_active_relative_error_db': float(worst_relative['relative_error_db']),
            'worst_active_case': {k: worst_relative[k] for k in ('case', 'cutoff_cents', 'q', 'frequency_hz')},
            'worst_abs_gain_delta_db': abs(float(worst_gain['gain_delta_db'])),
            'worst_gain_case': {k: worst_gain[k] for k in ('cutoff_cents', 'q', 'frequency_hz')},
            'worst_quiet_absolute_error_dbfs': float(worst_quiet['max_abs_error_dbfs']),
            'worst_terminal_state_difference': worst_state,
            'worst_dynamic_relative_error_db': max(float(r['relative_error_db']) for r in dyn),
            'all_outputs_and_states_finite': finite,
            'active_relative_gate_pass': relative_pass,
            'static_gain_gate_pass': gain_pass,
            'quiet_absolute_gate_pass': quiet_pass,
            'dynamic_transition_gate_pass': dynamic_pass,
            'quality_gate_pass': relative_pass and gain_pass and quiet_pass and dynamic_pass and finite,
            'bench_candidate_over_scalar_median': statistics.median(ratios),
            'bench_candidate_over_scalar_min': min(ratios),
            'bench_candidate_over_scalar_max': max(ratios),
        }

summary = {
    'date': '2026-10-16',
    'baseline_source_commit': '6be67a906b9d679ea930c8841fd0cac0d8a0a25e',
    'candidate': 'fixed-coefficient DFII recurrence expansion, widths 2 and 4',
    'method': {
        'precision': ['float', 'double'],
        'sample_rate_hz': 44100,
        'quality_frames': 131072,
        'dynamic_coefficients': 'smooth cutoff/Q changes at run lengths 1,2,3,4,5,31,64,127; short runs use scalar tail path where insufficient to fill a block',
        'quality_gates': {
            'active_relative_error_db_max': -60.0,
            'quiet_reference_rms_dbfs_below': -60.0,
            'quiet_absolute_error_dbfs_max': -80.0,
            'static_gain_delta_db_max': 0.1,
        },
        'bench': '7 scalar/candidate pairs per precision and width; 64-frame calls; precomputed coefficients and block polynomials; pinned to CPU 2',
    },
    'results': result,
    'decision': 'stop: float quality gates fail; no production change or end-to-end benchmark',
}
(ROOT / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')

def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()

source_root = Path('/root/code/fluidsynth-worktrees/algorithm-route-profile-20261016')
manifest_files = [ROOT / name for name in ('iir_block_screen.cpp', 'run.sh', 'summarize.py',
                                             'REPORT.md', 'summary.json', 'iir_block_screen')]
manifest_files += sorted(FINAL.glob('*.csv'))
manifest = {
    'date': '2026-10-16',
    'baseline_source_commit': subprocess.check_output(
        ['git', '-C', str(source_root), 'rev-parse', 'HEAD'], text=True).strip(),
    'baseline_source_clean': not subprocess.check_output(
        ['git', '-C', str(source_root), 'status', '--short'], text=True).strip(),
    'analysis_compiler': subprocess.check_output(['g++', '--version'], text=True).splitlines()[0],
    'compile_flags': '-O3 -DNDEBUG -std=c++17 -ffp-contract=off -Wall -Wextra -Wpedantic',
    'machine': platform.platform(),
    'cpu': 'Intel Xeon Gold 6140, CPU 2 affinity',
    'inputs': {'deterministic generated noise and sine/impulse vectors': 'seed 0x9e3779b97f4a7c15'},
    'outputs': {str(p.relative_to(ROOT)): sha256(p) for p in manifest_files},
    'production_source_changed': False,
    'performance_benchmarks_run': True,
}
(ROOT / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
