# PR: Speed up stable double-precision IIR filtering with a four-sample recurrence

The standard low-pass IIR filter currently updates its second-order recurrence once per sample, even when cutoff and Q are stable. This change evaluates four outputs from one set of recurrence terms when both smoothing counters are zero in a double build. It preserves the existing coefficient representation, gain updates, state writeback, denormal cleanup and scalar tail. Float builds and smoothing calls keep the scalar implementation.

In the preregistered 64-frame All The Small Things render, including synth/SF2/MIDI setup, the candidate/base geometric time ratio was 0.9422 (95% CI 0.9235–0.9613) across 30 pairs; two independent 15-pair batches both favored the candidate. Render-only ratio was 0.9061 (95% CI 0.8791–0.9339). PCM hashes matched for every primary pair. The required 2% practical-gain gate passed. Expanded block-size, sample-rate, polyphony, custom-filter and long-render checks showed no repeatable slowdown over 3%.

Release CTest passed 42/42 in both double and float configurations. Target-master Release double IIR test passed 1/1 after the patch applied cleanly. Float rendering was bitwise equal and showed no measurable performance change (ratio 1.0028, 95% CI 0.9716–1.0349). Debug interpolation has a pre-existing order-4 impulse failure on both the candidate and clean target master; Release passes.

The candidate patch and complete evidence are in [`REPORT.md`](REPORT.md), with inputs, environment, patch hashes and supporting result-file hashes in [`manifest.json`](manifest.json). The patch was prepared against target master `ae31c51454c4d2bab98c9d8ce65e9a5eafcf741c`; it remains subject to review.
