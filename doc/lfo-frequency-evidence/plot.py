"""Plot actual FluidSynth states and CC events; do not synthesize LFO data."""

import argparse
import json
from pathlib import Path
import sys

import matplotlib as mpl
mpl.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator
import numpy as np

BEFORE = "#C75F2D"
AFTER = "#287D8E"
EVENT = "#555B60"


def read_pair(data_dir, scenario):
    before = np.genfromtxt(data_dir / f"{scenario}-before.csv.gz", names=True, delimiter=",")
    after = np.genfromtxt(data_dir / f"{scenario}-after.csv.gz", names=True, delimiter=",")
    assert np.all(np.diff(before["time"]) > 0)
    for field in ("time", "cc", "event", "depth", "ticks", "delay"):
        assert np.array_equal(before[field], after[field]), field
    assert np.array_equal(np.abs(before["increment"]), np.abs(after["increment"]))
    return before, after


def draw_pair(before, after, title, overview, detail, output, qa_tools):
    mpl.rcParams.update({
        "font.family": "sans-serif", "font.sans-serif": ["DejaVu Sans"],
        "font.size": 11, "axes.titlesize": 11, "axes.linewidth": 0.7,
        "axes.spines.top": False, "axes.spines.right": False,
        "pdf.fonttype": 42, "svg.fonttype": "none",
        "figure.facecolor": "white", "axes.facecolor": "white",
    })
    fig, axes = plt.subplots(3, 2, figsize=(11.8, 6.2), sharex="col", sharey="row",
                             gridspec_kw={"width_ratios": [2.7, 1.2],
                                          "height_ratios": [0.4, 1, 1]})
    fig.subplots_adjust(left=0.10, right=0.98, top=0.87, bottom=0.12,
                        hspace=0.32, wspace=0.13)
    fig.suptitle(title, x=0.10, y=0.97, ha="left", fontsize=17)
    axes[0, 1].set_title("Detail", loc="left", pad=8)
    event_rows = before[before["event"] != 0]
    events = event_rows["time"]

    for column, limits in enumerate((overview, detail)):
        control = axes[0, column]
        control.step(before["time"], before["cc"], where="post", color=EVENT, linewidth=1)
        control.plot(events, event_rows["cc"], linestyle="none", marker="|",
                     markersize=6, markeredgewidth=0.7, color=EVENT)
        control.set_ylim(0, 142)
        control.set_yticks([20, 127])
        for row, data, color in ((1, before, BEFORE), (2, after, AFTER)):
            ax = axes[row, column]
            ax.axhline(0, color="#D7DDE1", linewidth=0.7, zorder=0)
            ax.plot(data["time"], data["val"], color=color, linewidth=1.25)
            ax.vlines(events, 1.075, 1.16, color=EVENT, linewidth=0.7)
            visible = events[(events >= limits[0]) & (events <= limits[1])]
            if len(visible) <= 15:
                ax.vlines(visible, -1.1, 1.0, color="#DDE2E5", linewidth=0.55,
                          linestyle=(0, (3, 4)), zorder=0)
            ax.set_ylim(-1.15, 1.20)
            ax.set_yticks([-1, 0, 1])
        for ax in axes[:, column]:
            ax.set_xlim(*limits)
            ax.xaxis.set_major_locator(MaxNLocator(6 if column == 0 else 4))
            ax.tick_params(labelsize=10)
        axes[2, column].set_xlabel("Time (s)")

    for ax in axes[:, 0]:
        ax.axvspan(*detail, color="#EAF0F3", zorder=-1)
    for row, label, color in ((0, "CC1", EVENT), (1, "Before", BEFORE), (2, "After", AFTER)):
        axes[row, 0].set_ylabel(label, color=color, rotation=0, ha="right",
                                va="center", labelpad=24, fontsize=11)

    fig.canvas.draw()
    layout = {"plot_rectangles": [list(ax.get_position().bounds) for ax in axes.flat],
              "overview": overview, "detail": detail, "rows": len(before),
              "events": len(events), "samples_drawn": "all rows, explicit axis windows"}
    output.with_suffix(".layout.json").write_text(json.dumps(layout, indent=2) + "\n")
    if qa_tools is not None:
        sys.path.insert(0, str(qa_tools))
        from audit_panel_alignment import require_matplotlib_panel_alignment
        require_matplotlib_panel_alignment(
            fig, json_out=output.with_suffix(".alignment.json"),
            overlay_svg=output.with_suffix(".alignment.svg"), strict=True)
    fig.savefig(output.with_suffix(".png"), dpi=300)
    fig.savefig(output.with_suffix(".pdf"))
    fig.savefig(output.with_suffix(".svg"))
    plt.close(fig)


def main():
    root = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data-dir", type=Path, default=root / "data")
    parser.add_argument("--output-dir", type=Path, default=root / "output")
    parser.add_argument("--qa-tools", type=Path, help="Optional directory containing figure audit tools")
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    for scenario, title, overview, detail in (
        ("dense", "20 ms updates", (1.5, 4.7), (2.28, 2.50)),
        ("sparse", "0.5–2 s steps", (0.6, 5.3), (1.42, 1.68)),
    ):
        before, after = read_pair(args.data_dir, scenario)
        draw_pair(before, after, title, overview, detail,
                  args.output_dir / scenario, args.qa_tools)


if __name__ == "__main__":
    main()
