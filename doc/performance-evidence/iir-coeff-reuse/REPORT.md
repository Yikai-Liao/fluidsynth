# IIR coefficient reuse review

## Change

`fluid_iir_filter_apply_local()` recalculates the biquad coefficients after every smoothed cutoff update. The coefficients depend on cutoff through the existing quantized sin/cos-table index, and on Q. The candidate keeps that index local to one apply call and skips a recalculation only when Q is stable and the index is unchanged.

The first smoothing update in each call still recalculates. Q smoothing continues to recalculate on every sample. The cache is local; this adds no filter state, ABI change, or shared mutable state.

## Correctness review

- The extracted table-index helper uses the same cast, table step, and clipping as the existing coefficient calculation.
- A moving Q invalidates coefficients regardless of cutoff index. Frequency-only smoothing compares the next index and updates the cached value after each step.
- The first-update guard preserves the existing recalculation point before this loop reuses a table-index match.
- Release float and double builds each passed all 41 enabled tests.
- For Maestro_1, POP909_010, and All The Small Things, 60-second API renders at 44.1 kHz with effects enabled matched baseline PCM hashes for both 64- and 8192-frame requests in both precision builds.
- `git diff --check` passed. No changes were made to public interfaces or filter state layout.

## Performance evidence

Five paired AB/BA repetitions per input and request size; candidate/baseline median wall-time ratios are below 1 when candidate is faster. CPU ratios followed the same pattern. RSS ratios stayed within 0.1% of baseline.

| Precision | MIDI | 64 frames | 8192 frames |
| --- | --- | ---: | ---: |
| Double | Maestro_1 | 0.871 | 0.869 |
| Double | POP909_010 | 0.871 | 0.873 |
| Double | All The Small Things | 1.002 | 1.053 |
| Float | Maestro_1 | 0.892 | 0.894 |
| Float | POP909_010 | 0.871 | 0.888 |
| Float | All The Small Things | 1.002 | 0.956 |

The first two inputs show roughly 11–13% faster renders in both precision builds. The third is near parity and varies by precision and request size. It does not support a universal speedup claim.

The runs used GCC 14.2 Release builds on a six-vCPU Intel Xeon Gold 6140 host. Both arms used OpenMP with one thread, 256 voices, gain 0.2, 4th-order interpolation, sample-timed MIDI, and the same MuseScore General SF2. The manifests include source, binary, MIDI, and soundfont hashes plus full settings.

## Evidence files

- `paired-cachevalid-summary.json` and `paired-cachevalid-manifest.json` — double build
- `paired-float-summary.json` and `paired-float-manifest.json` — float build
- `pcm-parity-cachevalid-20261016.json` and `pcm-parity-float-20261016.json` — exact output hashes

These are offline API renders, not tests with a realtime audio driver. Evidence covers three MIDI excerpts, one soundfont, 44.1 kHz, and the two request sizes above.


## Fork branch

Source commit: `25d275e4e5b5ca7c794a1baea8956a1520cd6943` on `perf/iir-coeff-reuse-20261016`. This evidence was added in a follow-up documentation commit on the same fork branch. No upstream pull request has been created.
