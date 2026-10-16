# Polyphase IIR halfband converter screen

## Decision

Stop this candidate at the isolated wet-quality screen. The designed converter met its own passband/stopband and group-delay targets, and the first cost sample was promising, but the lower-rate Dattorro path did not preserve the configured wet response. No production source change or performance candidate is retained.

## Setup

The test compares the same Dattorro room configuration at 88.2 kHz against a 44.1 kHz tank surrounded by a 2x polyphase IIR halfband downsampler and two upsamplers. The converter uses six allpass sections per path, a 5.55 kHz transition width, and a designed 86.72 dB stopband. The impulse render is about 12 seconds (1,058,368 frames). It isolates reverb output and does not include MIDI voices, dry/wet routing, or full-engine scheduling.

The filter response itself is clean over its declared 0–16.5 kHz passband. The two-stage group delay is 0.068–0.152 ms over that passband. The measured converter response therefore satisfies the local filter design goals; that does not establish that running the reverb network at half sample rate preserves its sound.

## Cost screen

One pinned-CPU microbenchmark used 65,536-frame inputs, 1,024 blocks, one warmup, and seven measured repetitions. The medians from `raw-timing-mono.txt` were:

| Path | Median time |
| --- | ---: |
| Full-rate Dattorro core | 3.207 ms |
| Half-rate Dattorro core alone (H) | 1.521 ms |
| Converter alone (C) | 1.306 ms |
| H + C | 2.827 ms |
| Complete multirate path | 2.887 ms |

The observed H+C median is below the plan's 3.2 ms feasibility ceiling. The complete path median was about 10% below the full-rate median, but the ranges overlap and the measurements were not an AB/BA paired run. This is a cost feasibility signal, not an accepted end-to-end speedup.

## Wet-response failure

On the same 12-second impulse, total candidate wet energy was about half the full-rate energy (−3.011 dB); matching total level would require a fixed +3.011 dB on the candidate. Left/right energy balance was close (−0.001 dB difference), but the frequency-dependent response was not: normalized one-third-octave color deltas ranged from −3.013 dB at 50 Hz to +13.251 dB at 32.256 kHz. T20 differed by more than 5% from 2.54 kHz upward, reaching +35.32% at 12.8 kHz. The late 8–12 second IACC difference was −0.056, beyond the plan's 0.05 limit.

The stopband and group-delay checks pass, while the total wet level, spectral shape, high-band decay, and late spatial-correlation checks fail. A single gain correction cannot repair frequency-dependent decay or coloration. The discrepancy comes from the combined system—half-rate Dattorro tuning and state evolution plus converter response—not from a failure to design a usable halfband filter.

## Reproduction and limits

Run `./run.sh` from this directory. It applies `g-hook.patch` only to the clean measurement worktree at `571bc72e72b756f25333b3e31fb3046161c27632`, builds the two temporary probes, writes the raw measurements, and restores the worktree on exit. The tested source remains outside the main checkout. This is a one-configuration early-stop screen, not a general conclusion about every IIR halfband design.
