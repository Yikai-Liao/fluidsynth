#!/usr/bin/env python3
"""Read-only frequency-domain screen for compensated 2x polynomial C.

This evaluates the published z-form coefficients used by the archived C screen,
finds the exact phase extrema of |H| for each sampled frequency by solving the
cubic derivative in u=z^2, then fits finite symmetric source-rate FIR
pre-emphasis filters to the minimax phase-independent compensation curve.
It does not read or modify production source or render SF2 samples.
"""

from __future__ import annotations

import cmath
import csv
from functools import lru_cache
import json
import math
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parent
U_MAX = 0.25
PASSBAND = 0.40
NYQUIST = 0.50
FREQ_STEP = 0.0001
DB_LIMIT = 0.1

# Pair j is at offsets +/- (j + 1/2) on the 2x grid. Coefficient order is
# [z^0 even, z^1 odd, z^2 even, z^3 odd, z^4 even]. Values copied from the
# archived standalone implementation; they are not generated here.
COEFFICIENTS = {
    4: [
        [0.45645918406487612, 0.47236675362442071, -0.253674794204558521,
         -0.37917091811631082, 0.04252164479749607],
        [0.04354173901996461, 0.17686613581136501, 0.25371918651882464,
         0.11952965967158000, -0.04289144034653719],
    ],
    6: [
        [0.37484203669443822, 0.19253897284651597, -0.154026006475653071,
         -0.06523685579716083, 0.03134095684084392],
        [0.11970939637439368, 0.22555179040018719, 0.10546111301131367,
         -0.04867197815057284, -0.04385804833432710],
        [0.00544862268096358, 0.02621377625620669, 0.04856757454258609,
         0.04200764942718964, 0.01249475765486819],
    ],
}


def quadratic_real_roots(a: float, b: float, c: float) -> list[float]:
    scale = max(abs(a), abs(b), abs(c), 1.0)
    eps = 2.0e-15 * scale
    if abs(a) <= eps:
        return [] if abs(b) <= eps else [-c / b]
    disc = b * b - 4.0 * a * c
    if disc < -eps * scale:
        return []
    if disc <= 0.0:
        return [-b / (2.0 * a)]
    root = math.sqrt(disc)
    q = -0.5 * (b + math.copysign(root, b))
    if q == 0.0:
        return [-b / (2.0 * a)]
    return [q / a, c / q]


def response_squared_coefficients(points: int, f: float) -> list[float]:
    theta = math.pi * f
    r0 = r1 = r2 = i0 = i1 = 0.0
    for j, a in enumerate(COEFFICIENTS[points]):
        phase = (j + 0.5) * theta
        c, s = math.cos(phase), math.sin(phase)
        r0 += 2.0 * c * a[0]
        i0 += 2.0 * s * a[1]
        r1 += 2.0 * c * a[2]
        i1 += 2.0 * s * a[3]
        r2 += 2.0 * c * a[4]
    # |H|^2 = (r0 + r1*u + r2*u^2)^2 + u*(i0+i1*u)^2.
    return [
        r0 * r0,
        2.0 * r0 * r1 + i0 * i0,
        r1 * r1 + 2.0 * r0 * r2 + 2.0 * i0 * i1,
        2.0 * r1 * r2 + i1 * i1,
        r2 * r2,
    ]


def response_polynomial(points: int, f: float) -> list[complex]:
    """Coefficients of H(z,f) in ascending powers of z."""
    theta = math.pi * f
    r0 = r1 = r2 = i0 = i1 = 0.0
    for j, a in enumerate(COEFFICIENTS[points]):
        phase = (j + 0.5) * theta
        c, s = math.cos(phase), math.sin(phase)
        r0 += 2.0 * c * a[0]
        i0 += 2.0 * s * a[1]
        r1 += 2.0 * c * a[2]
        i1 += 2.0 * s * a[3]
        r2 += 2.0 * c * a[4]
    return [r0, 1j * i0, r1, 1j * i1, r2]


