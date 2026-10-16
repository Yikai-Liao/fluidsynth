# E2 first-gate: observable 64-frame IIR blocks

Status: the double-precision W=4 prototype passes this diagnostic gate. It is not yet a production optimization result.

## Frozen comparison

- Baseline source: composite commit `6be67a906b9d679ea930c8841fd0cac0d8a0a25e` (the already accepted IIR coefficient reuse, Dattorro tap wrap and sinc LUT changes).
- Release double build, GCC 14.2.0-19, x86_64 Intel Xeon Gold 6140 KVM, one pinned CPU (CPU2), 44.1 kHz, 64-frame blocks. Production IIR source uses `-O3 -DNDEBUG -fno-math-errno -ffast-math`; the wrapper TUs use the same arithmetic flags. LTO is disabled.
- S0 calls `fluid_iir_filter_apply()` from a separate TU. S1 uses the stable-coefficient scalar recurrence; W2 and W4 use the expanded recurrences with per-call preparation included. Their wrappers are externally visible and `noinline`.
- For each measured arm, the input buffer is copied before timing. The timer surrounds all block calls. After timing the harness hashes every output sample, checks every sample and final state for finiteness, and records both history values and the gain state. Each arm's output hash repeats identically across all seven repetitions. Disassembly shows W4 reads four inputs before overwriting them and stores all four outputs per group; the store loop remains present. The S0/S1/W2/W4 assembly excerpts and actual compile commands are saved here.

## Diagnostic matrix and frozen limits

Eight cutoff/Q designs cover the ordinary cases plus legal extrema: 1500, 6000, 9000 and 13500 absolute cents (about 19.45 Hz, 262 Hz, 1.65 kHz and the 19.845 kHz/0.45-sample-rate cap), with Q values 0.001, 0.70710678, 20 and 44.7. Eight deterministic inputs cover broadband noise, quiet noise, impulse, and sine tones from 20 Hz through 19.845 kHz. Each 131,072-frame stream is run through all four arms.

The frozen quality gates are active-signal relative RMS error at most −60 dB, quiet absolute RMS error at most −80 dBFS, static sine gain difference at most 0.1 dB, and no nonfinite sample or state.

| Arm | Worst active relative RMS error | Worst quiet absolute RMS error | Maximum absolute sine gain difference | Finite |
|---|---:|---:|---:|---|
| W2 | −215.59 dB | −335.05 dBFS | 1.4e−10 dB | yes |
| W4 | −179.31 dB | −288.72 dBFS | 3.7e−9 dB | yes |

Both variants pass the declared first-gate quality bounds by a wide margin. This measures the local recurrence against the exact production scalar path under the current double build; it does not establish full voice or float behavior.

## Prepared-cost-included microbenchmark

The harness processes 8,192 64-frame calls (524,288 samples) per arm and design, using the same deterministic mixed-tone/noise input each time. Seven repetitions use rotated and reversed arm order. Each call prepares the W2/W4 polynomial coefficients inside the timed wrapper; buffer allocation and restoring the raw input occur outside timing. S1 isolates branch removal from the production path, while S0 remains the adoption baseline.

| Design | W4 / S0 median | Pair range | W2 / S0 median |
|---|---:|---:|---:|
| low_hiq | 0.647 | 0.527–0.937 | 0.677 |
| mid_lowq | 0.700 | 0.627–0.901 | 0.742 |
| mid_highq | 0.646 | 0.547–0.762 | 0.663 |
| near_nyquist | 0.758 | 0.649–1.304 | 0.661 |
| low_minq | 0.654 | 0.584–0.977 | 0.669 |
| low_maxq | 0.901 | 0.576–1.049 | 0.837 |
| high_minq | 0.683 | 0.557–1.116 | 0.778 |
| high_maxq | 0.583 | 0.125–0.884 | 0.556 |

Lower is faster. W4's medians are favorable in all eight designs, but several seven-pair ranges cross 1.0 and timing spread is large. Treat this as a credible reason to test real rendering, not as a stable speedup claim. The measured calls include polynomial preparation, the eligibility check and tail; the polynomial preparation is not hoisted into a cache.

## Reproduction artifacts

- `diagnostic-source/` contains the four benchmark TUs and interface used here.
- `diagnostic-instrumentation.patch` shows the temporary independent-TU hook and test target; it is not production code.
- `quality.csv`, `bench.csv`, `summary.json`, `manifest.json`, `compile-commands.json`, and `assembly-{s0,s1,w2,w4}.txt` preserve raw results and provenance.
- `summarize_gates.py` validates the row counts, full-output hash stability, finite checks and summarizes the measurements.

Next gate: use an independent diagnostic build to measure stable-call/sample coverage and standard-filter time in representative real MIDI/SF2 renders. Only if that coverage supports a meaningful full-render budget does the minimal double-only W4 path proceed to production integration.
