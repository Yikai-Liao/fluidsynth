# E2: double-only stable IIR W4 validation

Date: 2026-10-16
Status: all planned acceptance gates pass. The implementation and this evidence are on the fork branch `perf/iir-stable-block-w4-20261016`; no upstream PR has been opened.

## Change under test

The candidate adds a four-sample recurrence for the double build's standard low-pass filter when both coefficient-smoothing counters are zero. It keeps the existing coefficient type, gain-update order, state writeback, denormal cleanup and scalar tail. The float build and all smoothing calls stay on the existing scalar path. Custom filtering still runs first; the standard-filter block path remains eligible afterward.

The frozen comparison baseline is composite commit `6be67a906b9d679ea930c8841fd0cac0d8a0a25e`. The fork review branch adds the implementation and test commit to target master `ae31c51454c4d2bab98c9d8ce65e9a5eafcf741c`; the exact source-patch SHA-256 is recorded in [`manifest.json`](manifest.json). It changes only `fluid_iir_filter_impl.cpp`, the test registration, and the new stable-block test. It introduces no API, allocation, lock, SIMD library or platform-specific dependency.

The composite baseline is seven commits ahead of target `origin/master` (`ae31c51454c4d2bab98c9d8ce65e9a5eafcf741c`) and includes the already accepted voice-reuse, IIR coefficient-reuse, Dattorro tap and sinc phase-table work. Performance claims below compare the E2 increment against the composite baseline, so those existing gains are not counted again. The source-only E2 patch applies cleanly to target master, and its IIR unit test passes there; see `upstream-check.json`.

## Quality and state

The production-flag double build (`-O3 -DNDEBUG -fno-math-errno -ffast-math`, GCC 14.2) passed the frozen diagnostic matrix. Across 64 rows, W4's worst active relative RMS error was −179.31 dB, worst quiet absolute error was −288.72 dBFS, and maximum static sine gain difference was 3.69e−9 dB. The thresholds were −60 dB, −80 dBFS and 0.1 dB. All outputs and states were finite. Full row-level evidence is in [`first-gate/quality.csv`](first-gate/quality.csv) and [`first-gate/summary.json`](first-gate/summary.json).

The unit test covers every count from 0 through 64, four sample rates, low through near-limit cutoffs, Q values from 0.001 through 44.7, deterministic noise, quiet noise, impulses, tails and custom-filter ordering. It also drives real cutoff/Q generator callbacks through partial smoothing, verifies the scalar fallback advances the counters, then compares the next stable W4 call with the scalar recurrence. The stage-two coverage probe found 49–100% of active standard-filter samples stable at call entry across Maestro_1, POP909_010 and All The Small Things; 97.6–98.1% of calls also carried a gain ramp. Timing-instrumented coverage remains diagnostic, not a performance claim.

Five existing IIR renders (`render1415`, `1417`, `1424`, `1427`, `1481`) were bitwise equal in double and float. The primary real-render pilot had 9/10 exact PCM hashes; its sole difference was one double sample at 1.42e−14 absolute, −291.17 dB relative RMS and −336.40 dBFS absolute error. Extended 128/256-frame, 48/96 kHz and custom-filter cases were also bitwise equal. Sustained 96-note MIDI and the 30-second Maestro render each had tiny PCM differences; every one-second segment passed the frozen quality gates. The worst differing active segment was −216.83 dB relative / −254.16 dBFS absolute for the sustained-note input, and −267.64 dB relative / −314.36 dBFS absolute for Maestro. Details are in [`issue-render-diff.json`](issue-render-diff.json), [`float-parity/issue-render-summary.json`](float-parity/issue-render-summary.json) and [`expansion/summary.json`](expansion/summary.json).

## Performance

The preregistered primary was All The Small Things, default fourth-order interpolation, effects on, 44.1 kHz, 64-frame writes and a 10-second interval. It used 15 A/A pairs plus two independently ordered 15-pair candidate batches. The primary check below uses the more conservative *setup plus render* interval: create the synth, load the SF2 and MIDI, allocate/touch output, then render. This includes the same initialization work for both arms. Render-only time is reported separately.

| Measurement | Paired ratio geometric mean | 95% CI | Pairs right arm faster |
|---|---:|---:|---:|
| A/A setup plus render | 1.0167 | 0.9922–1.0419 | 5/15 |
| Batch 1 setup plus render | 0.9318 | 0.9110–0.9531 | 14/15 |
| Batch 2 setup plus render | 0.9527 | 0.9197–0.9869 | 11/15 |
| Combined setup plus render | **0.9422** | **0.9235–0.9613** | 25/30 |
| Combined render-only time | 0.9061 | 0.8791–0.9339 | 28/30 |

The preregistered minimum useful gain was 2%, so the upper confidence bound must be below 0.98. The complete setup-plus-render interval passes that gate, both independent batches favor the candidate, and the A/A interval contains 1. All paired primary PCM hashes match. Raw rows and the preregistration are in [`formal-primary/`](formal-primary/); the computed gate is in [`formal-primary/summary.json`](formal-primary/summary.json).

