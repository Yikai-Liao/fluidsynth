#!/usr/bin/env python3
import csv
import hashlib
import json
import os
import platform
import statistics
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent

def rows(name):
    with (ROOT / name).open(newline='') as f:
        return list(csv.DictReader(f))

def fnum(row, key):
    return float(row[key])

spline_static = rows('spline-static.csv')
fir_static = rows('fir-static.csv')
reconstruction = rows('spline-reconstruction.csv')
windows = rows('candidate-windows.csv')
loop_entry = rows('loop-entry-windows.csv')
baseline_repro = rows('baseline-reproduction.csv')

static = {'spline': {}, 'fir': {}}
for degree in ('3', '5'):
    static['spline'][degree] = {}
    for band in (0.2, 0.3, 0.4, 0.45):
        selected = [r for r in spline_static
                    if r['degree'] == degree and fnum(r, 'frequency_cycles_per_sample') <= band + 1e-12]
        worst = max(selected, key=lambda r: abs(fnum(r, 'gain_error_db')))
        static['spline'][degree][str(band)] = {
            'max_abs_gain_error_db': abs(fnum(worst, 'gain_error_db')),
            'frequency': fnum(worst, 'frequency_cycles_per_sample'),
            'phase': fnum(worst, 'phase'),
            'phase_error_deg': fnum(worst, 'phase_error_deg'),
        }
for taps in ('13', '17', '21', '25'):
    static['fir'][taps] = {}
    for band in (0.4, 0.45):
        selected = [r for r in fir_static
                    if r['taps'] == taps and fnum(r, 'frequency_cycles_per_sample') <= band + 1e-12]
        worst = max(selected, key=lambda r: fnum(r, 'max_abs_gain_error_db'))
        static['fir'][taps][str(band)] = {
            'max_abs_gain_error_db': fnum(worst, 'max_abs_gain_error_db'),
            'frequency': fnum(worst, 'frequency_cycles_per_sample'),
            'max_abs_phase_error_deg': fnum(worst, 'max_abs_phase_error_deg'),
        }

quality = {}
for kind in sorted({r['kind'] for r in windows}):
    all_rows = [r for r in windows if r['kind'] == kind]
    nonquiet = [r for r in all_rows if r['quiet_reference'] == '0']
    # Factors 0.5 and 1.0 are the preregistered mapped/down quality gate.
    # Keep factor 2.0 visible as a separate octave-up diagnostic; do not mix
    # it into the primary gate or silently drop it from the summary.
    selected = [r for r in nonquiet if fnum(r, 'transpose_factor') <= 1.0]
    deltas = [fnum(r, 'error_vs_sinc25_db') for r in selected]
    worst = max(selected, key=lambda r: fnum(r, 'error_vs_sinc25_db'))
    margins = [fnum(r, 'ref_margin_db') for r in all_rows]
    per_factor = {}
    for factor in (0.5, 1.0, 2.0):
        factor_rows = [r for r in nonquiet if abs(fnum(r, 'transpose_factor') - factor) < 1e-9]
        values = [fnum(r, 'error_vs_sinc25_db') for r in factor_rows]
        per_factor[str(factor)] = {
            'n_nonquiet': len(values),
            'median_delta_vs_sinc25_db': statistics.median(values) if values else None,
            'worst_delta_vs_sinc25_db': max(values) if values else None,
        }
    quality[kind] = {
        'n_windows': len(all_rows),
        'n_nonquiet_mapped_and_down': len(selected),
        'median_delta_vs_sinc25_db': statistics.median(deltas),
        'worst_delta_vs_sinc25_db': fnum(worst, 'error_vs_sinc25_db'),
        'worst_case': {k: worst[k] for k in ('sample', 'segment', 'step', 'transpose_factor')},
        'reference_gate_pass_count': sum(m <= -20.0 for m in margins),
        'reference_gate_total': len(margins),
        'worst_reference_margin_db': max(margins),
        'per_factor': per_factor,
    }

