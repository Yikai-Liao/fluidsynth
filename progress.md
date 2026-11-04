# FluidSynth MIDI performance work
Updated: 2026-11-04 11:40 Asia/Singapore (UTC+08:00)

## Goal

Continue measured performance optimization for regular MIDI rendering until 2026-11-04 18:00 Beijing time. Each accepted optimization must live on its own branch and be pushed to `origin`. Production changes must add fewer than 50 lines. Keep the original `/home/lyk/code/fluidsynth` worktree untouched because its `TODO` has unrelated uncommitted edits.

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

### New branch: `perf/reverb-idle-skip` (`773935f6`, +82/-0 with test)

`fluid_rvoice_mixer_process_fx()` called the reverb for every block even when no voice sent to it. When the reverb has only been fed zeros since it was created or reset, its delay lines and filters still hold zeros, so processing a zero block writes zeros and only moves ring positions. The mixer now skips `processmix`/`processreplace` for such blocks.

The decision is delegated to the engine: `_fluid_revmodel_t` gained a virtual `can_skip_silence()` that defaults to false, and only the Dattorro engine answers true (fixed delay lengths, no oscillator, no DC offset). The FDN and Signalsmith engines advance modulator phases while processing and the Freeverb engine adds a DC offset to its input, so they keep running and the output stays bit-identical for every engine. `fluid_revmodel_can_skip_silence()` exposes the answer to the C mixer, and `test/test_reverb_silence_skip.c` compares a skipped run against an explicit zero-input run for both Dattorro and FDN.

Measured on reverb-only renders (no voice sends to the reverb), 6 alternating pairs, geomean ratio 0.74 / 0.77. On the full corpus: neverending -13.3 %, class14 -3.7 %, 909.mid -1.2 %; files where voices do send to the reverb are unchanged.

Review fixes applied after the first version: the capability check (so engines with moving state cannot be skipped), the `FLUID_ASSERT`-style comment accuracy, and reordering `length` in `fluid_reverb_delay_line` for `-Wreorder`.

### New branch: `perf/chorus-idle-skip` (`35443f4d`, +182/-7 with tests)

Same idea for the chorus. Upstream already avoids the full `fluid_chorus_processmix()` on silent input, but the fallback `fluid_chorus_processsilence()` still runs the whole per-sample modulator and delay-line update loop. While the chorus has never been fed a non-zero block, only its modulator state advances; the delay line stays zero. The new `fluid_chorus_processsilence()` advances exactly the same state (index rate, modulators, oscillator phases, line positions, center position) but skips the delay-line reads/writes and the output mixing, so a later non-silent block is processed as if nothing had been skipped. `test/test_chorus_silence_skip.c` compares the skipped path against feeding explicit zeros.

Measured on chorus-free files: MAESTRO 2013 -3.6 %, 909.mid -2.0 %; files that use the chorus are unchanged (bit-identical).

### `perf/builtin-atomics` — withdrawn

An earlier branch replaced `fluid_locked_atomics.h` with a `__atomic`-builtin implementation. Withdrawn on user instruction: no compiler-specific syntax. The team already has `perf/mixer-std-atomic` (compile the rvoice mixer as C++, use `std::atomic` for mixer work scheduling), which is pending upstream, so that is the path for the mixer threading fix. The branch is deleted both locally and on origin; the numbers below no longer include it.

The measurements made with it are kept only as evidence for the follow-up work:
- multithreaded rendering was slower than single-threaded before it (ugly.mid: 8.86 s at one core, 10.37 s at four cores) because `fluid_locked_atomics.h` serialised every mixer worker on one global `std::mutex`.
- with it, 909.mid with 11-tap sinc interpolation scaled 14.43 s / 9.54 s / 6.70 s at 1 / 2 / 4 cores.
- once workers really run in parallel, per-worker destination buffer zeroing becomes the next cost: with `fluid_mixer_buffers_zero()` compiled out (timing only, output invalid) ugly.mid at four cores went 8.12 / 7.67 / 7.50 s -> 6.36 / 6.35 / 6.01 s. `fluid_mixer_buffers_zero()` already carries an upstream TODO about zeroing only the buffers that are used.

Multithreaded output is not byte-identical and never was: worker threads take voices from `mixer->current_rvoice` with an atomic fetch-and-add, so which voices land in which worker buffer depends on timing and the per-worker partial mixes are summed in a different grouping each run. Repeated `synth.cpu-cores=4` renders of ugly.mid differ from each other and from a single-threaded render in 0-24 % of the samples, by at most 1.2e-7 (about 2 float ULP at full scale). Unmodified master shows the same effect, just less often, because the global mutex used to serialise the workers. Single-threaded renders are unaffected.

### Final stack (eleven commits)

`combined` worktree: `2e838ae6` plus `perf/interp-buffer-steps`, `perf/iir-coeff-double`, `perf/iir-steady-loop`, `perf/interp-tap-window`, `perf/reverb-delay-size`, `perf/reverb-idle-skip`, `perf/chorus-idle-skip`, `perf/sinc-phase-table-rebased` + `perf/sinc-paired-samples` + `perf/sinc-kernel-fused-taps`.

Retired instructions, upstream -> stack (CLI fast render, 44.1 kHz, gain 0.5, MuseScore_General, `taskset -c 2`):

