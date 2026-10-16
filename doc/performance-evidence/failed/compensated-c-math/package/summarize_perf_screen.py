#!/usr/bin/env python3
"""Summarize paired timing and A/A rows from the compensated-C screen."""
import csv
import json
import math
import random
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parent
RAW = ROOT / "perf-raw.csv"
SETUP = ROOT / "setup.csv"
SEED = 20261016
BOOTSTRAPS = 20000


def geo(values):
    return math.exp(sum(math.log(v) for v in values) / len(values))


def percentile(values, p):
    values = sorted(values)
    pos = (len(values) - 1) * p
    lo = int(pos)
    hi = min(lo + 1, len(values) - 1)
    frac = pos - lo
    return values[lo] * (1 - frac) + values[hi] * frac


def boot_ci(values, rng):
    estimates = []
    n = len(values)
    for _ in range(BOOTSTRAPS):
        estimates.append(geo([values[rng.randrange(n)] for _ in range(n)]))
    return [percentile(estimates, 0.025), percentile(estimates, 0.975)]


with RAW.open(newline="") as f:
    rows = list(csv.DictReader(f))
actual = defaultdict(list)
aa = defaultdict(list)
checksum_checks = 0
for row in rows:
    base = int(row["baseline_ns"])
    cand = int(row["candidate_ns"])
    if base <= 0 or cand <= 0:
        raise SystemExit("non-positive timing in perf-raw.csv")
    for timing, warm, label in (
        (row["baseline_checksum"], row["baseline_warm_checksum"], "baseline"),
        (row["candidate_checksum"], row["candidate_warm_checksum"], "candidate"),
    ):
        if float(timing) != float(warm):
            raise SystemExit(f"{label} timed checksum differs from warm output")
        checksum_checks += 1
    key = (row["sample"], row["segment"], float(row["factor"]), row["precision"])
    ratio = cand / base
    if row["precision"].endswith("-aa"):
        # Reconstruct chronological second-run / first-run ratio despite randomized order.
        aa_ratio = cand / base if row["order"] == "first-first" else base / cand
        aa[key].append(aa_ratio)
    else:
        actual[key].append((int(row["pair"]), ratio, base, cand, row["order"]))

rng = random.Random(SEED)
groups = []
for key, entries in sorted(actual.items()):
    if len(entries) != 30 or len({e[0] for e in entries}) != 30:
        raise SystemExit(f"expected 30 unique pairs for {key}; got {len(entries)}")
    ratios = [e[1] for e in entries]
    baseline_per_frame = [e[2] / 8192 for e in entries]
    candidate_per_frame = [e[3] / 8192 for e in entries]
    center = geo(ratios)
    groups.append({
        "sample": key[0], "segment": key[1], "factor": key[2], "precision": key[3],
        "pairs": len(ratios), "candidate_over_baseline_geomean": center,
        "speedup_geomean": 1 / center,
        "ratio_bootstrap_ci95": boot_ci(ratios, rng),
        "baseline_median_ns_per_frame": percentile(baseline_per_frame, 0.5),
        "candidate_median_ns_per_frame": percentile(candidate_per_frame, 0.5),
        "baseline_first_pairs": sum(1 for e in entries if e[4] == "baseline-first"),
    })

precision_summary = {}
for precision in ("float", "double"):
    ratios = [e[1] for key, entries in actual.items() if key[3] == precision for e in entries]
    center = geo(ratios)
    precision_summary[precision] = {
        "case_count": len([g for g in groups if g["precision"] == precision]),
        "paired_observations": len(ratios),
        "candidate_over_baseline_geomean": center,
        "speedup_geomean": 1 / center,
        "ratio_bootstrap_ci95": boot_ci(ratios, rng),
    }

aa_summary = {}
for key, ratios in sorted(aa.items()):
    center = geo(ratios)
    aa_summary["/".join(map(str, key))] = {
        "pairs": len(ratios), "second_over_first_geomean": center,
        "median_second_over_first": percentile(ratios, 0.5),
        "ratio_bootstrap_ci95": boot_ci(ratios, rng),
        "min": min(ratios), "max": max(ratios),
    }

