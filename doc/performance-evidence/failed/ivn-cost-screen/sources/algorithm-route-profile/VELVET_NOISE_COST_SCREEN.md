# Velvet-noise late-reverb cost screen

Date: 2026-10-16. This is a static architecture/cost screen only; no implementation or runtime benchmark was started.

## Reference result and scope

The referenced IVN paper describes a late-reverberation network with four interleaved velvet-noise branches. Its reported 802 FLOP/output-sample point includes the sparse FIR taps and each branch's attenuation filter, and is compared with a 16-line FDN at 2065 FLOP/sample. The authors state that IVN and FDN are nearly equal in complexity around 16 lines and that FDN is less costly at smaller orders. These figures do not compare IVN against FluidSynth's implementation and do not establish an expected FluidSynth speedup. See [Välimäki and Prawda (2021), DOI 10.1109/TASLP.2021.3060165](https://doi.org/10.1109/TASLP.2021.3060165).

The paper's IVN is also aimed at late reverberation. Matching FluidSynth's complete optional FDN engine would require deciding how the early response, modulation, stereo decorrelation, room-size/damping controls, and short tails map to that late-tail design.

## FluidSynth comparison

The current FluidSynth FDN has eight delay lines, rank-one Householder feedback, a modulated fractional-delay read on each line, and one-pole damping per line (`src/rvoice/fluid_rev_fdn.cpp`, `src/rvoice/fluid_rev_filters.h`). In the direct-class Release double microbenchmark from the four-line screen, the eight-line implementation took an average median of 5.74746 ms per 131,072-sample pass (2,048 blocks × 64 samples), about 43.9 ns/sample on the Xeon Gold 6140 virtual machine. The FDN was 11.21% of one full-render profile, so even eliminating it entirely would cap whole-render savings near that share.

The paper's four-branch IVN uses a different operation mix: hundreds of sparse-FIR tap operations plus higher-order per-branch attenuation filters. Its 802-FLOP figure is for a different implementation and target network, so it cannot be converted directly to cycles or compared numerically to this timing. Still, the combination of (a) the paper's own warning that FDN tends to be cheaper at lower order and (b) FluidSynth's eight-line, one-pole-damped baseline gives no credible cost hypothesis that a full IVN replacement would beat the existing engine. Implementing only a late-tail IVN would also require additional split/transition work.

## Decision

Do not port IVN into FluidSynth on the current evidence. This is a cost/scope rejection before implementation, not a claim that IVN is generally slower or lower quality. Reopen only with a concrete lower-cost mapping to FluidSynth's eight-line FDN and a defined early/late split; do not reuse the paper's 60% comparison as a performance estimate.
