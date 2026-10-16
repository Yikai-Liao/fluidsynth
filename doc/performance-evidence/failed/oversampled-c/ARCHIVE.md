# Two-times oversampled interpolation table

- Branch: archive/failed-oversampled-c-20261016
- Status: stopped at static and SF2 quality gates
- Analysis source: /root/code/fluidsynth-analysis-20261015
- Outcome: The direct, un-pre-emphasized 2× float representation with 4/6-point polynomial reconstruction missed both passband and real-SF2 error budgets. The screen rejects this direct form only; an equalized/pre-emphasized form remains untested.

## Preserved files

The original report, reproduction scripts, patches, logs, and text/CSV/JSON evidence are copied below. Binary audio, PCM, SoundFont, MIDI, and build outputs are excluded; their hashes are retained in archive-manifest.json when they were in the selected source trees. The analysis reports also retain hashes for external inputs.

- sources/oversampled-c/REPORT.md
- sources/oversampled-c/baseline-reproduction.csv
- sources/oversampled-c/loop-entry.csv
- sources/oversampled-c/manifest.json
- sources/oversampled-c/quality.csv
- sources/oversampled-c/static.csv
- sources/oversampled-c/storage.csv
- sources/oversampled-c/summary.json
