# E2 second gate: stable-path coverage and timing budget

Status: coverage supports a minimal double-only production prototype; the measurements are diagnostic and are not a candidate-versus-baseline render result.

## Frozen workload

Baseline source is composite `6be67a906b9d679ea930c8841fd0cac0d8a0a25e`. Release double, GCC 14.2, 44.1 kHz, 64-frame requests, one core, `OMP_NUM_THREADS=1`, effects enabled, 10-second rendered interval, MuseScore General SF2. Inputs are Maestro_1, POP909_010 and All The Small Things; SHA-256 identities and the diagnostic library/probe hashes are in `summary.json` and the original manifest. The instrumentation is built separately from formal performance builds.

## Coverage

| MIDI | Standard calls | Stable samples / active samples | Stable sample fraction | Calls with gain ramp | Stable standard-filter time / render wall |
|---|---:|---:|---:|---:|---:|
| Maestro_1 | 178,964 | 7,011,840 / 11,453,696 | 61.2% | 98.1% | 17.0% |
| POP909_010 | 153,154 | 4,789,248 / 9,801,856 | 48.9% | 97.8% | 11.3% |
| All The Small Things | 223,659 | 14,313,780 / 14,313,780 | 100.0% | 97.6% | 25.7% |

Every standard-filter call is 64 frames except 22 final 46-frame calls in All The Small Things; those still contain 44 complete W4 groups and a 2-sample scalar tail. There were no custom-filter-active calls in these three SoundFont/MIDI cases. The candidate will remain after the custom filter call in the standard chain, so it need not disable itself when a custom filter is active.

The per-call diagnostic timer reports stable standard-filter time at 46.7% of total standard-filter time for Maestro_1, 35.7% for POP909_010, and 100% for All The Small Things. Timing probes add measurement overhead, so these fractions are cost-budget inputs, not acceptance measurements.

## Decision

Coverage is substantial: 49–100% of active standard-filter samples are stable at call entry, and the relevant workload uses 64-frame requests. Nearly all calls also carry a nonzero amplitude ramp, which the block implementation handles by preserving the original sequential gain updates. Together with the prepared-cost-included W4 microbenchmark medians of 0.65–0.76 in the ordinary configurations, this justifies trying a minimal production W4 path. A conservative local ratio of 0.75 would budget roughly 2.8–6.4% whole-render savings from measured stable-filter time shares; this is only an estimate, not a performance claim.

Before formal rendering, the candidate should be screened against these same three inputs with exact media and settings. The preregistered minimum useful improvement for the primary 64-frame, effects-on workload is δ=2%; the fixed-size confirmation should show a paired candidate/base render-time ratio 95% CI upper bound below 0.98 and the independent batch in the same direction. Any repeatable >3% slowdown in a guard workload stops expansion. Full correctness and PCM error gates remain mandatory. No production source or remote branch is accepted by this coverage report.

Reproduction: `summarize_stage2.py` parses the preserved instrumented logs into `summary.json`; raw coverage, stratified timing and unstratified timing files remain in the sibling directories.
