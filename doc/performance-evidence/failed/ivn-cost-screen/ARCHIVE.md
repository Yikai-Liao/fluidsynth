# Interleaved velvet-noise reverb cost screen

- Branch: archive/failed-ivn-cost-screen-20261016
- Status: stopped before implementation
- Analysis source: /root/code/fluidsynth-analysis-20261015
- Outcome: The cited 802 FLOP/sample design is compared against a 16-line FDN, while its own discussion says FDN is cheaper at low order. FluidSynth's baseline has eight lines and one-pole damping, so the literature comparison did not establish a credible cost advantage. No performance claim is made.

## Preserved files

The original report, reproduction scripts, patches, logs, and text/CSV/JSON evidence are copied below. Binary audio, PCM, SoundFont, MIDI, and build outputs are excluded; their hashes are retained in archive-manifest.json when they were in the selected source trees. The analysis reports also retain hashes for external inputs.

- sources/algorithm-route-profile/VELVET_NOISE_COST_SCREEN.md
