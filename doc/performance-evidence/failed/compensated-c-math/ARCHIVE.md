# Compensated six-point interpolation archive

Status: research-only; the candidate did not pass the frozen sinc25 quality gate and did not qualify as a same-speed quality improvement over production cubic. This branch contains documentation, source for the offline probes, raw measurements, summaries, and input hashes. It does not contain a production integration patch or a pull request.

The three-way decision and measurements are in [THREEWAY_FINAL_REPORT.md](package/THREEWAY_FINAL_REPORT.md). Earlier mathematical and real-sample gate results remain in [REPORT.md](package/REPORT.md); the original two-way kernel screen is [PERF_REPORT.md](package/PERF_REPORT.md). `package/manifest.json` records hashes for the archived artifacts and the frozen external input data.

Reproducing screens requires the external dataset root described in the reports and manifest. The SoundFont files are not copied into this archive. Repeating the complete-render probe also requires the isolated experimental checkout that contains the temporary candidate integration; no production source was added here.

Baseline source revision: `6be67a906b9d679ea930c8841fd0cac0d8a0a25e`.
