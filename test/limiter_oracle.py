"""Analytic contracts for the manual limiter probe, independent of its DSP.

No rendered samples are used to construct the source or steady target. For an
isolated delay change, project the output onto the segment between the two
known delayed inputs: one shared, monotone, 20 ms linear coefficient must
explain both channels. Adoption time is bounded by the setter's observed frame
range, callback buffering, the new lookahead and the documented fade duration.
Rapid coalesced requests are checked for initial/final convergence only; their
intermediate state sequence is deliberately not modeled here.
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
    # The target is useful for listening in stable regions. For delay changes it
    # is the new stationary target, not an invented exact adoption timestamp.
    expected = array.array("f")
    for frame in range(count):
        expected.extend(target_sample(run, frame, state))
    errors = array.array("f", [0]) * (2 * count)
    checked = bytearray(count)
    fade = math.ceil(0.020 * rate)
    largest_callback = max(row["frames"] for row in callbacks)
    tolerance = 2e-7
    coefficient_failures = 0
    max_coefficient_violation = 0.0
    previous_event_end = 0
    transition_energy = branch_energy = 0.0
    fade_windows = {}

    def compare(first, last, params):
        for frame in range(max(begin, first), min(end, last)):
            target = target_sample(run, frame, params)
            expected[2 * frame:2 * frame + 2] = array.array("f", target)
            for channel in range(2):
                errors[2 * frame + channel] = rendered[2 * frame + channel] - target[channel]
            checked[frame] = 1

    if run["mode"] == "rapid":
        first = int(events[0]["before_frame"]) if events else count
        compare(0, first, state)
        for event in events:
            state[event["setting"].removeprefix("synth.limiter.")] = float(event["value"])
        # A current transition and one final coalesced transition can each need
        # a maximum lookahead plus fade. No exact intermediate sequence is assumed.
        final = int(events[-1]["after_frame"]) + math.ceil(rate * 0.6) + largest_callback if events else first
        compare(final, count, state)
    elif run["source"] == "dc":
        # Only steady DC has an algebraic gain target; don't pretend its
        # attack/release transient is specified by that steady-state equation.
        cursor = 0
        for event in events:
            first = int(event["before_frame"])
            compare(cursor, first, state)
            state[event["setting"].removeprefix("synth.limiter.")] = float(event["value"])
            cursor = int(event["after_frame"]) + largest_callback + 128 + math.ceil(rate * (0.3 + state["attack"] * 0.001))
        compare(cursor, count, state)
    elif run["mode"] in ("static", "controls"):
        # Every input peak is <= 0.09, every threshold >= 0.12, and controls
        # never changes attack or bypass. Every sample must be delayed identity.
        compare(0, count, state)
    else:
        cursor = 0
        for event in events:
            first = int(event["before_frame"])
            compare(cursor, first, state)
            old = state.copy()
            state[event["setting"].removeprefix("synth.limiter.")] = float(event["value"])
            delay = math.ceil(state["attack"] * 0.001 * rate) if state["active"] else 0
            last = int(event["after_frame"]) + largest_callback + 192 + delay + fade
            if first < previous_event_end:
                raise ValueError("Isolated delay requests overlap; use rapid convergence checks")
            previous_event_end = last
            previous = None
            for frame in range(max(first, begin), min(last, end)):
                a, b = target_sample(run, frame, old), target_sample(run, frame, state)
                delta = [b[c] - a[c] for c in range(2)]
                actual = rendered[2 * frame:2 * frame + 2]
                norm = sum(v * v for v in delta)
                alpha = sum((actual[c] - a[c]) * delta[c] for c in range(2)) / norm if norm > 1e-20 else 0
                bounded = max(0, min(1, alpha))
                for c in range(2):
                    errors[2 * frame + c] = actual[c] - (a[c] + bounded * delta[c])
                checked[frame] = 1
                if norm > 1e-8:
                    epsilon = tolerance * 4 / math.sqrt(norm)
                    violation = max(0, -alpha - epsilon, alpha - 1 - epsilon)
                    if previous is not None:
                        p_frame, p_alpha, p_epsilon = previous
                        change = alpha - p_alpha
                        violation = max(violation, -change - epsilon - p_epsilon,
                                        change - (frame - p_frame) / fade - epsilon - p_epsilon)
                        if p_epsilon < p_alpha < 1 - p_epsilon and epsilon < alpha < 1 - epsilon:
                            violation = max(violation, abs(change - (frame - p_frame) / fade) - epsilon - p_epsilon)
                    coefficient_failures += violation > 0
                    max_coefficient_violation = max(max_coefficient_violation, violation)
                    previous = frame, alpha, epsilon
                if 0.05 < bounded < 0.95:
                    actual_energy = sum(v * v for v in actual)
                    independent_energy = sum((1 - bounded) * a[c] ** 2 + bounded * b[c] ** 2 for c in range(2))
                    transition_energy += actual_energy
                    branch_energy += independent_energy
                    bucket = frame // max(1, rate // 1000)
                    values = fade_windows.setdefault(bucket, [0, 0, 0])
                    values[0] += actual_energy
                    values[1] += independent_energy
                    values[2] += 1
            cursor = last
        compare(cursor, count, state)
    maximum = max((abs(errors[2 * frame + c]) for frame in range(begin, end)
                   if checked[frame] for c in range(2)), default=float("inf"))
    coverage = sum(checked[begin:end]) / (end - begin)
    expected_coverage = run["source"] != "dc" and run["mode"] != "rapid"
    # A defect at the end must not pass because the whole run was masked out.
    final_count = sum(checked[max(begin, end - rate // 10):end])
    checks = {"analytic_waveform_or_steady_gain_matches": maximum <= tolerance,
              "isolated_crossfade_coefficient_is_valid": coefficient_failures == 0,
              "final_target_was_checked": final_count >= rate // 10,
              "required_waveform_coverage": coverage >= 0.999 if expected_coverage else coverage > 0.05}
    worst_window = min(((10 * math.log10(max(1e-30, a) / b), bucket * max(1, rate // 1000) / rate)
                        for bucket, (a, b, n) in fade_windows.items() if b > 1e-20 and n >= rate * 0.0008),
                       default=(None, None))
    return {"checks": checks, "source": run["source"], "mode": run["mode"],
            "expected_from": "Analytic input and delayed identity, algebraic DC limiting, or the segment/monotonic-slope contract for isolated delay changes. No limiter output is used to construct the source or steady target.",
            "waveform_coverage_fraction": coverage, "intermediate_rapid_sequence_checked": False if run["mode"] == "rapid" else None,
            "tolerance": tolerance, "max_error": maximum,
            "coefficient_failures": coefficient_failures, "max_coefficient_violation": max_coefficient_violation,
            "crossfade_energy_ratio_db": 10 * math.log10(transition_energy / branch_energy) if transition_energy and branch_energy else None,
            "min_1ms_crossfade_energy_ratio_db": worst_window[0],
            "min_1ms_crossfade_time_s": worst_window[1],
            "expected_audio_scope": "Exact delayed input" if run["source"] != "dc" and run["mode"] in ("static", "controls") else "Stationary targets in checked regions only; expected.wav does not represent transition timing or unchecked regions"}, expected, errors, checked