def polynomial_moment(power: int, lam: float) -> complex:
    """Integral of z^power exp(-i*lam*z) from -1/2 to 1/2."""
    if abs(lam) < 12.0:
        result = 0j
        for m in range(100):
            if (power + m) % 2 == 0:
                moment = 2.0 * 0.5 ** (power + m + 1) / (power + m + 1)
                term = ((-1j * lam) ** m) * moment / math.factorial(m)
                result += term
                if m > abs(lam) + 20.0 and abs(term) < 1.0e-19:
                    break
        return result
    value = 2.0 * math.sin(lam / 2.0) / lam
    for n in range(1, power + 1):
        a = 0.5
        boundary = (2.0 * a**n * math.sin(lam * a) / lam if n % 2 == 0
                    else 2j * a**n * math.cos(lam * a) / lam)
        value = boundary - 1j * n * value / lam
    return value


def image_coefficient(points: int, f: float, k: int,
                      h: list[complex] | None = None) -> complex:
    """Fourier coefficient C_k of R(mu,f)=H(z,f)*exp(-i*theta*z)."""
    theta = math.pi * f
    lam = theta + 2.0 * math.pi * k
    if h is None:
        h = response_polynomial(points, f)
    return ((-1) ** k) * sum(h[n] * polynomial_moment(n, lam) for n in range(5))


def signed_fold(frequency: float) -> float:
    return (frequency + 0.5) % 1.0 - 0.5


def folded_line_metrics(points: int, f: float, step: float, prefilter_gain: float,
                        max_image: int = 64) -> dict:
    """Fold analytic reconstruction images into discrete output frequencies."""
    groups: dict[float, complex] = {}
    edge_tail = 0.0
    h = response_polynomial(points, f)
    c0 = image_coefficient(points, f, 0, h)
    for k in range(-max_image, max_image + 1):
        c = image_coefficient(points, f, k, h)
        if abs(k) == max_image:
            edge_tail = max(edge_tail, abs(c) / abs(c0))
        frequency = signed_fold(step * (f + 2.0 * k))
        key = round(abs(frequency), 10)
        positive_amplitude = c if frequency >= 0.0 else c.conjugate()
        groups[key] = groups.get(key, 0j) + positive_amplitude

    fundamental_key = round(abs(signed_fold(step * f)), 10)
    fundamental = groups.get(fundamental_key, 0j)
    aliases = [amplitude for key, amplitude in groups.items() if key != fundamental_key]
    alias_rms = math.sqrt(sum(abs(x) ** 2 for x in aliases))
    alias_peak_sum = sum(abs(x) for x in aliases)
    alias_line_max = max((abs(x) for x in aliases), default=0.0)
    fundamental_gain = prefilter_gain * abs(fundamental)
    alias_rms_input = prefilter_gain * alias_rms
    alias_peak_input = prefilter_gain * alias_peak_sum
    alias_line_input = prefilter_gain * alias_line_max
    ratio = alias_rms / abs(fundamental) if abs(fundamental) else math.inf
    return {
        "fundamental_gain_db": db(fundamental_gain),
        "alias_rms_input_db": db(alias_rms_input),
        "max_alias_line_input_db": db(alias_line_input),
        "alias_peak_sum_input_db": db(alias_peak_input),
        "alias_rms_over_fundamental_db": db(ratio),
        "folded_group_count": len(groups),
        "max_image_at_k_bound_over_c0": edge_tail,
    }


def validate_image_integrals_at_04(points: int, nphase: int = 32768) -> dict:
    """Cross-check analytic C_k against independent midpoint quadrature."""
    f = PASSBAND
    theta = math.pi * f
    h = response_polynomial(points, f)
    numerical = {k: 0j for k in range(-5, 6)}
    for i in range(nphase):
        mu = (i + 0.5) / nphase
        z = mu - 0.5
        hp = sum(h[n] * z**n for n in range(5))
        residual = hp * cmath.exp(-1j * theta * z)
        for k in numerical:
            numerical[k] += residual * cmath.exp(-2j * math.pi * k * mu)
    numerical = {k: value / nphase for k, value in numerical.items()}
    analytic = {k: image_coefficient(points, f, k, h) for k in numerical}
    c0 = abs(analytic[0])
    max_delta = max(abs(analytic[k] - numerical[k]) for k in numerical)
    worst_k = max((k for k in numerical if k),
                  key=lambda k: abs(analytic[k]) / c0)
    return {
        "frequency": f,
        "phase_samples": nphase,
        "max_abs_analytic_vs_midpoint_delta": max_delta,
        "largest_nonzero_image_k_minus5_to5": worst_k,
        "largest_nonzero_image_over_fundamental_db": db(abs(analytic[worst_k]) / c0),
        "analytic_coefficients": {str(k): [analytic[k].real, analytic[k].imag] for k in analytic},
    }


