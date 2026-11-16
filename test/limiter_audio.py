#!/usr/bin/env python3
"""Check limiter updates against analytic expectations and record a real backend.

Uses Python's standard library. Playback uses the C++ probe, never a Python
audio callback. See limiter-audio.md for scope, artifacts and platform setup.
"""
import argparse
import array
import contextlib
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import shlex
import shutil
import signal
import subprocess
import sys
import time
import wave

sys.dont_write_bytecode = True
from limiter_oracle import check_limiter

ROOT = Path(__file__).resolve().parents[1]


def command(argv, **kwargs):
    return subprocess.run(list(map(str, argv)), check=True, **kwargs)


def output(argv):
    return command(argv, capture_output=True, text=True).stdout.strip()


def stop(process):
    if process is not None and process.poll() is None:
        process.send_signal(signal.SIGINT if os.name != "nt" else signal.SIGTERM)
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()


def load_audio(path, rate):
    if path.suffix.lower() == ".wav":
        with wave.open(str(path), "rb") as wav:
            if wav.getframerate() != rate or wav.getnchannels() != 2:
                raise ValueError("Capture must be stereo at the run's sample rate")
            width = wav.getsampwidth()
            raw = wav.readframes(wav.getnframes())
        if width not in (2, 3, 4):
            raise ValueError("Use PCM16, PCM24 or PCM32 WAV")
        return array.array("f", (int.from_bytes(raw[i:i + width], "little", signed=True)
                                 / (2 ** (8 * width - 1))
                                 for i in range(0, len(raw), width)))
    raw = path.read_bytes()
    if len(raw) % 8:
        raise ValueError("Raw audio must contain complete stereo float32 frames")
    values = array.array("f")
    values.frombytes(raw)
    if sys.byteorder != "little":
        values.byteswap()
    return values


def write_wav(path, audio, rate):
    # Listening copies only. Unclipped float32 originals remain the evidence.
    pcm = array.array("h", (round(max(-1, min(32767 / 32768, v)) * 32768)
                           if math.isfinite(v) else 0 for v in audio))
    if sys.byteorder != "little":
        pcm.byteswap()
    with wave.open(str(path), "wb") as wav:
        wav.setparams((2, 2, rate, 0, "NONE", "not compressed"))
        wav.writeframes(pcm.tobytes())


