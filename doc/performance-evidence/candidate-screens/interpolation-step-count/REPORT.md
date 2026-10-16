# Integral-step interpolation count shortcut

**Decision:** rejected after the first representative screen; PCM matched, but all three timing pairs regressed.

The fast path uses integer arithmetic to compute the count for integral 32.32 phase increments, retaining the original 64-bit division for other cases and overflow-risk states. The 10-second 64-frame `All The Small Things` screen used three AB/BA pairs after warmups. PCM hashes were identical (`124f8cd830b16bbe`), while candidate/base wall ratios were 1.0177, 1.0248, and 1.0438 (median 1.0248); CPU ratios followed. The candidate was reverted without a larger matrix.