def poly_eval(c: list[float], u: float) -> float:
    return ((((c[4] * u + c[3]) * u + c[2]) * u + c[1]) * u + c[0])


def stationary_u(c: list[float]) -> list[float]:
    """Return all roots of d|H|^2/du in [0, 1/4]."""
    def d(u: float) -> float:
        return c[1] + 2.0 * c[2] * u + 3.0 * c[3] * u * u + 4.0 * c[4] * u * u * u

    def dd(u: float) -> float:
        return 2.0 * c[2] + 6.0 * c[3] * u + 12.0 * c[4] * u * u

    # The roots of d' partition the cubic d into monotone intervals. Bisection
    # on each interval captures every simple root; a zero at a partition point
    # also captures a repeated root.
    cuts = [0.0, U_MAX]
    cuts.extend(x for x in quadratic_real_roots(12.0 * c[4], 6.0 * c[3], 2.0 * c[2])
                if 0.0 < x < U_MAX)
    cuts = sorted(cuts)
    vals = [d(x) for x in cuts]
    scale = max(abs(x) for x in vals) if vals else 1.0
    tol = max(2.0e-15, scale * 2.0e-14)
    roots: list[float] = []
    for x, y in zip(cuts, vals):
        if abs(y) <= tol:
            roots.append(x)
    for left, right, dl, dr in zip(cuts, cuts[1:], vals, vals[1:]):
        if dl == 0.0 or dr == 0.0 or dl * dr > 0.0:
            continue
        lo, hi, flo = left, right, dl
        for _ in range(64):
            mid = (lo + hi) * 0.5
            fm = d(mid)
            if fm == 0.0:
                lo = hi = mid
                break
            if flo * fm <= 0.0:
                hi = mid
            else:
                lo, flo = mid, fm
        roots.append((lo + hi) * 0.5)
    roots.sort()
    out: list[float] = []
    for x in roots:
        if 0.0 <= x <= U_MAX and (not out or abs(x - out[-1]) > 1.0e-10):
            out.append(x)
    return out


@lru_cache(maxsize=100_000)
def extrema(points: int, f: float) -> tuple[float, float, float, float]:
    c = response_squared_coefficients(points, f)
    us = [0.0, U_MAX, *stationary_u(c)]
    vals = [(max(0.0, poly_eval(c, u)), u) for u in us]
    lo, umin = min(vals)
    hi, umax = max(vals)
    amin, amax = math.sqrt(lo), math.sqrt(hi)
    phase_span_db = 20.0 * math.log10(amax / amin)
    boost = 1.0 / math.sqrt(amin * amax)
    return amin, amax, phase_span_db, boost


