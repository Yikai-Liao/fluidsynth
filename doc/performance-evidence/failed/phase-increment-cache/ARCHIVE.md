# Pitch-to-phase increment cache

- Branch: archive/failed-phase-increment-cache-20261016
- Status: stopped at Amdahl bound
- Analysis source: /root/code/fluidsynth-analysis-20261015
- Outcome: The matched-shape profile put fluid_ct2hz_real() at 0.79% of sampled CPU time, bounding the whole-process gain below that before cache overhead. No source change was made.

## Preserved files

The original report, reproduction scripts, patches, logs, and text/CSV/JSON evidence are copied below. Binary audio, PCM, SoundFont, MIDI, and build outputs are excluded; their hashes are retained in archive-manifest.json when they were in the selected source trees. The analysis reports also retain hashes for external inputs.

- sources/phase-increment-cache/RESEARCH.md
