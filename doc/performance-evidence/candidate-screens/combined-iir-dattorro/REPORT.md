# Combined IIR coefficient reuse + Dattorro tap addressing

**Decision:** combined candidate retained as a measurement branch. It is not a separate optimization from its two component branches and must not be used to add their individual speedup percentages.

## Method and result

The branch stacks the validated IIR same-bin coefficient reuse and Dattorro tap single-wrap change over voice-reuse base `20913f669dd6220052ad0c68d0d6fe3bebcd7d62`. Release float and double builds (GCC 14.2, OpenMP, one render core) each passed 41/41 enabled tests. Five AB/BA pairs rendered 60 seconds of Maestro_1, POP909_010, and All The Small Things at 44.1 kHz, 64/8192-frame requests, effects on, with the same SF2 and one OpenMP thread.

Every compared PCM hash matched. Across the six case medians, the combined build was 15.0% faster in double and 15.8% faster in float. By input and block size, double improvements were 14.4/15.6% (Maestro), 18.5/19.6% (POP909), and 6.6/6.5% (All The Small Things); float improvements were 15.7/15.9%, 19.5/20.1%, and 8.3/8.6%. CPU-time ratios tracked wall time and peak RSS stayed effectively unchanged.

This is direct combined evidence on the listed workloads, not an additive estimate or a general system-wide claim. Individual source changes and their independent reports remain on `perf/iir-coeff-reuse-20261016` and `perf/dattorro-single-wrap-20261016`.
