#!/usr/bin/env python3
"""Localize one compensated-C SF2 error and measure its composite complex response."""

import csv
import json
import math
import os
from pathlib import Path

from compensated_c_math import COEFFICIENTS

ROOT = Path(__file__).resolve().parent
DATA_ROOT = Path(os.environ["FLUIDSYNTH_C_SCREEN_DATA_ROOT"]).resolve()


def db20(value: float) -> float:
    return 20.0 * math.log10(value) if value > 0.0 else float("-inf")


def rms(values):
    return math.sqrt(sum(x * x for x in values) / len(values))


def read_csv(path: Path):
    with path.open(newline="") as stream:
        return list(csv.DictReader(stream))


def inspect_window():
    sample = "strings-mid"
    segment = "sustain"
    factor = 0.5
    precision = "double"
    candidate = "6pt"
    quality = next(
        row for row in read_csv(ROOT / "quality.csv")
        if row["sample"] == sample
        and row["segment"] == segment
        and float(row["transpose_factor"]) == factor
        and row["precision"] == precision
        and row["candidate"] == candidate
    )
    meta = next(row for row in read_csv(DATA_ROOT / "fractional-delay-ab" / "samples.csv")
                if row["label"] == sample)
    step = float(meta["base_step"]) * factor
    raw_path = (DATA_ROOT / "cubic-sinc25" / "sf2-raw"
                / f"{sample}-{segment}-r{step:.6f}.csv")
    raw = read_csv(raw_path)
    reference = [float(row["ref4095"]) for row in raw]
    sinc_error = [float(row["sinc25"]) - target for row, target in zip(raw, reference)]
    sinc_error_rms = rms(sinc_error)
    sinc_error_max = max(abs(x) for x in sinc_error)
    candidate_error_dbfs = float(quality["error_rms_dbfs"])
    candidate_error_max_dbfs = float(quality["error_max_dbfs"])
    reference_rms_dbfs = float(quality["reference_rms_dbfs"])
    loop_start = int(meta["loop_start"])
    loop_end = int(meta["loop_end"])
    start = loop_start + 1024.0 + 0.271828
    last = start + step * (len(raw) - 1)
    return {
        "sample": sample,
        "segment": segment,
        "precision": precision,
        "frames": len(raw),
        "step_source_samples_per_output": step,
        "transpose_factor": factor,
        "reference_rms_dbfs": reference_rms_dbfs,
        "candidate_error_rms_dbfs": candidate_error_dbfs,
        "candidate_error_rms_linear": 10.0 ** (candidate_error_dbfs / 20.0),
        "candidate_error_max_dbfs": candidate_error_max_dbfs,
        "sinc25_error_rms_dbfs": db20(sinc_error_rms),
        "sinc25_error_rms_linear": sinc_error_rms,
        "sinc25_error_max_dbfs": db20(sinc_error_max),
        "candidate_to_sinc25_error_delta_db": candidate_error_dbfs - db20(sinc_error_rms),
        "candidate_error_relative_to_reference_rms_db": candidate_error_dbfs - reference_rms_dbfs,
        "sinc25_error_relative_to_reference_rms_db": db20(sinc_error_rms) - reference_rms_dbfs,
        "reference_convergence_margin_db": float(quality["reference_margin_db"]),
        "loop_start": loop_start,
        "loop_end": loop_end,
        "window_start_source_position": start,
        "window_last_source_position": last,
        "distance_from_loop_start_at_window_start": start - loop_start,
        "distance_to_loop_end_at_window_end": loop_end - last,
        "crosses_loop_boundary": last >= loop_end,
        "reference_raw_path": "cubic-sinc25/sf2-raw/" + raw_path.name,
    }


