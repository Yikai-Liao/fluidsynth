# Finished-voice reverse index screen

Status: stopped before a source prototype. The baseline profile puts the relevant mixer render function below a useful screening threshold, and the release-tail latency is modest in this targeted workload.

## Candidate

`fluid_mixer_buffer_process_finished_voices()` scans the active-rvoice array for each finished rvoice, then removes matches by swap. A slot lookup could reduce that work during a large release burst, but it would need identity validation: a finished voice may have been queued after its active slot was replaced. It must also preserve callback order and update indices after every swap.

## Baseline workload and identity

- Source: upstream `ae31c51454c4d2bab98c9d8ce65e9a5eafcf741c`, clean checkout.
- Build: Release, GCC 14.2, double precision, OpenMP enabled; probe renders on one pinned CPU with `OMP_NUM_THREADS=1`.
- Engine: 44.1 kHz, 64-frame calls, 256 voices, fourth-order interpolation, reverb and chorus disabled.
- SoundFont: `MuseScore_General.sf2`.
- MIDI: `release-train-16x64.mid`, 16 bursts of 64 simultaneous notes, 250 ms held, bursts every two seconds; 36-second offline render.
- Library SHA-256: `6c3c4c5e1345eca83383ba940ed3febf582b714477fe4508b6e4c75af349b0ad`.
- MIDI SHA-256: `cefd8375f59157bd1908d030b69c9310dce710bdea8e9358b0818716397ecb5d`.

## Measurements

The latency probe rendered 1,587,600 frames in 24,807 calls and produced PCM hash `beb894b50823d254`. Across all calls, p50/p99/p99.9/max were 193/627/1,573/6,917 microseconds. The maximum was block 8,269 (12.0 seconds), coincident with a new note-on burst.

For each burst, the 400 ms window from 1.25 to 1.65 seconds after onset covered the voice-release tail: its p99 call latency ranged from 108 to 183 microseconds, with medians from 28 to 36 microseconds. The following 300 ms quiet window had a median of about 0.9 microseconds. These timings include all remaining voice DSP, not just finished-voice removal.

`perf record -F 999 -e cpu-clock:u --call-graph dwarf,4096` captured 4,667 samples. In the no-children report, `fluid_rvoice_mixer_render` was 0.02% self overhead; no distinct finished-voice helper appeared. The largest costs were IIR filtering (59.31%), interpolation (22.93%), buffer mixing (4.54%), and rvoice writing (4.35%). The event-handler dispatch path was 0.09%.

## Decision and artifacts

The active-rvoice scan is not a measurable hotspot in this stress case, and the release-tail windows contain no distinct large latency spike attributable to cleanup. Any reverse-index state, identity checks, and swap maintenance would add lifecycle complexity without a credible performance budget. Stop here: no source change, no full test run, and this reserve candidate does not count toward the ten.

Artifacts: `generate_release_train.py`, `release-train-16x64.mid`, `render_latency_probe.c`, `baseline-release-train-latency.csv`, and `baseline-release-train.perf.data`.
