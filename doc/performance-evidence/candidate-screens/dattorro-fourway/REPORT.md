# Dattorro tap wrap/guard four-way screen

**Decision:** retain single-wrap tap indexing as the performance change; keep guard removal with it only as one Dattorro tap-addressing theme. The guard alone is not an independent optimization.

Four Release double variants were compared from the same base: baseline, single-wrap only, guard removal only, and both. GCC 14.2/OpenMP, one pinned render core, 60-second `All The Small Things`, MuseScore General SF2, 44.1 kHz, 64-frame blocks, Dattorro and chorus enabled; three order-balanced rounds. All PCM hashes matched (`124f8cd830b16bbe`).

Candidate/base wall ratios were 0.911/0.929/0.946 for single-wrap (median 0.929), 0.942/0.999/1.017 for guard-only (median 0.999), and 0.866/0.926/0.906 for both (median 0.906). The incremental combined-vs-wrap ratios were 0.950/0.997/0.957. The screen supports one combined theme, not a separate guard PR.
