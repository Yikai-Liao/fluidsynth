# SoundFont note-on zone index: early stop

**Decision:** stop before a prototype. The supplied dense-event profile bounds all preset note-on work to a small share of the render, and range checks have stateful behavior that an index cannot simply skip.

## Evidence

The 20,000-note real-player profile rendered one GM preset from MuseScore General SF2. `fluid_defpreset_noteon()` accounted for 1.48% of whole-process CPU samples; that includes zone lookup, voice allocation, generator setup, and modulation, so zone scanning is only a subset. This workload was deliberately dense in note events. No large multi-zone SoundFont hotspot was established.

`fluid_defpreset_noteon()` walks preset zones and, for each matching preset zone, scans instrument voice zones and checks key/velocity ranges. `fluid_zone_inside_range()` also reads and clears each range's mono-legato `ignore` flag before returning a match. An index that jumps over ranges would need to preserve those side effects, as well as tuned-key selection and the original zone ordering.

## Decision and limit

Even eliminating the full observed note-on function would cap the gain at 1.48% on the measured stream; a zone index would remove only part of that cost and add construction, lookup, and state-maintenance work. Stop under the early-screen rule. This does not establish a bound for every unusually large or layered SoundFont, but no such workload was supplied or profiled. No source edit, benchmark expansion, or performance candidate is retained.

Evidence: `../modulator/RESEARCH.md`, the dense MIDI/player profile and its input hashes.
