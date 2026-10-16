# Interpolation fast-path counters

**Decision:** research-only instrumentation; no interpolation optimization was validated by this branch.

The instrumentation counted 230,283,063 cubic output samples during one 160-second `All The Small Things` render at 44.1 kHz with 8192-frame requests, effects enabled, and one render core. Of those samples, 46,964,242 (20.394%) had an integral 32.32 phase increment; 46,824,137 (20.333%) reused the preceding coefficient row. These are sample-weighted rates from this single workload, not voice-level hit rates or general guarantees. Instrumented and baseline PCM hashes matched.

Fixed-row reuse, SIMD tap reduction, loop unrolling, and integer-step/count fast paths were separately screened. They did not show a stable end-to-end gain; details and compiler modeling are in `interpolation-step/COMPILER_HINTS_MCA.md` from the local study and in the candidate-specific reports. This worktree retains temporary counters so the measurement is inspectable; it is not suitable as a production patch.
