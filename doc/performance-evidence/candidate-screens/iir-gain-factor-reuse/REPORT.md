# IIR gain-factor reuse

**Decision:** rejected. The three-pair timing was effectively flat and the computed reuse changed float output slightly.

A 64-frame Release `Maestro_1` screen compared baseline and reuse of the `1/sqrt(Q)` gain factor across unchanged-Q coefficient updates. Candidate/base wall ratios were 0.9979, 1.0211, and 0.9826 (median 0.9979); CPU medians were also flat. PCM was not bit-exact: maximum absolute float difference was `3.7252903e-9`, RMS difference `5.9340e-11`. No meaningful speedup justified the numerical change, so the prototype was reverted. No full-suite expansion was run.
