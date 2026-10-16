# Constant-gain voice-buffer mixing

**Decision:** rejected; the apparent gain varied with request size and timing ranges crossed baseline.

A direct multiply-add path was screened on `All The Small Things` with effects enabled, one core, and three AB/BA pairs per request size. PCM matched exactly. At 64 frames, candidate/base wall ratios were 1.0254, 0.9626, and 0.9476 (median 0.9626, 3.74% faster). At 8192 frames they were 0.9857, 0.9944, and 1.0181 (median 0.9944, 0.56% faster). Since both ranges cross 1.0 and the larger-block gain is negligible, no source patch was retained. The two screen summaries are included; no exact candidate patch was saved for this trial.
