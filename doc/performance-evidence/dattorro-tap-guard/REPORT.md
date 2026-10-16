# Local review: Dattorro tap validity guard

- **Scope:** removes only the `size <= 0` return in the private Dattorro tap reader. The modulo and negative-index correction are untouched.
- **Lifecycle invariant:** `fluid_revmodel_dattorro` calls `setup()` in its constructor. `setup()` assigns every `tank_delay` and `tank_ap` buffer before a process method can call `fluid_dattorro_read_tap()`. `fluid_dattorro_seconds_to_samples()` returns `max(round(seconds * sample_rate), 1)`. If allocation fails, construction cannot complete and the process path cannot run.
- **Compatibility:** candidate/base float PCM was byte-identical for two 60-second MIDI inputs; candidate/base double PCM was also byte-identical for both. Release test suites passed 41/41 in each precision.
- **Performance:** pooled median across six 60-second pairs was 2.46% lower wall time and 2.49% lower CPU time (wall candidate/base ratios 0.942–1.012). The All The Small Things screen was consistent at 2.49%; POP909_010 had a 1.38% median gain and a wide pair range. Do not generalize beyond these targeted screens.
- **Publication:** source commit `89ec2230120a1f4383e2093b105316202f180a40` is pushed to the user fork branch `perf/dattorro-tap-guard-20261016`. No upstream pull request has been created.


## Fork branch

Source commit: `89ec2230120a1f4383e2093b105316202f180a40` on `perf/dattorro-tap-guard-20261016`. This evidence was added in a follow-up documentation commit on the same fork branch. No upstream pull request has been created.
