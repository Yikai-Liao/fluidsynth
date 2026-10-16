# Exact integer-phase sample-read screen

Status: early-stopped after a small end-to-end screen. PCM was exact, but timing was too variable to retain the branch as a validated optimization.

## Candidate and hit rate

For the default fourth-order interpolator, table row zero is exactly the sample-pick vector `[0, 1, 0, 0]`. The prototype bypassed the cubic calculation in the interior-sample loop only when both the block's starting phase and its fixed-point phase increment had zero fractional bits. Integer phase and increment preserve that condition across each interior span; boundary samples kept the original interpolation code.

The existing instrumented run counted `46,964,242 / 230,283,063` fourth-order output samples (20.394%) with an integer phase increment. A new counter added the stricter starting-phase condition on the same case and produced exactly the same count. The case was `All The Small Things`, MuseScore General SF2, 44.1 kHz, 160 seconds, 8,192-frame public render requests, effects enabled, one synth core. This is a workload observation, not a general guarantee; it indicates that, in this trace, the voices with integral increments also began each interpolation call on an integer phase.

The instrumented candidate and upstream baseline both produced PCM hash `4a143dbb742aaf26` for that 160-second case. This confirms exact output for the measured input only.

## Small timing screen

- Baseline and candidate: upstream commit `ae31c51454c4d2bab98c9d8ce65e9a5eafcf741c`, Release double precision, GCC 14.2, OpenMP enabled.
- Workload: `All The Small Things`, MuseScore General SF2, 44.1 kHz, 64-frame requests, effects enabled, one core, 10 seconds; CPU pinned to core 0.
- Order: one warmup per arm, then three AB/BA pairs.
- Candidate/baseline wall ratios: `0.9194`, `0.9533`, `1.2134`; median `0.9533` (4.7% faster), full range `0.9194–1.2134`.
- CPU ratios: `0.9201`, `0.9531`, `1.2092`; median `0.9531`.
- All three timed PCM hashes matched exactly (`fb61cfa69767f603`).

## Decision and artifacts

The strict condition is common enough in this one trace to justify the screen, but the first screen includes a 21% regression and does not establish stable gain. Following the user's early-stop rule, no second workload, precision matrix, or full test suite was run. The code was reverted; the clean experiment branch remains at the upstream base and this candidate does not count toward the ten.

Artifacts: `candidate.patch`, `run_screen.py`, `screen.json`, and `screen-instrumented-output.json` (the raw instrumented probe result).


The original PCM captures are omitted from the branch to keep it lightweight; their hashes and the paired measurements are in `screen.json`. The exact rejected patch is included as `candidate.patch`.
