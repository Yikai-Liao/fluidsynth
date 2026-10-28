The two figures compare the same measured FluidSynth control signal before and
after the two-site LFO direction fix. The first uses small updates 20 ms apart;
the second uses large changes 0.5, 1 and 2 s apart. The CC1 row gives both the
controller value and event time; short marks above each LFO trace repeat every
actual CC event. The right column enlarges the highlighted time window.

This is a quantitative grid: the input row defines the intervention, Before
shows the failure, and After tests the repair. English text is restricted to
short titles, row names, time units and one Detail label. Colors are consistent
across figures, and the before/after rows have identical vertical scales.

All rows in each captured CSV are plotted without smoothing or downsampling.
The overview and detail are explicit axis windows, not data deletion. The
windows are dense 1.5–4.7 s / 2.28–2.50 s and sparse 0.6–5.3 s / 1.42–1.68 s.
The dense detail shows sustained updates; the sparse detail contains a large
update while the LFO is descending. The full 6.14 s / 6 s traces are retained.

Time identifies the start of a 64-frame render block. The value is read after
that block and is the LFO state used by the renderer in that block. Lines join
these samples; the plotting script does not generate oscillator values. Input,
depth, delay, ticks and increment magnitudes must match across the paired
traces. Event times are read from the event column, not reconstructed as an
ideal uniform grid. The optional fixed-depth capture removes only the default
CC1-to-vibrato-depth modulator through the public API, leaving the original
SoundFont frequency mapping intact.

Each trace is one deterministic offline render. There are no statistical
replicates, confidence intervals, smoothing filters, hardware measurements or
listening scores. The pitch-control decomposition in metrics.json uses measured
internal state; it is not an acoustic fundamental-frequency estimator.

Python/matplotlib exclusively draws and exports PNG at 300 dpi, PDF with
embedded fonts, and SVG with editable text. DejaVu Sans is supplied by
matplotlib; no external Chinese font or machine-specific path is required.
Current delivery runs the optional panel-alignment gate, source checks,
PDF text-size check and PDF collision audit, and visually inspects both figures.
The unequal-width columns intentionally represent overview and detail; they
retain equal row heights and column edges. This is a PR illustration, with no
journal submission target. PNG preview, 300 dpi and on-screen dimensions are
intentional rather than journal TIFF/600 dpi defaults.
