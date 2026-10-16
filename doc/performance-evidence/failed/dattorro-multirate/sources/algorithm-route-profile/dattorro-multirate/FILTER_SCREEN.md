# Route B: wet-only multirate reverb screen

Date: 2026-10-16. Source baseline: `6be67a906b9d679ea930c8841fd0cac0d8a0a25e` (combined IIR reuse, Dattorro tap single-wrap, sinc LUT). This is a bounded feasibility screen, not a production change or full-render benchmark.

## Filter and latency tradeoff

A Kaiser-windowed, linear-phase low-pass was evaluated for 88.2 kHz input downsampling to 44.1 kHz and two-channel wet upsampling. The bands were fixed before measurement: passband 0 through the listed edge, stopband 22.05–44.1 kHz, passband ripple at most 0.1 dB, stopband attenuation at least 80 dB. The response was sampled at 4097 evenly spaced frequencies in each band. The upsampler uses the same normalized low-pass with the usual factor of two; symmetric tap pairs are summed before multiplication.

| Passband edge | Taps | Measured ripple | Stopband attenuation | Down+up group delay | Estimated MAC/s |
|---:|---:|---:|---:|---:|---:|
| 16.5 kHz | 89 | 0.0015 dB | 80.59 dB | 0.9977 ms | 5.95 million |
| 17 kHz | 99 | 0.0014 dB | 80.44 dB | 1.1111 ms | 6.62 million |
| 18 kHz | 121 | 0.0016 dB | 80.24 dB | 1.3605 ms | 8.07 million |
| 20 kHz | 235 | 0.0016 dB | 80.06 dB | 2.6531 ms | 15.61 million |

The group delay is the pair of linear-phase FIR delays; the impulse-response onset threshold required by the plan was not measured separately. Preserving wet bandwidth through 20 kHz exceeds the 1 ms group-delay budget. The 16.5 kHz option fits that budget but deliberately attenuates the upper 5.55 kHz of the destination's Nyquist band; its high-frequency wet quality would need a separate gate.

## Direct Dattorro microbenchmark

A test-only `process_n` hook let the 44.1 kHz Dattorro instance process 32 low-rate frames per 64-frame 88.2 kHz block. The harness used the 89-tap, 16.5 kHz filter, symmetric down/up FIR loops, double precision, GCC 14.2, Release, and 1024 blocks × 64 frames, repeated five times. Reverb parameters were roomsize 0.7, damping 0.35, width 1.0, level 0.8. It measured the exact production Dattorro class, but did not exercise the full mixer, MIDI, filters, or file output.

One run measured 3.227 ms for full-rate Dattorro, 8.949 ms for conversion alone, and 10.997 ms for conversion plus half-rate Dattorro. Thus conversion alone was 2.77× the full-rate tank time; the complete candidate was 3.41×. A second run measured 3.623 ms versus 12.478 ms (3.44×). These are directional microbenchmark results, not formal performance acceptance. The 5.95 million MAC/s estimate is the symmetric-pair operation count; elapsed time also reflects scalar loop and buffer access costs.

## Decision

Stop this linear-phase FIR Route B candidate at the net-cost screen. Its ideal whole-render saving ceiling was only about 5% from the measured Dattorro share; the measured conversion cost is several times the full-rate tank, so this design has no net-cost path. The 16.5 kHz design also sacrifices the upper wet band, while the 20 kHz version exceeds the group-delay budget. No wet-quality, complete-render, callback-latency, or production-integration claim is made. This does not reject every possible multirate design; it rejects this specified FIR family and the measured scalar implementation. A different filter family would need a new predeclared passband and cost hypothesis.

## Reproduction and artifacts

- `filter_screen.cpp` designs and scans the Kaiser FIR responses.
- `test_dattorro_multirate_probe.cpp` is the internal-class microbenchmark.
- `probe-hook.patch` shows the temporary test-only variable-count processing hook.
- Build: Release double, GCC 14.2. The temporary CMake build directory was under `algorithm-route-profile/dattorro-multirate/build` and was removed after the screen.
