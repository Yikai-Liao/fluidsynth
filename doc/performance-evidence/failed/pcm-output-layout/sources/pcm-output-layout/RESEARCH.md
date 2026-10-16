# Common PCM output-layout kernel: early screen

**Decision:** stop before source changes. The output-layout path has no demonstrated whole-render performance budget large enough to justify a specialized kernel.

## Source and code-generation review

The float and integer public writers walk internal stereo buffers and output-channel pointers using a runtime channel count, offsets, and strides. The common stereo float call still has per-sample pointer-array loads and pointer increments in the generic inner loop. Integer output uses a shared tag-dispatched converter for s16/s24/s32; its s16 path must preserve dither continuity and the legacy conversion order.

A special-case interleaved/planar kernel could remove some generic channel-index and stride work, but the existing rendered workload profile does not identify the conversion as a significant hotspot. The combined Release diagnostic profile attributes 0.16% self samples to `fluid_synth_write_float`; no integer conversion writer appears among the top sampled symbols. This is a directional 160-second profile, not a precise upper bound for all output APIs, but it is enough to stop this candidate under the user's early-screen rule.

## Decision

No source prototype or benchmark expansion was made. The candidate requires careful fallback behavior for offsets, strides, aliasing, NaNs, clipping, rounding, and s16 dither state, while the measured float writer share is too small to make an end-to-end improvement likely. It is not counted toward the ten.

Evidence: `../combined/profile-combined.txt` and the baseline Release disassembly of `fluid_synth_write_float_channels_LOCAL` in the retained `finished-voice-index/build-baseline` library. The disassembly confirms generic pointer/stride work exists; it does not establish that this work limits synthesis throughput.
