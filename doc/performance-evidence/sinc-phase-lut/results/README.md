# High-quality sinc phase table

This candidate replaces per-output-sample trigonometric coefficient generation and normalization for the optional 11-, 17-, and 25-tap sinc modes with lazily prepared, normalized 4096-bin phase tables. The renderer linearly interpolates adjacent coefficient rows using the remaining 20 fractional-phase bits. The default 4th-order interpolation path is unchanged.

The table for a mode is prepared when `fluid_synth_set_interp_method()` selects that mode, before voices can render through it. Tables are process-lifetime statics and are initialized once per order. In double precision, each table occupies 360,536 bytes (11 taps), 557,192 bytes (17 taps), or 819,400 bytes (25 taps); float precision uses half those sizes.

## Evidence

- Double and float Release `check` targets each pass 41/41 enabled tests. The updated interpolation test also checks periodic-input continuity across `UINT32_MAX -> 0` at four frequencies; maximum observed double jumps are `7.293e-10`, `7.285e-10`, and `7.275e-10` for orders 11, 17, and 25. Float jumps round to zero.
- A 4096-bin interval-arithmetic analysis uses directed MPFR sin/cos evaluation and checks every phase-table interval. Its real-arithmetic L1 coefficient interpolation bounds are `3.298e-7`, `3.362e-7`, and `3.420e-7` for orders 11, 17, and 25. These bound one normalized sinc convolution for unit-amplitude inputs; they do not bound floating-point accumulation or later reverb/chorus processing.
- On 10 seconds of `All The Small Things`, 44.1 kHz, effects on, 64-frame requests, one core, and highest-quality interpolation, the existing three-pair screen measured candidate/baseline render-wall ratios of `0.0751`, `0.0747`, and `0.0779` (92.5% median render-time reduction). The 10-second PCM difference was max `1.49e-8`, RMS `8.89e-10`, relative RMS `2.18e-8`.
- A 60-second double render and a 10-second pitch-bend excerpt with effects on were also compared. Full-chain maximum errors were `2.98e-8` and `3.73e-9`; the 13 pitch-bend event windows had maximum error `1.86e-9`. The 10-second float render had max error `8.94e-8`, RMS `9.52e-9`.

The three-pair timing screen was captured before the final refactor that hoisted the table reference out of the per-sample lambda. The refactor preserves the same table and interpolation arithmetic and passed both full test suites, but was not separately timed. Do not interpret the convolution bound as a whole-synth error bound.

## Files

- `PR_DESCRIPTION.md`: suggested pull request title and body.
- `REVIEW.md`: scope, validation, and remaining limits.
- `manifest.json`: inputs, build, timing, and PCM identities.
- `candidate.patch`: source diff for this worktree.
- `phase-error-bound.json`: continuous real-arithmetic interpolation bound.
- `segment-null.json`: full-render and event-window comparisons.
- `phase-wrap-test-{float,double}.log`: interpolation test output.
