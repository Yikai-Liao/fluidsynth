# Reverb delay-line size cache

**Decision:** rejected; Dattorro and FDN screens did not establish a repeatable benefit.

The candidate cached each delay-line vector length after allocation. It preserved PCM on both measured screens. On a 10-second Dattorro `All The Small Things` workload, three candidate/base wall ratios were 0.9766, 1.0150, and 0.9334 (median 0.9766), with a wide range crossing baseline. On a separate 10-second FDN `POP909_010` screen the ratios were 1.0105, 0.9808, and 1.0190 (median 1.0105; two of three slower). The change was reverted and not counted.
