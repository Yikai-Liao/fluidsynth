# FluidSynth MIDI performance work

Updated: 2026-11-04 00:38 Asia/Singapore (UTC+08:00)

## Goal

Continue measured performance optimization for regular MIDI rendering until 2026-11-04 10:00 Beijing time. Each accepted optimization must live on its own branch and be pushed to `origin`. Production changes must add fewer than 50 lines. Keep the original `/home/lyk/code/fluidsynth` worktree untouched because its `TODO` has unrelated uncommitted edits.

## Baseline and prior work

- Upstream baseline: `upstream/master` at `2e838ae6` when the prior optimization was prepared.
- Existing optimization: `perf/mixer-stable-amp`, commit `d4afd04a` (`perf: skip stable voice gain ramp arithmetic`), based on that upstream baseline. It adds 10 production lines and removes 1.
- Reported measurements from the prior run: ordinary generated 8-track MIDI cycles median 5.337B -> 5.128B (~4.1%); 128-voice sustain wall time 3.133 s -> 3.021 s. PCM hashes matched. Real 12-second MIDI measurements showed ~1.5% and ~4.2% lower cycles, with identical PCM hashes.
- Rejected experiments: IIR stable-block branch (no cycles gain) and interpolation sum reordering (gain within noise). Do not revive without new evidence.
- Exclude work already on `todo`, including voice index/owner, mixer index, exclusive bitmap/candidate scans, MIDI heap/track merge, and IIR W4 work.

## Current machine and profiling

- Ryzen 7 8845H, x86_64; 16 logical CPUs.
- `perf_event_paranoid` currently reads `0`; `perf` and `llvm-mca` are available.
- Existing saved profiles in `/tmp/fs-*.data` contain no lost samples. Ordinary MIDI profile: IIR 30.19%, 4th-order interpolation 28.4%. High-voice sustain profiles show IIR around 33-36% and interpolation about 34.7% in one reverb configuration. Profile samples are short and should be refreshed for candidates.
- Self-contained SoundFont: `sf2/VintageDreamsWaves-v2.sf2`.
- Available real MIDI examples include `/home/lyk/Music/asap-dataset-master/Bach/Fugue/bwv_846/Shi05M.mid` and `/home/lyk/Downloads/test.mid`. Prior real-MIDI runs used an additional 12-second normal and Mahler excerpt; locate their exact paths before reuse.

## Worktree state

- Main repo `/home/lyk/code/fluidsynth`: branch `ci/dev-android-migration`, unrelated modified `TODO`; do not edit or reset it.
- Perf worktree `/home/lyk/code/fluidsynth-perf-mixer`: clean branch `perf/mixer-stable-amp` at `d4afd04a`; currently ahead of `upstream/master` by one commit.
- Todo worktree `/home/lyk/code/fluidsynth-worktrees/todo`: branch `todo`, initially clean at `34f8594d`; this file is being added here.
- Existing builds are under `/home/lyk/code/fluidsynth-builds`; create a fresh optimized build for each compared source revision or ensure CMake points at the correct source tree.

## Next actions

1. Push `perf/mixer-stable-amp` to `origin` and commit/push this progress file on `todo`.
2. Refresh `upstream/master`; do not incorporate unrelated upstream changes without checking them.
3. Build a current baseline and refresh function-level `perf record` on representative regular MIDI, then pick candidates outside completed `todo` work.
4. For each candidate, benchmark alternating baseline/candidate runs on a pinned core, check PCM equality (or quantify float differences), reject noisy or regressive results, keep production additions below 50 lines, then commit and push that candidate branch.
5. Update this file after every experiment, accepted optimization, push, and any blocker.

## 2026-11-04 cache-line and rvoice investigation

