# Interpolation whole-block safe-count screen

## Decision

Rejected at the first paired screen and reverted. The candidate preserved PCM, but its timing range crossed baseline on the only representative case. No full suite, broader workload matrix, commit, or fork push was made.

## Candidate

Before computing the exact number of outputs before an interpolation boundary, test whether the remaining output block fits entirely in the current sample span. For `distance = boundary - phase`, positive step `s`, and `r` output slots remaining, `ceil(distance / s) >= r` exactly when `(r - 1) * s < distance`. The prototype formed the product from 32-bit limbs so it would not depend on compiler-specific 128-bit integer types. It also fell back if the original `(distance + s - 1)` intermediate could overflow, preserving the existing unsigned arithmetic path.

An offline property script checked 200,000 random products plus edge values and confirmed both the limb product and the safe-span implication. The production source compiled successfully in Release double mode with GCC 14.2, OpenMP enabled, and optional audio/file dependencies disabled.

## First screen

One 10-second `All The Small Things` render, default fourth-order interpolation, 64-frame blocks, effects on, 44.1 kHz, one core/thread. Baseline and candidate were both built from `ae31c51454c4d2bab98c9d8ce65e9a5eafcf741c`, and the probe was pinned to one available CPU. Three AB/BA pairs produced candidate/base wall ratios `0.965, 1.023, 0.931` (median `0.965`, range `0.931–1.023`); CPU ratios tracked them. Every output hash matched (`fb61cfa69767f603`), and RSS was effectively equal.

The median suggests a possible small benefit, but one pair regressed and the range crosses 1.0. Under the user's early-stop rule this is insufficient evidence to justify more runs, so the candidate was rejected. The prior integral-step fast-count experiment was also rejected after its three-pair screen regressed; it is a separate mechanism and evidence is in [interpolation-step/COMPILER_HINTS_MCA.md](COMPILER_HINTS_MCA.md).

## Artifacts

`candidate.patch` preserves the exact reverted prototype. `run_screen.py`, `screen.jsonl`, `screen-summary.json`, and `screen-manifest.json` preserve the screening command and measurements. The worktree `perf/interpolation-block-safe-count-20261016` remains clean at upstream baseline `ae31c514` and was not pushed.
