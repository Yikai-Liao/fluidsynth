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