- Pushed the prior accepted branch: `origin/perf/mixer-stable-amp` now points to `d4afd04a`.
- Added and pushed this progress file as `todo` commit `004a9e43`.
- Fresh `RelWithDebInfo` build of `d4afd04a` used GCC 16.2.1, double sample type, O2, same CLI/SoundFont settings for comparisons.
- On `/tmp/fluidsynth-conventional.mid`, `perf record -e cycles:u -F 999 -g` with zero lost samples attributed ~34.6% to IIR, ~31.0% to interpolation, ~23.0% to mixing, and ~5.3% to `fluid_rvoice_write`.
- Process-level 128-voice sustain counters: ~13.57B L1D loads / ~262M L1D load misses per render; cache-misses event varied from 4.5M to 10.3M across runs, so treat that event as noisy here.
- Cache-line prefetch experiment on a separate worktree/branch `perf/rvoice-prefetch` (not accepted): prefetched the next active rvoice's `envlfo` cacheline. Five alternating baseline/candidate runs gave cycles medians 14.616B vs 14.574B (~0.3%), identical PCM SHA-256, L1D load misses ~262.1M vs ~262.0M. No convincing cache benefit; discard this uncommitted branch change.
- `perf mem record` fails on this installation with `event '(null)' not supported`. Raw `perf record -e ibs_op// -d` samples successfully, but current `perf` did not resolve useful data addresses or load latency for this run. `perf c2c` targets cross-core cache-line sharing; this render path is single-threaded.
- Current focus: inspect `fluid_adsr_env_calc`/sustain state in `fluid_rvoice_write` for safe repeated work that can be removed under 50 added lines.

## 2026-11-04 ADSR experiment

- Tested a guarded sustain-section fast path in `fluid_adsr_env_calc` on `perf/adsr-sustain-fastpath` (uncommitted experiment, do not keep).
- Output was byte-identical for the 128-voice sustain render, a real Bach MIDI, and the generated conventional multi-track MIDI.
- Five paired high-voice runs: median cycles 14.618B -> 14.554B (~0.44%); instructions 59.161B -> 58.946B (~0.36%).
- Nine paired real-Bach runs: median cycles 5.551B -> 5.549B (~0.03%).
- Three paired conventional multi-track runs: median cycles 80.522B -> 80.465B (~0.07%).
- Reject: the normal MIDI results do not clear noise. Next investigate internal ADSR field layout for L1 locality; no accepted code change yet.

## 2026-11-04 ADSR layout experiment

- Reordered `fluid_adsr_env_t` so `section/count/val` precede the envelope stage array (`perf/adsr-hot-layout`, uncommitted experiment).
- 128-voice sustain render: L1D load misses median 268.74M -> 254.31M (-5.4%), but cycles 14.629B -> 14.644B (+0.1%, regression/noise); PCM identical.
- Real Bach MIDI: L1D misses 150.44M -> 149.97M (-0.3%), cycles 5.532B -> 5.548B (+0.3%, regression/noise); PCM identical.
- Reject: the cache counter change does not improve end-to-end MIDI rendering. `fluid_rvoice_write` remains a secondary ~5.3% hotspot; IIR/interpolation/mixing dominate.

## 2026-11-04 multithread cache-to-cache investigation

- `perf c2c record` works with AMD IBS on this machine when rendering with `synth.cpu-cores=4` and affinity to CPUs 4-7. One 128-voice run captured 26,057 records, 127 local HITM loads. The largest shared line was in pthread mutex lock/unlock code (40 local HITMs); another mixer worker/atomic line had 6. The recording is short and samples were sparse, so treat counts as directional.
- Tried local voice index batches of 4 on `perf/mixer-voice-batches`. A seven-run preliminary `perf stat` showed 2.9% lower aggregate cycles and 0.7% fewer instructions, but repeated renders had varying output lengths and very large PCM differences (thousands of 16-bit units). This is not acceptable.
- Setting batch size to 1 restored the exact baseline PCM hash across three runs, confirming the problem is tied to batched assignment/order. Discard the branch change; do not push it.
- `perf c2c` is the better tool for cross-core cache-line sharing; it identified mutex/scheduler lines. `perf mem` is unusable with this perf build (`event '(null)' not supported`), and raw IBS records did not resolve load addresses/latencies in `perf report`.

## 2026-11-04 10:00-deadline session on Xeon 6140 (second machine)

