# Route C: four-line FDN feasibility screen

Date: 2026-10-16. This is an isolated optional-engine feasibility screen, not an equivalent replacement for the existing eight-line FDN.

## Question and prototype

The current FDN accounts for 11.21% of render-stage samples in the selected 44.1 kHz profile. A four-line network could reduce the delay, damping, feedback, and output work. The implementation retains FluidSynth's rank-one Householder feedback structure, so it avoids the dense 4x4 matrix cost called out in the plan. The four heuristic prime delays are 1201, 1543, 1993, and 2267 samples (sum 7004), close to the existing eight-line sum of 7010. This is only a workload-reduction prototype; delay lengths, matrix, and output gains were not offline-optimized.

Both worktrees started at `6be67a906b9d679ea930c8841fd0cac0d8a0a25e`. Candidate `explore/fdn-four-line-20261016` temporarily changed `NBR_DELAYS` to 4 and supplied the four delay lengths. Control `explore/fdn-eight-control-20261016` used the normal eight-line implementation. No changes were made to the main checkout, and no branch was pushed.

## Build and test

- Release double, GCC 14.2, `-O3 -DNDEBUG`, threads enabled, OpenMP disabled; Intel Xeon Gold 6140 virtual machine with six vCPUs; the microbenchmark was pinned to CPU 2.
- A test-only harness instantiated `fluid_revmodel_fdn` directly at 44.1 kHz with room size 0.7, damping 0.35, width 1.0, and level 0.8.
- Impulse response: one unit impulse followed by silence for 12 seconds; 529,280 frames. Both outputs were retained as interleaved binary64 PCM.
- Cost screen: 2,048 blocks of 64 samples, seven repeated passes per timing, with the model reset before each pass. Two measurements were run in opposite order to limit order bias.

This did not run the full FluidSynth test suite, MIDI workloads, float build, realtime callback tests, or listening evaluation.

## Cost result

| Run | Eight-line median | Four-line median | Four/eight |
|---|---:|---:|---:|
| 1 | 5.90828 ms | 3.34690 ms | 0.5666 |
| 2 | 5.58664 ms | 3.26821 ms | 0.5850 |

The average of the two per-run medians is 5.74746 ms for eight lines and 3.30756 ms for four, a 42.45% reduction in this isolated FDN call. Applying that ratio to the 11.21% whole-render profile share gives an idealized whole-render ceiling near 4.76%, before any new-engine setup or integration cost. This is directional microbenchmark evidence, not an end-to-end speedup claim.

## Quality gates

The 12-second impulse energies were 13,360.2207 (four-line) and 54,032.3957 (eight-line), a raw level difference of **−6.068 dB**. Matching integrated energy would require a scalar gain of 2.011. The spectral comparison below already normalizes each output by its own integrated energy, so this gain cannot remove the remaining coloration.

- Normalized 1/3-octave band energy differed by more than 1 dB in 17 of 28 analyzed bands. The largest absolute delta was −4.405 dB at 635 Hz; at 2.54 kHz it was +2.889 dB. This fails the plan's ±1 dB coloration screen after loudness normalization.
- Left/right energy-ratio delta was −0.645 dB, beyond the 0.2 dB screen.
- Maximum absolute IACC delta across five time windows was 0.038, within the 0.05 screen.
- The exploratory band-energy decay fit reported T20 differences above 5% only at 31.5 Hz (+10.91%). These are extrapolated fits from the 12-second capture, not measured T30/60 dB decays; they are secondary to the clear level, coloration, and stereo-energy failures.

The analyzer applies a one-third-octave biquad and reverse-integrates block energy. The reported T20 fits use samples between −5 and −25 dB, and EDT fits use 0 to −10 dB. The capture's unfiltered EDC reaches about −38 dB by its end. No echo-density or long-tail stability analysis was done.

## Decision

Stop this four-line parameter set before full rendering or integration. It has promising isolated compute reduction, but its level, normalized spectrum, and left/right energy miss predeclared quality gates. The screen does **not** show that every offline-optimized four-line FDN must fail; it shows that this heuristic four-line configuration is not a candidate for retention. No production source change is accepted.

## Artifacts

- `candidate.patch`: exact temporary source and test-target diff.
- `test_fdn_probe.cpp`, `analyze_fdn.cpp`: reproducible harness and analyzer source.
- `fdn4-impulse.f64le`, `fdn8-impulse.f64le`, `fdn-quality.txt`: raw outputs and analyzed metrics.
- SHA-256 values are recorded in the adjacent `manifest.txt`.
