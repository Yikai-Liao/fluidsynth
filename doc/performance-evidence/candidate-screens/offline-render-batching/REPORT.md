# Offline file-render batch sizing

**Decision:** no source patch. The CLI already exposes the request size through `-z`; increasing its default would change exported file duration.

The actual `-F` file renderer was measured on three symusic MIDI excerpts, Release double, 44.1 kHz, effects enabled, raw S16 stereo. One sequential run per size per MIDI compared `-z 64` with `-z 8192`. The larger request reduced wall time by 15.6–21.4% and CPU time similarly, with essentially unchanged peak RSS. The common PCM prefix was bit-identical. It appended 1,920–3,904 near-silent frames (43.5–88.5 ms; maximum signed-16 magnitude 1). These are directional single runs, not a paired confidence estimate. Existing CLI control means no optimization patch is justified.
