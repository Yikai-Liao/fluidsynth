# Interpolation fixed coefficient-row reuse

**Decision:** rejected; source prototype is retained in this experiment branch for inspection, but the end-to-end gain was too small and mixed to justify it.

The candidate specializes the 4th-order interpolator when the 32.32 phase increment has no fractional bits, then reuses a coefficient-row pointer. Instrumentation found 20.333% same-row samples in one 160-second trace, but that is not a guaranteed hit rate. Double and float Release suites each passed 41/41 and PCM matched exactly. Across five paired repetitions, the median improvement across six workload/block case medians was 0.39% in double and 0.44% in float; POP909 regressed, and timing ranges crossed baseline. The prototype was rejected and is not a validated optimization.
