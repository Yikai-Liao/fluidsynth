# D: rate-dependent sample prefilter screen

Date: 2026-10-16. This is an isolated quality screen for the supplied plan's candidate D, a near-2× anti-alias sample layer. It makes no production change and makes no runtime-performance claim.

## Frozen setup

- Baseline source: measurement composite `6be67a906b9d679ea930c8841fd0cac0d8a0a25e`; main checkout remains at `ae31c51454c4d2bab98c9d8ce65e9a5eafcf741c`.
- Five extracted MuseScore General SF2 samples (piano, strings, saw lead, closed hi-hat and cymbal). Step is fixed at exactly 2 source samples/output frame. Eighteen 882-frame attack, interior/sustain, loop-seam, release and tail windows were generated, plus synthetic periodic tones at four source phases.
- First stage isolates prefiltering without decimation. The FIR is a normalized 4095-tap Kaiser-windowed low-pass, β=9.5 and cutoff 0.25 cycles/source-sample. The declared passband is 0–0.24, transition is 0.24–0.26, and stopband is 0.26–0.50. The same production sinc25 phase-table geometry then reads the filtered layer. The candidate is compared with the unfiltered sinc25 path and a cutoff-scaled, long-Hann-sinc reference.
- The second stage samples the filtered layer at every second source sample and changes the read step from 2 to 1. To preserve loop timing, loop boundaries must align to the decimated grid. The piano loop length is odd (81,107 frames), so that sample uses the full-rate filtered fallback; strings (30,844) and saw (1,946) loops align and use the reduced layer. One-shot samples use the reduced layer.
- The common quality gates are maximum absolute passband gain error ≤0.1 dB, stopband alias/input ≤−80 dB, active-window reference margin ≤−20 dB, and quiet-window absolute RMS error ≤−80 dBFS. The project plan also requires loop semantics and storage to be reported. These are screening thresholds, not universal audibility limits.
- GCC release build: `-O3 -DNDEBUG -std=c++17 -ffp-contract=off -Wall -Wextra -Wpedantic`. The long reference uses 65,535 taps normally and extends to 524,287 taps only on active windows that did not meet convergence at the shorter reference. Quiet windows are judged by absolute RMS and peak error instead of an unstable relative ratio. Preparation time is one unpinned run, not a performance comparison.

## Results

| Configuration | Active SF2 windows | Median error change vs sinc25 | Worst change | Worst active reference margin | Quiet worst RMS / peak | Static passband max error through 0.24 | Stopband worst alias/input from 0.26 |
|---|---:|---:|---:|---:|---:|---:|---:|
| Full-rate prefiltered layer, no decimation | 16 | −37.84 dB | −14.07 dB | −20.72 dB | −111.05 / −104.61 dBFS | 0.0025 dB | −118.10 dB |
| One decimated layer, with full-rate fallback for odd loops | 16 | −19.37 dB | −9.31 dB | −21.56 dB | −105.74 / −98.72 dBFS | 4.49 dB | −118.10 dB |

Both paths improved every active SF2 window relative to unfiltered sinc25, passed their active reference-convergence margins, remained finite, and stayed below the quiet absolute-error budget. The full-rate prefilter also meets the static passband and anti-alias limits. It does not reduce the sample read rate, however, and its retained full-rate float layer plus loop data costs 3.00× original PCM bytes for one-shot samples, 5.50× for piano, 5.89× for strings and 6.74× for the short saw sample in this conservative attack/periodic layout.

The decimated layer retains the strong stopband suppression, but its static response misses the passband limit: the worst error is −4.49 dB at 0.2395 cycles/source-sample and phase 0.75. At 0.22 the error already reaches −0.166 dB at phase 0.75. The declared passband extends through 0.24, so this cannot be accepted as a quality-preserving layer. Real-SF2 windows did not expose that static-tone loss because the selected windows do not adequately cover all legal high-frequency phases.

The odd piano loop was kept on the full-rate filtered path; no fractional-length loop conversion or altered loop period was attempted. Even-boundary strings and saw loops completed attack, sustain, first-wrap and release probes on the reduced layer without non-finite output. The reduced-layout storage ratio, including original int16 PCM and the conservative loop variants, is 3.45× for strings, 3.87× for saw and about 3× for one-shot samples. The preparation run took 5.72 seconds for all five samples; it is a single-run directional figure.

## Decision and limits

**Stop D's decimated layer at the static passband gate.** The un-decimated prefilter is a promising anti-alias quality result, but it adds a large sample representation and does not demonstrate a speed benefit. The reduced layer improves the selected SF2 windows and aliases well, yet loses up to 4.49 dB within the declared passband (and is already −0.166 dB at 0.22 cycles/source-sample). Do not proceed to level crossfades, pitch modulation, high-polyphony tests or performance measurement for this form. A different interpolation/equalization design would be a separate candidate.

This is a first-stage screen at one exact step, on five samples and four static phases. It does not cover all SF2 zones sharing PCM, arbitrary loop-boundary parity, vibrato, transitions, a full MIDI render, callback latency or target-device performance. No performance optimization is accepted from this experiment.

Run `./run.sh` in this folder to rebuild and reproduce the screen. Raw outputs are `windows.csv`, `decimated-windows.csv`, `static.csv` and `storage.csv`; input and source hashes are in `manifest.json`.
