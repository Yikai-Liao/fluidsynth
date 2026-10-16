# SoundFont note-on zone index

- Branch: archive/failed-noteon-zone-index-20261016
- Status: stopped at whole-path bound
- Analysis source: /root/code/fluidsynth-analysis-20261015
- Outcome: The real-player profile put the complete fluid_defpreset_noteon path at 1.48% of whole-process samples, with zone lookup only a subset. Range checks also clear mono-legato ignore flags. The screen stopped before a prototype.

## Preserved files

The original report, reproduction scripts, patches, logs, and text/CSV/JSON evidence are copied below. Binary audio, PCM, SoundFont, MIDI, and build outputs are excluded; their hashes are retained in archive-manifest.json when they were in the selected source trees. The analysis reports also retain hashes for external inputs.

- sources/noteon-zone-index/RESEARCH.md
