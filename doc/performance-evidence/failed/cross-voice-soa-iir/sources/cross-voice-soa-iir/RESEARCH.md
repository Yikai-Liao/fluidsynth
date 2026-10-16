# Cross-voice SoA IIR: architecture screen

**Decision:** no patch or performance claim. The filter is a major hotspot, but this candidate requires a mixer scheduling and scratch-buffer redesign rather than a bounded filter-kernel change. It remains an unvalidated architecture lead and is not counted.

## Evidence

The combined Release profile assigns `fluid_iir_filter_apply()` 29.04% of self samples; the dense 20,000-note profile assigns it 43.75%. That makes the filter path a plausible place for a larger redesign.

The current render pipeline processes one voice at a time. `fluid_render_loop_singlethread()` iterates active rvoices; `fluid_mixer_buffers_render_one()` calls `fluid_rvoice_write()` for each voice block and mixes that voice before proceeding. `fluid_rvoice_write()` advances that voice's envelopes/LFO/phase, interpolates into a reusable per-thread `local_buf`, then applies its two IIR filters. The multithread path distributes whole voices using an atomic next-rvoice index.

## Why no small prototype was attempted

An SoA kernel needs several voices' samples and filter states at the same sample position. The current path has only one reusable voice scratch buffer, and each voice's valid sample count and finish point can differ. A real cross-voice batch would therefore need to split voice preparation/interpolation from filtering, retain several voice blocks, then mix them in the original rvoice order. It must also preserve voice-finish removal/swap behavior, filter smoothing and state, optional custom-filter paths, and the existing per-thread whole-voice scheduling model.

This is materially larger than replacing one inner loop. A synthetic SIMD microkernel would not answer whether the required buffering, transposition, and scheduling cost improves whole renders, so no such result is reported. The idea is not rejected on hotspot size; it is deferred as an architecture-level experiment outside a quick independent candidate screen.
