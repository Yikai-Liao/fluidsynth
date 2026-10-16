# FDN coefficient search: no accepted change

Date: 2026-10-16. Baseline source is FluidSynth `ae31c514`; the experimental worktree was based on the voice-reuse correctness branch `20913f66`. This stage asked whether a fixed-cost orthogonal Householder FDN could use different sign patterns for its feedback vector and stereo output vectors while keeping the delay count and matrix operation count unchanged.

## Search and decision

The offline model enumerated 40,320 configurations: 128 canonical Householder sign vectors and 315 balanced orthogonal stereo output pairs, evaluated at three nominal decay times (1.5, 5, and 12.5 seconds). It used fixed nominal delay lengths and a linear frequency-domain model. No configuration met the screening rule across all three decay times: mean wet power within 1 dB of baseline, improved spectral flatness, no material regression in p99 and peak excess, and left/right energy balance within 1 dB. This model is a shortlist tool only; it does not simulate the production modulated delay lines or the full note/render distribution.

Four short 15-second renders used the actual FDN engine on one piano MIDI, with chorus disabled and reverb set to FDN. Dry output remained bit-identical. The first two candidates changed wet RMS to roughly 3.05x the baseline; one later candidate reduced it to 0.174x. The final energy-near model shortlist, masks `(u=207, left=178, right=195)`, reduced actual wet RMS to 0.180x baseline. Its measured flatness changes in 0.5, 1, 2, 4, 7, 10, and 12-second windows were `+7.78, +9.41, +5.35, +3.10, +6.87, -3.23, -8.88 dB`; peak-excess changes were `-7.46, -4.92, -2.44, +1.94, -0.37, +4.28, +4.70 dB`. The early improvement reverses late, and wet energy is far from matched.

The differences show that the simplified search does not predict production wet level well enough to select a robust setting. No candidate was accepted, and no performance benchmark was run; this is not a performance optimization result. The exploratory source edit was reverted. The full model CSV and source, render harness, and compact per-window analyses are included in this branch under `results/`. Raw PCM dumps and local build binaries remain in the local analysis workspace and are omitted here.

## Reproduction and limits

- Search: `g++ -O3 -DNDEBUG -std=c++17 search_full_signs.cpp -o search-full-signs && ./search-full-signs > search-full-signs.csv`.
- Real render: `python3 run_render_pair.py`; analysis: `python3 analyze_render.py`.
- The screen used one SoundFont/MIDI, one sample rate (44.1 kHz), one room setting, and short renders. It is not a listening study or a claim about all FDN configurations. A future study would need loudness-matched multi-program listening and multiple rooms/rates before recommending a sound-quality change.


## Fork branch

This is a research-only branch based on voice-reuse source commit `20913f669dd6220052ad0c68d0d6fe3bebcd7d62`. It contains no production source change. The report and reproducibility data were pushed to the user fork branch `explore/fdn-coefficient-tuning-20261016`; no upstream pull request has been created.
