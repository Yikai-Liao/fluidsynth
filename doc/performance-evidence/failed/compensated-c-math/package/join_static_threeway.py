#!/usr/bin/env python3
"""Join the matched synthetic frequency screens for all three interpolation paths."""
import csv
from pathlib import Path

ROOT = Path(__file__).resolve().parent
rows = []
with (ROOT / "static.csv").open(newline="") as f:
    for r in csv.DictReader(f):
        if r["candidate"] == "6pt-compensated-65":
            rows.append({
                "frequency_cycles_per_original_sample": r["frequency_cycles_per_original_sample"],
                "candidate": "compensated-6pt-65tap", "precision": r["precision"],
                "reference_rms": r["reference_rms"], "gain_error_db": r["gain_error_db"],
                "relative_error_db": r["relative_error_db"],
            })
with (ROOT / "static-threeway-kernel.csv").open(newline="") as f:
    rows.extend(csv.DictReader(f))
rows.sort(key=lambda r: (float(r["frequency_cycles_per_original_sample"]), r["precision"], r["candidate"]))
with (ROOT / "static-threeway.csv").open("w", newline="") as f:
    writer = csv.DictWriter(f, fieldnames=["frequency_cycles_per_original_sample", "candidate", "precision",
                                           "reference_rms", "gain_error_db", "relative_error_db"])
    writer.writeheader()
    writer.writerows(rows)
print(f"wrote {len(rows)} matched static rows")
