# Skip redundant IIR coefficient updates within a cutoff table bin

The voice IIR filter currently recalculates biquad coefficients on every smoothed cutoff step, including steps that still use the same precomputed sin/cos-table entry. Keep that table index for the current filter apply call and skip coefficient work while the index and Q remain stable. Preserve the first smoothing recalculation and recalculate every time Q changes.

Validation: Release float and double builds pass all 41 enabled tests. Three 60-second MIDI renders match baseline PCM hashes at 64- and 8192-frame requests in both builds. Five paired trials show about 11–13% lower wall and CPU time on Maestro_1 and POP909_010; All The Small Things is near parity and varies by precision/request size. RSS is unchanged within 0.1%.
