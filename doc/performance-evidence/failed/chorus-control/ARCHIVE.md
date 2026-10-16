# Chorus shared modulation-control hoist

- Branch: archive/failed-chorus-control-20261016
- Status: stopped at code-generation screen
- Analysis source: /root/code/fluidsynth-analysis-20261015
- Outcome: GCC 14.2 already hoists the shared modulation-rate predicate ahead of the tap loop, so the proposed source hoist removes no work. No source patch or benchmark expansion was justified.

## Preserved files

The original report, reproduction scripts, patches, logs, and text/CSV/JSON evidence are copied below. Binary audio, PCM, SoundFont, MIDI, and build outputs are excluded; their hashes are retained in archive-manifest.json when they were in the selected source trees. The analysis reports also retain hashes for external inputs.

- sources/chorus-control/RESEARCH.md
