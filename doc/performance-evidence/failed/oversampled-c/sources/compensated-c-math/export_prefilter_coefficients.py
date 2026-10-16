#!/usr/bin/env python3
"""Export the selected float32 source-rate equalizers from the math sweep."""

import csv
import struct
from pathlib import Path

import compensated_c_math as screen

ROOT = Path(__file__).resolve().parent
PASS_INDEX = round(screen.PASSBAND / screen.FREQ_STEP)
FILTER_LENGTHS = {4: 33, 6: 65}


def f32(x):
    return struct.unpack("<f", struct.pack("<f", x))[0]


rows = list(csv.DictReader((ROOT / "frequency_response.csv").open(newline="")))
output = []
for points, taps in FILTER_LENGTHS.items():
    selected = sorted((r for r in rows if int(r["points"]) == points),
                      key=lambda r: float(r["frequency_cycles_per_original_sample"]))
    target = [float(r["best_compensation"]) for r in selected]
    p0 = target[PASS_INDEX]
    slope = (target[PASS_INDEX + 1] - target[PASS_INDEX - 1]) / (2.0 * screen.FREQ_STEP)
    for i in range(PASS_INDEX + 1, len(target)):
        target[i] = screen.hermite_tail(points, i * screen.FREQ_STEP, p0, slope)
    half = screen.trapezoid_cosine_coefficients(target, taps)
    half = [f32(x) for x in half]
    full = list(reversed(half[1:])) + [half[0]] + half[1:]
    output.extend({
        "kernel_points": points,
        "filter_taps": taps,
        "tap_index": i,
        "coefficient_float32": format(coefficient, ".17g"),
    } for i, coefficient in enumerate(full))

with (ROOT / "prefilter_coefficients.csv").open("w", newline="") as out:
    writer = csv.DictWriter(out, fieldnames=list(output[0].keys()))
    writer.writeheader()
    writer.writerows(output)

# Check that the exported vectors reproduce the principal summarized values.
summary = __import__("json").loads((ROOT / "math-summary.json").read_text())
for points, taps in FILTER_LENGTHS.items():
    coeffs = [float(r["coefficient_float32"]) for r in output
              if r["kernel_points"] == points]
    dc = sum(coeffs)
    expected = summary["finite_equalizer_summary"][str(points)]
    assert len(coeffs) == taps
    assert abs(dc - expected["dc_gain"]) < 1.0e-12
    print(f"{points}-point: {taps} taps, DC {dc:.12g}, L1 {sum(abs(x) for x in coeffs):.9f}")
