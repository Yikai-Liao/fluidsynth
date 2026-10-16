# fractional-delay-ab experiment archive

- Branch: `archive/failed-fractional-delay-ab-20261016`
- Status: stopped at the frozen quality gate
- Analysis source: `/root/code/fluidsynth-analysis-20261015`
- Outcome: Prefiltered cardinal B-splines failed static passband and real-SF2 error gates. WLS FIR tables with 13/17/21 taps failed the real-sample budget; 25 taps narrowly missed the mapped/down median bound while keeping the same tap count as sinc25. The corrected report keeps the nonquiet octave-up factor-2 diagnostic separate from the mapped/down gate. No production code changed.

## Preserved files

Reports, reproduction scripts, summary data and text/CSV/JSON evidence are listed below. Binary PCM and build outputs are omitted; their hashes remain in `archive-manifest.json` where available. External input assets are identified by hashes in the experiment manifest.

- sources/fractional-delay-ab/REPORT.md
- sources/fractional-delay-ab/baseline-reproduction.csv
- sources/fractional-delay-ab/candidate-windows.csv
- sources/fractional-delay-ab/fir-coefficients.csv
- sources/fractional-delay-ab/fir-static.csv
- sources/fractional-delay-ab/fractional_delay_screen.cpp
- sources/fractional-delay-ab/loop-entry-windows.csv
- sources/fractional-delay-ab/make_metadata.py
- sources/fractional-delay-ab/manifest.json
- sources/fractional-delay-ab/run.sh
- sources/fractional-delay-ab/samples.csv
- sources/fractional-delay-ab/spline-reconstruction.csv
- sources/fractional-delay-ab/spline-static.csv
- sources/fractional-delay-ab/summarize.py
- sources/fractional-delay-ab/summary.json
