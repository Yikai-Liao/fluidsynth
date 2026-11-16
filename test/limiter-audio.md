# Real-time limiter evidence

Run `limiter_audio.py` to play known signals through the production final mixer,
change limiter settings on a control thread, record the audio service's output,
and produce listening files, an interactive waveform report and raw data.
Python needs only its standard library. Linux automatic recording uses `pactl`
and `parec`. The probe requires a build with Signalsmith support.

## Independent expectations

The driver callback's captured samples are **rendered output**, not a correctness
reference. Comparing those samples with a monitor recording checks transport
only. Both recordings can contain the same DSP error.

The independent oracle in `limiter_oracle.py` instead starts with specified input:

```
t = sample_index / sample_rate, g = 0.3
multitone L = g * (0.20 sin(2π223t) + 0.10 sin(2π617t))
multitone R = g * (0.18 sin(2π331t) + 0.12 sin(2π997t))
sine L = 0.20 g sin(2π500t), R = 0.15 g sin(2π500t)
DC L = 2g, R = -g
```

The C++ probe generates the input before the real final mixer processing stage.
Python independently computes its expected output; it neither calls Signalsmith
nor copies rendered samples to build that expectation. The test seam includes
the unchanged mixer source, like `test_synth_limiter`; it does not replace the
limiter. Public `write_float`, `process`, and s16 integration is checked separately
by `check_public_rendering` in `test_synth_limiter.cpp` at 44.1, 48, and 96 kHz,
including requests of 1, 7, 63, 64, 65, 127, 257, and 509 frames.

| Scenario | Independent assertion |
| --- | --- |
| `static`, multitone/sine | Input delayed by `ceil(5 ms × sample_rate)`. |
| `controls`, multitone/sine | The same delayed input, throughout every update. Input peaks are at most 0.09 and every threshold is at least 0.12, so changing gain-envelope settings must not change any sample. |
| `delay`, multitone/sine | Stable samples match the requested delayed input. During each isolated change, both channels must share one bounded, monotone mixing coefficient with a 20 ms linear slope between the two known delayed inputs. Adoption time is bounded using setter frame ranges, callback buffering and lookahead. |
| `controls`/`delay`, DC | Settled output has the algebraic gain implied by threshold and channel link. For input `(0.6, -0.3)`, limit 0.12 gives `(0.12, -0.06)` with full linking and `(0.12, -0.12)` without linking. Envelope transients are excluded from this steady-state equation. |
| `rapid`, multitone/sine | Initial identity and final convergence after requests stop. Exact intermediate coalesced request order is not modeled or claimed verified. |

Analytic sample tolerance is 2e-7. The report gives actual waveform coverage;
unchecked regions appear gray. The first and last 0.5 seconds are excluded from
comparison. `expected.wav` is an exact delayed reference for static/controls
tones; in other scenarios it contains stationary targets in checked regions,
not a reference for transition timing or unchecked samples.

The analyzer's negative tests alter *both* rendered output and monitor capture
with wrong gain, delay, limit and fade duration. The transport check still
passes, while the independent limiter check must fail. Other cases inject
silence, deleted/repeated frames, a one-sample click, nonfinite output, and an
exceeded callback budget:

```sh
python3 test/limiter_audio_test.py
```

## Listen and measure on Linux

From the repository root, using the existing build:

```sh
python3 test/limiter_audio.py record --driver pipewire --mode controls \
  --output build/scratch/limiter-audio/controls
```

It plays a quiet continuous stereo tone while changing settings. Each output
directory must be new. Open its `report.html`, or listen to `audio-rendered.wav`
and `capture.wav`. Use `expected.wav` for an independent listening reference in
the static/controls tone cases. The raw float32 files retain full precision;
16-bit listening copies can differ by quantization and are clipped to ±1.

Run the complete set **sequentially**, so recordings do not contaminate one
another. Use a fresh synth for every run:

```sh
python3 test/limiter_audio.py record --driver pipewire --mode static --output build/scratch/limiter-audio/pw-static
python3 test/limiter_audio.py record --driver pipewire --mode controls --output build/scratch/limiter-audio/pw-controls
python3 test/limiter_audio.py record --driver pipewire --mode delay --output build/scratch/limiter-audio/pw-delay
python3 test/limiter_audio.py record --driver pipewire --mode rapid --period 64 --output build/scratch/limiter-audio/pw-rapid
python3 test/limiter_audio.py record --driver pipewire --mode controls --source dc --output build/scratch/limiter-audio/pw-dc
```

Repeat with `--driver pulseaudio`. `--driver alsa` also works if its configured
default PCM routes to the service being monitored; an ALSA device that bypasses
that service will not appear in its monitor. Record the actual route. On a
PipeWire desktop, these can be different client APIs into the *same* server;
that is not evidence for an independent PulseAudio server or direct ALSA device.

