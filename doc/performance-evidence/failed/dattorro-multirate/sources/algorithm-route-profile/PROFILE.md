# Current render-stage cost profile

Date: 2026-10-16. This is a directional sampling profile for choosing one algorithmic route from the supplied plan. It is not a paired performance comparison.

## Method

- Optimized measurement source: `perf/combined-iir-dattorro-20261016` at `6be67a90` (IIR coefficient reuse, Dattorro tap single-wrap, and sinc phase LUT).
- Release double build, GCC 14.2, `-O3 -DNDEBUG -g`, OpenMP enabled with one worker, one pinned CPU, 44.1/88.2 kHz, 8192-frame requests, 160 seconds of `All The Small Things.mid`, MuseScore General SF2, fourth-order interpolation, chorus and reverb enabled.
- The probe waits before rendering. `perf record -p PID` is attached before the render gate opens; PCM hashing and teardown happen after recording stops. `perf` reported zero lost samples in both runs.
- Host: Intel Xeon Gold 6140 virtualized, 6 vCPU. These sample shares describe this workload and environment only.

## Results

| Case | Render wall / CPU | Peak RSS | Top sampled self symbols |
|---|---:|---:|---|
| Dattorro, 88.2 kHz | 7.179 / 7.147 s | 340,636 KiB | IIR apply 33.50%; interpolation 29.34%; voice-buffer mix 11.50%; Dattorro 10.01%; chorus 2.89% |
| FDN, 44.1 kHz | 3.788 / 3.768 s | 285,372 KiB | interpolation 30.17%; IIR apply 26.03%; voice-buffer mix 13.62%; FDN 11.21%; chorus 3.47% |

The first `engine=free` run was Freeverb, not the plan's FDN. It is retained under `freeverb-44100.*` as an auxiliary profile and excluded from the FDN result above. The corrected FDN run explicitly uses `engine=fdn`.

The earlier 44.1 kHz default-Dattorro render in `render-profile-results.json` took 3.788 s wall / 3.759 s CPU; its sampled Dattorro share was 10.87%. The high-rate Dattorro run therefore puts the reverb at about 10% of total sampled work, with roughly 5% as an ideal upper bound if network cost were halved without any conversion overhead. The 8-line FDN is about 11% of the 44.1 kHz profile; a 4-line design would have at most about 5.6% whole-render savings under an ideal linear-cost assumption. Both are directional Amdahl estimates, not predicted results.

`fluid_iir_filter_apply` is a large share (26–34%), but it contains the full-rate recursive filter and envelope gain, which remain per-sample work in route A. `fluid_iir_filter_calc` sampled below 1% in these profiles; coefficient calculation is also inlined into the smoothed update branch and was not independently resolved at this sampling density. There is no evidence that reducing coefficient-design frequency can produce a large end-to-end gain after same-bin reuse. Route A is therefore not the next prototype.

## Decision

Route B was screened and stopped. The specified wet-only Kaiser FIR design failed the 1 ms latency gate at the 16.5 kHz passband edge, while the full-band 20 kHz design required 2.65 ms and the measured converter cost exceeded the full-rate tank cost. See `dattorro-multirate/FILTER_SCREEN.md`.

Route C received a first four-line feasibility prototype. Keeping the existing rank-one Householder structure reduced the isolated FDN call by 42.45%, giving an idealized whole-render ceiling near 4.76%, but the heuristic parameter set failed wet-level, normalized-coloration, and left/right energy gates. It was stopped before full rendering or integration. See `fdn-four-line/RESEARCH.md`. No new production candidate came from these routes.

Artifacts: `render-profile-results.json`, `dattorro-88200.perf.data` and report, `fdn8-44100.perf.data` and report, `run_reverb_profiles.py`, and the auxiliary `freeverb-44100.*` capture.
