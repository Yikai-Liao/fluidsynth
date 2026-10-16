#!/usr/bin/env python3
"""Join compensated-C, production cubic and LUT-sinc25 on identical SF2 windows."""
import csv
import glob
import json
import math
import statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parent
RAW_DIR = Path("/root/code/fluidsynth-analysis-20261015/cubic-sinc25/sf2-raw")


def db20(value):
    return -math.inf if value <= 0 else 20 * math.log10(value)


def rms_error(rows, col, ref="ref4095"):
    return math.sqrt(sum((float(r[col]) - float(r[ref])) ** 2 for r in rows) / len(rows))


def describe(values):
    if not values:
        return None
    return {"count": len(values), "median_db": statistics.median(values), "min_db": min(values), "max_db": max(values)}


raw_index = {}
for path in RAW_DIR.glob("*.csv"):
    stem = path.stem
    try:
        pre, rate = stem.rsplit("-r", 1)
        sample, segment = pre.rsplit("-", 1)
        raw_index[(sample, segment, round(float(rate), 6))] = path
    except ValueError:
        continue

joined = []
with (ROOT / "quality.csv").open(newline="") as f:
    for q in csv.DictReader(f):
        if q["candidate"] != "6pt" or q["precision"] != "double":
            continue
        key = (q["sample"], q["segment"], round(float(q["step"]), 6))
        path = raw_index.get(key)
        if path is None:
            raise SystemExit(f"missing matching long-reference window for {key}")
        with path.open(newline="") as rf:
            raw = list(csv.DictReader(rf))
        if len(raw) != int(q["frames"]):
            raise SystemExit(f"window size mismatch for {key}: {len(raw)} vs {q['frames']}")
        cubic_db = db20(rms_error(raw, "cubic"))
        sinc_db = db20(rms_error(raw, "sinc25"))
        six_db = float(q["error_rms_dbfs"])
        joined.append({
            "sample": q["sample"], "segment": q["segment"], "step": float(q["step"]),
            "transpose_factor": float(q["transpose_factor"]), "frames": len(raw),
            "reference_rms_dbfs": float(q["reference_rms_dbfs"]), "quiet_reference": q["quiet_reference"] == "1",
            "cubic_error_rms_dbfs": cubic_db, "sixpoint_error_rms_dbfs": six_db,
            "sinc25_error_rms_dbfs": sinc_db,
            "sixpoint_minus_cubic_db": six_db - cubic_db,
            "sixpoint_minus_sinc25_db": six_db - sinc_db,
        })

active = [r for r in joined if not r["quiet_reference"]]
primary = [r for r in active if r["transpose_factor"] <= 1.0]
by_segment = {}
for segment in sorted({r["segment"] for r in joined}):
    rows = [r for r in joined if r["segment"] == segment and not r["quiet_reference"]]
    by_segment[segment] = {
        "cases": len(rows), "sixpoint_minus_cubic_db": describe([r["sixpoint_minus_cubic_db"] for r in rows]),
        "sixpoint_minus_sinc25_db": describe([r["sixpoint_minus_sinc25_db"] for r in rows]),
        "cases_better_than_cubic": sum(r["sixpoint_minus_cubic_db"] < 0 for r in rows),
    }

result = {
    "metadata": {"matched_windows": len(joined), "active_windows": len(active), "primary_active_windows_factor_le_1": len(primary),
                 "same_long_reference": "4095-point sinc reference in cubic-sinc25/sf2-raw; same saved window and transposition",
                 "precision": "double candidate for stable algorithm comparison; cubic/sinc outputs are from the production baseline render"},
    "active_all_transpositions": {
        "sixpoint_minus_cubic_db": describe([r["sixpoint_minus_cubic_db"] for r in active]),
        "sixpoint_minus_sinc25_db": describe([r["sixpoint_minus_sinc25_db"] for r in active]),
        "sixpoint_better_than_cubic_cases": sum(r["sixpoint_minus_cubic_db"] < 0 for r in active),
        "sixpoint_worse_than_cubic_cases": sum(r["sixpoint_minus_cubic_db"] > 0 for r in active),
    },
    "active_primary_transpositions_le_1": {
        "sixpoint_minus_cubic_db": describe([r["sixpoint_minus_cubic_db"] for r in primary]),
        "sixpoint_minus_sinc25_db": describe([r["sixpoint_minus_sinc25_db"] for r in primary]),
        "sixpoint_better_than_cubic_cases": sum(r["sixpoint_minus_cubic_db"] < 0 for r in primary),
        "sixpoint_worse_than_cubic_cases": sum(r["sixpoint_minus_cubic_db"] > 0 for r in primary),
    },
    "by_segment": by_segment,
    "matched_windows": joined,
}
(ROOT / "quality-threeway-summary.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps({k: v for k, v in result.items() if k != "matched_windows"}, indent=2))