def inspect_complex_response():
    coeff_rows = [row for row in read_csv(ROOT / "prefilter_coefficients.csv")
                  if row["kernel_points"] == "6"]
    prefilter = [float(row["coefficient_float32"]) for row in coeff_rows]
    midpoint = (len(prefilter) - 1) / 2.0
    taps = COEFFICIENTS[6]
    max_phase = (0.0, None, None)
    max_gain = (0.0, None, None)
    samples = []
    frequencies = 800
    phases = 1024
    z_values = [-0.5 + i / phases for i in range(phases + 1)]
    for fi in range(frequencies + 1):
        f = 0.4 * fi / frequencies
        theta = math.pi * f
        cosines = [math.cos((j + 0.5) * theta) for j in range(3)]
        sines = [math.sin((j + 0.5) * theta) for j in range(3)]
        hp = sum(v * math.cos(2.0 * math.pi * f * (i - midpoint))
                 for i, v in enumerate(prefilter))
        for z in z_values:
            z2 = z * z
            z3 = z2 * z
            z4 = z2 * z2
            re = 0.0
            im = 0.0
            for j, a in enumerate(taps):
                re += 2.0 * cosines[j] * (a[0] + a[2] * z2 + a[4] * z4)
                im += 2.0 * sines[j] * (a[1] * z + a[3] * z3)
            mag = hp * math.hypot(re, im)
            phase = math.atan2(im, re) - theta * z
            phase = (phase + math.pi) % (2.0 * math.pi) - math.pi
            gain_db = abs(db20(mag))
            if abs(phase) > max_phase[0]:
                max_phase = (abs(phase), f, z)
            if gain_db > max_gain[0]:
                max_gain = (gain_db, f, z)
        if fi in (100, 400, 800):
            f = 0.4 * fi / frequencies
            theta = math.pi * f
            for z in (-0.5, 0.0, 0.5):
                cosines = [math.cos((j + 0.5) * theta) for j in range(3)]
                sines = [math.sin((j + 0.5) * theta) for j in range(3)]
                hp = sum(v * math.cos(2.0 * math.pi * f * (i - midpoint))
                         for i, v in enumerate(prefilter))
                re = im = 0.0
                for j, a in enumerate(taps):
                    z2 = z * z
                    re += 2.0 * cosines[j] * (a[0] + a[2] * z2 + a[4] * z2 * z2)
                    im += 2.0 * sines[j] * (a[1] * z + a[3] * z2 * z)
                residual = hp * complex(re, im) * complex(math.cos(theta * z), -math.sin(theta * z))
                samples.append({"frequency": f, "z": z, "magnitude_error_db": db20(abs(residual)),
                                "phase_error_deg": math.degrees(math.atan2(residual.imag, residual.real))})

    math_summary = json.loads((ROOT / "math-summary.json").read_text())
    half = math_summary["finite_4095_tap_half_phase_interpolator"]
    coeff_symmetry = max(abs(prefilter[i] - prefilter[-1 - i]) for i in range(len(prefilter)))
    return {
        "model": "centered 65-tap float32 FIR * six-point polynomial response * ideal-delay removal",
        "formula": "H_total(f,z)=H_prefilter_centered(f)*H_6pt(z,f)*exp(-j*pi*f*z); table half-phase reconstruction is bounded separately",
        "frequency_grid_step": 0.4 / frequencies,
        "phase_grid_step_z": 1.0 / phases,
        "prefilter_taps": len(prefilter),
        "prefilter_coefficient_sum": sum(prefilter),
        "prefilter_symmetry_max_abs": coeff_symmetry,
        "prefilter_centered_phase_through_passband_rad": 0.0,
        "causal_fir_linear_phase_delay_if_used_causally_source_samples": midpoint,
        "effective_delay_in_this_offline_centered_application_source_samples": 0.0,
        "max_sampled_composite_magnitude_error_db": max_gain[0],
        "max_sampled_composite_magnitude_error_at": {"frequency": max_gain[1], "z": max_gain[2]},
        "max_sampled_composite_phase_error_rad": max_phase[0],
        "max_sampled_composite_phase_error_deg": math.degrees(max_phase[0]),
        "max_sampled_composite_phase_error_at": {"frequency": max_phase[1], "z": max_phase[2]},
        "phase_error_is_single_constant_delay": False,
        "finite_half_phase_table_max_complex_error": half["worst_complex_error_magnitude"],
        "finite_half_phase_output_error_bound_6point": half["conservative_output_error_bound_from_midpoint_kernel"]["6"],
        "response_points": samples,
        "scope": "Analytic coefficient response uses the actual quantized FIR coefficients and six-point constants. Per-sample float rounding is not a linear frequency response; the SF2 error measurement includes it.",
    }


def main():
    result = {
        "window": inspect_window(),
        "complex_response": inspect_complex_response(),
    }
    (ROOT / "alignment-diagnosis.json").write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
