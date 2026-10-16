# Replace Dattorro tap modulo with a single wrap

Dattorro output taps are nonnegative and remain shorter than their backing delay lines across FluidSynth's supported sample-rate range. Replace per-tap modulo normalization with one conditional subtraction, preserving the selected sample while removing runtime integer division from the tap reads.

Release float and double builds pass all 41 enabled tests. Baseline and candidate PCM match exactly for three 60-second MIDI renders at 44.1 kHz, plus 8 kHz and 96 kHz boundary renders in both precision builds. Five paired runs show median wall and CPU reductions of 3.8%–6.9% across the three inputs; RSS is unchanged within 0.1%.
