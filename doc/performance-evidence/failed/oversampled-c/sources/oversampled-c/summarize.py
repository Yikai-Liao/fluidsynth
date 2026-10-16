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
RAW_DIR = BASE / "cubic-sinc25" / "sf2-raw"


def read_csv(path):
    with path.open(newline="") as f:
        return list(csv.DictReader(f))


def f(row, key):
    return float(row[key])


def median(values):
    return statistics.median(values) if values else None


def sha256(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


quality = read_csv(ROOT / "quality.csv")
static = read_csv(ROOT / "static.csv")
loop_entry = read_csv(ROOT / "loop-entry.csv")
baseline = read_csv(ROOT / "baseline-reproduction.csv")
storage = read_csv(ROOT / "storage.csv")

groups = {}
for candidate in sorted({r["candidate"] + "/" + r["precision"] for r in quality}):
    cname, precision = candidate.split("/")
    rows = [r for r in quality if r["candidate"] == cname and r["precision"] == precision]
    primary = [r for r in rows if f(r, "transpose_factor") <= 1.0]
    active = [r for r in primary if r["quiet_reference"] == "0"]
    quiet = [r for r in primary if r["quiet_reference"] == "1"]
    stat_rows = [r for r in static
                 if r["candidate"] == cname and r["precision"] == precision
                 and f(r, "frequency_cycles_per_original_sample") <= 0.4000000001]
    loops = [r for r in loop_entry if r["candidate"] == cname and r["precision"] == precision]
    margins = [f(r, "reference_margin_db") for r in rows]
    active_delta = [f(r, "error_vs_sinc25_db") for r in active]
    quiet_error = [f(r, "error_rms_dbfs") for r in quiet]
    static_gain = [f(r, "gain_error_db") for r in stat_rows]
    loop_delta = [f(r, "error_vs_sinc25_db") for r in loops]
    groups[candidate] = {
        "all_rows": len(rows),
        "primary_rows_factor_le_1": len(primary),
        "active_primary_rows": len(active),
        "median_active_error_delta_vs_sinc25_db": median(active_delta),
        "worst_active_error_delta_vs_sinc25_db": max(active_delta) if active_delta else None,
        "worst_reference_margin_db_all_rows": max(margins) if margins else None,
        "reference_convergence_failures_gt_minus20_db": sum(v > -20.0 for v in margins),
        "quiet_primary_rows": len(quiet),
        "worst_quiet_error_dbfs": max(quiet_error) if quiet_error else None,
        "quiet_error_failures_above_minus80_dbfs": sum(v > -80.0 for v in quiet_error),
        "nonfinite_rows": sum(r["finite"] != "1" for r in rows),
        "static_passband_max_abs_gain_error_db_le_0_4": max(map(abs, static_gain)) if static_gain else None,
        "static_passband_failures_above_0_1_db": sum(abs(v) > 0.1 for v in static_gain),
        "loop_entry_rows": len(loops),
        "median_loop_entry_error_delta_vs_sinc25_db": median(loop_delta),
        "worst_loop_entry_error_delta_vs_sinc25_db": max(loop_delta) if loop_delta else None,
        "loop_entry_worst_reference_margin_db": max((f(r, "reference_margin_db") for r in loops), default=None),
    }

baseline_summary = {
    "rows": len(baseline),
    "max_sinc25_rms_diff_dbfs": max((f(r, "sinc25_rms_diff") for r in baseline), default=None),
    "max_sinc25_sample_diff_dbfs": max((f(r, "sinc25_max_diff") for r in baseline), default=None),
    "max_long_reference_rms_diff_dbfs": max((f(r, "long_ref_rms_diff") for r in baseline), default=None),
    "max_long_reference_sample_diff_dbfs": max((f(r, "long_ref_max_diff") for r in baseline), default=None),
}

prep_times = [f(r, "prepare_ms") for r in storage]
storage_summary = {
    "samples": len(storage),
    "total_prepare_ms": sum(prep_times),
    "max_screen_storage_ratio_vs_original": max(f(r, "screen_ratio_vs_original") for r in storage),
    "max_minimal_full_plus_loop_ratio_vs_original": max(f(r, "minimal_ratio_vs_original") for r in storage),
    "rows": storage,
}

summary = {
    "screen": "2x precomputed float sample representation with Niemitalo 4pt/6pt optimal polynomial",
    "baseline_source": "measurement composite 6be67a906b9d679ea930c8841fd0cac0d8a0a25e",
    "gates": {
        "median_active_error_delta_vs_sinc25_db_max": 1.0,
        "worst_active_error_delta_vs_sinc25_db_max": 3.0,
        "reference_margin_db_max": -20.0,
        "quiet_error_dbfs_max": -80.0,
        "static_passband_abs_gain_error_db_max_through_0_4": 0.1,
    },
    "candidate_results": groups,
    "baseline_reproduction": baseline_summary,
    "storage_and_preparation": storage_summary,
}
(ROOT / "summary.json").write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n")

source = ROOT / "oversampled_c_screen.cpp"
binary = ROOT / "oversampled_c_screen"
input_paths = [SAMPLES, *sorted(PCM_DIR.glob("*.s16le")), *sorted(RAW_DIR.glob("*.csv"))]
output_paths = [ROOT / name for name in (
    "quality.csv", "static.csv", "loop-entry.csv", "baseline-reproduction.csv", "storage.csv", "REPORT.md"
)]
compiler = subprocess.run(["g++", "--version"], check=True, capture_output=True, text=True).stdout.splitlines()[0]
manifest = {
    "command": "./run.sh",
    "compiler": compiler,
    "platform": platform.platform(),
    "baseline_source": summary["baseline_source"],
    "sha256": {str(p): sha256(p) for p in [source, ROOT / "run.sh", ROOT / "summarize.py",
                                           binary, *input_paths, *output_paths]},
}
(ROOT / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")

print(f"quality rows: {len(quality)}, static rows: {len(static)}, loop-entry rows: {len(loop_entry)}")
for key, result in groups.items():
    print(f"{key}: median delta={result['median_active_error_delta_vs_sinc25_db']} dB, "
          f"worst delta={result['worst_active_error_delta_vs_sinc25_db']} dB, "
          f"passband max={result['static_passband_max_abs_gain_error_db_le_0_4']} dB")
print(f"preparation: {storage_summary['total_prepare_ms']:.1f} ms; manifest hashes {len(manifest['sha256'])} files")
