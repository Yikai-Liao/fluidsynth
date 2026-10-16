# Fractional-delay A/B offline quality screen

Date: 2026-10-16. This is a bounded offline quality screen for the supplied plan’s A (prefiltered cardinal B-spline) and B (short weighted-least-squares fractional-delay FIR table). It makes no production change and makes no runtime-performance claim.

## Frozen setup

- Baseline source: measurement composite `6be67a906b9d679ea930c8841fd0cac0d8a0a25e` (combined IIR coefficient reuse, Dattorro tap addressing, and the accepted sinc25 phase LUT). The main checkout remains untouched.
- Samples: five extracted MuseScore General SF2 samples from the prior Stage 3 screen (piano, strings, saw lead, closed hi-hat, cymbal). Existing 54 source windows contain cubic, sinc25, 1023/2047/4095-tap reference outputs; the 54×2 baseline outputs make the original 108 comparison rows. This screen adds 9 short first-loop-wrap windows.
- Output/reference: existing 44.1 kHz, double-precision direct-interpolator screen; 882-frame windows for the 54 primary cases. The added loop-entry probes use 128 frames and 2047/4095-tap references.
- Frozen fractional-delay FIR target: uniform least squares on source-normalized 0–0.40 cycles/sample, exact coefficient sum of one, 4096 phase intervals plus a guard row, and linear coefficient interpolation. The transition region is 0.40–0.50 cycles/sample; no rate-scaled anti-alias cutoff is claimed.
- The primary real-sample relative-error gate uses nonquiet mapped/down-pitch windows (step factors 0.5 and 1.0): median candidate error may be at most 1 dB above sinc25; worst window at most 3 dB. Reference convergence must be at least 20 dB below the candidate error.

## Methods and basic checks

The degree-3 and degree-5 B-spline coefficient arrays solve the discrete sample-reconstruction equations using double-precision conjugate gradients. Finite samples use clamped coefficient extension; loop regions use a separate cyclic coefficient solution. The chosen equations are `c[n-1]+4c[n]+c[n+1]=6x[n]` and `c[n-2]+26c[n-1]+66c[n]+26c[n+1]+c[n+2]=120x[n]`. Integer reconstruction passed across all selected samples and periodic loops; see `spline-reconstruction.csv` for exact maxima. The impulse and DC rows are included there as well.

The optimized FIR rows are solved independently for each phase from a fixed, small KKT system, with a unit-sum constraint. Runtime phase selection mirrors the accepted sinc25 table: the source phase is advanced by one half sample, the 12-bit table row and remaining fraction are used, and the tap window follows the same centered geometry. A separately regenerated Hann-sinc25 table reproduces the existing production raw windows within 4.95e-6 maximum sample error and −128.27 dBFS worst RMS; this bounds phase/data alignment error for this standalone evaluator.

The 4095-vs-2047 reference difference stayed at least 20 dB below every candidate error in the 54 primary windows. All nine loop-entry probes also passed that reference margin; their worst margin is reported in `loop-entry-windows.csv`.

## Static frequency/phase results

| Candidate | Max gain error through 0.20 | Through 0.40 | At 0.45 | Passes 0.1 dB through 0.40? |
|---|---:|---:|---:|---|
| B-spline degree 3 | 0.0821 dB | 3.5953 dB | 8.5272 dB | no |
| B-spline degree 5 | 0.0046 dB | 1.5384 dB | 5.3907 dB | no |
| WLS FIR 13 taps | — | 0.1795 dB | 3.3010 dB | no |
| WLS FIR 17 taps | — | 0.0501 dB | 2.4604 dB | yes |
| WLS FIR 21 taps | — | 0.0140 dB | 1.8813 dB | yes |
| WLS FIR 25 taps | — | 0.0039 dB | 1.4635 dB | yes |

The spline maximum occurs at half phase and the high end of each band. Degree 5 is substantially better than degree 3 but still misses the 0.1 dB budget by 1.44 dB at 0.40 cycles/sample. For WLS FIR, 17/21/25 taps pass the static 0–0.40 gain check, while 13 taps miss it; the transition response degrades quickly near 0.45 and is intentionally not treated as a passband.

## Real-sample window results

| Candidate | Nonquiet mapped/down windows | Median error change vs sinc25 | Worst change | Reference gate |
|---|---:|---:|---:|---:|
| spline3 | 32 | +11.56 dB | +18.73 dB | 54/54 |
| spline5 | 32 | +8.45 dB | +13.25 dB | 54/54 |
| fir13 | 32 | +5.48 dB | +27.90 dB | 54/54 |
| fir17 | 32 | +3.84 dB | +16.94 dB | 54/54 |
| fir21 | 32 | +2.63 dB | +6.40 dB | 54/54 |
| fir25 | 32 | +1.03 dB | +2.34 dB | 54/54 |

Positive values mean more RMS error than current sinc25. The 13/17/21-tap designs fail the 1 dB median budget by wide margins. The 25-tap design is closest, but its mapped/down median is +1.03 dB (the frozen limit is +1.00 dB) and it retains all 25 multiply-accumulates, so it has no tap-count performance case. Its worst primary window is hi-hat interior at half-step, +2.34 dB. At one-octave-up all candidates are evaluated separately; that case is not used to hide the lower-rate error.

Both spline variants are much farther from the long reference than sinc25 in mapped/down windows: medians are +11.56 dB (degree 3) and +8.45 dB (degree 5). Their exact integer reconstruction does not translate into good fractional-delay fidelity. The first-loop-wrap windows expose additional transition risk: the degree-3/5 spline worst error is about +39 dB versus sinc25; 25-tap WLS stays within +1.85 dB there, but the primary gate still fails.

## Decision

- **A, prefiltered cardinal B-spline: stop.** Exact finite/periodic integer reconstruction passed, but both degrees fail the predeclared 0.1 dB passband budget and real-sample error contract. No playback integration or performance benchmark is justified.
- **B, short WLS FIR: stop this design.** Thirteen taps fail even the static 0.1 dB budget. Seventeen and twenty-one taps pass only through the declared 0.40 band but fail the real-sample contract. Twenty-five taps nearly matches the quality contract but narrowly misses its median limit and does not reduce arithmetic. Do not claim 13/17/21/25 tap reductions preserve current sinc25 quality.
- **No performance tests were run for A or B.** B’s target band is intentionally limited to 0.40 cycles/sample and does not imply rate-scaled anti-aliasing. The next independent candidate is E (fixed-coefficient IIR W=2/4), as ordered in the supplied plan.

## Limits and reproduction

This is double-precision offline analysis at 44.1 kHz using five selected SF2 samples and the existing 54 primary windows plus nine loop-entry probes. It is not a full SF2/MIDI render, float-table quantization test, real-time test, generalized listening result, or proof for other SoundFonts/loop zones. The periodic spline coefficients are built per selected loop region; that correctly exposes the per-zone preprocessing/storage problem rather than assuming one coefficient transform can serve every loop.

Run `./run.sh` from this folder to regenerate the metadata, compile with the recorded command, rerun all screens, and regenerate `summary.json`. The raw inputs and prior reference CSVs are read-only; generated CSVs are retained alongside `fractional_delay_screen.cpp`.
