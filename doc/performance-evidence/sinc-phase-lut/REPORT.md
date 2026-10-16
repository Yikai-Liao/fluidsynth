# Review notes: sinc phase-coefficient table

## Scope and design

- Applies only to `FLUID_INTERP_MID`, `FLUID_INTERP_HIGH`, and `FLUID_INTERP_HIGHEST` (11, 17, and 25 taps).
- Preserves the existing normalized Hann-windowed sinc kernel at each table node and linearly interpolates the normalized coefficients between nodes.
- Prepares the selected table through `fluid_synth_set_interp_method()` before the synth can render with that mode. C++ local-static initialization provides one-time, thread-safe construction.
- Leaves `FLUID_INTERP_4THORDER` and all default-mode behavior untouched.

## Validation

- Release double: `cmake --build build-double --target check --parallel 4`, 41/41 passed.
- Release float: `cmake --build build-float --target check --parallel 4`, 41/41 passed.
- `test_rvoice_dsp_interpolate` exercises DC input across 65,536 sampled phases and periodic sample continuity at four frequencies across `UINT32_MAX -> 0` for the production orders. The wrap test's largest double jumps are below `7.3e-10`; float samples are identical at the seam.
- `phase-error-bound.py` uses outward-rounded interval arithmetic and MPFR directed sin/cos bounds on each of the 4096 phase intervals. The continuous real-arithmetic coefficient L1 bounds are `3.2973e-7` (11 taps), `3.3617e-7` (17 taps), and `3.4191e-7` (25 taps). These are bounds for one convolution with unit-amplitude inputs. They exclude coefficient-generation and dot-product floating-point rounding and do not bound reverb, chorus, or total output error.
- Full-render comparisons cover a 60-second double excerpt, a 10-second double excerpt with 13 pitch-bend events, and a 10-second float excerpt. Maximum absolute sample differences are `2.98e-8`, `3.73e-9`, and `8.94e-8`, respectively. The pitch-bend event windows have maximum error `1.86e-9`.
- The saved three-pair performance screen uses 10 seconds of `All The Small Things`, 44.1 kHz, 64-frame requests, effects enabled, one core, and `FLUID_INTERP_HIGHEST`. Median candidate/baseline render ratio is `0.0751` (92.5% less render wall time). Ratios are `0.07515`, `0.07471`, and `0.07788`.

## Limits

- The benchmark is a single workload and mode; it does not establish the same gain for 11- and 17-tap modes, realtime deadlines, or other instruments.
- The 4096-bin timing screen predates the final small refactor that hoists the table reference outside the per-sample lambda. The refactor keeps the same LUT values and arithmetic and passes both full suites, but was not separately timed. The recorded percentage is evidence for this LUT optimization, not a measurement of the exact final source revision.
- Selected sinc tables remain allocated for the process lifetime. In double precision, a 25-tap table uses about 800 KiB; selecting all three modes retains about 1.66 MiB.
- Whole-render PCM is not bit-identical, as expected from coefficient interpolation. The tiny measured differences and coefficient bound support the chosen compatibility target but are not a perceptual listening study.


## Fork branch

Source commit: `84936b5d402630da8818aeecbbfde2d624059bab` on `perf/sinc-phase-lut-20261016`. This evidence was added in a follow-up documentation commit on the same fork branch. No upstream pull request has been created.
