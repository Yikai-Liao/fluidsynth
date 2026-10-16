# Modulator destination index: early screen

The candidate would preserve destination discovery order and each destination's contributor order while replacing repeated full-list scans with per-destination contributor indexes. Its correctness and lifecycle costs include topology changes, voice rebuilds, generator read-after-update ordering, and per-voice memory.

A real `fluid_player` profile of a generated 20,000-note MIDI placed `fluid_defpreset_noteon()` at 1.48% of sampled whole-process CPU and the zone-modulator merge helper at 0.27%. Even removing all observed merge-helper cost would cap the gain below 0.3% on this deliberately note-dense workload, before index construction and lookup overhead. The change was stopped before source edits or another benchmark.

**Result:** no optimization established; not counted. The profile and workload limits are recorded in `modulator/RESEARCH.md`.
