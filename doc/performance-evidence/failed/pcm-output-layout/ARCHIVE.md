# Common PCM output-layout kernel

- Branch: archive/failed-pcm-output-layout-20261016
- Status: stopped at profile screen
- Analysis source: /root/code/fluidsynth-analysis-20261015
- Outcome: The float output writer accounted for 0.16% self samples in the diagnostic profile, and no integer writer appeared among top symbols. A specialized shared kernel was not justified before a prototype.

## Preserved files

The original report, reproduction scripts, patches, logs, and text/CSV/JSON evidence are copied below. Binary audio, PCM, SoundFont, MIDI, and build outputs are excluded; their hashes are retained in archive-manifest.json when they were in the selected source trees. The analysis reports also retain hashes for external inputs.

- sources/pcm-output-layout/RESEARCH.md
