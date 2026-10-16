# Two-times oversampled interpolation table

- Branch: `archive/failed-oversampled-c-20261016`
- Status: stopped at the real-SF2 quality gate
- Analysis source: local analysis tree; the compensated screen uses `FLUIDSYNTH_C_SCREEN_DATA_ROOT`
- Outcome: the direct, un-pre-emphasized 2× float representation missed the passband and real-SF2 error budgets. A later phase-independent 65-tap float32 compensation met the static passband limit (0.0102 dB max) but worsened active-window error versus sinc25 by a median 7.28 dB and a worst 15.68 dB. Focused analysis found this error in an interior periodic-sustain window and no fixed sample delay or half-sample grid offset. Both forms stop before performance testing.

## Preserved files

The original screen and compensated follow-up reports, scripts, manifests, patches and text/CSV/JSON evidence are preserved below. Binary audio, PCM, SoundFont, MIDI, generated executables and build outputs are excluded; their hashes are recorded in the manifests when available. The compensated report's manifest also records hashes for all external SF2 sample and reference-window inputs.

- `sources/oversampled-c/` — original direct-table screen and follow-up note
- `sources/compensated-c-math/` — mathematical, real-SF2, and alignment-localization evidence

To reproduce the follow-up with the same external inputs, set `FLUIDSYNTH_C_SCREEN_DATA_ROOT` to the analysis data root described in the report's manifest, then run `bash run_all.sh` from `sources/compensated-c-math/`. The focused alignment check is `python3 diagnose_alignment.py`.