def finite_half_phase_response(points: int = 4095, frequency_steps: int = 1000) -> dict:
    """Check the archived 4095-tap Hann-windowed half-phase reconstruction."""
    half = points // 2
    coeff = []
    ts = []
    for j in range(-half, half + 1):
        t = j - 0.5
        if abs(t) > half:
            value = 0.0
        else:
            a = math.pi * t
            sinc = 1.0 if abs(a) < 1.0e-14 else math.sin(a) / a
            window = 0.5 * (1.0 + math.cos(math.pi * t / half))
            value = sinc * window
        coeff.append(value)
        ts.append(t)
    norm = sum(coeff)
    coeff = [x / norm for x in coeff]

    worst_mag_db = (0.0, None)
    worst_phase = (0.0, None)
    max_complex_error = 0.0
    at_edge = None
    for i in range(frequency_steps + 1):
        f = PASSBAND * i / frequency_steps
        h = sum(c * cmath.exp(2j * math.pi * f * t) for c, t in zip(coeff, ts))
        mag_db = db(abs(h))
        phase = math.atan2(h.imag, h.real)
        if abs(mag_db) > worst_mag_db[0]:
            worst_mag_db = (abs(mag_db), (f, mag_db))
        if abs(phase) > worst_phase[0]:
            worst_phase = (abs(phase), (f, phase))
        max_complex_error = max(max_complex_error, abs(h - 1.0))
        if i == frequency_steps:
            at_edge = {"frequency": f, "magnitude_db": mag_db,
                       "phase_error_rad": phase, "magnitude": abs(h)}

    # A conservative bound for the output error from imperfectly reconstructed
    # odd 2x-grid samples: epsilon * the largest odd-sample coefficient L1 sum.
    odd_l1 = {}
    for npoints, pairs in COEFFICIENTS.items():
        peak = 0.0
        for i in range(10001):
            z = -0.5 + i / 10000.0
            even = [a[0] + a[2] * z * z + a[4] * z**4 for a in pairs]
            odd = [a[1] * z + a[3] * z**3 for a in pairs]
            for base_parity in (0, 1):
                total = 0.0
                for j, (e, o) in enumerate(zip(even, odd)):
                    # Pair j's positive sample has weight e+o, negative e-o.
                    positive_is_odd = (base_parity + j + 1) % 2 == 1
                    total += abs(e + o if positive_is_odd else e - o)
                peak = max(peak, total)
        odd_l1[npoints] = peak
    return {
        "taps": points,
        "normalization_sum_before_normalizing": norm,
        "frequency_steps_through_0p4": frequency_steps,
        "worst_magnitude_frequency": worst_mag_db[1][0],
        "worst_abs_magnitude_db": worst_mag_db[0],
        "worst_magnitude_db_signed": worst_mag_db[1][1],
        "worst_phase_frequency": worst_phase[1][0],
        "worst_abs_phase_error_rad": worst_phase[0],
        "worst_complex_error_magnitude": max_complex_error,
        "at_0p4": at_edge,
        "odd_sample_coefficient_l1_bound": {str(k): v for k, v in odd_l1.items()},
        "conservative_output_error_bound_from_midpoint_kernel": {
            str(k): max_complex_error * v for k, v in odd_l1.items()
        },
    }


def phase_response(points: int, f: float, z: float) -> float:
    theta = math.pi * f
    re = im = 0.0
    for j, a in enumerate(COEFFICIENTS[points]):
        phase = (j + 0.5) * theta
        re += 2.0 * math.cos(phase) * (a[0] + a[2] * z * z + a[4] * z**4)
        im += 2.0 * math.sin(phase) * (a[1] * z + a[3] * z**3)
    return math.hypot(re, im)


def ideal_boost_db(points: int, f: float) -> float:
    amin, amax, _, _ = extrema(points, f)
    return -10.0 * math.log10(amin * amax)


def hermite_tail(points: int, f: float, p0: float, slope0: float) -> float:
    t = (f - PASSBAND) / (NYQUIST - PASSBAND)
    h00 = 2.0 * t**3 - 3.0 * t**2 + 1.0
    h10 = t**3 - 2.0 * t**2 + t
    h01 = -2.0 * t**3 + 3.0 * t**2
    return h00 * p0 + h10 * (NYQUIST - PASSBAND) * slope0 + h01


def trapezoid_cosine_coefficients(target: list[float], taps: int,
                                  integration_intervals: int = 32768) -> list[float]:
    """Fourier cosine coefficients for a source-rate symmetric FIR."""
    half = taps // 2
    df = NYQUIST / integration_intervals
    samples = []
    for i in range(integration_intervals + 1):
        f = i * df
        index = min(int(f / FREQ_STEP), len(target) - 2)
        alpha = (f - index * FREQ_STEP) / FREQ_STEP
        samples.append(target[index] + alpha * (target[index + 1] - target[index]))
    coeff = []
    for k in range(half + 1):
        total = 0.5 * samples[0] + 0.5 * samples[-1] * math.cos(2.0 * math.pi * NYQUIST * k)
        total += sum(samples[i] * math.cos(2.0 * math.pi * i * df * k)
                     for i in range(1, integration_intervals))
        coeff.append(2.0 * df * total)
    dc = coeff[0] + 2.0 * sum(coeff[1:])
    return [x / dc for x in coeff]


