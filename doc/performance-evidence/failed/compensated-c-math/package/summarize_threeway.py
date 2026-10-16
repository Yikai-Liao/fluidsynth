#!/usr/bin/env python3
"""Summarize the paired three-way cubic / compensated-six-point / sinc25 screen."""
import csv
import json
import math
import random
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent
RAW = ROOT / "threeway-raw.csv"
BOOTSTRAPS = 20000
SEED = 20261016


def geo(values):
    return math.exp(sum(math.log(x) for x in values) / len(values))


def percentile(values, p):
    values = sorted(values)
    x = (len(values) - 1) * p
    lo = int(x)
    hi = min(lo + 1, len(values) - 1)
    return values[lo] * (1 - (x - lo)) + values[hi] * (x - lo)


def ci(values, rng):
    n = len(values)
    draws = [geo([values[rng.randrange(n)] for _ in range(n)]) for _ in range(BOOTSTRAPS)]
    return [percentile(draws, .025), percentile(draws, .975)]


with RAW.open(newline="") as f:
    rows = list(csv.DictReader(f))
groups = defaultdict(list)
checksum_checks = 0
for row in rows:
    ns = {name: int(row[name + "_ns"]) for name in ("cubic", "sixpoint", "sinc25")}
    if min(ns.values()) <= 0:
        raise SystemExit("non-positive timing in threeway-raw.csv")
    for name in ("cubic", "sixpoint", "sinc25"):
        if float(row[name + "_checksum"]) != float(row[name + "_warm_checksum"]):
            raise SystemExit(f"{name} timed/warm checksum mismatch")
        checksum_checks += 1
    key = (row["sample"], row["segment"], float(row["factor"]), row["precision"])
    groups[key].append((int(row["pair"]), ns, row["order"]))

rng = random.Random(SEED)
summaries = []
for key, entries in sorted(groups.items()):
    if len(entries) != 30 or len({x[0] for x in entries}) != 30:
        raise SystemExit(f"expected 30 unique triplets for {key}; got {len(entries)}")
    ratios6 = [e[1]["sixpoint"] / e[1]["cubic"] for e in entries]
    ratiosinc = [e[1]["sinc25"] / e[1]["cubic"] for e in entries]
    ratios6inc = [e[1]["sixpoint"] / e[1]["sinc25"] for e in entries]
    summaries.append({
        "sample": key[0], "segment": key[1], "factor": key[2], "precision": key[3], "pairs": len(entries),
        "sixpoint_over_cubic_geomean": geo(ratios6), "sixpoint_over_cubic_ci95": ci(ratios6, rng),
        "cubic_over_sixpoint_speedup": 1 / geo(ratios6),
        "sinc25_over_cubic_geomean": geo(ratiosinc), "sinc25_over_cubic_ci95": ci(ratiosinc, rng),
        "cubic_over_sinc25_speedup": 1 / geo(ratiosinc),
        "sixpoint_over_sinc25_geomean": geo(ratios6inc), "sixpoint_over_sinc25_ci95": ci(ratios6inc, rng),
        "sinc25_over_sixpoint_speedup": 1 / geo(ratios6inc),
        "cubic_median_ns_per_frame": percentile([e[1]["cubic"] / 8192 for e in entries], .5),
        "sixpoint_median_ns_per_frame": percentile([e[1]["sixpoint"] / 8192 for e in entries], .5),
        "sinc25_median_ns_per_frame": percentile([e[1]["sinc25"] / 8192 for e in entries], .5),
    })

overall = {}
for precision in ("float", "double"):
    entries = [e for key, group in groups.items() if key[3] == precision for e in group]
    r6 = [e[1]["sixpoint"] / e[1]["cubic"] for e in entries]
    rs = [e[1]["sinc25"] / e[1]["cubic"] for e in entries]
    r6s = [e[1]["sixpoint"] / e[1]["sinc25"] for e in entries]
    overall[precision] = {
        "cases": sum(1 for k in groups if k[3] == precision), "paired_triplets": len(entries),
        "sixpoint_over_cubic_geomean": geo(r6), "sixpoint_over_cubic_ci95": ci(r6, rng),
        "cubic_over_sixpoint_speedup": 1 / geo(r6),
        "sinc25_over_cubic_geomean": geo(rs), "sinc25_over_cubic_ci95": ci(rs, rng),
        "cubic_over_sinc25_speedup": 1 / geo(rs),
        "sixpoint_over_sinc25_geomean": geo(r6s), "sixpoint_over_sinc25_ci95": ci(r6s, rng),
        "sinc25_over_sixpoint_speedup": 1 / geo(r6s),
    }

result = {"metadata": {"rows": len(rows), "cases": len(groups), "paired_triplets": sum(map(len, groups.values())),
                        "checksum_equalities_checked": checksum_checks, "frames_per_render": 8192,
                        "bootstrap_resamples": BOOTSTRAPS, "bootstrap_seed": SEED,
                        "scope": "interior sample kernel only; no full renderer"},
          "overall": overall, "groups": summaries}
(ROOT / "threeway-summary.json").write_text(json.dumps(result, indent=2) + "\n")
lines = ["# Paired three-way interior-kernel timing", "", "Ratios below compare six-point or sinc25 against production cubic; below 1 is faster.", ""]
for row in summaries:
    lo, hi = row["sixpoint_over_cubic_ci95"]
    los, his = row["sinc25_over_cubic_ci95"]
    lo6s, hi6s = row["sixpoint_over_sinc25_ci95"]
    lines.append(f"- {row['sample']} {row['segment']} {row['factor']:g}x {row['precision']}: six-point/cubic {row['sixpoint_over_cubic_geomean']:.3f} [{lo:.3f}, {hi:.3f}] ({row['cubic_over_sixpoint_speedup']:.2f}x); sinc25/cubic {row['sinc25_over_cubic_geomean']:.3f} [{los:.3f}, {his:.3f}] ({row['cubic_over_sinc25_speedup']:.2f}x); six-point/sinc25 {row['sixpoint_over_sinc25_geomean']:.3f} [{lo6s:.3f}, {hi6s:.3f}] ({row['sinc25_over_sixpoint_speedup']:.2f}x).")
lines.extend(["", "Precision-wide paired ratios:"])
for precision, row in overall.items():
    lo, hi = row["sixpoint_over_cubic_ci95"]
    los, his = row["sinc25_over_cubic_ci95"]
    lo6s, hi6s = row["sixpoint_over_sinc25_ci95"]
    lines.append(f"- {precision}: six-point/cubic {row['sixpoint_over_cubic_geomean']:.3f} [{lo:.3f}, {hi:.3f}] ({row['cubic_over_sixpoint_speedup']:.2f}x); sinc25/cubic {row['sinc25_over_cubic_geomean']:.3f} [{los:.3f}, {his:.3f}] ({row['cubic_over_sinc25_speedup']:.2f}x); six-point/sinc25 {row['sixpoint_over_sinc25_geomean']:.3f} [{lo6s:.3f}, {hi6s:.3f}] ({row['sinc25_over_sixpoint_speedup']:.2f}x).")
(ROOT / "threeway-summary.txt").write_text("\n".join(lines) + "\n")
print("\n".join(lines))
print(f"validated {len(rows)} triplets and {checksum_checks} timed/warm output checksums")
