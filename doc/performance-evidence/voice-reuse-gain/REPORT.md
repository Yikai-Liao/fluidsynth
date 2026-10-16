# First PR candidate: reset mixer gains on voice reuse

## Problem and cause

The original offline comparison used the same public rendering API and sample-clock MIDI player with request sizes of 64 and 8192 frames. Both paths retained the internal 64-frame DSP/control step.

Minimization reduced the first guitar-track discrepancy to four MIDI events: program 30, note-on 55 at tick 601, note-off 55 at tick 721, and note-on 60 at tick 961 (PPQ 240, default tempo). The two note-on callbacks occur at synth sample ticks 55232 and 88320 for both request sizes.

The allocation trace shows that the second note reuses rvoice slots 0/1 with 64-frame requests; those voices retain nonzero mixer `current_amp`. With 8192-frame requests it uses fresh slots 2/3 with zero mixer gain. Finished voices are collected at different render batch boundaries, so the slot history differs. The baseline minimized output has a maximum absolute difference of approximately 0.000434.

`fluid_rvoice_reset()` resets synthesis history but leaves the mixer buffer record count and gains intact. Buffer mapping initialization only initializes gains for newly added records. A reused voice therefore starts its gain ramp from the previous note's gain.

## Change

Commits on the branch: `0299db62eabde6f26d597d59d7644addabaa2d49` (fix and regression test), `7970e75d63ff6f930169e9261281cfc36bd3c6c0` (test-only simplification), and `20913f669dd6220052ad0c68d0d6fe3bebcd7d62` (remove unused test include), on `fix/voice-reuse-gain`, based on `ae31c51454c4d2bab98c9d8ce65e9a5eafcf741c`. The branch is pushed to `origin/fix/voice-reuse-gain` in the user's fork; no GitHub PR has been created. The source branch and original master checkout were clean when this report was prepared.

Set `voice->buffers.count = 0` in `fluid_rvoice_reset()`. The existing mapping initialization then initializes each record's current and target gain. All mappings and target gains are already set up for every new voice by `fluid_voice_init()` and runtime synthesis parameter calculation.

The production change adds one assignment and a local explanation. It introduces no new state, API, DSP arithmetic, or render loop. A live voice's pan smoothing still follows the same code. Reset is called when initializing a new voice; legato reuse has its own path. Each overflow rvoice owns its own buffers.

The output intentionally changes where a reused voice previously inherited a gain. This gives new notes the same onset ramp as fresh voices. The patch does not change file-render request sizes or realtime driver configuration.

## Validation

- `test_voice_reuse_gain` uses a generated 256-frame RAM sample with polyphony 1 and compares 128-frame fresh/reused output, including a preceding note with a different pan. Two 64-frame mixer blocks cover the gain ramp and its steady state. It verifies both dry channels contain sound and uses exact sample comparison. It needs no downloaded soundfont or loop setup, and no longer includes the unused `rvoice/fluid_rvoice.h` header.
- The final regression test passes with the reset and fails at the PCM equality assertion when the reset assignment is temporarily removed. The current test was rebuilt and run via `voice-reuse-build`; the older 512-frame version also failed on the original production code (log `../test-voice-baseline.log`).
- Release double: 41/41 enabled tests passed.
- Release float: 41/41 enabled tests passed.
- Debug ASan/UBSan: the new test and existing voice callback lifecycle test passed. The initial callback run lacked staged test data; after building `test_data`, both passed. Both logs were retained.
- Three symusic MIDI excerpts, each 60 seconds, MuseScore General SF2, effects on/off: candidate 64-frame and 8192-frame outputs are byte-identical in all six cases. See `fixed-full-comparison.jsonl` and retained PCM.
- Baseline and candidate Release C/C++ compilation flags match; see `compile-flags.json`. Paired timing predates the test-only simplification; the production source is unchanged. `paired-manifest.json` retains the test hash used for that run.
- Builds disable libsndfile and Signalsmith. SF3 and the external manual regression assets were not exercised. No realtime audio driver was tested.

## Paired timing

Measured after all builds and other tests finished: five paired repetitions for each of three 60-second excerpts and two request sizes, 60 process runs in total, with alternating baseline/candidate order. KVM guest exposing six Xeon Gold 6140 vCPUs; GCC 14.2.0 Release `-O3`, double DSP, identical compilation flags, one synth core, `OMP_NUM_THREADS=1`, effects enabled. Setup, checksum, and file writes are outside the render timer. The complete source/input/library identities are in `paired-manifest.json`; the commit above contains the measured production source and test bytes.

| MIDI | Request | Wall seconds, baseline / fixed | CPU seconds, baseline / fixed | Median paired wall ratio |
|---|---:|---:|---:|---:|
| Maestro_1 | 64 | 2.289 / 2.268 | 2.280 / 2.265 | 0.989 |
| Maestro_1 | 8192 | 1.861 / 1.863 | 1.854 / 1.861 | 1.001 |
| POP909_010 | 64 | 1.792 / 1.744 | 1.786 / 1.743 | 0.985 |
| POP909_010 | 8192 | 1.396 / 1.401 | 1.394 / 1.393 | 1.000 |
| All The Small Things | 64 | 1.727 / 1.731 | 1.725 / 1.729 | 1.018 |
| All The Small Things | 8192 | 1.386 / 1.376 | 1.385 / 1.373 | 0.993 |

Times are medians; paired ratios are calculated before taking their median. RSS medians stay within 244-245 MiB, including SF2 and the preallocated PCM buffer. Individual wall ratios span approximately 0.923-1.084 across the full set. There is no consistent timing regression in this sample and no speedup claim for this fix. Five pairs on this guest cannot exclude small performance differences.

## Review artifacts

- `PR_DESCRIPTION.md`: proposed upstream title and body.
- `minimal.mid`, `minimal-events.txt`, `minimize.py`, `minimize.jsonl`: minimized case and reduction evidence.
- `render_trace.c`, `trace-64.log`, `trace-8192.log`: allocation/gain trace against the original library.
- `run_paired.py`, `paired-manifest.json`, `paired.jsonl`, `paired-summary.json`: paired timing and identity records.

## Static review conclusion

Independent review of the production lifecycle and the final test found no correctness or design issue. `fluid_rvoice_reset()` is reached through voice initialization; reset, all four mapping/gain updates, and voice activation are queued in order. The existing lazy mapping initializer then clears stale current/target gains. Active-note legato uses a separate path, so it retains its intended smoothing state. Resetting the record count reuses that initializer and is the smallest sufficient change in the current call graph.

The test proves reuse of the same voice slot after a different pan and compares the reused onset against a fresh-state rendering. The only review limit is that this focused case tests dry output and one pan transition; it does not exercise the whole MIDI-player scheduler. The minimized MIDI reproduction and full-excerpt comparisons cover that adjacent integration behavior separately.

The final 128-frame test passes with the fix. For regression signal, temporarily removing the reset assignment makes the current test fail at the exact output-equality assertion. These targeted runs used `<build-dir>`; the broader 41-test double/float results above were recorded before this test-only shortening.


## Fork branch

Source commit: `20913f669dd6220052ad0c68d0d6fe3bebcd7d62` on `fix/voice-reuse-gain`. This evidence was added in a follow-up documentation commit on the same fork branch. No upstream pull request has been created.
