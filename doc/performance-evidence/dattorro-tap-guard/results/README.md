# Dattorro tap-guard candidate

`fluid_dattorro_read_tap()` is called from the Dattorro process loop after the model constructor runs `setup()`. Setup initializes every tap-bearing `tank_delay` and `tank_ap` using `fluid_dattorro_seconds_to_samples()`, which clamps each requested length to at least one sample. The experiment removes the per-tap `size() <= 0` fallback branch; tap wrapping and sample arithmetic remain unchanged.

The candidate preserved PCM byte-for-byte for 60-second renders of `All The Small Things` and `POP909_010` in both double- and float-precision Release builds. Each precision's enabled test suite passed 41/41. Test and PCM artifacts are recorded in `build-check-*.log` and `pcm-parity.json`.

Screening used GCC 14.2, Release double, OpenMP with one thread, one synth core, 44.1 kHz, 64-frame requests, effects on, one 10-second warmup per arm, and three alternating pairs per MIDI. `All The Small Things` candidate/base wall ratios were 0.9757, 0.9751, and 0.9736 (2.49% median improvement). `POP909_010` ratios were 1.0123, 0.9862, and 0.9420 (1.38% median improvement, with a noisy range). Across all six pairs, the pooled median wall ratio was 0.9754 (2.46% improvement; range 0.9420–1.0123); pooled median CPU improvement was 2.49%. This is a targeted screen, not a universal speedup claim.

The worktree is an independent branch based on `fix/voice-reuse-gain`. Exact source diff, build configuration, inputs, library hashes, raw timing rows, PCM captures, and commands are retained in this directory; `manifest.json` records their identities.
