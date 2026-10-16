# Compensated six-point interpolation: three-way assessment

Date: 2026-10-16. This report compares the experimental compensated six-point path with FluidSynth's production four-point cubic interpolator and the LUT-backed 25-tap sinc path. The strict sinc25 replacement gate remains failed. The six-point path also does not qualify as a same-speed quality improvement over the production cubic path, so it remains a research-only candidate.

## Decision

On the matched real-SF2 windows at 0.5× and 1×, the compensated six-point path consistently reduces error RMS versus the production cubic path: all 32 active windows improve, with a median reduction of 8.74 dB. But its isolated kernel takes 2.61× (float) to 2.77× (double) as long as cubic, and the complete 64-frame renderer takes 1.49× the wall time. Its p99.9 callback time is 1.90× the cubic median. That is not a same-speed result.

Against sinc25, the six-point path remains worse on the primary real-SF2 gate: median active error rises 7.28 dB and the worst case rises 15.68 dB (6.08× RMS error amplitude). It is faster than sinc25 in the isolated kernel test, but the end-to-end renderer comparison shows that this speed margin does not make the candidate faster than cubic. No production source was changed.

## Matched real-SF2 quality

The comparison uses 54 matched windows from five SF2 sample groups (piano, strings, saw lead, closed hi-hat, and cymbal), the same saved windows and transpositions, and the same 4095-tap reference. The active windows include attack, interior, sustain, loop seam, and release-exit regions. Errors below are RMS waveform error in dBFS; a negative delta means the six-point error is lower than the comparator's.

| Active transposition set | Windows | Six-point minus cubic error | Six-point minus sinc25 error |
|---|---:|---:|---:|
| 0.5× and 1× (primary) | 32 | median −8.735 dB; range −14.885 to −4.044; 32/32 improved | median +7.282 dB; range +1.886 to +15.678; all 32 worse |
| 0.5×, 1×, and 2× | 48 | median −5.292 dB; range −14.885 to +3.114; 36 improved, 12 worse | median +4.253 dB; range −1.215 to +15.678 |

The primary median reduction versus cubic corresponds to about 2.73× lower RMS error amplitude. Across the primary set, each attack, interior, sustain, loop-seam, and release-exit window is part of the matched comparison; octave-up diagnostics are reported separately because sinc25's fixed cutoff can perform differently there. The 2× set has 12 windows where six-point is worse than cubic, so the improvement does not generalize to every transposition.

Across all three transpositions (48 active windows), the segment breakdown is:

| Window type | Active windows | Better than cubic | Median six-point minus cubic error | Median six-point minus sinc25 error |
|---|---:|---:|---:|---:|
| Attack | 15 | 12/15 | −7.622 dB | +6.319 dB |
| Interior | 6 | 4/6 | −10.872 dB | +6.846 dB |
| Sustain | 9 | 7/9 | −4.395 dB | +3.395 dB |
| Loop seam | 9 | 7/9 | −4.765 dB | +4.519 dB |
| Release exit | 9 | 6/9 | −4.966 dB | +3.042 dB |

The unchanged strict replacement gate is median error increase no greater than +1 dB and worst increase no greater than +3 dB versus sinc25. The measured primary median is +7.282 dB; the worst case is `strings-mid/sustain` at 0.5×: six-point −81.6725 dBFS RMS versus sinc25 −97.3505 dBFS RMS. The 15.678 dB difference is 6.08× RMS error amplitude. This window is well inside a periodic sustain and does not cross a loop boundary. The gate therefore still fails independently of loop-entry behavior.

## Static frequency response and synthetic images

The static scan uses the production cubic coefficient table, the same 4096-row sinc25 LUT, and the compensated six-point polynomial after its symmetric 65-tap source-rate prefilter. Through 0.40 cycles per source sample, maximum absolute gain errors are:

| Method | Maximum absolute gain error, 0–0.40 |
|---|---:|
| Production four-point cubic | 2.30494 dB |
| LUT sinc25 | 0.010721 dB |
| Compensated six-point, 65-tap prefilter | 0.003685 dB |

The candidate meets the static magnitude target over the designed passband. Its transition rolls off above that band: gain error is −3.483 dB at 0.45 and −8.395 dB at 0.49 cycles per source sample. The prefilter is symmetric and centered; the earlier alignment check found no fixed or half-sample delay.

The separate 16,384-frame float sine screen estimates the best-fit fundamental, then reports non-fundamental residual RMS over passband tones through 0.40. Worst residual values by playback rate were:

| Playback rate | Cubic | LUT sinc25 | Compensated six-point |
|---|---:|---:|---:|
| 0.5× | −16.98 dBFS | −78.68 dBFS | −146.07 dBFS |
| 1× | −145.85 dBFS | −140.40 dBFS | −146.07 dBFS |
| 2× | −145.84 dBFS | −140.72 dBFS | −143.84 dBFS |

This synthetic interior residual is an image/spur diagnostic, not a substitute for reference error: fitting and removing the fundamental intentionally excludes its gain and phase error. The table also excludes sample edges, loop transitions, envelopes, and filtering. The matched SF2 windows above are the quality evidence for those behaviors. Double-precision versions are preserved in `image-threeway-double.csv`; the image measurements are not used as a quality-gate pass.

