#!/usr/bin/env python3
from pathlib import Path
import struct

OUT = Path('/root/code/fluidsynth-analysis-20261015/finished-voice-index/release-burst-64.mid')
PITCHES = list(range(24, 88))

def vlq(value):
    out = [value & 0x7f]
    value >>= 7
    while value:
        out.insert(0, (value & 0x7f) | 0x80)
        value >>= 7
    return bytes(out)

track = bytearray()
track += b'\x00\xff\x51\x03\x07\xa1\x20'  # 120 BPM
track += b'\x00\xc0\x00'                   # piano program
for pitch in PITCHES:
    track += b'\x00\x90' + bytes((pitch, 100))
for i, pitch in enumerate(PITCHES):
    track += vlq(240 if i == 0 else 0)       # release all notes at 250 ms
    track += b'\x80' + bytes((pitch, 64))
track += vlq(3840) + b'\xff\x2f\x00'       # end four seconds after release
header = b'MThd' + struct.pack('>IHHH', 6, 0, 1, 480)
OUT.write_bytes(header + b'MTrk' + struct.pack('>I', len(track)) + track)
print(f'wrote {OUT} ({len(PITCHES)} simultaneous notes, 250 ms hold)')
