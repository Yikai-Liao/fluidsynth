# Custom + SoundFont IIR buffer fusion: early screen

**Decision:** reject and revert. The per-sample fused prototype was slower in all three paired renders and did not preserve bit-identical PCM. No production source change or PR candidate remains.

## Candidate and scope

`fluid_iir_filter_apply()` applies the optional custom filter to the whole sample buffer, then applies the SF2 low-pass filter to the whole buffer. A fused loop could avoid the intermediate buffer write/read when both filters are active. The public custom filter is disabled by default, so any gain would apply only to synths that opt in.

The prototype kept independent histories, coefficients, smoothing counters, and final envelope gain for both filters. It passed each custom-filter output through a `fluid_real_t` temporary before the SF2 filter. The exact source diff is preserved in `candidate.patch` (SHA-256 `d2ff9a6992f8b1c9a05bccd371850d76f83f8099348d1b05fcae380643a033b1`).

## Screen setup

- Source base: upstream `ae31c51454c4d2bab98c9d8ce65e9a5eafcf741c`.
- Build: GCC 14.2, Release, double samples, OpenMP enabled, same `enable-*` options as the retained baseline build.
- Probe: public FluidSynth API, 44.1 kHz, one core, 64-frame calls, effects off; render timer excludes SoundFont loading and setup.
- Input: first 10 seconds of `All The Small Things.mid` with `MuseScore_General.sf2`.
- Custom stage: low-pass enabled, cutoff generator 9000 cents, Q generator 50 centibels; the ordinary SF2 filter remains active.
- One warmup per version, then three order-balanced AB/BA pairs pinned to CPU 0. Full rows, setup values, hashes, and input/library identities are in `screen-custom.json`; `run_screen.py` reproduces the screen when the libraries are rebuilt.

## Result

Candidate/baseline render wall-time ratios were `1.0462`, `1.0514`, and `1.0531`; median `1.0514` (5.1% slower). CPU-time ratios were `1.0479`, `1.0502`, and `1.0555`. The paired ranges consistently show a regression, so the screen stopped without a second input or full test suite.

Output FNV hashes differ (`b1ed47d11cd97539` baseline, `fe4e8ee0a0110ed9` candidate). Comparing the float output files found 6 changed samples out of 882,000, maximum absolute difference `1.14e-13`, RMS difference `1.76e-16`; these changes are negligible in level but fail exact PCM equality. RMS and peak summaries matched at the printed precision.

The measured regression is consistent with extra per-sample state/register pressure in the fused loop, although assembly was not analyzed. The custom filter is opt-in, and the fused version regressed even in the workload that enables it. Stop and move on; this candidate does not count toward the ten.