## Kernel timing

The paired hot-kernel screen rendered 8192 frames per timing, with 30 randomized triplets per case across piano and strings sustains at 0.5×/1× and a cymbal interior at 1×. Float and double were measured separately, for 300 triplets total and 900 timed-versus-warm checksum equalities. Ratios are candidate time divided by comparator time; bootstrap intervals are 20,000 paired resamples on one pinned KVM host.

| Precision | Six-point / cubic time (95% CI) | Sinc25 / cubic time (95% CI) | Six-point / sinc25 time (95% CI) |
|---|---:|---:|---:|
| Float | 2.614× (2.516–2.713) | 11.066× (10.596–11.550) | 0.236× (0.226–0.246), 4.23× faster |
| Double | 2.774× (2.658–2.896) | 13.762× (13.145–14.379) | 0.202× (0.194–0.210), 4.96× faster |

Thus six-point costs about 2.6–2.8× cubic kernel time. The candidate is faster than sinc25 in this isolated kernel test; those direct ratios and intervals are in the summary JSON. This test excludes table construction, sample/loop guards, voice scheduling, envelopes, filters, effects, and mixing.

## Full renderer and callback tails

The Release probe ran on a KVM VM reporting an Intel Xeon Gold 6140 at 2.30 GHz and rendered 30,000 consecutive 64-frame calls (43.5 seconds of audio) at 44.1 kHz, with 32 active voices from MuseScore General's GM strings program, built-in reverb and chorus enabled, and CPU affinity pinned to one core. Cubic and six-point each have two repeated runs; sinc25 has one run. Per-method repeat output hashes match, and all output was finite and nonzero. The 64-frame scheduling deadline is 1451 μs.

| Method | p50 | p99 | p99.9 | Observed max | Total render wall time |
|---|---:|---:|---:|---:|---:|
| Production cubic (two-run median) | 8.97 μs | 33.14 μs | 48.39 μs | 185 / 909 μs across runs | 335.8 ms |
| Compensated six-point (two-run median) | 12.45 μs | 69.78 μs | 92.10 μs | 416 / 1827 μs across runs | 500.6 ms |
| LUT sinc25 (one run) | 160.54 μs | 2431.34 μs | 2856.65 μs | 18,684.9 μs | 6430.6 ms |

Relative to cubic, six-point takes 1.388× at p50, 2.106× at p99, 1.903× at p99.9, and 1.491× total render wall time. Both six-point p99.9 values remain below the 1451 μs block deadline, while one observed maximum exceeded it; cubic's two maxima did not. Sinc25's p99 and p99.9 exceed the block deadline. Six-point was slower in both repeated runs; the two-run-per-method screen does not estimate a formal confidence interval, but the observed 49% wall-time increase and near-2× p99.9 do not satisfy the no-significant-slowdown condition. This is a bounded single-host callback probe, not a multi-device xrun study; a single sinc25 run is not an A/A estimate.

## Preparation, memory, and break-even

In the full-render profile, the candidate precomputed tables for two active looped strings samples. Preparation took 1.884–2.052 s per run. The tables occupy 3,207,920 bytes compared with 305,110 bytes of source PCM; keeping both requires 11.51× the source PCM size. Per-sample preparation was approximately 0.90–1.15 s for these two mappings. The full renderer had no 24-bit active samples, so this profile does not validate 24-bit handling.

There is no break-even against cubic in these measurements: the six-point renderer is slower before adding its table setup cost. The earlier isolated one-voice model against sinc25 estimated setup payback after 20.5 minutes for the piano sample, 8.2 minutes for strings, and 24.8 minutes for the one-shot cymbal at 44.1 kHz. Those figures compare only one sample's table-preparation cost with isolated interior-kernel savings; they omit other samples and all complete-render costs and do not apply to a cubic baseline.

## Reproduction and artifacts

From this directory, with `FLUIDSYNTH_C_SCREEN_DATA_ROOT` set to the directory containing `fractional-delay-ab/` and `cubic-sinc25/`:

```sh
FLUIDSYNTH_PERF_CPU=2 bash run_perf_screen.sh
python3 summarize_threeway.py
bash run_image_threeway.sh
python3 summarize_quality_threeway.py
python3 summarize_threeway_evidence.py
FLUIDSYNTH_PERF_CPU=2 bash run_full_renderer_probe.sh /path/to/separate-output-dir
```

The raw quality, kernel timing, static-response, image, and callback measurements are in `quality.csv`, `threeway-raw.csv`, `static-threeway.csv`, `image-threeway-float.csv`, `image-threeway-double.csv`, and the `renderer-*-strings-fx*.csv`/`.json` files. Compact summaries are in `quality-threeway-summary.json`, `threeway-summary.json`, and `threeway-evidence-summary.json`. `manifest.json` records artifact and external-input hashes. `REPORT.md` retains the original strict-gate decision; `PERF_REPORT.md` contains the earlier two-way hot-kernel/setup record and points here for the authoritative three-way results.

The candidate integration exists only in the isolated research worktree `explore/compensated-c-perf-20261016`. The main checkout remains clean at the baseline commit; no production patch or PR was created.