This section records experiments run on the Xeon Gold 6140 box (6 cores, 2.3 GHz, GCC 13, /root/code paths). It supplements, and does not replace, the Ryzen results above. Every comparison below used `/root/code/fluidsynth-delivery-20261018/archive/research/20261015/assets/MuseScore_General.sf2`, 44.1 kHz, gain 0.5, `synth.cpu-cores=1`, 64-frame interleaved `fluid_synth_write_float` writes, rendering to a memory buffer only, `taskset -c 2`, alternating base/candidate process pairs.

### Profiling refresh (9 ordinary MIDI files, pop + game + piano)

- perf, no children: `fluid_iir_filter_apply` 31.1%, `fluid_rvoice_dsp_interpolate` 27.0%, `fluid_rvoice_buffers_mix` 9.5%, Dattorro reverb 6.6%, `fluid_rvoice_write` 4.1%, chorus 1.9%, `fluid_iir_filter_calc` 1.6%.
- Line-level annotation of the interpolator on POP909 909.mid: int16-to-double conversion 17.4% of the function, the four tap loads 23%, table row 8%, store 6%. A 64-bit division in `compute_interpolation_steps` runs two to four times per 64-sample block.
- Instrumented build counters for 909.mid: 6.05M interpolate calls, 387.4M voice samples (about 70 active voices per block), 70.8M per sample segment. IIR received 387.4M samples; 118.6M of them (30.6%) had an active cutoff/Q ramp; 10.5M coefficient table-row changes.
- Instrumented increment check: no voice sample in 909.mid had an integer phase increment, and only 5.4% of samples in one MIDI Collection file played at exactly 1.0. A sliding-window fast path for unity playback was therefore not pursued.

### Accepted: `perf/interp-buffer-steps` (pushed, `9ed43975`)

- Change: in `compute_interpolation_steps()`, compare the remaining phase distance with the remaining output buffer before dividing. When the buffer ends first, return the buffer remainder and skip the 64-bit division. +10/-1 production lines, no compiler-specific code.
- Paired fast-render runs, 10 pairs each, median render-loop time:
  - POP909 909.mid: 6.695 s -> 6.566 s, paired geomean ratio 0.986 (95% CI 0.975-0.996).
  - MIDI Collection dreamadream.mid: 12.276 s -> 12.127 s, ratio 0.978 (95% CI 0.964-0.993).
- Rendered WAVs byte-identical for both files. Standalone micro benchmark of the function (half the calls taking the new early exit): 10.0 ns -> 4.8 ns per call, stable across five repetitions.
- Not yet covered: other generators/corpora, float/double builds, 32/48/96 kHz, Clang/MSVC. These remain open before an upstream PR.

### Rejected or not pursued in this session

- Mixer steady-amplitude fast path: already implemented and pushed as `perf/mixer-stable-amp` (`d4afd04a`); not duplicated.
- IIR stable-coefficient four-sample block: already recorded as rejected above; not revived.
- Interpolation sum reassociation: already recorded as rejected above; not revived.
- 16-bit tap shift removal by pre-scaling the cubic coefficient table: considered and dropped. It would need a second coefficient table for the 24-bit path and touches the table generator; the expected saving is four integer shifts per sample.

## 2026-11-04 second update (Xeon session): filter coefficients and interpolation tap window

All branches below are one commit on top of the fetched `upstream/master=2e838ae6`; none of them exists on `todo` or in the prior branch list.

### Accepted: `perf/iir-coeff-double` (pushed, `eed962d8`, +12/-2)

- The biquad coefficients are stored as float (`IIR_COEFF_T`) while the history is `fluid_real_t` (double in the default build). The generated code widened both coefficients with `cvtss2sd` on every sample. Converting the four coefficients once into loop-local `fluid_real_t` copies and refreshing them after each recalculated row removes those conversions.
- Safety: the float-to-double conversion is exact, the stored and recalculated values remain float, and every arithmetic operand is the same value as before, so the result is bit-identical. Rendered WAV is byte-identical.
- Paired runs, POP909 909.mid, 10 pairs: 6.654 s -> 6.404 s, geomean ratio 0.959 (95% CI 0.942-0.975). Profile share of `fluid_iir_filter_apply` fell from 31.1% to 29.0% on the nine-file set.

