#!/usr/bin/env python3
import csv
import hashlib
import json
import platform
import statistics
import subprocess
import sys
from pathlib import Path


ROOT = Path(sys.argv[1]).resolve()
BASE = ROOT.parent
SAMPLES = BASE / "fractional-delay-ab" / "samples.csv"
PCM_DIR = BASE / "cubic-sinc25" / "sf2-samples"


def read_csv(path):
    with path.open(newline="") as stream:
        return list(csv.DictReader(stream))


def value(row, key):
    return float(row[key])


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def summarize_windows(path):
    rows = read_csv(path)
    active = [row for row in rows if row["quiet_reference"] == "0"]
    quiet = [row for row in rows if row["quiet_reference"] == "1"]
    deltas = [value(row, "error_change_vs_sinc25_db") for row in active]
    margins = [value(row, "reference_margin_db") for row in active]
    return {
        "rows": len(rows),
        "active_rows": len(active),
        "quiet_rows": len(quiet),
        "active_median_error_change_vs_sinc25_db": statistics.median(deltas) if deltas else None,
        "active_worst_error_change_vs_sinc25_db": max(deltas) if deltas else None,
        "active_improved_rows": sum(delta < 0.0 for delta in deltas),
        "active_reference_worst_margin_db": max(margins) if margins else None,
        "quiet_worst_absolute_error_rms_dbfs": max((value(row, "candidate_error_dbfs") for row in quiet), default=None),
        "quiet_worst_absolute_error_peak_dbfs": max((value(row, "candidate_max_error_dbfs") for row in quiet), default=None),
        "finite_failures": sum(row["finite"] != "1" for row in rows),
        "reference_tap_counts": sorted({int(row["reference_taps"]) for row in rows}),
    }


windows = summarize_windows(ROOT / "windows.csv")
reduced_windows = summarize_windows(ROOT / "decimated-windows.csv")
static = read_csv(ROOT / "static.csv")
static_summary = {}
for config in ("full-rate", "decimated-2x"):
    rows = [row for row in static if row["configuration"] == config]
    passband = [row for row in rows if row["band"] == "passband"]
    stopband = [row for row in rows if row["band"] == "stopband"]
    worst_pass = max(passband, key=lambda row: abs(value(row, "candidate_gain_error_db")))
    worst_stop = max(stopband, key=lambda row: value(row, "alias_input_db"))
    static_summary[config] = {
        "rows": len(rows),
        "passband_max_abs_gain_error_db_through_0_24": abs(value(worst_pass, "candidate_gain_error_db")),
        "passband_worst_case": {
            "frequency": value(worst_pass, "frequency_cycles_per_source_sample"),
            "phase": value(worst_pass, "source_phase"),
            "gain_error_db": value(worst_pass, "candidate_gain_error_db"),
        },
        "stopband_worst_alias_input_db_from_0_26": value(worst_stop, "alias_input_db"),
    }

storage = read_csv(ROOT / "storage.csv")
storage_summary = {
    "samples": len(storage),
    "total_preparation_ms_single_run": sum(value(row, "prepare_ms") for row in storage),
    "rows": storage,
}

summary = {
    "screen": "fixed-step-2 Kaiser prefilter, first without decimation then with one decimated layer",
    "baseline_source": "measurement composite 6be67a906b9d679ea930c8841fd0cac0d8a0a25e",
    "design": {
        "filter_taps": 4095,
        "kaiser_beta": 9.5,
        "cutoff_cycles_per_source_sample": 0.25,
        "declared_passband_cycles_per_source_sample": [0.0, 0.24],
        "declared_transition_cycles_per_source_sample": [0.24, 0.26],
        "declared_stopband_cycles_per_source_sample": [0.26, 0.5],
        "lowpass_input_gain_normalized": True,
    },
    "gates": {
        "passband_max_abs_gain_error_db": 0.1,
        "stopband_alias_input_db_max": -80.0,
        "reference_margin_db_max_for_active_rows": -20.0,
        "quiet_absolute_error_rms_dbfs_max": -80.0,
    },
    "full_rate_prefilter": windows,
    "decimated_layer": reduced_windows,
    "static_response": static_summary,
    "storage_and_preparation": storage_summary,
}
(ROOT / "summary.json").write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n")

inputs = [SAMPLES, *sorted(PCM_DIR.glob("*.s16le"))]
outputs = [ROOT / name for name in (
    "windows.csv", "decimated-windows.csv", "static.csv", "storage.csv", "REPORT.md", "summary.json"
)]
compiler = subprocess.run(["g++", "--version"], check=True, capture_output=True, text=True).stdout.splitlines()[0]
manifest = {
    "command": "./run.sh",
    "compiler": compiler,
    "platform": platform.platform(),
    "baseline_source": summary["baseline_source"],
    "sha256": {str(path): sha256(path) for path in [ROOT / "mipmap_d_screen.cpp", ROOT / "run.sh",
        ROOT / "summarize.py", ROOT / "mipmap_d_screen", *inputs, *outputs]},
}
(ROOT / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")

print(f"windows: {windows['rows']} full-rate, {reduced_windows['rows']} decimated/fallback")
for config, result in static_summary.items():
    print(f"{config}: passband max={result['passband_max_abs_gain_error_db_through_0_24']:.6f} dB, "
          f"stopband worst={result['stopband_worst_alias_input_db_from_0_26']:.2f} dB")
print(f"preparation: {storage_summary['total_preparation_ms_single_run']:.1f} ms; "
      f"manifest hashes {len(manifest['sha256'])} files")
