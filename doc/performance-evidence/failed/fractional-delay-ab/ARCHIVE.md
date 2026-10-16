# Fractional-delay interpolation A/B screen

- Branch: archive/failed-fractional-delay-ab-20261016
- Status: stopped at frozen quality gates
- Analysis source: /root/code/fluidsynth-analysis-20261015
- Outcome: Prefiltered cardinal B-splines failed static passband and real-SF2 error gates. WLS FIR tables with 13/17/21 taps failed the real-sample budget; 25 taps narrowly missed the median bound while keeping the same tap count as sinc25. No production code changed.

## Preserved files

The original report, reproduction scripts, patches, logs, and text/CSV/JSON evidence are copied below. Binary audio, PCM, SoundFont, MIDI, and build outputs are excluded; their hashes are retained in archive-manifest.json when they were in the selected source trees. The analysis reports also retain hashes for external inputs.

- sources/fractional-delay-ab/REPORT.md
- sources/fractional-delay-ab/baseline-reproduction.csv
- sources/fractional-delay-ab/candidate-windows.csv
- sources/fractional-delay-ab/fir-coefficients.csv
- sources/fractional-delay-ab/fir-static.csv
- sources/fractional-delay-ab/loop-entry-windows.csv
- sources/fractional-delay-ab/samples.csv
- sources/fractional-delay-ab/spline-reconstruction.csv
- sources/fractional-delay-ab/spline-static.csv
- sources/fractional-delay-ab/summary.json
