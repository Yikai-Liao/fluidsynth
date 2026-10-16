# Fixed-coefficient IIR block recurrence W=2/4

- Branch: archive/failed-iir-block-e-20261016
- Status: stopped at float-quality gate
- Analysis source: /root/code/fluidsynth-analysis-20261015
- Outcome: Float W=2 and W=4 failed the frozen active-error and 0.1 dB gain gates at a legal 1500-cent/Q=20 case. W=4's prepared microkernel was faster in isolation, but omitted block preparation and render integration; float quality is mandatory. No production change was accepted.

## Preserved files

The original report, reproduction scripts, patches, logs, and text/CSV/JSON evidence are copied below. Binary audio, PCM, SoundFont, MIDI, and build outputs are excluded; their hashes are retained in archive-manifest.json when they were in the selected source trees. The analysis reports also retain hashes for external inputs.

- sources/iir-block-e/REPORT.md
- sources/iir-block-e/bench.csv
- sources/iir-block-e/dynamic.csv
- sources/iir-block-e/final/bench.csv
- sources/iir-block-e/final/dynamic.csv
- sources/iir-block-e/final/quality.csv
- sources/iir-block-e/manifest.json
- sources/iir-block-e/quality.csv
- sources/iir-block-e/summary.json