def fir_gain(coeff: list[float], f: float) -> float:
    return coeff[0] + 2.0 * sum(x * math.cos(2.0 * math.pi * f * k)
                                for k, x in enumerate(coeff[1:], 1))


def db(x: float) -> float:
    if x <= 0.0:
        return float("-inf")
    return 20.0 * math.log10(x)


def finite_or_none(x: float) -> float | None:
    return x if math.isfinite(x) else None


def main() -> None:
    steps = round(NYQUIST / FREQ_STEP)
    rows: dict[int, list[tuple[float, float, float, float, float]]] = {}
    for points in (4, 6):
        seq = []
        for i in range(steps + 1):
            f = i * FREQ_STEP
            amin, amax, span, boost = extrema(points, f)
            seq.append((f, amin, amax, span, boost))
        rows[points] = seq

    # Refine the global sampled-grid phase-spread peaks using golden-section
    # search around every local peak. This remains a numerical frequency screen,
    # not a formal interval enclosure over continuous frequency.
    def golden_max(fn, left: float, right: float) -> tuple[float, float]:
        ratio = (math.sqrt(5.0) - 1.0) * 0.5
        a, b = left, right
        x1, x2 = b - ratio * (b - a), a + ratio * (b - a)
        y1, y2 = fn(x1), fn(x2)
        for _ in range(80):
            if y1 < y2:
                a, x1, y1 = x1, x2, y2
                x2 = a + ratio * (b - a)
                y2 = fn(x2)
            else:
                b, x2, y2 = x2, x1, y1
                x1 = b - ratio * (b - a)
                y1 = fn(x1)
        x = 0.5 * (a + b)
        return x, fn(x)

    phase_peaks = {}
    for points, seq in rows.items():
        passband_seq = seq[:round(PASSBAND / FREQ_STEP) + 1]
        local = []
        for i in range(1, len(passband_seq) - 1):
            if passband_seq[i][3] >= passband_seq[i - 1][3] and passband_seq[i][3] >= passband_seq[i + 1][3]:
                f, value = golden_max(lambda x: extrema(points, x)[2], passband_seq[i - 1][0], passband_seq[i + 1][0])
                local.append((value, f))
        local.extend([(passband_seq[0][3], passband_seq[0][0]), (passband_seq[-1][3], passband_seq[-1][0])])
        peak_span, peak_f = max(local)
        amin, amax, _, boost = extrema(points, peak_f)
        phase_peaks[points] = {
            "frequency": peak_f,
            "phase_span_db": peak_span,
            "best_residual_abs_db": peak_span / 2.0,
            "min_gain": amin,
            "max_gain": amax,
            "best_compensation": boost,
            "boost_db": db(boost),
        }

    # Match the ideal minimax compensation through f=.40. Beyond the frozen
    # passband, smoothly taper to unity at source Nyquist with a cubic Hermite
    # extension matching the passband slope at .40 and zero slope at .50.
    filter_summaries = {}
    filter_rows = []
    selected_coefficients = {}
    for points, seq in rows.items():
        index40 = round(PASSBAND / FREQ_STEP)
        p0 = seq[index40][4]
        slope0 = (seq[index40 + 1][4] - seq[index40 - 1][4]) / (2.0 * FREQ_STEP)
        target = [boost for _, _, _, _, boost in seq]
        for i in range(index40 + 1, len(target)):
            target[i] = hermite_tail(points, i * FREQ_STEP, p0, slope0)
        if min(target) <= 0.0:
            raise RuntimeError(f"nonpositive compensation target for {points}pt")

        best = None
        for taps in (9, 17, 33, 65, 129):
            coeff = trapezoid_cosine_coefficients(target, taps)
            coeff32 = [struct.unpack("<f", struct.pack("<f", x))[0] for x in coeff]
            for precision, c in (("float32-coefficients", coeff32), ("double-coefficients", coeff)):
                worst = (-1.0, None)
                margins = []
                for i in range(round(PASSBAND / (FREQ_STEP / 4.0)) + 1):
                    f = i * (FREQ_STEP / 4.0)
                    amin, amax, _, _ = extrema(points, f)
                    gain = fir_gain(c, f)
                    low_db, high_db = db(gain * amin), db(gain * amax)
                    current = max(abs(low_db), abs(high_db))
                    if current > worst[0]:
                        worst = (current, (f, low_db, high_db, gain))
                    margins.append((f, low_db, high_db))
                l1 = abs(c[0]) + 2.0 * sum(abs(x) for x in c[1:])
                dc_gain = c[0] + 2.0 * sum(c[1:])
                summary = {
                    "kernel_points": points,
                    "taps": taps,
                    "precision": precision,
                    "max_abs_gain_error_db_0_0p4": worst[0],
                    "worst_frequency": worst[1][0],
                    "worst_low_gain_error_db": worst[1][1],
                    "worst_high_gain_error_db": worst[1][2],
                    "dc_gain": dc_gain,
                    "dc_error_db": db(dc_gain),
                    "impulse_l1_bound": l1,
                    "coefficient_l1_excess_db": db(l1),
                    "nyquist_gain_db": db(fir_gain(c, NYQUIST)),
                    "gain_at_0p4_db": db(fir_gain(c, PASSBAND)),
                    "gain_at_0p45_db": db(fir_gain(c, 0.45)),
                    "gain_at_0p49_db": db(fir_gain(c, 0.49)),
                    "passes_0p1db": worst[0] <= DB_LIMIT,
                    "passes_0p05db_reserve": worst[0] <= 0.05,
                }
                filter_rows.append(summary)
                if precision == "float32-coefficients" and summary["passes_0p05db_reserve"]:
                    best = summary
                    selected_coefficients[points] = c
                    break
            if best:
                break
        filter_summaries[points] = best or min(
            (x for x in filter_rows if x["kernel_points"] == points and x["precision"] == "float32-coefficients"),
            key=lambda x: x["max_abs_gain_error_db_0_0p4"])

    folding_summaries = {}
    folding_rows = []
    step_ratios = [0.5, 0.999, 1.0, 1.001, 1.25, math.sqrt(2.0), 1.5, 2.0, 4.0]
    alias_step = 0.001
    max_image = 64
    for points, coeff in selected_coefficients.items():
        per_step = {}
        for ratio in step_ratios:
            legal_max = min(PASSBAND, 0.5 / ratio)
            nfreq = math.floor(legal_max / alias_step + 1.0e-12)
            worst_rms = (-math.inf, None)
            worst_peak = (-math.inf, None)
            worst_line = (-math.inf, None)
            worst_fundamental = (0.0, None)
            max_edge_tail = 0.0
            for i in range(nfreq + 1):
                f = i * alias_step
                metrics = folded_line_metrics(points, f, ratio, fir_gain(coeff, f), max_image)
                if worst_rms[1] is None or metrics["alias_rms_input_db"] > worst_rms[0]:
                    worst_rms = (metrics["alias_rms_input_db"], (f, metrics))
                if worst_peak[1] is None or metrics["alias_peak_sum_input_db"] > worst_peak[0]:
                    worst_peak = (metrics["alias_peak_sum_input_db"], (f, metrics))
                if worst_line[1] is None or metrics["max_alias_line_input_db"] > worst_line[0]:
                    worst_line = (metrics["max_alias_line_input_db"], (f, metrics))
                if abs(metrics["fundamental_gain_db"]) > worst_fundamental[0]:
                    worst_fundamental = (abs(metrics["fundamental_gain_db"]),
                                         (f, metrics["fundamental_gain_db"]))
                max_edge_tail = max(max_edge_tail, metrics["max_image_at_k_bound_over_c0"])
            summary = {
                "kernel_points": points,
                "source_step_per_output": ratio,
                "legal_input_frequency_max": legal_max,
                "frequency_grid_step": alias_step,
                "image_orders": [-max_image, max_image],
                "worst_alias_rms_input_db": finite_or_none(worst_rms[0]),
                "worst_alias_rms_frequency": worst_rms[1][0] if math.isfinite(worst_rms[0]) else None,
                "worst_alias_peak_sum_input_db": finite_or_none(worst_peak[0]),
                "worst_alias_peak_sum_frequency": worst_peak[1][0] if math.isfinite(worst_peak[0]) else None,
                "worst_single_folded_alias_line_input_db": finite_or_none(worst_line[0]),
                "worst_single_folded_alias_line_frequency": worst_line[1][0] if math.isfinite(worst_line[0]) else None,
                "worst_fundamental_gain_error_abs_db": worst_fundamental[0],
                "worst_fundamental_gain_frequency": worst_fundamental[1][0],
                "max_abs_edge_image_over_c0_k64": max_edge_tail,
                "meets_80db_rms_alias_diagnostic": worst_rms[0] <= -80.0,
                "meets_80db_single_line_alias_diagnostic": worst_line[0] <= -80.0,
            }
            per_step[str(ratio)] = summary
            folding_rows.append(summary)
        folding_summaries[points] = per_step

    with (ROOT / "folded_image_screen.csv").open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(folding_rows[0].keys()))
        writer.writeheader()
        writer.writerows(folding_rows)

    # Validate the exact phase-extremum solver at f=.40 against an independent
    # uniform phase grid and persist the sampled frequency-response rows.
    validation = {}
    image_validation = {}
    for points in (4, 6):
        amin, amax, span, boost = extrema(points, PASSBAND)
        dense = [phase_response(points, PASSBAND, -0.5 + i / 10000.0) for i in range(10001)]
        validation[points] = {
            "f_0p4_min_exact": amin,
            "f_0p4_max_exact": amax,
            "f_0p4_phase_span_exact_db": span,
            "f_0p4_best_boost_db": db(boost),
            "dense_phase_min": min(dense),
            "dense_phase_max": max(dense),
            "dense_phase_span_db": 20.0 * math.log10(max(dense) / min(dense)),
        }
        image_validation[points] = validate_image_integrals_at_04(points)

    with (ROOT / "frequency_response.csv").open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["points", "frequency_cycles_per_original_sample", "min_amplitude", "max_amplitude", "phase_span_db", "best_compensation", "best_compensation_db"])
        for points, seq in rows.items():
            for freq, amin, amax, span, boost in seq:
                writer.writerow([points, f"{freq:.7f}", f"{amin:.15g}", f"{amax:.15g}", f"{span:.12g}", f"{boost:.15g}", f"{db(boost):.12g}"])

    finite_midpoint = finite_half_phase_response()
    result = {
        "method": "exact phase extrema from cubic derivative in u=z^2; sampled frequency sweep with local golden-section refinement of phase-spread peaks",
        "frequency_grid_step": FREQ_STEP,
        "frozen_passband": [0.0, PASSBAND],
        "passband_gain_budget_db": DB_LIMIT,
        "finite_equalizer_target": "minimax inverse geometric mean of exact phase-extrema envelope through f=0.40; cubic Hermite taper to unity at f=0.50",
        "equalizer_design": "truncated Fourier cosine series, symmetric linear-phase FIR; DC normalized; coefficients quantized to float32 for the quantized rows",
        "phase_peak_summary": phase_peaks,
        "f_0p4_solver_validation": validation,
        "f_0p4_image_integral_validation": image_validation,
        "finite_4095_tap_half_phase_interpolator": finite_midpoint,
        "finite_equalizer_summary": filter_summaries,
        "finite_equalizer_trials": filter_rows,
        "folded_image_screen": {
            "method": "analytic Fourier coefficients C_k of the polynomial phase residual; images k=-64..64 folded by source_step*(f+2k), exact frequency collisions summed coherently, float32-coefficient FIR prefilter applied to every source component",
            "frequency_grid_step": alias_step,
            "image_orders": [-max_image, max_image],
            "steps": step_ratios,
            "per_kernel": folding_summaries,
        },
        "limitations": [
            "frequency extrema are numerically searched, not formally interval-enclosed over continuous frequency",
            "does not include the finite 4095-tap 2x interpolator response, SF2 boundaries, loops, float convolution roundoff, aliases, or real-sample errors",
            "the FIR is only a frequency-domain feasibility screen; no performance or production claim is made",
        ],
    }
    (ROOT / "math-summary.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result["phase_peak_summary"], indent=2))
    print(json.dumps(result["f_0p4_solver_validation"], indent=2))
    print(json.dumps(result["finite_equalizer_summary"], indent=2))


if __name__ == "__main__":
    main()
