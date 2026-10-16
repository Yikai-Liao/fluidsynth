#!/usr/bin/env python3
"""Creates the deterministic 96-simultaneous-note MIDI guard input."""
import struct
from pathlib import Path

OUT=Path(__file__).resolve().parent/'high_poly_96.mid'

def vlq(n):
    b=[n&127]; n >>= 7
    while n:
        b.insert(0,(n&127)|128); n >>= 7
    return bytes(b)

track=bytearray(b'\x00\xff\x51\x03\x07\xa1\x20')
for i in range(96):
    channel=i%16; key=36+(i//16)
    track += b'\x00'+bytes([0x90|channel,key,100])
for i in range(96):
    channel=i%16; key=36+(i//16)
    track += vlq(38400 if i==0 else 0)+bytes([0x80|channel,key,0])
track += b'\x00\xff\x2f\x00'
OUT.write_bytes(b'MThd'+struct.pack('>IHHH',6,0,1,480)+b'MTrk'+struct.pack('>I',len(track))+track)
print(OUT)
