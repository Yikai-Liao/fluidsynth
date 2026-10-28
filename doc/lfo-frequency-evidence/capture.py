"""Capture matching dense and sparse inputs from two FluidSynth builds."""

import argparse
import gzip
import hashlib
import json
from pathlib import Path
import subprocess


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path, required=True, help="Unmodified build's lfo_trace executable")
    parser.add_argument("--after", type=Path, required=True, help="Fixed build's lfo_trace executable")
    parser.add_argument("--soundfont", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    assert sha256(args.soundfont) == "37570538713ec8edf31c2d69ce22f2100b252bdd838d8e9d4efccb970e7f9b78"
    metadata = {"soundfont_sha256": sha256(args.soundfont), "sample_rate": 48000,
                "block_frames": 64, "traces": {}}
    jobs = [(scenario, label, binary, False)
            for scenario in ("dense", "sparse")
            for label, binary in (("before", args.before), ("after", args.after))]
    jobs.append(("sparse", "fixed-depth", args.after, True))
    for scenario, label, binary, fixed_depth in jobs:
        name = f"{scenario}-{label}"
        prefix = args.output_dir / name
        command = [str(binary.resolve()), str(args.soundfont.resolve()), scenario, str(prefix)]
        if fixed_depth:
            command.append("--fixed-depth")
        with prefix.with_suffix(".log").open("w") as log:
            subprocess.run(command, check=True, stdout=log, stderr=log)
        csv = prefix.with_suffix(".csv")
        packed = prefix.with_suffix(".csv.gz")
        packed.write_bytes(gzip.compress(csv.read_bytes(), mtime=0))
        metadata["traces"][name] = {"csv_sha256": sha256(csv),
            "csv_gz_sha256": sha256(packed), "binary_sha256": sha256(binary),
            "fixed_depth": fixed_depth}
    (args.output_dir / "capture.json").write_text(json.dumps(metadata, indent=2) + "\n")


if __name__ == "__main__":
    main()
