# Freeverb block-processing screen

## Candidate

The experiment reorganized `fluid_revmodel_freeverb::process()` from sample-major traversal to block-major passes: prepare input and zero output arrays, process each comb across the block, then each serial allpass stage across the block, and finally apply wet mixing. The comb and allpass state still advances in the original sample order; the per-sample comb accumulation order is unchanged. The candidate used three `FLUID_BUFSIZE` stack arrays and read the full input block before writing output to preserve input/output aliasing.

## Screen

- Baseline: upstream commit `ae31c51454c4d2bab98c9d8ce65e9a5eafcf741c`; candidate branch `perf/freeverb-block-process-20261016` in its isolated worktree.
- Release double, GCC 14.2, OpenMP enabled; one render core, pinned CPU 0, `OMP_NUM_THREADS=1`.
- One 10-second `All The Small Things` render at 44.1 kHz, 64-frame blocks, Freeverb engine enabled, chorus disabled; three AB/BA pairs.
- All six baseline/candidate PCM hashes matched (`06a46ee8506836d0`). Frames, RMS, peak, and peak RSS were also effectively identical.

| Measure | Candidate / baseline |
|---|---:|
| Wall time, pair 1 | 1.0158 |
| Wall time, pair 2 | 0.9534 |
| Wall time, pair 3 | 0.9785 |
| Wall time, median | 0.9785 |
| Wall time, range | 0.9534–1.0158 |
| CPU time, median | 0.9754 |

The median suggests about 2.2% lower render wall time, but one pair regressed by 1.6% and the range crosses baseline. Under the user's early-stop rule, this is not a clear, stable gain; the candidate was reverted without a larger matrix or full test suite. No performance change is retained or counted.

The exact source diff is preserved in [`candidate.patch`](candidate.patch); runner, probe, manifest, raw timings, and summary are alongside this report. The result applies only to this short Freeverb workload and is a screening result, not a general benchmark claim.
