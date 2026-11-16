"""Visualize recorded limiter phase cancellation against analytic 500 Hz inputs.

One deterministic recording, not a replicate aggregate. All waveform samples in
the declared crop are plotted. Only the coefficient estimate excludes samples
where its denominator is ill-conditioned; no audio samples are removed.
"""
import argparse
import csv
import hashlib
import json
import sys
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--skill-scripts", type=Path, required=True)
parser.add_argument("--input", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
sys.dont_write_bytecode = True
sys.path.insert(0, str(args.skill_scripts))
from audit_panel_alignment import require_matplotlib_panel_alignment
import numpy as np
import matplotlib as mpl
mpl.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import FixedLocator
from matplotlib.transforms import ScaledTranslation

HERE = args.output.resolve()
HERE.mkdir(parents=True, exist_ok=True)
(HERE / "qa").mkdir(exist_ok=True)
SOURCE = args.input.resolve()
report = json.loads((SOURCE / "analysis.json").read_text())
run = report["run"]
rate = run["rate"]
assert run["source"] == "sine" and run["mode"] == "delay"
observed = np.fromfile(SOURCE / "audio-rendered.f32", dtype="<f4").reshape(-1, 2)
with (SOURCE / "audio-events.csv").open() as file:
    events = list(csv.DictReader(file))

# Focus the already reported worst 1 ms window; do not search a new recording.
worst_start = report["limiter_oracle"]["min_1ms_crossfade_time_s"]
center = worst_start + 0.0005
state = {"attack": 5.0, "active": 1}
for event in events:
    if int(event["before_frame"]) / rate > center:
        break
    old = state.copy()
    state[event["setting"].removeprefix("synth.limiter.")] = float(event["value"])
    selected = event
old_delay = int(np.ceil(old["attack"] * 0.001 * rate)) if old["active"] else 0
new_delay = int(np.ceil(state["attack"] * 0.001 * rate)) if state["active"] else 0
assert old_delay == 12000 and new_delay == 48

frames = np.arange(int(np.ceil((center - 0.030) * rate)), int(np.floor((center + 0.030) * rate)) + 1)
time_s = frames / rate
x_ms = (time_s - center) * 1000
assert np.all(np.diff(time_s) > 0)
amplitudes = run["gain"] * np.array([0.2, 0.15])
old_input = np.sin(2 * np.pi * 500 * ((frames - old_delay) / rate))[:, None] * amplitudes
new_input = np.sin(2 * np.pi * 500 * ((frames - new_delay) / rate))[:, None] * amplitudes
actual = observed[frames].astype(float)
assert np.max(np.abs(old_input + new_input)) < 1e-10, "This crop must show opposite phases"

# Infer a shared stereo mixing coefficient as a measurement, not an oracle.
delta = new_input - old_input
norm = np.sum(delta * delta, axis=1)
valid = norm > 1e-8
weight = np.full(len(frames), np.nan)
weight[valid] = np.sum((actual[valid] - old_input[valid]) * delta[valid], axis=1) / norm[valid]
interior = valid & (weight > 0.05) & (weight < 0.95)
fade_samples = int(np.ceil(0.020 * rate))
ramp_start = float(np.median(frames[interior] + 1 - fade_samples * weight[interior]))
model_weight = np.clip((frames - ramp_start + 1) / fade_samples, 0, 1)

# Global, nonoverlapping 1 ms bins; use an independent, unmodified tone as the
# denominator. Old/new branch energies are identical here because phases oppose.
energy = []
for bucket in range(int(np.ceil(time_s[0] * 1000)), int(np.floor(time_s[-1] * 1000))):
    start = round(bucket * 0.001 * rate)
    stop = round((bucket + 1) * 0.001 * rate)
    n = np.arange(start, stop)
    baseline = np.sin(2 * np.pi * 500 * ((n - old_delay) / rate))[:, None] * amplitudes
    measured_energy = float(np.sum(observed[start:stop].astype(float) ** 2))
    baseline_energy = float(np.sum(baseline ** 2))
    assert measured_energy > 0 and baseline_energy > 0
    energy.append(((bucket + 0.5) * 0.001, 10 * np.log10(measured_energy / baseline_energy)))
energy = np.asarray(energy)
energy_x = (energy[:, 0] - center) * 1000
worst = int(np.argmin(energy[:, 1]))
assert abs(energy[worst, 1] - report["limiter_oracle"]["min_1ms_crossfade_energy_ratio_db"]) < 1e-5

with (HERE / "waveform-source.csv").open("w", newline="") as file:
    writer = csv.writer(file)
    writer.writerow(["frame", "time_s", "relative_ms", "old_left", "old_right", "new_left", "new_right",
                     "recorded_left", "recorded_right", "inferred_new_weight", "linear_20ms_weight"])
    for i, frame in enumerate(frames):
        writer.writerow([int(frame), time_s[i], x_ms[i], *old_input[i], *new_input[i], *actual[i],
                         float(weight[i]) if valid[i] else "", model_weight[i]])
np.savetxt(HERE / "energy-source.csv", np.column_stack((energy[:, 0], energy_x, energy[:, 1])),
           delimiter=",", header="time_s,relative_ms,energy_ratio_db", comments="")

mpl.rcParams.update({
    "font.family": "sans-serif", "font.sans-serif": ["DejaVu Sans"],
    "font.size": 8, "axes.labelsize": 8, "axes.titlesize": 8.5,
    "xtick.labelsize": 7.5, "ytick.labelsize": 7.5,
    "legend.fontsize": 7.2, "legend.frameon": False,
    "pdf.fonttype": 42, "svg.fonttype": "none", "text.usetex": False,
    "axes.spines.top": False, "axes.spines.right": False,
    "axes.linewidth": 0.65, "xtick.major.width": 0.65, "ytick.major.width": 0.65,
    "xtick.major.size": 3, "ytick.major.size": 3,
    "savefig.facecolor": "white",
})
blue, gray, output = "#4775A3", "#83909A", "#B74A36"
fig = plt.figure(figsize=(183 / 25.4, 190 / 25.4))
grid = fig.add_gridspec(3, 2, height_ratios=[1.2, 1, 1])
a = fig.add_subplot(grid[0, :])
b = fig.add_subplot(grid[1, 0])
c = fig.add_subplot(grid[1, 1])
d = fig.add_subplot(grid[2, :])
axes = [a, b, c, d]
fig.subplots_adjust(left=0.115, right=0.975, top=0.86, bottom=0.085, hspace=0.72, wspace=0.36)
fig.text(0.115, 0.968, "500 Hz tone: attack 250 → 1 ms", fontsize=10, fontweight="bold")
fig.text(0.115, 0.942, "Recorded output; 48 kHz. Shading marks the lowest-energy 1 ms window.", fontsize=8)
for label, ax in zip("abcd", axes):
    ax.set_gid(label)
    label_offset = 27 if label in "bc" else 9
    ax.text(0, 1, label, transform=ax.transAxes + ScaledTranslation(-22 / 72, label_offset / 72, fig.dpi_scale_trans),
            fontsize=10, fontweight="bold", va="bottom", ha="left")
    ax.axvspan(-0.5, 0.5, color="#EFE8DF", zorder=0)
    ax.set_xlim(-30, 30)
    ax.xaxis.set_major_locator(FixedLocator([-30, -20, -10, 0, 10, 20, 30]))
    ax.tick_params(direction="out")

a.set_title("The recorded waveform develops a deep amplitude dip", loc="left", pad=10)
a.plot(x_ms, actual[:, 0], color=output, linewidth=1.0, label="Recorded output")
a.axhline(amplitudes[0], color=gray, linewidth=0.75, linestyle=(0, (4, 3)), label="Unmodified tone peak")
a.axhline(-amplitudes[0], color=gray, linewidth=0.75, linestyle=(0, (4, 3)))
a.set_ylim(-0.071, 0.071)
a.set_yticks([-0.06, 0, 0.06])
a.set_ylabel("Amplitude (FS)")
a.set_xlabel("Time relative to the selected window centre (ms)")
a.legend(loc="upper right", ncol=2, bbox_to_anchor=(1, 1.22), borderaxespad=0, handlelength=2.2)

b.set_title("The two delayed inputs oppose", loc="left", pad=24)
detail = np.abs(x_ms) <= 3
b.plot(x_ms[detail], old_input[detail, 0], color=blue, linewidth=1.05, label="Old: 250 ms")
b.plot(x_ms[detail], new_input[detail, 0], color=gray, linewidth=1.05, linestyle=(0, (4, 2)), label="New: 1 ms")
b.plot(x_ms[detail], actual[detail, 0], color=output, linewidth=1.15, label="Output")
b.set_xlim(-3, 3)
b.set_xticks([-3, -2, -1, 0, 1, 2, 3])
b.set_ylim(-0.071, 0.071)
b.set_yticks([-0.06, 0, 0.06])
b.set_ylabel("Amplitude (FS)")
b.set_xlabel("Relative time (ms)")
b.legend(loc="lower left", bbox_to_anchor=(0, 1.015), borderaxespad=0, ncol=3,
         columnspacing=0.7, handlelength=1.5, handletextpad=0.4)

c.set_title("Mixing weights pass through ½", loc="left", pad=24)
c.plot(x_ms, 1 - model_weight, color=blue, linewidth=1.15, label="Old weight")
c.plot(x_ms, model_weight, color=gray, linewidth=1.15, linestyle=(0, (4, 2)), label="New weight")
point_indices = np.flatnonzero(valid)[::40]
c.plot(x_ms[point_indices], weight[point_indices], marker=".", markersize=3, linestyle="none",
       color=output, label="Inferred new")
c.set_ylim(-0.08, 1.08)
c.set_yticks([0, 0.5, 1])
c.set_xticks([-30, -15, 0, 15, 30])
c.set_ylabel("Weight")
c.set_xlabel("Relative time (ms)")
c.legend(loc="lower left", bbox_to_anchor=(0, 1.015), borderaxespad=0, ncol=3,
         columnspacing=0.65, handlelength=1.5, handletextpad=0.4)

d.set_title("A 1 ms energy measurement quantifies the cancellation", loc="left", pad=10)
d.axhline(0, color=gray, linewidth=0.8, linestyle=(0, (4, 3)))
d.plot(energy_x, energy[:, 1], color=output, linewidth=1.4, marker=".", markersize=3)
d.plot(energy_x[worst], energy[worst, 1], marker="o", markersize=4, color=output)
d.annotate(f"{energy[worst, 1]:.2f} dB", xy=(energy_x[worst], energy[worst, 1]),
           xytext=(8, -25), textcoords="data", fontsize=9, color=output,
           arrowprops={"arrowstyle": "-", "color": output, "linewidth": 0.7})
d.text(29, 1.8, "Unmodified tone", ha="right", va="bottom", fontsize=7.5, color=gray)
d.set_ylim(-37, 6)
d.set_yticks([-30, -20, -10, 0])
d.set_ylabel("Energy ratio (dB)")
d.set_xlabel("Time relative to the selected window centre (ms)")

fig.canvas.draw()
require_matplotlib_panel_alignment(fig, panel_ids=list("abcd"),
    json_out=HERE / "phase-cancellation.alignment.json",
    overlay_svg=HERE / "qa" / "alignment.svg", require_panel_labels=True, strict=True)
fig.savefig(HERE / "phase-cancellation.pdf")
fig.savefig(HERE / "phase-cancellation.svg")
fig.savefig(HERE / "phase-cancellation.png", dpi=300)
fig.savefig(HERE / "phase-cancellation.tiff", dpi=600, pil_kwargs={"compression": "tiff_lzw"})
plt.close(fig)

metadata = {
    "sample_rate": rate, "frequency_hz": 500, "old_delay_samples": old_delay, "new_delay_samples": new_delay,
    "selected_window_start_s": worst_start, "selected_window_center_s": center,
    "crop_start_frame": int(frames[0]), "crop_last_frame_inclusive": int(frames[-1]),
    "crop_sample_count": len(frames), "coefficient_samples_excluded": int(np.sum(~valid)),
    "coefficient_exclusion_rule": "Squared distance between branch vectors <= 1e-8; audio remains plotted",
    "inferred_ramp_start_frame": ramp_start, "energy_ratio_db": float(energy[worst, 1]),
    "independent_reference": "Unmodified analytic 500 Hz tone, identical channel amplitudes; no limiter output",
    "coefficient_provenance": "Least-squares projection of recorded output onto analytic branch difference",
    "line_model": "20 ms linear ramp; only its time origin is fitted from inferred interior weights",
    "waveform_transform": "No filtering, resampling, smoothing or normalization; left channel displayed",
    "energy_transform": "Both-channel sum of squares in global nonoverlapping 1 ms bins; 10 log10(output / analytic baseline)",
    "recordings": 1, "uncertainty": "None; one deterministic diagnostic recording, not a population estimate",
    "source_sha256": {name: hashlib.sha256((SOURCE / name).read_bytes()).hexdigest()
                      for name in ["audio-rendered.f32", "analysis.json", "audio-events.csv"]},
    "packages": {"numpy": np.__version__, "matplotlib": mpl.__version__},
}
(HERE / "figure-metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
print(json.dumps(metadata, indent=2))
