#!/usr/bin/env python3
"""Hash the compensated C screen and its frozen external inputs."""

import hashlib
import json
import os
import platform
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
DATA_ROOT = Path(os.environ["FLUIDSYNTH_C_SCREEN_DATA_ROOT"]).resolve()


def record(path: Path, label: str) -> dict:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return {"path": label, "size_bytes": path.stat().st_size, "sha256": h.hexdigest()}


artifact_names = [
    "INPUT_ASSESSMENT.md",
    "REPORT.md",
    "PERF_REPORT.md",
    "THREEWAY_FINAL_REPORT.md",
    "DIAGNOSIS.md",
    "diagnose_alignment.py",
    "alignment-diagnosis.json",
    "compensated_c_math.py",
    "export_prefilter_coefficients.py",
    "run_math.sh",
    "run_all.sh",
    "compensated_c_sf2_screen.cpp",
    "run_sf2_screen.sh",
    "compensated_c_perf_screen.cpp",
    "run_perf_screen.sh",
    "image_threeway.cpp",
    "run_image_threeway.sh",
    "full_renderer_probe.cpp",
    "run_full_renderer_probe.sh",
    "summarize_threeway.py",
    "summarize_quality_threeway.py",
    "summarize_threeway_evidence.py",
    "join_static_threeway.py",
    "summarize_perf_screen.py",
    "summarize_sf2.py",
    "make_manifest.py",
    "math-summary.json",
    "sf2-summary.json",
    "frequency_response.csv",
    "folded_image_screen.csv",
    "prefilter_coefficients.csv",
    "quality.csv",
    "baseline-reproduction.csv",
    "loop-entry.csv",
    "static.csv",
    "storage.csv",
    "perf-raw.csv",
    "setup.csv",
    "perf-summary.json",
    "perf-summary.txt",
    "threeway-raw.csv",
    "threeway-summary.json",
    "threeway-summary.txt",
    "quality-threeway-summary.json",
    "static-threeway-kernel.csv",
    "static-threeway.csv",
    "image-threeway-float.csv",
    "image-threeway-double.csv",
    "threeway-evidence-summary.json",
    "renderer-cubic-strings-fx.csv",
    "renderer-cubic-strings-fx.json",
    "renderer-cubic-strings-fx-aa2.csv",
    "renderer-cubic-strings-fx-aa2.json",
    "renderer-sixpoint-strings-fx.csv",
    "renderer-sixpoint-strings-fx.json",
    "renderer-sixpoint-strings-fx.csv.prepare.csv",
    "renderer-sixpoint-strings-fx-aa2.csv",
    "renderer-sixpoint-strings-fx-aa2.json",
    "renderer-sixpoint-strings-fx-aa2.csv.prepare.csv",
    "renderer-sinc25-strings-fx.csv",
    "renderer-sinc25-strings-fx.json",
]
artifacts = [record(ROOT / name, name) for name in artifact_names]

samples_csv = DATA_ROOT / "fractional-delay-ab" / "samples.csv"
pcm_dir = DATA_ROOT / "cubic-sinc25" / "sf2-samples"
raw_dir = DATA_ROOT / "cubic-sinc25" / "sf2-raw"
inputs = [record(samples_csv, "fractional-delay-ab/samples.csv")]
inputs.extend(record(p, f"cubic-sinc25/sf2-samples/{p.name}") for p in sorted(pcm_dir.glob("*.s16le")))
inputs.extend(record(p, f"cubic-sinc25/sf2-raw/{p.name}") for p in sorted(raw_dir.glob("*.csv")))

compiler = subprocess.run(["g++", "--version"], check=True, capture_output=True, text=True).stdout.splitlines()[0]
manifest = {
    "screen": "compensated C mathematical, real-SF2 quality, three-way kernel, spectral, and renderer screen",
    "baseline_source_commit": "6be67a906b9d679ea930c8841fd0cac0d8a0a25e",
    "commands": {
        "math": "bash run_math.sh",
        "all": "FLUIDSYNTH_C_SCREEN_DATA_ROOT=<data-root> bash run_all.sh",
        "real_sample": "FLUIDSYNTH_C_SCREEN_DATA_ROOT=<data-root> bash run_sf2_screen.sh && python3 summarize_sf2.py",
        "performance": "FLUIDSYNTH_PERF_CPU=2 FLUIDSYNTH_C_SCREEN_DATA_ROOT=<data-root> bash run_perf_screen.sh && python3 summarize_perf_screen.py",
        "three_way_kernel": "FLUIDSYNTH_PERF_CPU=2 FLUIDSYNTH_C_SCREEN_DATA_ROOT=<data-root> bash run_perf_screen.sh && python3 summarize_threeway.py",
        "image_screen": "bash run_image_threeway.sh",
        "full_renderer": "FLUIDSYNTH_PERF_CPU=2 bash run_full_renderer_probe.sh <separate-output-dir>",
        "final_summaries": "python3 summarize_quality_threeway.py && python3 summarize_threeway_evidence.py",
        "compile_flags": "-O3 -DNDEBUG -std=c++17 -ffp-contract=off -Wall -Wextra -Wpedantic",
    },
    "toolchain": {"compiler": compiler, "python": sys.version.split()[0], "platform": platform.platform()},
    "artifacts": artifacts,
    "inputs": inputs,
    "input_root_environment_variable": "FLUIDSYNTH_C_SCREEN_DATA_ROOT",
}
(ROOT / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
print(f"wrote manifest for {len(artifacts)} artifacts and {len(inputs)} input files")