def onset(audio):
    peak = max((abs(v) for v in audio if math.isfinite(v)), default=0)
    threshold = max(1e-6, peak * 0.001)
    return next((i // 2 for i, v in enumerate(audio) if abs(v) > threshold), 0)


def find_lag(rendered, capture, begin, end, center, radius, step):
    positions = list(range(begin, end, max(1, (end - begin) // 512)))
    positions = [i for i in positions if 0 <= i < len(rendered) // 2]
    ref = [rendered[2 * i] for i in positions]
    energy = sum(v * v for v in ref)
    best = (-1, 0, center)
    for lag in range(center - radius, center + radius + 1, step):
        if not positions or positions[0] + lag < 0 or positions[-1] + lag >= len(capture) // 2:
            continue
        cap = [capture[2 * (i + lag)] for i in positions]
        dot = sum(a * b for a, b in zip(ref, cap))
        power = sum(b * b for b in cap)
        score = dot * abs(dot) / max(1e-30, energy * power)
        # Constant plateaus have many equally good offsets. Preserve the onset
        # estimate on ties instead of silently choosing the latest candidate.
        best = max(best, (score, -abs(lag - center), lag))
    return best[2], best[0]


def signal_metrics(audio, rate):
    finite_count = nonfinite = clipped = 0
    energy = peak = 0
    for value in audio:
        if math.isfinite(value):
            finite_count += 1
            energy += value * value
            peak = max(peak, abs(value))
            clipped += abs(value) >= 1
        else:
            nonfinite += 1
    frames = len(audio) // 2
    zero_start = None
    gaps = []
    # The initial lookahead and stopping the stream are outside this detector.
    begin, end = int(0.5 * rate), frames - int(0.5 * rate)
    for i in range(begin, end):
        quiet = max(abs(audio[2 * i]), abs(audio[2 * i + 1])) < 1e-7
        if quiet and zero_start is None:
            zero_start = i
        if zero_start is not None and (not quiet or i == end - 1):
            stop_frame = i if not quiet else i + 1
            if stop_frame - zero_start >= 0.002 * rate:
                gaps.append({"start_s": zero_start / rate, "duration_ms": 1000 * (stop_frame - zero_start) / rate})
            zero_start = None
    return {"frames": frames, "nonfinite_samples": nonfinite,
            "peak": peak, "rms": math.sqrt(energy / max(1, finite_count)),
            "clipped_samples": clipped,
            "max_sample_step": max((abs(audio[i] - audio[i - 2])
                                    for i in range(2 * begin + 2, 2 * end)), default=0),
            "near_zero_runs_at_least_2ms": gaps}


def write_report(folder, report, windows, events):
    (folder / "analysis.json").write_text(json.dumps(report, indent=2, allow_nan=False) + "\n")
    with (folder / "waveform.csv").open("w", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=list(windows[0]))
        writer.writeheader()
        writer.writerows(windows)
    # A compact interactive view; min/max bins preserve short peaks that a
    # decimated line plot would hide. Exact samples are in the float32 files.
    data = json.dumps({"report": report, "windows": windows, "events": events}, allow_nan=False).replace("<", "\\u003c")
    page = """<!doctype html><meta charset="utf-8"><title>Limiter audio evidence</title>
<style>body{font:16px system-ui;max-width:1200px;margin:24px auto;padding:0 16px}canvas{width:100%;border:1px solid #aaa}pre{white-space:pre-wrap}label{margin-right:24px}</style>
<h1>Limiter audio evidence</h1><p id="scope"></p>
<p>Independent expected waveform: <audio controls src="expected.wav"></audio><br><strong id="expected-scope"></strong></p>
<p>Limiter output before the driver: <audio controls src="audio-rendered.wav"></audio></p>
<p>Audio service monitor: <audio controls src="capture.wav"></audio></p>
<label>Start <input id="start" type="range" min="0" step="0.01" value="0"></label>
<label>View <select id="span"><option>1</option><option>0.1</option><option selected>5</option><option>20</option><option>300</option></select> seconds</label>
<canvas width="1200" height="680"></canvas><pre id="hover"></pre>
<details><summary>Measurements and acceptance checks</summary><pre id="metrics"></pre></details>
<p>Blue: limiter output; green: aligned monitor with one fixed gain correction per channel; red: errors. The limiter error uses an independent analytic oracle. The transport error compares the two actual outputs and cannot prove DSP correctness. Each vertical line is a control request, not its DSP activation time. The view shows 10 ms min/max bins.</p>
<script>
const data=DATA, rows=data.windows, canvas=document.querySelector('canvas'), ctx=canvas.getContext('2d'), start=document.querySelector('#start'), span=document.querySelector('#span');
document.querySelector('#scope').textContent=data.report.scope;
document.querySelector('#expected-scope').textContent=data.report.limiter_oracle.expected_audio_scope;
document.querySelector('#metrics').textContent=JSON.stringify(data.report,null,2);
start.max=rows.at(-1).time_s;
function draw(){const a=+start.value,b=a+(+span.value),w=canvas.width;ctx.clearRect(0,0,w,680);
const tracks=[['rendered_min','rendered_max','#1261a0','Limiter output'],['capture_min','capture_max','#25843b','Monitor'],['limiter_error_min','limiter_error_max','#b32424','Limiter error vs independent oracle'],['error_min','error_max','#b32424','Transport error']];
tracks.forEach((track,k)=>{const y=85+k*170;ctx.fillStyle='#222';ctx.fillText(track[3]+' (scale ±1; gray = oracle not evaluated)',8,y-65);ctx.strokeStyle='#ccc';ctx.beginPath();ctx.moveTo(0,y);ctx.lineTo(w,y);ctx.stroke();ctx.strokeStyle=track[2];ctx.beginPath();for(const r of rows){if(r.time_s<a||r.time_s>b)continue;const x=(r.time_s-a)/(b-a)*w;if(k===2&&r.oracle_checked_fraction<1){ctx.fillStyle='#ddd';ctx.fillRect(x,y-60,Math.max(1,w*0.01/(b-a)),120);continue;}ctx.moveTo(x,y-70*r[track[0]]);ctx.lineTo(x,y-70*r[track[1]]);}ctx.stroke();});
ctx.strokeStyle='#888';for(const e of data.events){if(e.time_s<a||e.time_s>b)continue;const x=(e.time_s-a)/(b-a)*w;ctx.beginPath();ctx.moveTo(x,0);ctx.lineTo(x,680);ctx.stroke();}}
canvas.onmousemove=e=>{const t=+start.value+(e.offsetX/canvas.clientWidth)*(+span.value);const r=rows.reduce((a,b)=>Math.abs(a.time_s-t)<Math.abs(b.time_s-t)?a:b);const events=data.events.filter(e=>Math.abs(e.time_s-t)<0.1);document.querySelector('#hover').textContent=JSON.stringify({window:r,nearby_requests:events},null,2);};start.oninput=span.onchange=draw;draw();
</script>"""
    (folder / "report.html").write_text(page.replace("DATA", data))


def analyze(args):
    folder = args.output.resolve()
    manifest = json.loads((folder / "run.json").read_text())
    rate = manifest["rate"]
    rendered = load_audio(folder / "audio-rendered.f32", rate)
    capture = load_audio(args.capture or folder / "capture.f32", rate)
    rendered_metrics = signal_metrics(rendered, rate)
    capture_metrics = signal_metrics(capture, rate)
    if rendered_metrics["nonfinite_samples"] or capture_metrics["nonfinite_samples"]:
        raise ValueError("Nonfinite audio samples; waveform comparison is invalid")
    if rendered_metrics["rms"] < 1e-5 or capture_metrics["rms"] < 1e-5:
        raise ValueError("No usable rendered or monitor signal; check routing/mute")
    write_wav(folder / "audio-rendered.wav", rendered, rate)
    write_wav(folder / "capture.wav", capture, rate)
    first = onset(rendered)
    lag, _ = find_lag(rendered, capture, 0, first + rate // 2,
                      onset(capture) - first, rate // 10, 16)
    lag, correlation = find_lag(rendered, capture, 0, first + rate // 2, lag, 16, 1)
    begin = max(rate // 2, -lag)
    end = min(len(rendered) // 2 - rate // 2, len(capture) // 2 - lag)
    if end - begin < rate:
        raise ValueError("Less than one second of aligned audio")
    gains = []
    for channel in range(2):
        indices = range(begin, min(begin + rate, end), 4)
        denominator = sum(rendered[2 * i + channel] ** 2 for i in indices)
        gain = sum(rendered[2 * i + channel] * capture[2 * (i + lag) + channel]
                   for i in indices) / max(1e-30, denominator)
        if gain <= 0.01 or gain > 10:
            raise ValueError("Monitor gain/correlation is invalid; check channel mapping and routing")
        gains.append(gain)
    windows, local_lags = [], []
    total_error = total_signal = peak_error = 0
    dropout_windows = 0
    window = max(1, rate // 100)
    for frame in range(begin, end, window):
        last = min(frame + window, end)
        ref = rendered[2 * frame:2 * last]
        cap = [capture[2 * (i + lag) + channel] / gains[channel]
               for i in range(frame, last) for channel in range(2)]
        error = [b - a for a, b in zip(ref, cap)]
        energy = sum(v * v for v in ref)
        error_energy = sum(v * v for v in error)
        rms = math.sqrt(energy / len(ref))
        cap_rms = math.sqrt(sum(v * v for v in cap) / len(cap))
        total_signal += energy
        total_error += error_energy
        peak_error = max(peak_error, max(map(abs, error)))
        dropout_windows += rms > 1e-4 and cap_rms < rms * 0.05
        windows.append({"time_s": frame / rate, "rendered_rms": rms,
                        "capture_rms": cap_rms, "error_rms": math.sqrt(error_energy / len(error)),
                        "rendered_min": min(ref), "rendered_max": max(ref),
                        "capture_min": min(cap), "capture_max": max(cap),
                        "error_min": min(error), "error_max": max(error)})
    # Recheck alignment throughout the run. A changing offset can indicate
    # missing/repeated frames or clock/resampler drift; it is not auto-corrected.
    for frame in range(begin, end - rate // 4, rate):
        local, score = find_lag(rendered, capture, frame, frame + rate // 4,
                                lag, max(1024, manifest["period"] * 2), 8)
        local, score = find_lag(rendered, capture, frame, frame + rate // 4, local, 8, 1)
        local_lags.append({"time_s": frame / rate, "offset_frames": local, "correlation": score})
    snr = 10 * math.log10(total_signal / total_error) if total_error else None
    with (folder / "audio-callbacks.csv").open() as file:
        callbacks = [{key: int(value) for key, value in row.items()} for row in csv.DictReader(file)]
    measured = [row for row in callbacks if begin <= row["frame"] < end]
    with (folder / "audio-events.csv").open() as file:
        events = [{**row, "time_s": int(row["before_frame"]) / rate} for row in csv.DictReader(file)]
    oracle, expected, limiter_errors, checked = check_limiter(manifest, rendered, events, callbacks)
    write_wav(folder / "expected.wav", expected, rate)
    if sys.byteorder != "little":
        expected.byteswap()
    (folder / "expected.f32").write_bytes(expected.tobytes())
    for row in windows:
        frame = round(row["time_s"] * rate)
        values = limiter_errors[2 * frame:2 * min(frame + window, end)]
        row.update(limiter_error_min=min(values), limiter_error_max=max(values),
                   oracle_checked_fraction=sum(checked[frame:min(frame + window, end)]) / (len(values) / 2))
    render_over = sum(row["render_ns"] > row["frames"] * 1e9 / rate for row in measured)
    capture_over = sum(row["capture_ns"] > row["frames"] * 1e9 / rate for row in measured)
    checks = {**{"limiter_" + key: value for key, value in oracle["checks"].items()},
              "monitor_matches_rendered_output": (snr is None or snr >= args.min_snr) and peak_error <= args.max_error,
              "no_detected_rendered_silence_gap": not rendered_metrics["near_zero_runs_at_least_2ms"],
              "no_detected_monitor_dropout": dropout_windows == 0,
              "render_calls_within_frame_budget": render_over == 0,
              "probe_capture_within_frame_budget": capture_over == 0,
              "render_calls_succeeded": all(row["status"] == 0 for row in callbacks),
              "comparison_covers_playback": end >= len(rendered) // 2 - rate // 2}
    report = {"scope": "Known input through the production final mixer: limiter behavior checked against an independent analytic oracle; monitor transport checked against captured output. First/last 0.5 s excluded. See oracle coverage: rapid updates check convergence; DC checks steady gain. No hardware or speaker coverage.",
              "analysis_source_sha256": {name: hashlib.sha256((ROOT / "test" / name).read_bytes()).hexdigest()
                                         for name in ("limiter_audio.py", "limiter_oracle.py")},
              "run": manifest, "checks": checks, "all_checks_pass": all(checks.values()),
              "limiter_oracle": oracle, "rendered": rendered_metrics, "capture": capture_metrics,
              "comparison": {"offset_frames": lag, "initial_correlation": correlation,
                             "fixed_monitor_gains": gains, "compared_seconds": (end - begin) / rate,
                             "snr_db": snr, "exact_match_after_alignment_and_gain": total_error == 0,
                             "max_absolute_error": peak_error,
                             "dropout_windows_10ms": dropout_windows,
                             "min_snr_db": args.min_snr, "max_error_allowed": args.max_error,
                             "local_offsets": local_lags},
              "timing": {"callbacks": len(callbacks), "measured_callbacks": len(measured),
                         "actual_callback_frames": sorted(set(row["frames"] for row in callbacks)),
                         "render_over_budget": render_over, "capture_over_budget": capture_over,
                         "max_render_us": max(row["render_ns"] for row in measured) / 1000,
                         "max_render_and_copy_us": max(row["capture_ns"] for row in measured) / 1000,
                         "max_callback_interval_us": max(row["interval_ns"] for row in measured) / 1000}}
    write_report(folder, report, windows, events)
    print(json.dumps({"checks": checks, "snr_db": snr, "max_absolute_error": peak_error,
                      "render_over_budget": render_over, "report": str(folder / "report.html")}, indent=2))
    return 0 if report["all_checks_pass"] else 1


def record(args):
    folder = args.output.resolve()
    folder.mkdir(parents=True, exist_ok=False)
    build = args.build.resolve()
    probe = args.probe or build / "test" / ("limiter_audio_probe.exe" if os.name == "nt" else "limiter_audio_probe")
    if args.probe is None:
        command(["cmake", "--build", build, "--target", "limiter_audio_probe", "--parallel", "2"],
                env={**os.environ, "TMPDIR": str(folder)})
    if sys.byteorder != "little":
        raise ValueError("The probe currently writes native little-endian float32")
    capture_command = args.capture_command
    capture_to_file = bool(capture_command and "{output}" in capture_command)
    if args.no_capture:
        capture_argv = []
    elif capture_command:
        capture_argv = [part.format(output=str(folder / "capture.f32"), rate=args.rate)
                        for part in shlex.split(capture_command, posix=os.name != "nt")]
    else:
        monitor = args.monitor or output(["pactl", "get-default-sink"]) + ".monitor"
        capture_argv = ["parec", "--raw", "--format=float32le", "--rate=" + str(args.rate),
                        "--channels=2", "--latency-msec=20", "--device=" + monitor]
    manifest = {"driver": args.driver, "mode": args.mode, "source": args.source,
                "rate": args.rate, "period": args.period, "seconds": args.seconds, "gain": args.gain,
                "capture_command": capture_argv, "probe": str(probe),
                "probe_sha256": hashlib.sha256(probe.read_bytes()).hexdigest(),
                "tool_source_sha256": {name: hashlib.sha256((ROOT / "test" / name).read_bytes()).hexdigest()
                                       for name in ("limiter_audio.py", "limiter_audio_probe.cpp", "limiter_oracle.py")},
                "revision": output(["git", "-C", ROOT, "rev-parse", "HEAD"]),
                "tracked_diff_sha256": hashlib.sha256(command(["git", "-C", ROOT, "diff", "HEAD"], capture_output=True).stdout).hexdigest(),
                "platform": sys.platform, "started_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
    cache = build / "CMakeCache.txt"
    if cache.exists():
        manifest["build_options"] = [line for line in cache.read_text().splitlines()
                                     if line.startswith(("CMAKE_BUILD_TYPE:", "enable-floats:", "enable-signalsmith:"))]
    (folder / "run.json").write_text(json.dumps(manifest, indent=2) + "\n")
    for name, argv in [("server.txt", ["pactl", "info"]), ("sinks.txt", ["pactl", "list", "sinks"]),
                       ("links.txt", ["pw-link", "-l"]), ("libraries.txt", ["ldd", str(probe)])]:
        if shutil.which(argv[0]):
            result = subprocess.run(argv, capture_output=True, text=True)
            (folder / name).write_text(result.stdout + result.stderr)
    recorder = player = top = None
    with contextlib.ExitStack() as files:
        rec_log = files.enter_context((folder / "recorder.log").open("w"))
        driver_log = files.enter_context((folder / "driver.log").open("w"))
        top_log = files.enter_context((folder / "pw-top.log").open("w"))
        audio = files.enter_context((folder / "capture.f32").open("wb")) if capture_argv else rec_log
        try:
            if capture_argv:
                recorder = subprocess.Popen(capture_argv, stdout=rec_log if capture_to_file else audio, stderr=rec_log)
            if shutil.which("pw-top"):
                top = subprocess.Popen(["pw-top", "-b"], stdout=top_log, stderr=subprocess.STDOUT)
            time.sleep(0.5)
            if recorder is not None and recorder.poll() is not None:
                raise RuntimeError("Recorder exited; inspect recorder.log")
            argv = [str(probe), args.driver, str(folder / "audio"),
                    str(args.rate), str(args.period), str(args.seconds), args.mode, args.source, str(args.gain)]
            player = subprocess.Popen(argv, stdout=driver_log, stderr=subprocess.STDOUT)
            deadline = time.monotonic() + args.seconds + 30
            while player.poll() is None:
                if recorder is not None and recorder.poll() is not None:
                    raise RuntimeError("Recorder stopped during playback; inspect recorder.log")
                if time.monotonic() > deadline:
                    raise RuntimeError("Probe timed out; inspect driver.log")
                time.sleep(0.1)
            if player.returncode:
                raise RuntimeError("Probe failed; inspect driver.log")
            time.sleep(0.5)
        finally:
            stop(player)
            stop(recorder)
            stop(top)
    args.capture = None
    if args.no_capture:
        rendered = load_audio(folder / "audio-rendered.f32", args.rate)
        write_wav(folder / "audio-rendered.wav", rendered, args.rate)
        with (folder / "audio-events.csv").open() as file:
            events = list(csv.DictReader(file))
        with (folder / "audio-callbacks.csv").open() as file:
            callbacks = [{key: int(value) for key, value in row.items()} for row in csv.DictReader(file)]
        oracle, expected, _, _ = check_limiter(manifest, rendered, events, callbacks)
        write_wav(folder / "expected.wav", expected, args.rate)
        (folder / "limiter-analysis.json").write_text(json.dumps(oracle, indent=2) + "\n")
        print("Rendered output saved to", folder, "-- export the external loopback recording, then run analyze --capture FILE.wav")
        print(json.dumps(oracle, indent=2))
        return 0 if all(oracle["checks"].values()) else 1
    return analyze(args)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    record_parser = sub.add_parser("record", help="Play analytic signals through the limiter, change settings, capture the monitor")
    record_parser.add_argument("--driver", default="pipewire")
    record_parser.add_argument("--build", type=Path, default=ROOT / "build")
    record_parser.add_argument("--probe", type=Path, help="Use an already built probe, including another revision")
    record_parser.add_argument("--rate", type=int, default=48000)
    record_parser.add_argument("--period", type=int, default=256)
    record_parser.add_argument("--seconds", type=float, default=12)
    record_parser.add_argument("--gain", type=float, default=0.3)
    record_parser.add_argument("--mode", choices=("static", "controls", "delay", "rapid"), default="controls")
    record_parser.add_argument("--source", choices=("multitone", "sine", "dc"), default="multitone")
    record_parser.add_argument("--monitor", help="Pulse/PipeWire monitor source (default: default sink's monitor)")
    record_parser.add_argument("--capture-command", help="External recorder argv, no shell; {output}=float32le stereo file, {rate}=Hz. Without {output}, write raw PCM to stdout.")
    record_parser.add_argument("--no-capture", action="store_true", help="Play and save rendered only; record the service output in an external application")
    analyze_parser = sub.add_parser("analyze", help="Reanalyze, optionally with an external platform's loopback WAV")
    analyze_parser.add_argument("--capture", type=Path, help="Stereo PCM WAV or float32le raw capture")
    for child in (record_parser, analyze_parser):
        child.add_argument("--output", type=Path, required=True, help="Run directory (record refuses to overwrite it)")
        child.add_argument("--min-snr", type=float, default=60)
        child.add_argument("--max-error", type=float, default=0.002)
    args = parser.parse_args()
    try:
        return record(args) if args.action == "record" else analyze(args)
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        parser.exit(2, str(error) + "\n")


if __name__ == "__main__":
    sys.exit(main())
