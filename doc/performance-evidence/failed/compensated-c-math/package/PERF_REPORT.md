# Compensated C: bounded interpolation performance screen

Date: 2026-10-16. This report preserves the original two-way hot-kernel and setup measurements. The subsequent production-cubic / compensated-six-point / LUT-sinc25 comparison supersedes its kernel ratios and adds a bounded complete-render callback probe; use [THREEWAY_FINAL_REPORT.md](THREEWAY_FINAL_REPORT.md) for the final decision and current three-way figures. No FluidSynth production source was changed.

## Result

In this report's original two-way isolated interior-sample run, six-point was 4.13× faster than LUT sinc25 in float and 4.77× in double. The later direct three-way paired rerun measured 4.23× (95% CI 4.07–4.42) and 4.96× (4.76–5.15), respectively; those are the current kernel ratios in [THREEWAY_FINAL_REPORT.md](THREEWAY_FINAL_REPORT.md). Neither result establishes a complete FluidSynth speedup over production cubic.

The quality cost remains the one already measured. In the worst active case, `strings-mid/sustain` at 0.5×, candidate error is −81.6725 dBFS RMS versus −97.3505 dBFS RMS for sinc25: +15.678 dB, or 6.08× the RMS error amplitude. Across active windows the error delta has a +7.282 dB median. These are absolute waveform-error RMS values; the ratios below are render timings.

The table-generation cost is substantial. Preparing the three samples took 0.79–2.65 seconds each, compared with 1–5 ms to read their original int16 PCM. The persistent original PCM plus unique candidate tables occupy 5.00–10.79× the original PCM bytes; the prototype's duplicated cymbal allocation reaches 9×. At the measured single-voice savings, the extra per-sample setup takes 8–25 minutes of continuous single-voice playback to amortize at 44.1 kHz. Reusing one table across simultaneous voices shortens that time in proportion to the number of voices using that sample, but whole-SoundFont load time and asset-wide memory have not been measured.

## Timing results

The following table records the original two-way run: each row is 30 paired 8,192-output-frame renders. The baseline is a 25-tap sinc kernel with a shared 4,096-row phase table and linearly interpolated coefficients. The candidate reads six-point interpolation from the prepared 2× float sample table. Both use the same 32.32 phase and source step. Windows stay away from sample and loop boundaries to isolate the inner kernel. See the final report for the later direct three-way comparison with production cubic.

| Sample and step | Candidate/baseline, float (95% CI) | Candidate/baseline, double (95% CI) |
|---|---:|---:|
| Piano sustain, 0.5× | 0.2306 (0.2185–0.2429) | 0.1759 (0.1624–0.1923) |
| Piano sustain, 1× | 0.2245 (0.2072–0.2448) | 0.2009 (0.1814–0.2237) |
| Strings sustain, 0.5× | 0.2517 (0.2448–0.2645) | 0.2157 (0.2117–0.2207) |
| Strings sustain, 1× | 0.2492 (0.2459–0.2536) | 0.2072 (0.2021–0.2117) |
| Cymbal interior, 1× | 0.2575 (0.2422–0.2733) | 0.2551 (0.2425–0.2698) |
| **Geometric mean across five cases** | **0.2424 (0.2362–0.2489)** | **0.2095 (0.2023–0.2170)** |

Float and double A/A controls rendered the same sinc25 case twice. The second/first timing ratio was 1.0063 for both; 95% intervals were 0.9810–1.0308 (float) and 0.9646–1.0450 (double). Individual-pair ranges were 0.787–1.195 and 0.696–1.281, showing timing noise, but the candidate effect is much larger. The reported intervals are percentile bootstraps over paired ratios on this one machine and run, not cross-machine uncertainty.

| Sample | Candidate preparation | Raw int16 read | Combined persistent storage / original PCM | Single-voice break-even |
|---|---:|---:|---:|---:|
| Piano, 321,868 frames | 2,650.8 ms | 5.0 ms | 10.01× | 54.2M output frames (20.5 min at 44.1 kHz) |
| Strings, 69,013 frames | 792.2 ms | 1.0 ms | 10.79× | 21.6M output frames (8.2 min at 44.1 kHz) |
| Cymbal, 375,831 frames | 2,170.1 ms | 2.9 ms | 5.00× unique; 9.00× allocated in this prototype | 65.6M output frames (24.8 min at 44.1 kHz) |

Break-even subtracts the raw PCM read from candidate preparation, then divides that excess by the measured factor-1 float per-frame saving. It is a single-voice, per-sample estimate; it omits other soundfont samples, startup work, cache effects, and the rest of the synth.

## Method and limits

The harness uses three real SF2 samples (mid-register piano, strings, and a one-shot cymbal), exact 32.32 phase increments, randomized baseline-first/candidate-first order, and 30 pairs per case. It checks candidate output against the separate six-point renderer and, in double precision, checks the phase-table sinc25 interior path against the saved sinc25 render path. Both checks passed. The summarizer validated all 360 raw timing rows and 720 timed-versus-warm output checksums. A/A rows quantify within-run timing variation.

The code builds with `g++ -O3 -DNDEBUG -std=c++17 -ffp-contract=off -Wall -Wextra -Wpedantic`, pinned to CPU 2 on a KVM virtual machine reporting an Intel Xeon Gold 6140 at 2.30 GHz. The final run's shared sinc25 phase table and float/double copies took 7.88 ms to prepare.

This is an interior-kernel upper bound, not a whole-synth benchmark. It omits guard handling, voice scheduling, envelopes, filters, effects, mixing, real block-size scheduling, playback deadlines, and full-SoundFont construction. It also does not resolve loop-seam quality, transient behavior, asset-wide memory, or cold-cache behavior. Those remain necessary before accepting the tradeoff in a production renderer. No performance threshold was added to or substituted for the frozen quality gates.

## Reproduction

From this directory, set `FLUIDSYNTH_C_SCREEN_DATA_ROOT` to the directory containing `fractional-delay-ab/` and `cubic-sinc25/`, then run:

```sh
FLUIDSYNTH_PERF_CPU=2 bash run_perf_screen.sh
python3 summarize_perf_screen.py
```

The raw pairs are in `perf-raw.csv`, setup and storage measurements in `setup.csv`, and deterministic bootstrap summaries in `perf-summary.json` and `perf-summary.txt`. The original strict quality results remain in [REPORT.md](REPORT.md); that report's frozen gates still fail.
