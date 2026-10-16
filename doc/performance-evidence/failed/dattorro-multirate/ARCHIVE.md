# Wet-only multirate Dattorro reverb

- Branch: explore/dattorro-multirate-20261016
- Status: stopped at net-cost gate
- Analysis source: /root/code/fluidsynth-analysis-20261015
- Outcome: The tested Kaiser FIR choices traded passband against group delay, and measured conversion alone took 2.77× a full-rate Dattorro tank; the complete candidate took 3.41×. This rejects the specified FIR/scalar implementation, not every possible multirate design.

## Preserved files

The original report, reproduction scripts, patches, logs, and text/CSV/JSON evidence are copied below. Binary audio, PCM, SoundFont, MIDI, and build outputs are excluded; their hashes are retained in archive-manifest.json when they were in the selected source trees. The analysis reports also retain hashes for external inputs.

- sources/algorithm-route-profile/dattorro-multirate/FILTER_SCREEN.md
- sources/algorithm-route-profile/dattorro-multirate/fdn-quality.txt
- sources/algorithm-route-profile/PROFILE.md