loop_summary = {}
for kind in sorted({r['kind'] for r in loop_entry}):
    selected = [r for r in loop_entry if r['kind'] == kind]
    deltas = [fnum(r, 'error_vs_sinc25_db') for r in selected]
    margins = [fnum(r, 'ref_margin_db') for r in selected]
    worst = max(selected, key=lambda r: fnum(r, 'error_vs_sinc25_db'))
    loop_summary[kind] = {
        'n_windows': len(selected),
        'median_delta_vs_sinc25_db': statistics.median(deltas),
        'worst_delta_vs_sinc25_db': fnum(worst, 'error_vs_sinc25_db'),
        'worst_case': {k: worst[k] for k in ('sample', 'step')},
        'worst_reference_margin_db': max(margins),
    }

recon_by_degree = {}
for degree in ('3', '5'):
    selected = [r for r in reconstruction if r['degree'] == degree]
    recon_by_degree[degree] = {
        'max_integer_reconstruction_error': max(fnum(r, 'max_integer_error') for r in selected),
        'cases': len(selected),
    }

summary = {
    'design': {
        'spline_degrees': [3, 5],
        'fir_taps': [13, 17, 21, 25],
        'fir_design': 'unit-weight least squares over 0..0.40 cycles/sample, exact DC sum constraint',
        'phase_table_rows': 4097,
        'phase_interpolation': 'linear interpolation using 12 phase bits plus a guard row',
        'analysis_precision': 'double',
        'predeclared_passband_cycles_per_sample': [0.0, 0.40],
        'transition_band_cycles_per_sample': [0.40, 0.50],
    },
    'static_response': static,
    'spline_integer_reconstruction': recon_by_degree,
    'baseline_reproduction': {
        'cases': len(baseline_repro),
        'max_abs_sample_error': max(fnum(r, 'max_abs_error') for r in baseline_repro),
        'max_rms_error_dbfs': max(fnum(r, 'rms_error_dbfs') for r in baseline_repro),
    },
    'real_sf2_windows': quality,
    'first_loop_wrap_windows': loop_summary,
}
(ROOT / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')

lines = [
    '# Fractional-delay A/B offline quality screen',
    '',
    'Date: 2026-10-16. This is a bounded offline quality screen for the supplied plan’s A (prefiltered cardinal B-spline) and B (short weighted-least-squares fractional-delay FIR table). It makes no production change and makes no runtime-performance claim.',
    '',
    '## Frozen setup',
    '',
    '- Baseline source: measurement composite `6be67a906b9d679ea930c8841fd0cac0d8a0a25e` (combined IIR coefficient reuse, Dattorro tap addressing, and the accepted sinc25 phase LUT). The main checkout remains untouched.',
    '- Samples: five extracted MuseScore General SF2 samples from the prior Stage 3 screen (piano, strings, saw lead, closed hi-hat, cymbal). Existing 54 source windows contain cubic, sinc25, 1023/2047/4095-tap reference outputs; the 54×2 baseline outputs make the original 108 comparison rows. This screen adds 9 short first-loop-wrap windows.',
    '- Output/reference: existing 44.1 kHz, double-precision direct-interpolator screen; 882-frame windows for the 54 primary cases. The added loop-entry probes use 128 frames and 2047/4095-tap references.',
    '- Frozen fractional-delay FIR target: uniform least squares on source-normalized 0–0.40 cycles/sample, exact coefficient sum of one, 4096 phase intervals plus a guard row, and linear coefficient interpolation. The transition region is 0.40–0.50 cycles/sample; no rate-scaled anti-alias cutoff is claimed.',
    '- The primary real-sample relative-error gate uses nonquiet mapped/down-pitch windows (step factors 0.5 and 1.0): median candidate error may be at most 1 dB above sinc25; worst window at most 3 dB. Reference convergence must be at least 20 dB below the candidate error.',
    '',
    '## Methods and basic checks',
    '',
    'The degree-3 and degree-5 B-spline coefficient arrays solve the discrete sample-reconstruction equations using double-precision conjugate gradients. Finite samples use clamped coefficient extension; loop regions use a separate cyclic coefficient solution. The chosen equations are `c[n-1]+4c[n]+c[n+1]=6x[n]` and `c[n-2]+26c[n-1]+66c[n]+26c[n+1]+c[n+2]=120x[n]`. Integer reconstruction passed across all selected samples and periodic loops; see `spline-reconstruction.csv` for exact maxima. The impulse and DC rows are included there as well.',
    '',
    'The optimized FIR rows are solved independently for each phase from a fixed, small KKT system, with a unit-sum constraint. Runtime phase selection mirrors the accepted sinc25 table: the source phase is advanced by one half sample, the 12-bit table row and remaining fraction are used, and the tap window follows the same centered geometry. A separately regenerated Hann-sinc25 table reproduces the existing production raw windows within 4.95e-6 maximum sample error and −128.27 dBFS worst RMS; this bounds phase/data alignment error for this standalone evaluator.',
    '',
    'The 4095-vs-2047 reference difference stayed at least 20 dB below every candidate error in the 54 primary windows. All nine loop-entry probes also passed that reference margin; their worst margin is reported in `loop-entry-windows.csv`.',
    '',
    '## Static frequency/phase results',
    '',
    '| Candidate | Max gain error through 0.20 | Through 0.40 | At 0.45 | Passes 0.1 dB through 0.40? |',
    '|---|---:|---:|---:|---|',
]
for d in ('3', '5'):
    s = static['spline'][d]
    lines.append(f"| B-spline degree {d} | {s['0.2']['max_abs_gain_error_db']:.4f} dB | {s['0.4']['max_abs_gain_error_db']:.4f} dB | {s['0.45']['max_abs_gain_error_db']:.4f} dB | no |")
for taps in ('13', '17', '21', '25'):
    s = static['fir'][taps]
    lines.append(f"| WLS FIR {taps} taps | — | {s['0.4']['max_abs_gain_error_db']:.4f} dB | {s['0.45']['max_abs_gain_error_db']:.4f} dB | {'yes' if s['0.4']['max_abs_gain_error_db'] <= 0.1 else 'no'} |")
lines += [
    '',
    'The spline maximum occurs at half phase and the high end of each band. Degree 5 is substantially better than degree 3 but still misses the 0.1 dB budget by 1.44 dB at 0.40 cycles/sample. For WLS FIR, 17/21/25 taps pass the static 0–0.40 gain check, while 13 taps miss it; the transition response degrades quickly near 0.45 and is intentionally not treated as a passband.',
    '',
    '## Real-sample window results',
    '',
    '| Candidate | Nonquiet mapped/down windows | Median error change vs sinc25 | Worst change | Reference gate |',
    '|---|---:|---:|---:|---:|',
]
for kind in ('spline3', 'spline5', 'fir13', 'fir17', 'fir21', 'fir25'):
    q = quality[kind]
    lines.append(f"| {kind} | {q['n_nonquiet_mapped_and_down']} | {q['median_delta_vs_sinc25_db']:+.2f} dB | {q['worst_delta_vs_sinc25_db']:+.2f} dB | {q['reference_gate_pass_count']}/{q['reference_gate_total']} |")
lines += [
    '',
    'Positive values mean more RMS error than current sinc25. The 13/17/21-tap designs fail the 1 dB median budget by wide margins. The 25-tap design is closest, but its mapped/down median is +1.03 dB (the frozen limit is +1.00 dB) and it retains all 25 multiply-accumulates, so it has no tap-count performance case. Its worst primary window is hi-hat interior at half-step, +2.34 dB. At one-octave-up all candidates are evaluated separately; that case is not used to hide the lower-rate error.',
    '',
    '### Separate octave-up diagnostic (transpose factor 2.0)',
    '',
    'This diagnostic is summarized from all nonquiet factor-2 rows. It is deliberately excluded from the mapped/down acceptance gate above, but remains visible because the fixed sinc25 cutoff can behave differently at higher playback rates.',
    '',
    '| Candidate | Nonquiet octave-up windows | Median error change vs sinc25 | Worst change |',
    '|---|---:|---:|---:|',
]
for kind in ('spline3', 'spline5', 'fir13', 'fir17', 'fir21', 'fir25'):
    q = quality[kind]['per_factor']['2.0']
    lines.append(f"| {kind} | {q['n_nonquiet']} | {q['median_delta_vs_sinc25_db']:+.2f} dB | {q['worst_delta_vs_sinc25_db']:+.2f} dB |")
lines += [
    '',
    'Both spline variants are much farther from the long reference than sinc25 in mapped/down windows: medians are +11.56 dB (degree 3) and +8.45 dB (degree 5). Their exact integer reconstruction does not translate into good fractional-delay fidelity. The first-loop-wrap windows expose additional transition risk: the degree-3/5 spline worst error is about +39 dB versus sinc25; 25-tap WLS stays within +1.85 dB there, but the primary gate still fails.',
    '',
    '## Decision',
    '',
    '- **A, prefiltered cardinal B-spline: stop.** Exact finite/periodic integer reconstruction passed, but both degrees fail the predeclared 0.1 dB passband budget and real-sample error contract. No playback integration or performance benchmark is justified.',
    '- **B, short WLS FIR: stop this design.** Thirteen taps fail even the static 0.1 dB budget. Seventeen and twenty-one taps pass only through the declared 0.40 band but fail the real-sample contract. Twenty-five taps nearly matches the quality contract but narrowly misses its median limit and does not reduce arithmetic. Do not claim 13/17/21/25 tap reductions preserve current sinc25 quality.',
    '- **No performance tests were run for A or B.** B’s target band is intentionally limited to 0.40 cycles/sample and does not imply rate-scaled anti-aliasing. The next independent candidate is E (fixed-coefficient IIR W=2/4), as ordered in the supplied plan.',
    '',
    '## Limits and reproduction',
    '',
    'This is double-precision offline analysis at 44.1 kHz using five selected SF2 samples and the existing 54 primary windows plus nine loop-entry probes. It is not a full SF2/MIDI render, float-table quantization test, real-time test, generalized listening result, or proof for other SoundFonts/loop zones. The periodic spline coefficients are built per selected loop region; that correctly exposes the per-zone preprocessing/storage problem rather than assuming one coefficient transform can serve every loop.',
    '',
    'Set `FLUIDSYNTH_FD_DATA_ROOT` to the directory containing the external `sf2-samples/` and `sf2-raw/` inputs, then run `./run.sh` from this folder. The five extracted SF2 samples and prior reference CSVs are not copied into this report archive. Their source SF2 SHA-256 is `ee51d2c4b1525e70f19a45909c4fd7a2e26d91d115fa89dbf5a6bc413d8b9bf3`; expected input file hashes are listed in `manifest.json`. The default source-worktree path can be overridden with `FLUIDSYNTH_FD_SOURCE_ROOT`.',
]
(ROOT / 'REPORT.md').write_text('\n'.join(lines) + '\n')

source_root = Path(os.environ.get(
    'FLUIDSYNTH_FD_SOURCE_ROOT', ROOT.parent.parent / 'fluidsynth-worktrees/algorithm-route-profile-20261016'))
input_root = Path(os.environ.get(
    'FLUIDSYNTH_FD_DATA_ROOT', ROOT.parent.parent / 'fluidsynth-analysis-20261015/cubic-sinc25'))
def sha256(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()

input_files = [input_root / 'sf2-samples/manifest.json', input_root / 'sf2-rate-context.json']
input_files += sorted((input_root / 'sf2-samples').glob('*.s16le'))
input_files += sorted((input_root / 'sf2-raw').glob('*.csv'))
output_files = [ROOT / name for name in (
    'make_metadata.py', 'fractional_delay_screen.cpp', 'run.sh', 'summarize.py',
    'samples.csv', 'spline-static.csv', 'fir-static.csv', 'fir-coefficients.csv',
    'spline-reconstruction.csv', 'baseline-reproduction.csv', 'candidate-windows.csv',
    'loop-entry-windows.csv', 'summary.json', 'REPORT.md', 'fractional_delay_screen')]
manifest = {
    'date': '2026-10-16',
    'baseline_source_commit': subprocess.check_output(
        ['git', '-C', str(source_root), 'rev-parse', 'HEAD'], text=True).strip(),
    'baseline_source_clean': not subprocess.check_output(
        ['git', '-C', str(source_root), 'status', '--short'], text=True).strip(),
    'analysis_compiler': subprocess.check_output(['c++', '--version'], text=True).splitlines()[0],
    'compile_flags': '-O3 -DNDEBUG -std=c++17 -ffp-contract=off -Wall -Wextra -Wpedantic',
    'machine': platform.platform(),
    'sf2_sample_source_sha256': 'ee51d2c4b1525e70f19a45909c4fd7a2e26d91d115fa89dbf5a6bc413d8b9bf3',
    'inputs': {str(p.relative_to(input_root)): sha256(p) for p in input_files},
    'outputs': {p.name: sha256(p) for p in output_files},
    'performance_benchmarks_run': False,
    'production_source_changed': False,
}
(ROOT / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print('wrote REPORT.md and summary.json')
