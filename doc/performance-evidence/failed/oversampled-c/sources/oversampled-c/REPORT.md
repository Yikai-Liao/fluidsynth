# C: 2× precomputed sample representation screen

Date: 2026-10-16. This is an isolated offline quality and storage screen for the supplied plan's candidate C. It makes no production change and makes no render-performance claim.

## Frozen setup

- Baseline source: measurement composite `6be67a906b9d679ea930c8841fd0cac0d8a0a25e`; main checkout remains at `ae31c51454c4d2bab98c9d8ce65e9a5eafcf741c`.
- Five existing MuseScore General SF2 samples: piano, strings, saw lead, closed hi-hat and cymbal. The 54 frozen 882-frame windows cover attack, interior/sustain, loop seam, release exit and tail at 0.5×, 1× and 2× source-step factors. Nine additional 128-frame probes cross the first loop wrap.
- The evaluator builds a 2× float representation from the extracted PCM. Odd samples use a normalized 4095-tap Hann-windowed sinc at half phase; the looped samples have separate finite/attack and periodic-loop representations, with the loop-end guard recomputed using first-pass loop semantics. The candidate then uses the published 2× optimal 4-point 4th-order or 6-point 4th-order z-form polynomial, in float and double arithmetic. The formulas are transcribed from [Niemitalo (2001), sections 6.11–6.12](https://yehar.com/blog/wp-content/uploads/2009/08/deip.pdf).
- This screen does not apply passband pre-emphasis. The paper says its quality treatment presumes pre-emphasis and leaves the oversampling/pre-emphasis filter design to the implementer. The result therefore applies to this direct 2× table form; an equalized variant remains untested.
- Quality gates are the supplied plan's budgets: mapped/down nonquiet median error change vs sinc25 ≤+1 dB, worst window ≤+3 dB; reference convergence margin ≤−20 dB; quiet absolute RMS error ≤−80 dBFS; maximum absolute gain error ≤0.1 dB through 0.40 cycles per original sample. These are screening thresholds, not universal audibility limits.
- GCC release build: `-O3 -DNDEBUG -std=c++17 -ffp-contract=off -Wall -Wextra -Wpedantic`. Double and float output arithmetic were both screened. The standalone run was not pinned or repeated for timing; preparation time is only a single-run directional measurement.

## Alignment and reference checks

The evaluator regenerated the current sinc25 and 4095-tap references for all 54 primary windows. Worst RMS differences from the saved rows were −128.27 dBFS for sinc25 and −129.64 dBFS for the long reference; worst single-sample differences were −106.12 and −106.14 dBFS, respectively. This supports phase, window and reference alignment. All candidate rows met the 20 dB reference-convergence margin: the closest margin was −25.02 dB for 4-point and −29.19 dB for 6-point.

## Results

| Candidate | Precision | Active mapped/down windows | Median error change vs sinc25 | Worst change | Max passband gain error through 0.40 | Quiet max absolute RMS error | Decision |
|---|---|---:|---:|---:|---:|---:|---|
| 4-point 4th-order | float | 32 | +16.44 dB | +37.40 dB | 2.96 dB | −96.62 dBFS | fail |
| 6-point 4th-order | float | 32 | +21.12 dB | +42.78 dB | 5.65 dB | −92.45 dBFS | fail |
| 4-point 4th-order | double | 32 | +16.44 dB | +37.40 dB | 2.96 dB | −96.62 dBFS | fail |
| 6-point 4th-order | double | 32 | +21.12 dB | +42.78 dB | 5.65 dB | −92.45 dBFS | fail |

The largest degradation is concentrated in the strings sustain/loop-seam windows. At 0.40 cycles per original sample, the measured gain errors are −2.96 dB (4-point) and −5.65 dB (6-point). At 0.45 cycles/sample they are −3.76 dB and −7.23 dB; at 0.49 they are −4.48 dB and −8.66 dB. Both candidates fail the required passband gain limit and the real-sample error budget by wide margins. Float and double results are nearly identical, so arithmetic precision is not the cause.

The nine first-loop-wrap probes per candidate show median error increases of +9.45 dB (4-point) and +14.58 dB (6-point); worst cases are +40.10 and +45.45 dB. They pass reference convergence and finite-output checks, but expose additional seam sensitivity.

The screen allocated both finite and attack 2× tables for looped samples so their first-pass loop guard could be compared without aliasing attack data. Including original int16 PCM and the periodic loop table, this conservative screen layout reaches 12.48× the original PCM bytes on the short saw sample. A lower-bound layout with one full 2× float table plus a periodic loop table reaches 8.48× on that sample (piano 6.01×, strings 6.79×). One-shot samples require 5× including the original PCM. These are this layout's storage totals, not a proposed production format or a project memory-budget decision.

The single preparation run took 5.72 seconds total across the five samples. Since the quality gates fail, no playback microbenchmark, cold-cache test, high-polyphony test or amortization analysis was run.

## Decision and limits

**Stop this un-pre-emphasized C form at the quality gate.** It misses both the static passband and real-SF2 budgets, so further optimization or playback integration is not justified. The paper's pre-emphasis prerequisite is an important scope limit: these results do not reject every possible compensated 2× representation. No production source changed, and this screen is not a performance optimization result.

This is a first-stage analysis on five samples and 54 existing windows plus nine loop-entry probes. It does not cover all SoundFonts, multiple zones sharing PCM, arbitrary loop lengths, rapid pitch modulation, cross-layer transitions, full MIDI renders or real-time callback behavior.

Run `./run.sh` in this folder to rebuild the evaluator, rerun the screen and regenerate CSVs, summary and hashes. Raw tables are `quality.csv`, `static.csv`, `loop-entry.csv`, `baseline-reproduction.csv` and `storage.csv`; file identities are in `manifest.json`.
