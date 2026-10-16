#!/usr/bin/env python3
"""Create compact matched frequency, image, and full-renderer summaries."""
import csv
import json
import math
import statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parent


def read_csv(path):
    with (ROOT / path).open(newline="") as f:
        return list(csv.DictReader(f))


static = read_csv("static-threeway.csv")
static_summary = {}
for precision in ("float", "double"):
    by_method = {}
    for method in sorted({r["candidate"] for r in static}):
        rows = [r for r in static if r["precision"] == precision and r["candidate"] == method]
        passband = [r for r in rows if float(r["frequency_cycles_per_original_sample"]) <= .400000001]
        at = {}
        for f in (.1, .2, .3, .4, .45, .49):
            row = min(rows, key=lambda r: abs(float(r["frequency_cycles_per_original_sample"]) - f))
            at[str(f)] = {"measured_frequency": float(row["frequency_cycles_per_original_sample"]),
                          "gain_error_db": float(row["gain_error_db"]),
                          "relative_error_db": float(row["relative_error_db"])}
        by_method[method] = {
            "frequency_count_through_0p4": len(passband),
            "max_abs_gain_error_db_through_0p4": max(abs(float(r["gain_error_db"])) for r in passband),
            "worst_relative_error_db_through_0p4": max(float(r["relative_error_db"]) for r in passband),
            "selected_frequency_points": at,
        }
    static_summary[precision] = by_method

image_summary = {}
for precision, path in (("float", "image-threeway-float.csv"), ("double", "image-threeway-double.csv")):
    rows = read_csv(path)
    by_rate = {}
    for rate in (.5, 1.0, 2.0):
        methods = {}
        for method in ("production_cubic", "lut_sinc25", "compensated_6pt"):
            selected = [r for r in rows if float(r["requested_rate"]) == rate and r["band"] == "pass"
                        and float(r["input_cycles_per_sample"]) <= .400000001 and r["method"] == method]
            if not selected:
                continue
            methods[method] = {
                "tone_count": len(selected),
                "worst_residual_rms_dbfs": max(float(r["residual_rms_dbfs"]) for r in selected),
                "worst_max_spur_dbc": max(float(r["max_spur_dbc"]) for r in selected),
                "worst_analytic_error_dbfs": max(float(r["analytic_error_rms_dbfs"]) for r in selected),
            }
        by_rate[str(rate)] = methods
    image_summary[precision] = {
        "method": "16,384-frame interior sine render; same rate/frequency grid and FFT residual procedure for all arms",
        "input_frequency_limit_cycles_per_original_sample": .4,
        "rates": by_rate,
    }

renderer_files = {
    "production_cubic": ["renderer-cubic-strings-fx.json", "renderer-cubic-strings-fx-aa2.json"],
    "compensated_6pt": ["renderer-sixpoint-strings-fx.json", "renderer-sixpoint-strings-fx-aa2.json"],
    "lut_sinc25": ["renderer-sinc25-strings-fx.json"],
}
renderer = {}
for method, files in renderer_files.items():
    runs = [json.loads((ROOT / name).read_text()) for name in files]
    renderer[method] = {
        "runs": [{"file": name, "blocks": row["blocks"], "active_voices": row["active_voices"],
                  "p50_us": row["p50_us"], "p99_us": row["p99_us"], "p999_us": row["p999_us"],
                  "max_us": row["max_us"], "render_wall_ms": row["render_wall_ms"],
                  "audio_hash": row["audio_hash"], "candidate_prepare_ms": row["candidate_prepare_ms"],
                  "candidate_table_bytes": row["candidate_table_bytes"],
                  "source_pcm_bytes": row["source_pcm_bytes"],
                  "combined_storage_ratio": row["combined_storage_ratio"]} for name, row in zip(files, runs)],
        "p50_us_median": statistics.median(r["p50_us"] for r in runs),
        "p99_us_median": statistics.median(r["p99_us"] for r in runs),
        "p999_us_median": statistics.median(r["p999_us"] for r in runs),
        "render_wall_ms_median": statistics.median(r["render_wall_ms"] for r in runs),
        "deadline_us": runs[0]["deadline_us"],
    }

result = {
    "static_frequency_response": static_summary,
    "spectral_images": image_summary,
    "full_renderer_64_frame_callback": {
        "profile": "MuseScore General GM Strings program 48; 16 repeated note-ons at MIDI 60; 32 active voices; 44.1 kHz; built-in reverb and chorus enabled; 30,000 measured 64-frame calls per run; CPU 2 pinned; release build",
        "renderer": renderer,
        "relative_medians_vs_cubic": {
            "compensated6_p50_ratio": renderer["compensated_6pt"]["p50_us_median"] / renderer["production_cubic"]["p50_us_median"],
            "compensated6_p99_ratio": renderer["compensated_6pt"]["p99_us_median"] / renderer["production_cubic"]["p99_us_median"],
            "compensated6_p999_ratio": renderer["compensated_6pt"]["p999_us_median"] / renderer["production_cubic"]["p999_us_median"],
            "compensated6_render_wall_ratio": renderer["compensated_6pt"]["render_wall_ms_median"] / renderer["production_cubic"]["render_wall_ms_median"],
        },
    },
}
(ROOT / "threeway-evidence-summary.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps(result["full_renderer_64_frame_callback"]["relative_medians_vs_cubic"], indent=2))
print("frequency and image summaries written to threeway-evidence-summary.json")
