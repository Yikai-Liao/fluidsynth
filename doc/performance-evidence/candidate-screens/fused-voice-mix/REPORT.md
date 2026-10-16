# Fused voice-buffer mixer

**Decision:** rejected; the tested sample-major fusion was slower and was reverted from the source branch. The exact prototype patch and screen records are included beside this report.

The screen used Release double GCC 14.2/OpenMP, one pinned render core, 64-frame requests, effects on, and a 10-second `All The Small Things` render. Three AB/BA pairs produced candidate/base wall ratios 1.102, 1.093, and 1.209 (median 1.102, about 10.2% slower). CPU time followed the same direction. All PCM hashes matched (`124f8cd830b16bbe`), so the rejection is based on performance, not audio differences. The screen was stopped without a larger matrix or full suite.
