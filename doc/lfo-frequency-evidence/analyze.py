"""Report measured LFO state and pitch-control changes at actual CC events."""

import argparse
import json
from pathlib import Path

import numpy as np


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data-dir", type=Path, default=Path(__file__).resolve().parent / "data")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = {}
    for name in ("dense-before", "dense-after", "sparse-before", "sparse-after", "sparse-fixed-depth"):
        data = np.genfromtxt(args.data_dir / f"{name}.csv.gz", names=True, delimiter=",")
        events = []
        for index in np.flatnonzero(data["event"]):
            row = data[index]
            # Separate the measured pitch-control change into depth and motion terms.
            depth_term = (row["depth"] - row["depth_before"]) * row["val_before"]
            motion_term = row["depth"] * (row["val"] - row["val_before"])
            events.append({"time": float(row["time"]), "cc": int(row["cc"]),
                "val_before": float(row["val_before"]), "val": float(row["val"]),
                "increment_before": float(row["increment_before"]),
                "increment": float(row["increment"]),
                "depth_before_cents": float(row["depth_before"]), "depth_cents": float(row["depth"]),
                "depth_term_cents": float(depth_term), "motion_term_cents": float(motion_term),
                "pitch_control_change_cents": float(depth_term + motion_term),
                "render_pitch_ratio_change_cents": float(1200 * np.log2(row["phase_incr"] / data[index - 1]["phase_incr"]))})
        if name.startswith("dense"):
            active = data[(data["time"] >= 2.25) & (data["time"] < 4.12)]
        else:
            active = data[(data["time"] >= 1) & (data["time"] < 4.5)]
        result[name] = {"rows": len(data), "events": events,
            "minimum": float(active["val"].min()), "maximum": float(active["val"].max()),
            "negative_fraction": float(np.mean(active["val"] < 0))}
    args.output.write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    main()
