# E: IIR short-block recurrence screen

Date: 2026-10-16. This is an isolated fixed-coefficient kernel screen for the supplied plan's candidate E, DFII recurrence expansion at widths 2 and 4. It makes no production change. The original sample-by-sample path remains the reference and the intended path for changing coefficients.

## Frozen setup

- Baseline source worktree: `algorithm-route-profile-20261016`, commit `6be67a906b9d679ea930c8841fd0cac0d8a0a25e`, clean. The main checkout remains unchanged at `ae31c51454c4d2bab98c9d8ce65e9a5eafcf741c`.
- The standalone evaluator follows FluidSynth's DFII form: `w[n] = x[n] - a1*w[n-1] - a2*w[n-2]`; `y[n] = b02*(w[n]+w[n-2]) + b1*w[n-1]`. It precomputes the real-arithmetic recurrence polynomials, retains the existing float coefficient values, quantizes each expanded state to the selected sample type, and uses the scalar path for a short tail.
- Quality vectors: 131,072 frames at 44.1 kHz, deterministic noise at −12 dBFS peak, sine input at −14 dBFS peak across 20 Hz, 100 Hz, 1 kHz, 6 kHz, 0.30 Fs and 0.45 Fs, and an impulse with a 65,536-frame tail check. Six cutoff/Q pairs cover approximately 20 Hz–10 kHz and Q 0.707–40, including low-cut/high-Q and near-Nyquist cases.
- Dynamic transition vectors change cutoff and Q smoothly at 1, 2, 3, 4, 5, 31, 64 and 127-frame runs. At run length 1 the candidate uses only the scalar fallback; other runs compare state continuity across coefficient changes. The 1-frame row is therefore a fallback sanity check, not evidence for a block speedup.
- The quality gates were taken from the plan: active relative error ≤ −60 dB, quiet references (< −60 dBFS RMS) absolute error ≤ −80 dBFS, static magnitude difference ≤ 0.1 dB, and finite output/state. These are project screening budgets, not universal audibility thresholds.
- Kernel timing used GCC 14.2, `-O3 -DNDEBUG -std=c++17 -ffp-contract=off`, Intel Xeon Gold 6140, pinned to CPU 2, seven candidate/scalar pairs per precision and width. It processes 64-frame calls with coefficients and expanded polynomials prepared outside the measured loop. The result is a kernel-only upper bound; it excludes real voice filtering, coefficient updates, dispatch and full-render effects.

## Results

| Type | Width | Worst active relative error | Worst gain delta | Worst quiet absolute error | Median candidate/scalar time | Range |
|---|---:|---:|---:|---:|---:|---:|
| float | 2 | −37.59 dB | 0.113 dB | −123.64 dBFS | 1.132 | 1.129–1.139 |
| float | 4 | −37.70 dB | 0.112 dB | −125.22 dBFS | 0.786 | 0.714–0.870 |
| double | 2 | −224.91 dB | <0.000001 dB | −294.26 dBFS | 0.722 | 0.700–0.753 |
| double | 4 | −212.34 dB | <0.000001 dB | −282.17 dBFS | 0.604 | 0.554–0.642 |

Both float widths fail the mandatory active-error budget at 20 Hz for the lowest cutoff (1500 cents, Q=20), with the 20 Hz gain difference also just over 0.1 dB. The float noise rows at the same design are −58.79 dB (W=2) and −59.87 dB (W=4). The terminal state differs from the scalar reference by as much as 1,152 for W=2 and 1,073 for W=4 in this stressed case. All quiet-case absolute errors are below −80 dBFS. Double passes these measured numerical gates. The float dynamic-change rows with at least one full expanded block are around −100 dB relative error; coefficient transitions themselves do not remove the static low-frequency/high-Q failure. Every tested output and terminal state is finite.

W=2 is slower than scalar in float (median ratio 1.132). W=4 is faster in the seven float pairs (median 0.786, range 0.714–0.870). Double medians are 0.722 (W=2) and 0.604 (W=4), with all seven measured pairs below 1 for both widths. This fixed-coefficient kernel result excludes the work needed to identify stable coefficient spans or prepare block parameters, and does not establish an end-to-end gain.

## Decision

**Stop E at the quality gate.** The target builds include float and double modes; the float candidate violates two frozen quality limits in a stressed but legal low-cut/high-Q case. Do not proceed to integration, full-render performance, or optimization of the failing candidate. No production source was changed. The saved benchmark is directional evidence only and does not count as a validated optimization.

## Reproduction and limits

Run `./run.sh` here to rebuild, pin the screen to CPU 2, regenerate the CSVs, summary and manifest. Set `IIR_SCREEN_CPU` to choose a different available CPU. Raw quality data are in `final/quality.csv`, coefficient-transition data in `final/dynamic.csv`, and all seven timing pairs in `final/bench.csv`.

This short screen does not exercise a full FluidSynth render, real voice modulation trajectories, all supported parameter combinations, audio callbacks, SIMD vectorization, or hardware other than the recorded host. It decides only that this particular W=2/4 expansion is not ready to advance because its float quality gate fails.
