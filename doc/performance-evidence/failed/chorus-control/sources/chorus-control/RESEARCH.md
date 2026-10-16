# Chorus shared-control hoist: source and codegen screen

## Decision

No source change. In the GCC 14.2 Release build from upstream baseline `ae31c514`, `fluid_chorus_processmix` already tests the shared `index_rate >= mod_rate` condition once per sample before the tap loop. The generated path separates update and no-update work, so moving this predicate out of `get_mod_delay()` would duplicate compiler output rather than remove per-tap checks. The profile attributed 1.94% self samples to chorus processing in one effects-on diagnostic; that is the whole chorus function, not the cost of this already-hoisted predicate.

The waveform-type branch remains in the modulation-update loop, but specializing it would be a different candidate than the shared event/update check and has not been measured. Stop this direction without a patch or benchmark expansion.

## Evidence

- Source: `src/rvoice/fluid_chorus.c`, `fluid_chorus_processmix()` and inlined `get_mod_delay()`.
- Assembly: `interpolation-block-safe-count/build-baseline/src/libfluidsynth.so.3.6.1`, GCC 14.2, Release, OpenMP enabled. The outer sample loop compares the incremented index rate with the modulation rate before entering the tap loop; see `objdump -Cd` at `fluid_chorus_processmix` around `0x2f6a2`.
- Profile: `combined/profile-combined.txt` (directional single run; chorus 1.94% self overhead).

This is a code-generation screen only. It is not a measured optimization candidate and does not count toward the 10.
