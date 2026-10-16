# Dattorro tap-wrap review

## Change

`fluid_dattorro_read_tap()` now wraps the ring-buffer index with one conditional subtraction instead of `% size` plus a negative-index correction. The helper returns zero for an unallocated delay as before. It is used only for the 14 configured Dattorro output taps.

The single-wrap precondition holds for the default tap/delay pairs. The shortest margin is between tap 1228 and the 1800-sample backing delay at the paper's base rate. At the minimum supported sample rate (8 kHz), that gap is about 154 samples before integer rounding. The bounds script checks all 14 tap/backing-delay pairs at every integer rate from 8 to 96 kHz, in both float and double sample-rate arithmetic; minimum measured margin is 153 samples. `line_out` is kept in `[0, size)` by the delay-line process and reset paths, so `line_out + tap` needs at most one subtraction. The sample-rate setting is registered with the same 8–96 kHz range in `src/synth/fluid_synth.c`.

## Correctness and compatibility

- Release double and float builds each passed all 41 enabled tests.
- For Maestro_1, POP909_010, and All The Small Things, 60-second renders at 44.1 kHz with Dattorro effects on matched baseline PCM exactly in both precision builds.
- All The Small Things also matched exactly at the supported 8 kHz and 96 kHz endpoints in both precision builds.
- The change adds no state, interface, allocation, or engine-selection behavior. Other reverb engines are unaffected.
- `git diff --check` passed.

## Performance

Five paired AB/BA runs per input, 60-second in-memory sample-clock renders at 44.1 kHz, 64-frame requests, one synthesis core, Dattorro effects enabled, GCC 14.2 Release double, and the same MuseScore General SF2. Median candidate/baseline ratios:

| MIDI | Wall | CPU | Wall wins |
| --- | ---: | ---: | ---: |
| Maestro_1 | 0.962 | 0.962 | 5/5 |
| POP909_010 | 0.935 | 0.935 | 5/5 |
| All The Small Things | 0.931 | 0.932 | 5/5 |

This is a measured 3.8%–6.9% median reduction in wall and CPU time across these three inputs. Peak RSS stayed within 0.1% of baseline. The test excludes realtime drivers and measures one host/configuration. The candidate object contains no integer divide instructions in `fluid_rev_dattorro.cpp`; the baseline object contains 52 across its instantiated process paths.

## Evidence files

- `bounds.json` and `check_bounds.py` — supported-range tap/delay bounds
- `pcm-parity.json` and `pcm-parity-float.json` — exact PCM hashes for both precision builds
- `paired.jsonl`, `paired-summary.json`, and `paired-manifest.json` — raw paired timings, summary, and source/input/binary/host identity
- `run_parity.py`, `run_parity_float.py`, and `run_paired.py` — reproduction helpers


## Fork branch

Source commit: `33c269f4b951a70e3453eadcca7df51ade877697` on `perf/dattorro-single-wrap-20261016`. This evidence was added in a follow-up documentation commit on the same fork branch. No upstream pull request has been created.
