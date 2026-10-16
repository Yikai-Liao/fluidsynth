# Skip the redundant empty-buffer check for Dattorro tap reads

Dattorro initializes every tap-bearing delay line in `setup()`, and its sample-length helper clamps lengths to at least one. Remove the per-tap empty-buffer fallback branch in `fluid_dattorro_read_tap()`; tap wrapping and sample arithmetic remain unchanged.

Validation: Release float and double builds pass all 41 enabled tests. Two 60-second MIDI renders match baseline PCM byte-for-byte in both precisions. Three paired screens on each MIDI show a pooled median 2.46% wall-time reduction and 2.49% CPU-time reduction (pair ratios 0.942–1.012). The `All The Small Things` result was consistent; `POP909_010` had a wide pair range, so this remains a targeted screening result rather than a general speedup claim.
