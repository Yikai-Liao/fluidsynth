# LFO frequency update evidence for #1310

These figures use measured FluidSynth LFO states, not generated triangle waves.
They compare `f784f7833acbd07181ea3d3bd6b0d16253b883f4` with the same production
sources plus `fix.patch`. The original [#1310 SoundFont](https://github.com/FluidSynth/fluidsynth/files/14774802/sample-LFO-freq.zip)
is loaded unchanged. Its SHA256 and the capture-binary hashes are in
`data/capture.json`; source/build provenance is in `provenance.json`.

Both runs use 48 kHz, 64-frame blocks, key 60, velocity 126, and disabled effects.
The input sequences are controlled reproductions, not naturally occurring MIDI
performances. `dense` sends CC1=21 through 127 at 20 ms intervals starting at
2 s, after an initial CC1=20. `sparse` switches 20→127→20→127→20 at 1, 1.5,
2.5 and 4.5 s. Each before/after pair receives identical input.

Run from this directory to regenerate the English figures from the committed
compressed CSV files:

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
.venv/bin/python plot.py
```

The outputs are `output/dense.png` and `output/sparse.png`, with editable PDF/SVG
copies. The CC1 row shows the actual controller trajectory. Every short mark
above an LFO trace is an actual recorded event; the right column enlarges the
highlighted window. All samples are retained and connected without smoothing
or downsampling. See `FIGURE_NOTES.md` for the display and validation contract.
`plot.py --qa-tools PATH` optionally runs the panel-alignment audit available
in that directory; ordinary plotting requires only numpy and matplotlib.

To recapture, prepare two source checkouts at the baseline above and apply
`fix.patch` only to the second checkout (`git apply --unidiff-zero /path/to/fix.patch`).
The patch has zero context and is intended for that exact baseline.
Build the diagnostic executable
against each source tree. For example:

```sh
cmake -S . -B build-before -DFLUIDSYNTH_SOURCE_DIR=/path/to/before \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -Denable-signalsmith=OFF \
  -Denable-native-dls=OFF -Denable-openmp=OFF
cmake --build build-before --target lfo_trace -j4
cmake -S . -B build-after -DFLUIDSYNTH_SOURCE_DIR=/path/to/after \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -Denable-signalsmith=OFF \
  -Denable-native-dls=OFF -Denable-openmp=OFF
cmake --build build-after --target lfo_trace -j4
python3 capture.py --before build-before/lfo_trace --after build-after/lfo_trace \
  --soundfont /path/to/sample-LFO-freq.sf2 --output-dir output/captured
.venv/bin/python plot.py --data-dir output/captured
.venv/bin/python analyze.py --data-dir output/captured --output output/metrics.json
```

`trace_lfo.c` renders through the public API and reads the vibrato LFO between
synchronous offline render calls. `time` is the start of the rendered block;
`val_before` is the previous state and `val` is the state used in that block.
`event=1` means the CC was sent immediately before rendering it. The executable
also saves left-channel float PCM beside each CSV. Generated images, builds,
logs and PCM remain outside the commit.

Sparse changes expose the same wrong-direction update without suppressing
the full oscillation. At 1.5 s, the old code changes a descending value from
0.6137 to 0.6460; the fixed code continues to 0.5813. The fixed LFO still has a
slope change because its requested frequency changes abruptly.

The original preset also has the default CC1-to-vibrato-depth mapping. In the
fixed run at 1.5 s, the pitch-control contribution changes by −26.55 cents;
−25.65 cents comes from changing depth at the existing LFO position. The extra
`sparse-fixed-depth` control removes only that default mapping through the
public API, keeps the same LFO trajectory, and measures −0.65 cents of normal
one-block motion at that event. These are measured control-state quantities,
not listening scores or acoustic pitch estimates. The direction repair does
not add controller smoothing or make large depth changes gradual.
