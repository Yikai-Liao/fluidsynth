#!/usr/bin/env python3
"""Summarize the compensated C real-SF2 screen without changing frozen gates."""

import csv
import json
import statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parent


def read_csv(name):
    with (ROOT / name).open(newline="") as f:
        return list(csv.DictReader(f))


def numbers(rows, key):
    return [float(r[key]) for r in rows]


def describe(rows):
    all_rows = list(rows)
    primary = [r for r in all_rows if float(r["transpose_factor"]) <= 1.0]
    active = [r for r in primary if r["quiet_reference"] == "0"]
    quiet = [r for r in primary if r["quiet_reference"] == "1"]
    deltas = numbers(active, "error_vs_sinc25_db")
    margins = numbers(all_rows, "reference_margin_db")
    quiet_errors = numbers(quiet, "error_rms_dbfs")
    return {
        "all_rows": len(all_rows),
        "primary_rows_factor_le_1": len(primary),
        "active_primary_rows": len(active),
        "quiet_primary_rows": len(quiet),
        "median_active_error_delta_vs_sinc25_db": statistics.median(deltas),
        "worst_active_error_delta_vs_sinc25_db": max(deltas),
        "worst_active_sample_segment_factor": max(active, key=lambda r: float(r["error_vs_sinc25_db"]))[
            "sample"] + "/" + max(active, key=lambda r: float(r["error_vs_sinc25_db"]))["segment"] + "/factor=" +
            max(active, key=lambda r: float(r["error_vs_sinc25_db"]))["transpose_factor"],
        "worst_reference_margin_db_all_rows": max(margins),
        "quiet_worst_error_dbfs": max(quiet_errors) if quiet_errors else None,
        "nonfinite_rows": sum(r["finite"] != "1" for r in all_rows),
        "passes_active_median_gate": statistics.median(deltas) <= 1.0,
        "passes_active_worst_gate": max(deltas) <= 3.0,
        "passes_reference_convergence_gate": max(margins) <= -20.0,
        "passes_quiet_absolute_gate": not quiet_errors or max(quiet_errors) <= -80.0,
    }


quality = read_csv("quality.csv")
loop = read_csv("loop-entry.csv")
static = read_csv("static.csv")
baseline = read_csv("baseline-reproduction.csv")
storage = read_csv("storage.csv")
math_summary = json.loads((ROOT / "math-summary.json").read_text())

precision_results = {}
for precision in ("float", "double"):
    rows = [r for r in quality if r["precision"] == precision]
    loop_rows = [r for r in loop if r["precision"] == precision]
    static_rows = [r for r in static if r["precision"] == precision
                   and float(r["frequency_cycles_per_original_sample"]) <= 0.4000000001]
    q = describe(rows)
    loop_deltas = numbers(loop_rows, "error_vs_sinc25_db")
    loop_margins = numbers(loop_rows, "reference_margin_db")
    q["loop_entry"] = {
        "rows": len(loop_rows),
        "median_error_delta_vs_sinc25_db": statistics.median(loop_deltas),
        "worst_error_delta_vs_sinc25_db": max(loop_deltas),
        "worst_reference_margin_db": max(loop_margins),
    }
    q["static_response"] = {
        "frequencies_through_0p4": len(static_rows),
        "max_abs_gain_error_db_through_0p4": max(abs(x) for x in numbers(static_rows, "gain_error_db")),
        "worst_static_relative_error_db_through_0p4": max(numbers(static_rows, "relative_error_db")),
    }
    precision_results[precision] = q

baseline_summary = {
    "rows": len(baseline),
    "max_sinc25_rms_reproduction_dbfs": max(numbers(baseline, "sinc25_rms_diff")),
    "max_sinc25_peak_reproduction_dbfs": max(numbers(baseline, "sinc25_max_diff")),
    "max_long_reference_rms_reproduction_dbfs": max(numbers(baseline, "long_ref_rms_diff")),
    "max_long_reference_peak_reproduction_dbfs": max(numbers(baseline, "long_ref_max_diff")),
}