The fixed 15-pair expansion covered 128/256-frame requests, 48/96 kHz, 96 simultaneous note-ons, an active custom low-pass filter, and a 30-second real piece. No scenario had a repeatable slowdown over 3% by its paired 95% interval. Setup-plus-render geometric ratios ranged from 0.8896 to 0.9782; the 30-second Maestro interval was 0.9544–1.0026. Full tables and raw pairs are in [`expansion/summary.json`](expansion/summary.json) and its sibling JSONL files.

Separate five-run process-call latency diagnostics found no repeatable p99 regression over 5% in any guard case. Some measured offline calls exceeded the nominal block duration (for example, 11 baseline and 7 candidate calls at 96 kHz); these are offline process-call observations, not audio-device xrun tests or a real-time guarantee. See [`expansion/callback-summary.json`](expansion/callback-summary.json) for p50/p95/p99/max and deadline counts.

The double-only dispatch did not measurably burden float. In 15 paired 64-frame All The Small Things renders, float candidate/base geometric ratio was 1.0028 (95% CI 0.9716–1.0349), PCM was bitwise equal, and 7/15 pairs favored the candidate. It provides no float speedup; the prior float W2/W4 experiment remains rejected on its quality gate.

## Build and regression checks

- Release double: full CTest, 42/42 passed.
- Release float: full CTest, 42/42 passed.
- Debug double and float: the IIR stable-block and voice-reuse tests passed in both builds.
- AddressSanitizer + UndefinedBehaviorSanitizer double and float: the IIR stable-block and voice-reuse tests passed in both builds.
- Debug interpolation test `test_rvoice_dsp_interpolate` fails in both candidate and clean target-master builds at its order-4 impulse case. The same failure in baseline master shows it is pre-existing and unrelated; the Release interpolation test passes.
- Target-master compatibility: the E2 implementation patch applies cleanly to `origin/master`, and the targeted IIR test passes in a Release double build. No performance number is claimed against master.

The diagnostic production compile command is preserved in [`compile-command.txt`](compile-command.txt). Tests and sanitizers are correctness-only; no sanitizer timing is used.

## Reproduction and materials

Run from this analysis directory after building the matching source revisions. The runners require the external material locations and CPU affinity to be set; they have no machine-specific asset defaults. The logical input paths and SHA-256 identities are recorded in [`manifest.json`](manifest.json):

```sh
export FLUIDSYNTH_E2_ASSET_DIR=/path/to/assets
export FLUIDSYNTH_E2_MIDI_ROOT=/path/to/symusic/tests/testcases
export FLUIDSYNTH_E2_CPU=2
export FLUIDSYNTH_E2_SOURCE=/path/to/fluidsynth-source
export FLUIDSYNTH_E2_BASELINE_BUILD=/path/to/build-baseline
./build_probes.sh
python3 run_formal_primary.py aa
python3 run_formal_primary.py candidate --batch 1
python3 run_formal_primary.py candidate --batch 2
python3 summarize_formal_primary.py
python3 run_expansion.py
python3 summarize_expansion.py
python3 run_callback_diagnostics.py
python3 summarize_callback_diagnostics.py
python3 run_float_regression.py
```

The scripts generate the 96-note MIDI guard input with `generate_high_poly_96.py`. They use MuseScore General SF2 and MIDI files identified by relative logical path and SHA-256 in the manifest. Set `FLUIDSYNTH_E2_ASSET_DIR` and `FLUIDSYNTH_E2_MIDI_ROOT` to the directories containing those inputs. Build directories default to `build-*` here and can be overridden with `FLUIDSYNTH_E2_BASELINE_BUILD`, `FLUIDSYNTH_E2_CANDIDATE_BUILD`, `FLUIDSYNTH_E2_BASELINE_FLOAT_BUILD` and `FLUIDSYNTH_E2_CANDIDATE_FLOAT_BUILD`. Timings were pinned to CPU 2 on an Intel Xeon Gold 6140, with one worker and GCC 14.2.

The candidate source was tested on `explore/iir-block-e2-20261016` at base `6be67a906b9d679ea930c8841fd0cac0d8a0a25e`. The target-master compatibility check used `ae31c51454c4d2bab98c9d8ce65e9a5eafcf741c`. Absolute compiler and linker paths in `compile-command.txt` are retained as captured build provenance; the reproduction scripts themselves use relative build locations or environment overrides.

The fork branch includes the production change, regression test, this report, the PR description, reproduction tools and the primary/expanded render timing evidence. Generated build directories, binary PCM files and high-volume per-callback trace CSVs are omitted from Git; [`archive-manifest.json`](archive-manifest.json) records their SHA-256 and size alongside hashes for the files included in the branch. Callback summary statistics and the aggregate callback-run records remain included. The full local evidence set remains indexed by [`manifest.json`](manifest.json).
