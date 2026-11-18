"""Independent delayed-input, reset and steady-gain contracts for the probe.

Isolated delay updates validate an empty delay line at a bounded processing-block
boundary. Grouped controls and rapid updates check stable regions and final
convergence; their intermediate publication order is not modeled.
"""
import array
import math


def source_sample(run, frame):
    if frame < 0:
        return 0.0, 0.0
    gain = run["gain"]
    if run["source"] == "dc":
        return 2 * gain, -gain
    t = frame / run["rate"]
    tau = 2 * math.pi
    if run["source"] == "sine":
        return gain * 0.2 * math.sin(tau * 500 * t), gain * 0.15 * math.sin(tau * 500 * t)
    return (gain * (0.2 * math.sin(tau * 223 * t) + 0.1 * math.sin(tau * 617 * t)),
            gain * (0.18 * math.sin(tau * 331 * t) + 0.12 * math.sin(tau * 997 * t)))


def target_sample(run, frame, state):
    delay = math.ceil(state["attack"] * 0.001 * run["rate"]) if state["active"] else 0
    values = source_sample(run, frame - delay)
    if run["source"] != "dc" or not state["active"]:
        return values
    gains = [min(1, state["output-limit"] / max(abs(v), 1e-30)) for v in values]
    linked = min(gains)
    return tuple(v * (g + (linked - g) * state["link-channels"]) for v, g in zip(values, gains))


def check_limiter(run, rendered, events, callbacks):
    rate = run["rate"]
    count = len(rendered) // 2
    begin, end = rate // 2, count - rate // 2
    if end <= begin:
        raise ValueError("Not enough rendered audio for the independent oracle")
    if run["source"] != "dc" and run["gain"] > 0.3:
        raise ValueError("Multitone exceeds the oracle's below-threshold input bound")
    state = {"attack": 5, "active": 1, "output-limit": 0.35, "link-channels": 1}
    expected = array.array("f")
    for frame in range(count):
        expected.extend(target_sample(run, frame, state))
    errors = array.array("f", [0]) * (2 * count)
    checked = bytearray(count)
    largest_callback = max(row["frames"] for row in callbacks)
    tolerance = 2e-7
    reset_windows = []

    def compare(first, last, params):
        for frame in range(max(begin, first), min(end, last)):
            target = target_sample(run, frame, params)
            expected[2 * frame:2 * frame + 2] = array.array("f", target)
            for channel in range(2):
                errors[2 * frame + channel] = rendered[2 * frame + channel] - target[channel]
            checked[frame] = 1

    cursor = 0
    for event in events:
        first = int(event["before_frame"])
        compare(cursor, first, state)
        old = state.copy()
        state[event["setting"].removeprefix("synth.limiter.")] = float(event["value"])
        delay = math.ceil(state["attack"] * 0.001 * rate) if state["active"] else 0
        latest = int(event["after_frame"]) + largest_callback + 128
        last = latest + delay
        reset_windows.append((first, last))
        if run["mode"] == "delay" and run["source"] != "dc":
            # The setter can overlap a callback. Fit only its bounded adoption
            # block, not the output shape: old delayed input, an empty new delay,
            # then the new delayed input. Both channels must share that block.
            if first < cursor:
                raise ValueError("Isolated delay requests overlap; use rapid convergence checks")
            candidates = range(((first + 63) // 64) * 64, latest + 1, 64)
            best_error, best_start = float("inf"), None
            for start in candidates:
                maximum = 0
                for frame in range(max(begin, first), min(end, last)):
                    target = (target_sample(run, frame, old) if frame < start else
                              (0, 0) if frame < start + delay else target_sample(run, frame, state))
                    maximum = max(maximum, *(abs(rendered[2 * frame + c] - target[c]) for c in range(2)))
                    if maximum > best_error:
                        break
                if maximum < best_error:
                    best_error, best_start = maximum, start
            if best_start is None:
                raise ValueError("No adoption block inside the setter/callback bounds")
            for frame in range(max(begin, first), min(end, last)):
                target = (target_sample(run, frame, old) if frame < best_start else
                          (0, 0) if frame < best_start + delay else target_sample(run, frame, state))
                expected[2 * frame:2 * frame + 2] = array.array("f", target)
                for c in range(2):
                    errors[2 * frame + c] = rendered[2 * frame + c] - target[c]
                checked[frame] = 1
        elif run["source"] == "dc":
            # DC has an algebraic steady gain, not an independent transient
            # envelope model. Exclude settling after the last overlapping update.
            last += math.ceil(rate * 0.3)
        cursor = max(cursor, last)
    compare(cursor, count, state)
    maximum = max((abs(errors[2 * frame + c]) for frame in range(begin, end)
                   if checked[frame] for c in range(2)), default=float("inf"))
    coverage = sum(checked[begin:end]) / (end - begin)
    complete = run["source"] != "dc" and run["mode"] in ("static", "delay")
    final_count = sum(checked[max(begin, end - rate // 10):end])
    checks = {"analytic_waveform_or_steady_gain_matches": maximum <= tolerance,
              "final_target_was_checked": final_count >= rate // 10,
              "required_waveform_coverage": coverage >= 0.999 if complete else coverage > 0.05}
    return {"checks": checks, "source": run["source"], "mode": run["mode"],
            "expected_from": "Analytic delayed input, empty delay after reconfiguration, or algebraic steady DC gain. Adoption is bounded by observed setter/callback frames. No limiter output constructs the source or target.",
            "waveform_coverage_fraction": coverage,
            "intermediate_rapid_sequence_checked": False if run["mode"] in ("rapid", "controls") else None,
            "reset_windows_frames": reset_windows,
            "tolerance": tolerance, "max_error": maximum,
            "expected_audio_scope": "Exact delayed input and bounded reset timing" if complete else "Stationary targets in checked regions only; expected.wav does not represent unchecked transients"}, expected, errors, checked
