# Cross-voice SoA IIR

- Branch: archive/deferred-cross-voice-soa-iir-20261016
- Status: deferred after architecture screen
- Analysis source: /root/code/fluidsynth-analysis-20261015
- Outcome: IIR is a large profile hotspot, but a useful prototype needs split render/filter stages, retained per-voice blocks, scratch ownership, and ordered mixing. The bounded screen did not justify that wider redesign.

## Preserved files

The original report, reproduction scripts, patches, logs, and text/CSV/JSON evidence are copied below. Binary audio, PCM, SoundFont, MIDI, and build outputs are excluded; their hashes are retained in archive-manifest.json when they were in the selected source trees. The analysis reports also retain hashes for external inputs.

- sources/cross-voice-soa-iir/RESEARCH.md