| file | upstream | stack | ratio |
|---|---:|---:|---:|
| neverending | 68.41B | 61.84B | 0.904 |
| ugly | 48.10B | 43.49B | 0.904 |
| class14 | 63.05B | 57.58B | 0.913 |
| paul-simon call | 30.71B | 28.01B | 0.912 |
| dreamadream | 74.47B | 66.22B | 0.889 |
| 8076fe75 | 13.56B | 12.47B | 0.920 |
| MAESTRO 2014 | 114.13B | 92.51B | 0.811 |
| MAESTRO 2013 | 63.98B | 51.68B | 0.808 |
| MAESTRO 2015 | 59.30B | 50.21B | 0.847 |

Geometric mean 0.878 (-12.2 %).

Wall clock, same runs:

| file | upstream | stack | ratio |
|---|---:|---:|---:|
| neverending | 12.45 s | 11.89 s | 0.955 |
| ugly | 8.77 s | 8.05 s | 0.918 |
| class14 | 10.78 s | 10.37 s | 0.962 |
| paul-simon call | 5.59 s | 5.59 s | 1.000 |
| dreamadream | 13.34 s | 12.71 s | 0.953 |
| 8076fe75 | 2.65 s | 2.50 s | 0.943 |
| MAESTRO 2014 | 20.41 s | 17.14 s | 0.840 |
| MAESTRO 2013 | 11.62 s | 9.60 s | 0.826 |
| MAESTRO 2015 | 10.97 s | 9.23 s | 0.841 |

Geometric mean 0.913 (-8.7 %). Instruction counts are the more reliable metric here; wall clock on this box moves by 1-3 % between runs.

Identity against upstream (float output, interp 4): 909.mid 3 of 10,960,768 samples differ by at most 4.7e-10, MAESTRO 2013 2 of 27.72M, ugly.mid 17 of 17.37M, all from the `-ffast-math` reassociation enabled by the two IIR changes. Sinc output is byte-identical.

### Other configurations

| sample rate | differing samples | max abs |
|---|---:|---:|
| 44.1 kHz | 3 / 10,960,768 | 4.7e-10 |
| 48 kHz | 1 / 11,929,856 | 1.7e-18 |
| 96 kHz | 8 / 23,859,200 | 2.3e-10 |

FDN reverb (`synth.reverb.engine=fdn`) on 909.mid: 7.30 s -> 5.97 s (-18.2 %). The output differs in 99.99 % of the samples by at most 4.0e-8, because a one-ULP difference recurses through the feedback network; it does not grow over the file (quarters: 4.0e-8 / 1.5e-8 / 1.5e-8 / 1.5e-8).

TimGM6mb.sf2 on 909.mid: byte-identical, 1.85 s -> 1.69 s (-8.7 %).

### ASan

Full stack rebuilt with AddressSanitizer, 909.mid at `synth.cpu-cores=1` and `=4`: no reports with `alloc_dealloc_mismatch` suppressed. The suppression is needed because upstream aborts first:

```
ERROR: AddressSanitizer: alloc-dealloc-mismatch (operator new vs free)
    #1 fluid_free             src/utils/fluid_sys.c:329
    #2 fluid_thread_high_prio src/utils/fluid_sys.c:987
    #3 thread_wrapper         src/utils/fluid_sys_cpp11.cpp:50
```

`new_fluid_thread()` allocates `fluid_thread_info_t` with `new` and `fluid_thread_high_prio()` releases it with `FLUID_FREE`. Unchanged upstream code (verified against `2e838ae6`); it only fires when a high-priority thread is created, i.e. with `synth.cpu-cores > 1`. Not touched here, but worth a separate fix.

### Build option: float vs double samples

`fluid_real_t` is `double` unless `-Denable-floats=1` is given (`fluidsynth_priv.h`, `CMakeLists.txt:458`). Same stack, that option:

| file | double (default) | float |
|---|---:|---:|
| MAESTRO 2013 ORIG-MIDI_03 | 9.20 / 8.64 / 8.97 s | 8.07 / 8.31 / 8.08 s |
| 909.mid | 5.79 / 5.83 / 5.85 s | 5.16 / 5.17 / 5.10 s |

Geomean 0.90 and 0.89. Limited because both hot loops are latency bound (IIR feedback chain, cubic coefficient chain), not SIMD-throughput bound. Recorded as a data point; the default build stays double. `make check` also passes 51/51 with `enable-floats=1`.

### Clean-build verification

Fresh configure and build of the eleven-commit stack in an empty directory, default double samples, multithread rendering enabled: build succeeds with no warnings, `make check` passes 51/51, and the 909.mid render matches the reference as listed above. Running `ctest` without `make check` first fails `VintageDreamsWaves-v2.sf3_dump_test` because `test/utils/dump_sfont` is only built by the `check` target.

### Overlap with the branches owned by the std::atomic / IIR work

- `perf/mixer-std-atomic` renames `src/rvoice/fluid_rvoice_mixer.c` to `.cpp` and adds `static_cast`s in `fluid_rvoice_mixer_process_fx()`. `perf/reverb-idle-skip` and `perf/chorus-idle-skip` insert code in that same function, so they need a rebase after it lands. No logical conflict.
- `perf/iir-stable-block` rewrites the same region of `fluid_iir_filter_apply_samples()` that `perf/iir-steady-loop` touches, with a four-sample recurrence expansion. Both are kept on user instruction; only one should be merged.
- `perf/iir-coeff-double` only changes how the coefficients are held inside the sample loop, which does not overlap `fluid_iir_filter_calculate_coefficients()` or `fluid_iir_filter_apply_local()` from `perf/iir-cutoff-coefficient-reuse`.
- No overlap: `fluid_rvoice_dsp.cpp` interpolation branches, `fluid_rev_filters.h`, the reverb/chorus engine files, and the two sinc branches.