### Accepted: `perf/interp-tap-window` (pushed, `e74f2b08`, +32/-10)

- The fourth-order interpolator loaded and converted four taps on every output sample. When the phase index advances by exactly one sample, the next window is the previous one shifted by one, so only the newest tap needs loading. The tap values, coefficient row and multiply-add order are unchanged, so the output is bit-identical (verified by byte-identical WAVs).
- Instrumented phase increments: 30-59% of voice samples run at 0.99-1.01 samples per output sample, 33-59% at 1.01-1.25, the rest below 0.99; no larger increments appeared.
- Paired runs: 909.mid, 8 pairs, 6.505 s -> 6.371 s (0.964, CI 0.942-0.990); re-measured after reverting a rejected variant, 6 pairs, 6.665 s -> 6.405 s (0.963, CI 0.944-0.992). dreamadream.mid, 5 pairs, 12.844 s -> 12.262 s (0.978, CI 0.952-1.007).

### Rejected in this session

- Window shift for a two-sample phase advance (`adv == 2`): 909.mid, 8 pairs, ratio 0.992 (CI 0.970-1.016), i.e. the extra branch costs more than the two taps it saves. Reverted; the pushed branch only specialises `adv == 1`.
- Removing the IIR amplitude-ramp add chain entirely (forced `dsp_amp_incr = 0`, an upper bound that changes audio): 909.mid, 5 pairs, ratio about 0.99. The ramp chain is not the IIR bottleneck, so no steady-amplitude IIR specialisation was written.
- Reconsidered and dropped without benchmarking: 16-bit tap shift removal by pre-scaling the cubic coefficient table (needs a second table for the 24-bit path); sliding-window fast path for unity playback (does not exist in the measured corpus).

### Accepted: `perf/reverb-delay-size` (pushed, `d91699c9`, +7/-1)

- `fluid_reverb_delay_line::size()` recomputed the vector bounds on every delay and tap access; the Dattorro loop performs dozens per sample. The length is now stored in `set_buffer()` and returned by `size()`. The buffer is only resized there, so the value is identical and the output is byte-identical.
- Reverb-only workload (5-minute silent MIDI, 12 pairs of `perf stat` cycles, taskset -c 2): 5.966B -> 5.842B, geomean ratio 0.984 (95% CI 0.972-0.994). Full 909.mid render, 6 pairs: 0.980 (CI 0.953-1.002), median 6.623 s -> 6.598 s; the box was shared during these runs, so the full-render number is weaker.

### Combined effect of the four branches (scratch worktree, not pushed)

Cherry-picked `bd6c7ec2`, `eed962d8`, `e74f2b08`, `d91699c9` onto `upstream/master` (no conflicts) and compared against plain upstream on 909.mid:

| Build | Retired instructions | Wall-clock median (6 pairs) |
|---|---:|---:|
| upstream | 40.028B | 6.772 s |
| +`interp-buffer-steps` | 39.997B | - |
| +`iir-coeff-double` | 38.120B | - |
| +`interp-tap-window` | 39.444B | - |
| +`reverb-delay-size` | 40.020B | - |
| all four | 37.514B (-6.3%) | 6.387 s (ratio 0.944, CI 0.919-0.968) |

Instruction counts are deterministic and were taken once per build; wall-clock and cycle counters on this shared machine fluctuate by 2-3% between runs, so the combined wall-clock result (5.6%) is the weakest of these numbers. WAV output of the combined build is byte-identical to upstream for 909.mid.

### Boundary fix in `perf/interp-tap-window` (amended to `486e327a`)

The first version refreshed the tap window after every iteration, including the last one of a segment. That last refresh reads `dsp_phase_index + 2`, which can already lie past the loop or sample end when the phase advanced beyond the segment end.

Evidence, Debug build with `-fsanitize=address,undefined`:

