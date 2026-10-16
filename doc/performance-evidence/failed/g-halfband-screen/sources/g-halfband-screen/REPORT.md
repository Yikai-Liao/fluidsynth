# Polyphase IIR halfband converter screen

## Decision

Stop this candidate at the isolated wet-quality screen. The designed converter met its own passband/stopband and group-delay targets, and the first cost sample was promising, but the lower-rate Dattorro path did not preserve the configured wet response. No production source change or performance candidate is retained.

## Setup

The test compares the same Dattorro room configuration at 88.2 kHz against a 44.1 kHz tank surrounded by a 2x polyphase IIR halfband downsampler and two upsamplers. The converter uses six allpass sections per path, a 5.55 kHz transition width, and a designed 86.72 dB stopband. The impulse render is about 12 seconds (1,058,368 frames). It isolates reverb output and does not include MIDI voices, dry/wet routing, or full-engine scheduling.

The filter response itself is clean over its declared 0–16.5 kHz passband. The two-stage group delay is 0.068–0.152 ms over that passband. The measured converter response therefore satisfies the local filter design goals; that does not establish that running the reverb network at half sample rate preserves its sound.

## Cost screen

The saved pinned-CPU runs each used one warmup and seven measured repetitions. The 65,536-frame run used 1,024 blocks; its medians from [`raw-timing-mono.txt`](raw-timing-mono.txt) are:

| Path | Median time |
| --- | ---: |
| Full-rate Dattorro core | 3.096 ms |
| Half-rate Dattorro core alone (H) | 1.538 ms |
| Converter alone (C) | 0.760 ms |
| H + C (sum of separate medians) | 2.298 ms |
| Complete multirate path | 2.882 ms |

For this run, complete/full is `2.881588 / 3.095512 = 0.9309`; the reciprocal full/complete is `1.0742`. These are ratios of separate medians, not paired speedup estimates. H+C is likewise a sum of independently timed component medians and is not the complete path time.

A second, longer 1,048,576-frame run is preserved in [`raw-timing-mono-long.txt`](raw-timing-mono-long.txt). Its full-rate and complete-path medians were 51.149683 ms and 36.236817 ms: complete/full `0.7084`, reciprocal full/complete `1.4115`. The short and long ratios differ materially, and neither run used an AB/BA paired design. These data only show that this microbenchmark can look favorable at some sizes; they do not establish a repeatable performance gain or end-to-end benefit. The original 3.2 ms H+C feasibility threshold was for the short input, while the long-run H+C component-median sum is 37.044 ms.

## Wet-response failure

On the same 12-second impulse, total candidate wet energy was about half the full-rate energy (−3.011 dB); matching total level would require a fixed +3.011 dB on the candidate. Left/right energy balance was close (−0.001 dB difference), but the frequency-dependent response was not: normalized one-third-octave color deltas ranged from −3.013 dB at 50 Hz to +13.251 dB at 32.256 kHz. T20 differed by more than 5% from 2.54 kHz upward, reaching +35.32% at 12.8 kHz. The late 8–12 second IACC difference was −0.056, beyond the plan's 0.05 limit.

The stopband and group-delay checks pass, while the total wet level, spectral shape, high-band decay, and late spatial-correlation checks fail. A single gain correction cannot repair frequency-dependent decay or coloration. The discrepancy comes from the combined system—half-rate Dattorro tuning and state evolution plus converter response—not from a failure to design a usable halfband filter.

## Reproduction and limits

Set `FLUIDSYNTH_G_WORKTREE` to a clean worktree at `571bc72e72b756f25333b3e31fb3046161c27632`, then run `./run.sh`. The script applies `g-hook.patch` only to that worktree, builds the two temporary probes, writes `raw-timing-final.txt` and the other raw outputs, runs [`summarize.py`](summarize.py), and restores the worktree on exit. It requires no external SF2 or MIDI assets. [`summary.json`](summary.json) reports the short and long saved timing sets and both ratio directions. This is a one-configuration early-stop screen, not a general conclusion about every IIR halfband design.