with SETUP.open(newline="") as f:
    setup_rows = list(csv.DictReader(f))
setup_summary = []
for row in setup_rows:
    sample = row["sample"]
    factor = 1.0
    precision = "float"
    matches = [g for g in groups if g["sample"] == sample and g["factor"] == factor and g["precision"] == precision]
    if not matches:
        raise SystemExit(f"missing factor-1 float case for {sample}")
    g = matches[0]
    baseline_ns = g["baseline_median_ns_per_frame"]
    candidate_ns = g["candidate_median_ns_per_frame"]
    saved_ns = baseline_ns - candidate_ns
    extra_ms = max(0.0, float(row["candidate_prepare_ms"]) - float(row["base_load_ms"]))
    breakeven_frames = extra_ms * 1e6 / saved_ns if saved_ns > 0 else None
    setup_summary.append({
        "sample": sample,
        "base_load_ms": float(row["base_load_ms"]),
        "candidate_prepare_ms": float(row["candidate_prepare_ms"]),
        "excess_setup_ms_vs_int16_read": extra_ms,
        "unique_combined_storage_ratio": float(row["combined_storage_ratio"]),
        "allocated_combined_storage_ratio": (
            float(row["original_pcm_bytes"]) + float(row["candidate_prototype_allocated_table_bytes"])
        ) / float(row["original_pcm_bytes"]),
        "baseline_median_ns_per_frame": baseline_ns,
        "candidate_median_ns_per_frame": candidate_ns,
        "single_voice_breakeven_output_frames": breakeven_frames,
        "single_voice_breakeven_minutes_at_44100_hz": breakeven_frames / 44100 / 60 if breakeven_frames else None,
    })

result = {
    "metadata": {
        "raw_rows": len(rows), "actual_groups": len(groups), "aa_groups": len(aa),
        "checksum_equalities_checked": checksum_checks, "bootstrap_resamples": BOOTSTRAPS,
        "bootstrap_seed": SEED, "frames_per_render": 8192,
        "timing_scope": "paired hot-kernel renders on interior sample regions only",
    },
    "precision_summary": precision_summary,
    "groups": groups,
    "aa": aa_summary,
    "setup": setup_summary,
}
(ROOT / "perf-summary.json").write_text(json.dumps(result, indent=2) + "\n")

lines = ["# Paired hot-kernel summary", "", "Candidate/baseline ratio (<1 is faster):", ""]
for g in groups:
    lo, hi = g["ratio_bootstrap_ci95"]
    lines.append(
        f"- {g['sample']} {g['segment']} {g['factor']:g}x {g['precision']}: "
        f"{g['candidate_over_baseline_geomean']:.4f} [{lo:.4f}, {hi:.4f}] "
        f"({g['speedup_geomean']:.2f}x; {g['pairs']} pairs)"
    )
lines.extend(["", "Precision-wide paired geometric means:"])
for precision, summary in precision_summary.items():
    lo, hi = summary["ratio_bootstrap_ci95"]
    lines.append(f"- {precision}: {summary['candidate_over_baseline_geomean']:.4f} [{lo:.4f}, {hi:.4f}] ({summary['speedup_geomean']:.2f}x)")
lines.extend(["", "A/A second/first timing ratios:"])
for key, summary in aa_summary.items():
    lo, hi = summary["ratio_bootstrap_ci95"]
    lines.append(f"- {key}: geomean {summary['second_over_first_geomean']:.4f}, median {summary['median_second_over_first']:.4f}, CI [{lo:.4f}, {hi:.4f}], range [{summary['min']:.4f}, {summary['max']:.4f}]")
(ROOT / "perf-summary.txt").write_text("\n".join(lines) + "\n")
print("\n".join(lines))
print(f"validated {len(rows)} raw rows and {checksum_checks} warm/timed checksum equalities")
