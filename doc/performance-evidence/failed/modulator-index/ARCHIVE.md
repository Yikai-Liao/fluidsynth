# Modulator destination index

- Branch: archive/failed-modulator-index-20261016
- Status: stopped at Amdahl bound
- Analysis source: /root/code/fluidsynth-analysis-20261015
- Outcome: On the generated 20,000-note player workload, note-on was 1.48% of sampled whole-process CPU and the zone-modulator merge helper was 0.27%. Index lifecycle and lookup overhead were not justified by that bound.

## Preserved files

The original report, reproduction scripts, patches, logs, and text/CSV/JSON evidence are copied below. Binary audio, PCM, SoundFont, MIDI, and build outputs are excluded; their hashes are retained in archive-manifest.json when they were in the selected source trees. The analysis reports also retain hashes for external inputs.

- sources/modulator-index/RESEARCH.md
