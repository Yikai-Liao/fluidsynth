#!/usr/bin/env python3
"""Summarize saved halfband timing and wet-quality evidence without rerunning it."""
import hashlib
import json
import re
import statistics
import sys
from pathlib import Path

ROOT = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parent


def sha256(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def parse_timing(path):
    data = {}
    header = {}
    for line in path.read_text().splitlines():
        fields = line.split()
        if not fields:
            continue
        if fields[0] in ('frames', 'blocks', 'repeats', 'taps_per_halfband_path', 'transition', 'attenuation_db'):
            for i in range(0, len(fields) - 1, 2):
                header[fields[i]] = fields[i + 1]
        elif fields[0].endswith('_ms'):
            values = [float(x) for x in fields[1:]]
            data[fields[0][:-3]] = {
                'samples_ms': values,
                'median_ms': statistics.median(values),
                'min_ms': min(values),
                'max_ms': max(values),
            }
    required = {'full_rate_core', 'half_rate_core_direct', 'converter_without_reverb', 'complete_multirate'}
    if not required.issubset(data):
        raise ValueError(f'{path}: missing timing rows {sorted(required - data.keys())}')
    full = data['full_rate_core']['median_ms']
    complete = data['complete_multirate']['median_ms']
    data['h_plus_c_median_ms'] = (data['half_rate_core_direct']['median_ms'] +
                                  data['converter_without_reverb']['median_ms'])
    data['complete_over_full_median_ratio'] = complete / full
    data['full_over_complete_median_ratio'] = full / complete
    return {'source_file': path.name, 'header': header, 'timing': data,
            'comparison_note': 'Ratios are ratios of independent sample medians, not paired speedup estimates.'}


def wet_summary(path):
    text = path.read_text()
    energy = re.search(r'raw_delta_db ([^ ]+) gain_to_match ([^\n]+)', text)
    color = [float(x) for x in re.findall(r'normalized_color_delta ([^ ]+)dB', text)]
    t20 = [float(x) for x in re.findall(r't20_delta ([^ ]+)%', text)]
    iacc = re.search(r'iacc 8\.00-12\.00s full ([^ ]+) halfband ([^ ]+) delta ([^\n]+)', text)
    if not (energy and color and t20 and iacc):
        raise ValueError(f'{path}: did not parse expected wet-quality fields')
    return {
        'raw_quality_file': path.name,
        'raw_delta_db_magnitude': float(energy.group(1)),
        'gain_to_match_field': float(energy.group(2)),
        'normalized_color_delta_min_db': min(color),
        'normalized_color_delta_max_db': max(color),
        't20_delta_min_pct': min(t20),
        't20_delta_max_pct': max(t20),
        'late_iacc_delta': float(iacc.group(3)),
    }


timing_files = [name for name in ('raw-timing-mono.txt', 'raw-timing-mono-long.txt', 'raw-timing-final.txt')
                if (ROOT / name).is_file()]
timings = [parse_timing(ROOT / name) for name in timing_files]
quality_path = ROOT / 'raw-wet-quality.txt'
response_path = ROOT / 'raw-frequency-response.txt'
summary = {
    'experiment': 'G polyphase IIR halfband wet reverb screen',
    'baseline_commit': '571bc72e72b756f25333b3e31fb3046161c27632',
    'timing': timings,
    'wet_quality': wet_summary(quality_path),
    'frequency_response_raw_sha256': sha256(response_path),
}
(ROOT / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')

manifest_path = ROOT / 'manifest.json'
manifest = json.loads(manifest_path.read_text()) if manifest_path.exists() else {}
files = manifest.setdefault('files_sha256', {})
for name in ('REPORT.md', 'run.sh', 'summarize.py', 'summary.json', 'g-hook.patch',
             'g_probe.cpp', 'wet_probe.cpp', 'frequency_response.cpp', 'analyze_wet.cpp',
             'raw-timing.txt', 'raw-timing-mono.txt', 'raw-timing-mono-long.txt',
             'raw-timing-final.txt', 'raw-wet-render.txt', 'raw-wet-quality.txt',
             'raw-frequency-response.txt'):
    path = ROOT / name
    if path.is_file():
        files[name] = sha256(path)
manifest['timing_ratio_convention'] = 'candidate_over_baseline = complete_multirate_median / full_rate_core_median; reciprocal is baseline_over_candidate; neither is a paired estimate.'
short = next((row['timing'] for row in timings if row['source_file'] == 'raw-timing-mono.txt'), None)
if short:
    manifest['timing_medians_ms'] = {
        'full_rate_core': short['full_rate_core']['median_ms'],
        'half_rate_core_H': short['half_rate_core_direct']['median_ms'],
        'converter_C': short['converter_without_reverb']['median_ms'],
        'H_plus_C': short['h_plus_c_median_ms'],
        'complete_multirate': short['complete_multirate']['median_ms'],
    }
    manifest['timing_ratios'] = {
        'complete_over_full_candidate_over_baseline': short['complete_over_full_median_ratio'],
        'full_over_complete_baseline_over_candidate': short['full_over_complete_median_ratio'],
        'paired_estimate': False,
    }
long = next((row['timing'] for row in timings if row['source_file'] == 'raw-timing-mono-long.txt'), None)
if long:
    manifest['long_timing_medians_ms'] = {
        'full_rate_core': long['full_rate_core']['median_ms'],
        'half_rate_core_H': long['half_rate_core_direct']['median_ms'],
        'converter_C': long['converter_without_reverb']['median_ms'],
        'H_plus_C': long['h_plus_c_median_ms'],
        'complete_multirate': long['complete_multirate']['median_ms'],
        'complete_over_full_candidate_over_baseline': long['complete_over_full_median_ratio'],
        'full_over_complete_baseline_over_candidate': long['full_over_complete_median_ratio'],
    }
manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
print(json.dumps({'timing_cases': len(timings), 'summary': str(ROOT / 'summary.json')}, indent=2))
