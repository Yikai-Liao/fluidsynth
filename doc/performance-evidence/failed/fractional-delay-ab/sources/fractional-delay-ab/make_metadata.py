#!/usr/bin/env python3
import csv
import json
import os
from pathlib import Path

root = Path(os.environ.get(
    'FLUIDSYNTH_FD_DATA_ROOT', Path(__file__).resolve().parent.parent / 'cubic-sinc25'))
manifest = json.loads((root / 'sf2-samples/manifest.json').read_text())
rates = {item['label']: item['base_phase_step'] for item in json.loads((root / 'sf2-rate-context.json').read_text())}
out = Path(__file__).resolve().parent / 'samples.csv'
with out.open('w', newline='') as f:
    w = csv.writer(f)
    w.writerow(['label', 'raw_file', 'frames', 'loop_start', 'loop_end', 'samplemode', 'base_step'])
    for s in manifest['samples']:
        w.writerow([s['label'], s['raw_file'], s['frames'], s['relative_loop_start'],
                    s['relative_loop_end'], s.get('samplemode', 0), rates[s['label']]])
print(out)