storage_rows = []
for r in storage:
    storage_rows.append({
        "sample": r["sample"],
        "persistent_screen_ratio_vs_original": float(r["screen_ratio_vs_original"]),
        "lower_bound_full_plus_loop_ratio_vs_original": float(r["minimal_ratio_vs_original"]),
        "prefilter_scratch_peak_bytes": int(r["prefilter_scratch_peak_bytes"]),
        "single_prepare_ms_unpaired": float(r["prepare_ms"]),
    })

alias_steps = math_summary["folded_image_screen"]["per_kernel"]["6"]
alias_summary = {
    "frequency_step": math_summary["folded_image_screen"]["frequency_grid_step"],
    "image_orders": math_summary["folded_image_screen"]["image_orders"],
    "worst_alias_rms_input_db": max(x["worst_alias_rms_input_db"] for x in alias_steps.values()
                                     if x["worst_alias_rms_input_db"] is not None),
    "worst_single_alias_line_input_db": max(x["worst_single_folded_alias_line_input_db"] for x in alias_steps.values()
                                             if x["worst_single_folded_alias_line_input_db"] is not None),
    "worst_coherent_peak_sum_upper_db": max(x["worst_alias_peak_sum_input_db"] for x in alias_steps.values()
                                            if x["worst_alias_peak_sum_input_db"] is not None),
    "by_step": alias_steps,
}

float_result = precision_results["float"]
static_pass = float_result["static_response"]["max_abs_gain_error_db_through_0p4"] <= 0.1
overall_pass = all((
    float_result["passes_active_median_gate"],
    float_result["passes_active_worst_gate"],
    float_result["passes_reference_convergence_gate"],
    float_result["passes_quiet_absolute_gate"],
    float_result["nonfinite_rows"] == 0,
    static_pass,
))

summary = {
    "screen": "source-rate 65-tap float32 FIR pre-emphasis + 2x float table + 6-point 4th-order polynomial",
    "scope": "offline quality screen only; no production changes and no playback performance measurement",
    "frozen_gates": {
        "active_median_error_delta_vs_sinc25_db_max": 1.0,
        "active_worst_error_delta_vs_sinc25_db_max": 3.0,
        "reference_margin_db_max": -20.0,
        "quiet_absolute_error_dbfs_max": -80.0,
        "static_passband_gain_error_abs_db_max_through_0p4": 0.1,
    },
    "precision_results": precision_results,
    "baseline_reproduction": baseline_summary,
    "storage_and_prep": {
        "rows": storage_rows,
        "max_persistent_screen_ratio_vs_original": max(r["persistent_screen_ratio_vs_original"] for r in storage_rows),
        "max_lower_bound_ratio_vs_original": max(r["lower_bound_full_plus_loop_ratio_vs_original"] for r in storage_rows),
        "max_prefilter_scratch_peak_bytes": max(r["prefilter_scratch_peak_bytes"] for r in storage_rows),
        "prep_times_are_single_unpaired_records": True,
    },
    "folded_image_diagnostic": alias_summary,
    "decision": {
        "overall_quality_gate_pass": overall_pass,
        "stop_before_performance_testing": not overall_pass,
        "failed_gates": [
            key for key, passed in (
                ("active median error delta", float_result["passes_active_median_gate"]),
                ("active worst error delta", float_result["passes_active_worst_gate"]),
                ("reference convergence", float_result["passes_reference_convergence_gate"]),
                ("quiet absolute error", float_result["passes_quiet_absolute_gate"]),
                ("finite outputs", float_result["nonfinite_rows"] == 0),
                ("static passband gain", static_pass),
            ) if not passed
        ],
    },
}
(ROOT / "sf2-summary.json").write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n")
print(json.dumps(summary["decision"], indent=2))
for precision, result in precision_results.items():
    print(precision, "median/worst delta", result["median_active_error_delta_vs_sinc25_db"],
          result["worst_active_error_delta_vs_sinc25_db"], "static gain", result["static_response"]["max_abs_gain_error_db_through_0p4"])
