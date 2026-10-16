#!/usr/bin/env python3
from pathlib import Path
import struct

OUT = Path('/root/code/fluidsynth-analysis-20261015/finished-voice-index/release-train-16x64.mid')
PITCHES = range(24, 88)
BURSTS = 16
INTERVAL_TICKS = 1920  # two seconds at 120 BPM / 480 PPQ
HOLD_TICKS = 240       # 250 ms

def vlq(value):
    out = [value & 0x7f]
    value >>= 7
    while value:
        out.insert(0, (value & 0x7f) | 0x80)
        value >>= 7
    return bytes(out)

events = [(0, 0, b'\xff\x51\x03\x07\xa1\x20'), (0, 1, b'\xc0\x00')]
for burst in range(BURSTS):
    start = burst * INTERVAL_TICKS
    for pitch in PITCHES:
        events.append((start, 2, b'\x90' + bytes((pitch, 100))))
        events.append((start + HOLD_TICKS, 3, b'\x80' + bytes((pitch, 64))))

track = bytearray()
last_tick = 0
for tick, _, event in sorted(events):
    track += vlq(tick - last_tick) + event
    last_tick = tick
track += vlq((BURSTS - 1) * INTERVAL_TICKS + HOLD_TICKS + 3840 - last_tick)
track += b'\xff\x2f\x00'
header = b'MThd' + struct.pack('>IHHH', 6, 0, 1, 480)
OUT.write_bytes(header + b'MTrk' + struct.pack('>I', len(track)) + track)
print(f'wrote {OUT} ({BURSTS} bursts, {len(PITCHES)} notes each, {len(track)} track bytes)')
