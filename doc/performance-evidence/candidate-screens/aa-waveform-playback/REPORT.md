# Antialiased waveform playback prototype: early stop

## Decision

Stop the `libadawata` Type1/Type2/Type5 prototype at the static periodic-waveform gate. It does not meet the predeclared passband requirement, so dynamic modulation, SoundFont, and real-time callback testing were not started. No production code change or performance candidate results from this quality-only experiment.

## Screen

The offline probe compares `libadawata::Oscillator<FilterType::kType1/kType2/kType5>` with FluidSynth's optimized 25-tap sinc phase-table kernel and a Fourier reference. Each case uses a 4096-sample periodic sine, saw, or 25%-duty pulse table at 44.1 kHz, a coherent 16384-sample analysis window, and fundamental bins 128, 512, 2048, 4096, 6144, and 7000. The analyzer reports unaligned output-reference error power, a diagnostic best fractional phase advance, legal-harmonic magnitude error, and nonharmonic spur level. Positive `error_gain_db` means less error than sinc; `compensated_gain_db` uses an offline per-tone best-advance fit and is not a real-time correction result.

The predeclared passband limit is <=0.1 dB. All three filter types exceed it on the sine's fundamental by bin 2048 (5.51 kHz): Type1 is -0.532 dB, Type2 -0.454 dB, and Type5 -0.453 dB. At bins 4096–7000 (11.0–18.8 kHz), deviations range from 1.82 to 7.19 dB. For saw/pulse cases, raw reference-error improvement is mixed and frequently negative; offline phase fitting improves some cases but cannot erase the passband failures or establish a dynamic phase strategy.

This is an early screening result, not a SoundFont suitability claim. It used synthetic periodic tables only; it does not cover real sample attacks, loops, bend/vibrato, mipmap transitions, CPU tail latency, or memory under a SoundFont workload. Those stages were skipped because the first quality gate failed. The source probes and CSV summaries are preserved beside this note (`aa_static_probe*.cpp`, `analyze_static.cpp`, `static-screen-summary*.csv`, and `static-screen*/`).

## Reproduction

The saved probes were compiled against the locally checked-out `libadawata` reference and the `perf/sinc-phase-lut-20261016` worktree at `84936b5d`. The final phase-fit analyzer is `analyze_static.cpp`; it reads the saved `*-aa.f64`, `*-sinc.f64`, and `*-reference.f64` files. There are no production source modifications on `explore/aa-waveform-playback-20261016`.


## Branch note

This branch was based on the validated sinc phase-table commit `84936b5d`; the AA waveform study made no production-source changes. Raw `.f64` waveform captures and the third-party reference checkout are not duplicated here; CSV summaries and probe sources are included.