- Without the `if(safe_count)` guard, `test/test_rvoice_dsp_interpolate` aborts with `AddressSanitizer: stack-buffer-overflow ... in fluid_rvoice_get_sample16` (64-sample fixture, offset 272 over the `data` array) in the integer-phase test.
- With the guard, the whole test passes, including the short-loop, end-of-sample, high-playback-rate and phase-ramp cases.

Re-measured with the guard, POP909 909.mid, 6 pairs: 6.386 s / 6.726 s, geomean ratio 0.954 (95% CI 0.943-0.965); PCM still byte-identical.

### Amendment in `perf/reverb-delay-size` (amended to `e89c8734`)

`gcc -fanalyzer` on the modified files reported `-Wreorder` for `fluid_reverb_delay_line` because the new `length` member was declared between `line_in` and `line_out` while the constructor initializes it after `line_out`. The member was moved after `line_out`; no warning, PCM unchanged.

### New branch: `perf/reverb-idle-skip` (`0de60265`, +36/-0)

`fluid_rvoice_mixer_process_fx()` called the reverb for every block. When no voice sends to the reverb (MIDI without CC91 or SF2 reverb

[Exact evidence: /root/.pi/agent/sessions/--root-code-fluidsynth--/pi-fold-evidence/pi-session/sha256/c1/c12bda6f1bedfa781826d0bb8bd093201a5017bc5a3b332f52e6f8f593be5a9c.txt; sha256=c12bda6f1bedfa781826d0bb8bd093201a5017bc5a3b332f52e6f8f593be5a9c; bytes=18517]
### Float-level verification of every branch (correction)

Earlier checks compared 16-bit WAVs, which quantize away last-bit differences. Re-running every branch with `fluid_synth_write_float` (raw float32 output, 909.mid, fourth-order interpolation, 10,960,768 samples) separates them:

| Branch | Raw-float difference vs upstream |
|---|---|
| `perf/interp-buffer-steps` | none |
| `perf/interp-tap-window` | none |
| `perf/reverb-delay-size` | none |
| `perf/reverb-idle-skip` | none |
| `perf/iir-coeff-double` | 3 samples, max relative 1.1e-7 (1 float ULP) |
| `perf/iir-steady-loop` | 3 samples, same positions and magnitude |

Both IIR branches move work out of the per-sample loop, and `fluid_iir_filter_impl.cpp` is compiled with `-ffast-math`, so GCC reassociates the biquad expression differently once the loop shape changes. The result is mathematically equivalent but no longer bit-identical at the double level; the 16-bit WAVs remain byte-identical. The two branch descriptions now state this instead of claiming byte-identical output.

With all six changes plus `perf/mixer-stable-amp`, `perf/sinc-phase-table-rebased` and `perf/sinc-kernel-fused-taps` stacked, the raw-float differences against upstream are: 909.mid 3/10.96M (max rel 1.1e-7), ugly.mid 17/17.37M (max rel 1.8e-7), MAESTRO 2013 2/27.72M (max rel 1.2e-7). Sinc output is byte-identical to the phase-table branch for all three orders.

### Stacked build (nine changes)

`perf/interp-buffer-steps`, `perf/iir-coeff-double`, `perf/interp-tap-window`, `perf/reverb-delay-size`, `perf/reverb-idle-skip`, `perf/iir-steady-loop`, `perf/mixer-stable-amp`, `perf/sinc-phase-table-rebased`, `perf/sinc-kernel-fused-taps`. All cherry-pick onto current master without conflicts.

909.mid, MuseScore General, 44.1 kHz, taskset -c 2, single runs:

| Interpolation | upstream | stacked | speed-up |
|---|---:|---:|---:|
| 4th order (default) | 6.99 s | 6.02 s | 1.16x |
| sinc MID (11 taps) | 215.15 s | 14.37 s | 15.0x |
| sinc HIGH (17 taps) | 335.20 s | 21.05 s | 15.9x |
| sinc HIGHEST (25 taps) | - | 27.14 s | - |
