# FDN output common-gain screen

## Candidate

The prototype accumulated the existing alternating left/right sign projections without multiplying each delay-line sample by its stored `±wet1` coefficient, then multiplied each channel sum by `wet1` once. It left the existing coefficient arrays and their update path in place for this first, conservative timing screen. This changes floating-point grouping, so the screen checks the public output and does not assume internal double bit equality.

## Screen

- Baseline and candidate both use upstream commit `ae31c51454c4d2bab98c9d8ce65e9a5eafcf741c`; candidate source was in isolated branch `perf/fdn-output-gain-20261016`.
- Release double, GCC 14.2, OpenMP enabled; one render core pinned to CPU 0, `OMP_NUM_THREADS=1`.
- One 10-second `All The Small Things` render at 44.1 kHz and 64-frame blocks; FDN reverb enabled, chorus disabled. Three AB/BA pairs, with one short warmup per build.
- The public float PCM hash matched for all six renders: `a12d57148bf899d87e18ca98ce11878c45edb6df8062aa2c1154cf76c3e97648`.

| Measure | Candidate / baseline |
|---|---:|
| Wall time, pair 1 | 0.9647 |
| Wall time, pair 2 | 0.9941 |
| Wall time, pair 3 | 1.0111 |
| Wall time, median | 0.9941 |
| Wall time, range | 0.9647–1.0111 |
| CPU time, median | 0.9945 |

The median improvement is about 0.6%, and the pair range crosses baseline. The public float output is byte-identical in this test, but internal double rounding and other room/width settings were not tested. Under the user's early-stop rule, the candidate does not show a stable end-to-end benefit; it was reverted before broader numerical or full-suite testing and is not counted.

The exact prototype is in [`candidate.patch`](candidate.patch). [`run_screen.py`](run_screen.py), `screen-manifest.json`, `screen.jsonl`, and raw per-pair PCM are retained for reproduction. The probe measures the FDN effect on this workload only; the result is not a general performance claim.
