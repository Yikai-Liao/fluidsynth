# Compensated C error and alignment localization

Date: 2026-10-16. Focused diagnostic only; no quality threshold changed and no playback-performance test was run.

## Interior periodic-sustain failure

The selected case is `strings-mid/sustain`, double precision, transpose factor 0.5, source step 0.342448730225 samples/output, 882 frames. Its periodic sustain starts at source position 39164.271828: 1024.27 samples after loop start 38140. It ends at 39465.969159, still 29,518.03 samples before loop end 68984, and does not cross either loop boundary. This is not an entry/seam window.

Both algorithms are compared to the same 4095-tap reference. The compensated candidate's RMS error comes from `quality.csv`; sinc25's RMS error is recomputed directly from the saved `sinc25` and `ref4095` columns in the corresponding frozen raw window.

| Quantity | Compensated six-point | sinc25 |
|---|---:|---:|
| RMS error, absolute dBFS | −81.6725 dBFS | −97.3505 dBFS |
| RMS error, linear full scale | 8.24851e−5 | 1.35668e−5 |
| RMS error relative to window RMS | −69.1048 dB | −84.7828 dB |
| Maximum absolute error | −75.3943 dBFS | −88.4043 dBFS |

The candidate error is 15.6780 dB, or 6.08× RMS, above sinc25. The signal window itself is −12.5677 dBFS RMS. The 2047-versus-4095 reference difference is −134.014 dBFS RMS, 52.34 dB below the candidate error. The reference is therefore sufficiently converged for this comparison. The absolute values show sinc25's baseline error is already small; the ratio alone would overstate what can be concluded about audibility. The frozen relative-quality gate nevertheless fails.

## Composite complex response and delay

The response calculation uses the actual 65 float32 FIR coefficients, the six-point polynomial constants, and the ideal fractional-delay removal:

```text
H_total(f,z) = H_prefilter_centered(f) * H_6pt(z,f) * exp(-j*pi*f*z)
```

Here `f` is cycles per original source sample and `z` is the centered fractional phase on the 2× table. The scan covers 0–0.40 with frequency spacing 0.0005 and 1025 equally spaced phase values. Maximum sampled magnitude error is 0.010194 dB at `f=0.3955, z=−0.38671875`; maximum sampled residual phase is 0.009554° at `f=0.40, z=−0.5`. At `f=0.40`, phase error is +0.009554° at `z=−0.5`, zero at `z=0`, and −0.009554° at `z=+0.5`. This is a small phase-dependent residual, not a single constant delay.

The FIR is exactly symmetric and is applied as a centered sum over offsets −32…+32. Its conventional causal realization would have a 32-source-sample linear-phase delay, but this offline sample-table precomputation reads both past and future samples around index `i`, so its effective sample shift is zero. The table origin is consistent: even slot `2*i` stores filtered sample `i`; odd slot `2*i+1` reconstructs time `i+0.5` with weights `sinc(j−0.5)`; playback evaluates the same source phase after multiplying the 32.32 phase coordinate by two. No half-source-sample offset was found.

The finite 4095-tap half-phase reconstruction has a maximum complex-response error of `1.4272e−9` through the passband; the saved six-point coefficient bound limits its output contribution to `7.1554e−10`. The complex-response model excludes data-dependent float32 rounding at the stored table samples; those operations are present in the real-SF2 comparison. The full result therefore rules out a gross fixed delay or a loop-entry-only explanation, but it does not attribute every part of the residual to one source. The six-point candidate still fails the frozen real-sample gate and remains stopped.

## Reproduction

From this directory:

```sh
FLUIDSYNTH_C_SCREEN_DATA_ROOT=<data-root> python3 diagnose_alignment.py
```

The data root contains `fractional-delay-ab/samples.csv`, `cubic-sinc25/sf2-samples/`, and `cubic-sinc25/sf2-raw/`. The run writes `alignment-diagnosis.json`. The script's frequency/phase grid is a diagnostic scan, not a continuous interval proof.
