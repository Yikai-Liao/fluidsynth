# SF2 modulator preparation profile

## Decision

No source change. A proposed load-time merge of immutable preset/instrument zone modulators would add ownership and compatibility rules around local-over-global replacement, caps, and per-voice add/overwrite behavior. The measured zone-merge helper was only 0.27% of whole-process CPU samples on this dense event stream. Even eliminating that helper entirely would have a small upper bound on total render time for this workload.

## Workload and result

Generated `dense-20000.mid` as format-0 SMF at 120 BPM and 480 ticks per quarter. It contains 20,000 note-ons, each followed one tick later by note-off and then the next note-on one tick later: 41.67 seconds of MIDI time. The real `fluid_player` path rendered 45 seconds at 44.1 kHz, 64-frame blocks, one synthesis core, effects disabled, and GM program 0 from MuseScore General SF2.

The timed render interval was 8.201 seconds wall / 8.182 seconds CPU. Peak RSS was 245,496 KiB. PCM checksum was `ca315bb9bcde6455`; RMS was `0.0477911307`.

`perf record -F 499 -e cpu-clock:u --call-graph dwarf` captured 4,123 CPU-clock samples with no lost samples. The profile covers process setup and teardown as well as rendering, so the small note-on percentages are slightly diluted by non-render work. Top relevant symbols:

| Function | Whole-process samples |
| --- | ---: |
| `fluid_iir_filter_apply` | 43.75% |
| `fluid_rvoice_dsp_interpolate` | 16.08% |
| `fluid_voice_get_overflow_prio` | 3.90% |
| `fluid_synth_alloc_voice_LOCAL` | 1.63% |
| `fluid_defpreset_noteon` | 1.48% |
| `fluid_mod_test_identity` | 0.97% |
| `fluid_voice_add_mod_local` | 0.36% |
| `fluid_defpreset_noteon_add_mod_to_voice` | 0.27% |

`fluid_mod_test_identity` has more than one possible call site, so its full share is not attributed to zone merging. This was one preset and one soundfont; a modulator-heavy bank could differ. The result is sufficient to stop this candidate for the current upstream acceleration plan, not a universal upper bound for every SF2.

## Reproduction

The helper linked against the stage-1 candidate library built from commit `20913f669dd6220052ad0c68d0d6fe3bebcd7d62`:

```sh
env LD_LIBRARY_PATH=/root/code/fluidsynth-analysis-20261015/build-partition/src \
  perf record -F 499 -e cpu-clock:u --call-graph dwarf \
  -o /root/code/fluidsynth-analysis-20261015/modulator/dense-20000.perf.data \
  -- /root/code/fluidsynth-analysis-20261015/render_probe \
  /root/code/fluidsynth-analysis-20261015/assets/MuseScore_General.sf2 \
  /root/code/fluidsynth-analysis-20261015/modulator/dense-20000.mid \
  64 1 45 0
```

Input SHA-256: MIDI `059252d02780c336ae37cc785795ee005281828c6063446a88e73449f066fc9b`; soundfont `ee51d2c4b1525e70f19a45909c4fd7a2e26d91d115fa89dbf5a6bc413d8b9bf3`. Profile SHA-256: `36a2dd6030af58906c2c0b3d51d2e94161360929391e9ce2a0b20515abb6f490`.

The earlier public-API note-on-only harness was discarded as invalid: API calls enqueue work, so without rendering the event ringbuffer filled before `fluid_defpreset_noteon` ran. The profile here follows real player rendering and drains events normally.
