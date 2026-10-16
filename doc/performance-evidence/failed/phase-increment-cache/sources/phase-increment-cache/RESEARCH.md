# Exact pitch-to-phase increment cache: early screen

The candidate would reuse `fluid_ct2hz_real()` output when the exact pitch expression and sample root pitch match the previous block. It must not skip envelopes, LFOs, portamento, or any control update.

The existing matched-shape combined profile reported `fluid_ct2hz_real()` at 0.79% of sampled process CPU time. Even eliminating the conversion entirely would therefore cap the maximum whole-process saving below 0.8% on that workload, before adding key comparisons, cache state, invalidation, and per-voice memory. That is too small to justify a new stateful cache for the measured path. The candidate was stopped before source changes or a benchmark; the profile is directional, based on the diagnostic workload described in `REPORT.md`, and does not rule out a different workload with a materially larger share.

**Result:** no optimization established; not counted. Candidate source locations and exact cache-key constraints are documented in the supplied `fluidsynth-10-independent-performance-pr-candidates.md.txt`.
