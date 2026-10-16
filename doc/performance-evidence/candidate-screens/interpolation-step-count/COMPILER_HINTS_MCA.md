# Interpolation compiler-hint and phase-step checks

Date: 2026-10-16. All source experiments were local to `perf/interpolation-step-count-20261016`; the worktree is clean at base `20913f66` after reverting them.

## Inlining and vectorization

The active 4th-order cubic path's `interp_cubic` lambda and `fluid_rvoice_get_float_sample<IS_24BIT>()` helper are inline in the optimized GCC assembly. The selected steady-state sample loop has no function calls. Clang's optimized IR also shows the arithmetic in the loop body; its vectorization remarks attribute the missed vectorization to phase/index values used across iterations. An explicit inline hint would therefore duplicate compiler behavior in this hot path without removing a call.

## LLVM-MCA

GCC 14.2 production-flag assembly was analyzed with LLVM-MCA 19.1.7 for `skylake-avx512`, the Xeon Gold 6140 host model. On the steady-state cubic loop, base and integral-step candidate assemblies each contain 48 instructions per output; LLVM-MCA reports a block throughput of 9.5 cycles/output for each. This loop is unchanged by the candidate: the experiment only changes the safe-count calculation before a sample span.

The test-only `#pragma GCC unroll 2` version uses an explicit countdown loop so GCC can form a two-sample body and an odd-tail path. Its modeled body is 93 instructions per two outputs, with block throughput of 18.5 cycles per pair (9.25/output). LLVM-MCA does not model actual cache behavior, memory latency from the loaded soundfont, branch frequencies, or the mix of boundary and steady-state spans; this is a static hint, not a runtime prediction.

Assembly snippets and output are in `baseline-cubic-loop.s`, `candidate-cubic-loop.s`, `candidate-unrolled-cubic-loop.s`, and the matching `*-mca.txt` files. The original production flags and Clang IR commands are recorded in `llvm-mca/*-commands.txt`.

## Runtime screening

The unroll experiment passed all 41 enabled tests in both double and float Release configurations. Its double PCM hashes exactly matched the non-unrolled control for three 60-second MIDI excerpts, both 64- and 8192-frame requests. Five AB/BA repetitions per case produced these candidate/control median wall ratios:

| MIDI | 64 frames | 8192 frames |
| --- | ---: | ---: |
| Maestro_1 | 1.017 (+1.7% slower) | 1.002 (flat) |
| POP909_010 | 0.998 (flat) | 0.957 (4.3% faster) |
| All The Small Things | 1.020 (2.0% slower) | 0.959 (4.1% faster) |

All six observed ratio ranges cross 1.0, so the apparent large-block gains are not stable. The double shared-library text grew by 1,280 bytes; the affected double object grew by 1,326 bytes, while the float object size was unchanged. No unroll hint is retained.

The integral-step safe-count candidate had exact PCM hashes against base for all three 60-second MIDI excerpts at 64 frames. It passed the existing 41-test float and double suites. A deliberately small screen used All The Small Things, 64-frame requests, and three AB/BA pairs after one warm-up per version. The candidate/control median wall ratio was 1.025 (range 1.018–1.044); all three pairs regressed. This candidate is rejected without expanding the run matrix.

Raw and summarized records are in `unroll-paired.jsonl`, `unroll-paired-summary.json`, `unroll-paired-manifest.json`, and `step-count-screen*.jsonl` / `step-count-screen-summary.json`. The paired unroll run used 30 pairs (60 timed renders) plus 12 warm-ups; the progress counter in the first run printed the wrong denominator, but the manifest and summary contain the full 30-pair dataset.

## Decision

No interpolation code change is carried forward from these compiler-hint tests. The fixed coefficient-row and SIMD candidates were already rejected in the main report. Subsequent screening should begin with one representative workload and three pairs, then expand only if the pilot shows a clear, repeatable gain.
