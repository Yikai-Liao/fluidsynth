#!/usr/bin/env python3
"""Analyze baseline/candidate wet output dumps from run_render_pair.py."""
import argparse
import array
import cmath
import json
import math
import sys
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('render_dir', type=Path)
parser.add_argument('--rate', type=int, default=44100)
args = parser.parse_args()
ROOT = args.render_dir
RATE = args.rate
FFT_N = 8192

def read_f32(path):
    data = array.array('f')
    data.frombytes(Path(path).read_bytes())
    if sys.byteorder != 'little':
        data.byteswap()
    return data

def fft(values):
    n = len(values)
    data = [complex(v, 0.0) for v in values]
    j = 0
    for i in range(1, n):
        bit = n >> 1
        while j & bit:
            j ^= bit
            bit >>= 1
        j ^= bit
        if i < j:
            data[i], data[j] = data[j], data[i]
    size = 2
    while size <= n:
        half = size // 2
        root = cmath.exp(-2j * math.pi / size)
        for start in range(0, n, size):
            twiddle = 1 + 0j
            for k in range(half):
                even = data[start + k]
                odd = twiddle * data[start + k + half]
                data[start + k] = even + odd
                data[start + k + half] = even - odd
                twiddle *= root
        size *= 2
    return data

def spectral_metrics(left, right, start_frame):
    powers = []
    for channel in (left, right):
        windowed = []
        for i in range(FFT_N):
            hann = 0.5 - 0.5 * math.cos(2.0 * math.pi * i / (FFT_N - 1))
            windowed.append(channel[start_frame + i] * hann)
        spectrum = fft(windowed)
        powers.append([abs(spectrum[i]) ** 2 for i in range(1, FFT_N // 2)])
    combined = [(a + b) * 0.5 for a, b in zip(*powers)]
    mean = sum(combined) / len(combined)
    floor = max(mean * 1e-20, 1e-300)
    db = [10.0 * math.log10(max(v, floor)) for v in combined]
    ordered = sorted(combined)
    p99 = ordered[int(0.99 * (len(ordered) - 1))]
    mean_db = sum(db) / len(db)
    return {
        'spectral_flatness_db': 10.0 * math.log10(math.exp(sum(math.log(max(v, floor)) for v in combined) / len(combined)) / mean),
        'log_power_std_db': math.sqrt(sum((v - mean_db) ** 2 for v in db) / len(db)),
        'p99_excess_db': 10.0 * math.log10(max(p99, floor) / mean),
        'peak_excess_db': 10.0 * math.log10(max(combined) / mean),
        'mean_power': mean,
    }

def measure(arm):
    dry = read_f32(ROOT / f'piano-single-{arm}-fx0.f32')
    wet_mix = read_f32(ROOT / f'piano-single-{arm}-fx1.f32')
    frames = len(dry) // 2
    wet_l = [wet_mix[2*i] - dry[2*i] for i in range(frames)]
    wet_r = [wet_mix[2*i+1] - dry[2*i+1] for i in range(frames)]
    wet_values = [x for pair in zip(wet_l, wet_r) for x in pair]
    rms = math.sqrt(sum(x*x for x in wet_values) / len(wet_values))
    peak = max(abs(x) for x in wet_values)
    envelope = []
    hop = RATE // 2
    for start in range(0, frames - hop + 1, hop):
        segment = wet_values[2*start:2*(start+hop)]
        envelope.append({'start_s': start / RATE, 'rms': math.sqrt(sum(x*x for x in segment) / len(segment))})
    windows = {}
    for seconds in (0.5, 1.0, 2.0, 4.0, 7.0, 10.0, 12.0):
        start = int(seconds * RATE)
        if start + FFT_N <= frames:
            windows[str(seconds)] = spectral_metrics(wet_l, wet_r, start)
    return {'wet_rms': rms, 'wet_peak': peak, 'envelope_500ms': envelope,
            'spectral_windows': windows, 'wet_l': wet_l, 'wet_r': wet_r}

base = measure('baseline')
cand = measure('candidate')
dry_base = read_f32(ROOT / 'piano-single-baseline-fx0.f32')
dry_cand = read_f32(ROOT / 'piano-single-candidate-fx0.f32')
dry_diff = [a-b for a,b in zip(dry_base,dry_cand)]
wet_diff = [a-b for a,b in zip(base['wet_l'],cand['wet_l'])] + [a-b for a,b in zip(base['wet_r'],cand['wet_r'])]
result = {
    'sample_rate': RATE,
    'frames': len(dry_base)//2,
    'dry_pcm_identical': dry_base == dry_cand,
    'dry_diff_peak': max(abs(x) for x in dry_diff),
    'wet_candidate_vs_baseline_relative_rms': math.sqrt(sum(x*x for x in wet_diff)/len(wet_diff)) / base['wet_rms'],
    'baseline': {k:v for k,v in base.items() if k not in ('wet_l','wet_r')},
    'candidate': {k:v for k,v in cand.items() if k not in ('wet_l','wet_r')},
}
(ROOT / 'analysis-piano-single.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