The automatic recorder reads the default sink's monitor. Select another with
`--monitor SOURCE_NAME`. Use `pactl list short sources` to find it. Other audio
mixed into that sink changes the captured waveform and should fail comparison.
The script does not change system volume, default devices, graph rate or quantum.
Keep sample rates equal to avoid intentional resampling confounding a strict
sample comparison. `--rate` is a client request, not a forced server rate.

Try supported periods 64, 128, 256 and 512, then longer runs with `--seconds 60`
or `--seconds 300`, at idle and under representative load. The callback CSV
records actual request sizes. Memory use grows with duration and sample rate
because audio and timing storage are allocated and touched before playback.

## Examine delay-change coloration

```sh
python3 test/limiter_audio.py record --driver pipewire --mode delay --source sine \
  --output build/scratch/limiter-audio/phase
```

A 500 Hz sine can be opposite in phase at the old and new delays. A linear
crossfade then has an amplitude dip even if the implementation follows its
contract perfectly. The report measures fade energy relative to the weighted
energy of the two branches, including the lowest 1 ms window. A passing contract
check is **not** a claim of amplitude transparency. Eliminating that artifact
would require a separate transition-policy decision.

## Windows and macOS loopback

Build the same probe against the target platform's libraries:

```sh
cmake --build build --target limiter_audio_probe --config Release
```

With a multi-configuration generator, use `--probe build/test/limiter_audio_probe.exe`
on Windows (omit `.exe` on macOS) to use that exact executable without rebuilding.
Make its matching dependency DLLs available as for the normal FluidSynth CLI.

On Windows, select the WASAPI host and the playback endpoint's **loopback** input
in Audacity, stereo, 48 kHz, and start recording before the probe. This records
the audio engine's output rather than a microphone. See the
[Audacity procedure](https://support.audacityteam.org/basics/recording-desktop-audio)
and [Microsoft's loopback contract](https://learn.microsoft.com/en-us/windows/win32/coreaudio/loopback-recording).
This method uses shared mode; WASAPI loopback does not cover exclusive streams.

```sh
python test/limiter_audio.py record --driver wasapi --no-capture --mode controls \
  --probe build/test/limiter_audio_probe.exe \
  --output build/scratch/limiter-audio/windows-controls
```

Stop recording after playback ends. Export the **whole recording**, including
its leading silence, as stereo PCM16/24/32 WAV at the requested rate; do not
normalize, trim gaps, suppress silence or add effects. The script reads integer
PCM WAV, not IEEE-float WAV. Then:

```sh
python test/limiter_audio.py analyze \
  --output build/scratch/limiter-audio/windows-controls --capture loopback.wav
```

On macOS use `--driver coreaudio --no-capture`, with a loopback recorder or an
existing virtual audio device that records the same CoreAudio output endpoint.
Keep channel order and rate unchanged. The same `analyze --capture` command
applies. `--no-capture` still performs the independent limiter check immediately
and writes `limiter-analysis.json`; adding loopback later verifies transport.
These platform procedures need execution on those platforms before claiming
coverage; a Linux build does not validate them.

For an external command-line recorder, `--capture-command` accepts argv with
`{rate}` and optional `{output}` placeholders, without a shell. It must output
stereo float32 little-endian PCM to that file or stdout. The script starts and
stops its child processes, including on errors or interruption.

## Read the evidence

`analysis.json` separates independent DSP assertions from monitor transport and
timing assertions. Transport uses one global offset and one fixed gain per
channel; per-second offset estimates are diagnostic and never correct the audio
to hide dropped/repeated frames. Default transport thresholds are SNR ≥60 dB and
maximum absolute error ≤0.002. An exact match has zero error and `snr_db: null`.

`audio-callbacks.csv` includes render duration, render plus capture-copy duration,
and callback intervals. Render duration includes analytic input generation. It
excludes the backend's later conversion/submission work. A callback can fit its
frame budget and still be scheduled late; bursts can also be normal backend
buffering. Inspect driver logs and the optional `pw-top.log` ERR counters along
with waveform differences. Sanitizer runs diagnose correctness, not real-time
performance.

`run.json` records the revision, tracked-diff hash, probe and tool-source hashes,
build options and recorder command. `driver.log`, service information, graph
observations and raw float32 files support reproduction. Preserve the full
directory for a reported failure. Remove it after review; no permanent server,
device reconfiguration or recording process is needed.

For upstream review, submit the input specification and assertions, negative
tests, exact reproduction command, raw evidence, and a table of platforms that
were actually run. Test finite runs and configurations; do not turn those
observations into a guarantee for every driver, scheduler or load.
