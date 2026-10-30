# Sostenuto: released old voice ID inherited in a later pedal cycle

Regression fixture: `sostenuto-two-cycle-real-derived.mid` (94 bytes).

This is a **real-derived regression excerpt**, not the complete untouched musical file. It contains just two repetitions of MIDI key47 on channel5 (zero-based), lasting3.291280125seconds. It was extracted from Lakh Clean's `The Beatles/Two of Us.1.mid`; the complete source independently reproduced the problem before extraction. The original song is not included.

The two note pairs and four pedal messages are copied from actual source events; their relative tick times and ordering are unchanged. Unrelated notes/events are omitted. Original bank/program/mix controller values and tempo are copied to tick0 as documented setup. No pedal or note event was invented. The extraction manifest identifies every original tick, track and event ordinal and the full source SHA256.

Sequence: first NoteOn → CC66down → first NoteOff → CC66up → CC66down → second NoteOn → second NoteOff → CC66up. The second NoteOn is only11.467875ms after the preceding pedal release. It starts AFTER the new pedal-down, so it must not be held by that pedal cycle.

## Verified result
Pristine FluidSynth fc1c55f, FLOAT,44100Hz,default Poly and synth.note-cut0,polyphony256,one rendering thread; native MIDI player. Ordinary GeneralUser GS and TimGM6mb SoundFonts both reproduce it. The source's bank32/program24 uses normal bank0/program24 fallback when bank32 is unavailable.

Original: second voice inherits released old id0; second NoteOff leaves it sostenuto-held. Diagnostic one-condition fix (`fluid_voice_is_sostenuto(voice) && !voice->has_noteoff`): new id1; second NoteOff enters release normally. Both arms have peak2voices and zero event-handler errors. `comparison.json` records PCM comparisons for identical25second rendering (the MIDI events themselves end at3.291seconds; remaining time is tail).

There are no repeated high CC66 messages. This is distinct from F46's redundant-pedal-high issue. Full-library candidate tests passed45/45; this fixture is additional real-derived evidence, not a claim that the tiny patch is already upstream.

## Provenance
Dataset: Colin Raffel, Lakh MIDI Dataset v0.1 Clean subset, https://colinraffel.com/projects/lmd/ . Dataset authors declare CC BY4.0, but this does not independently establish rights to every underlying song/transcription. This short single-pitch diagnostic selection does not carry a newly asserted license over the original musical work. The full song is deliberately excluded; publication scope is reviewed separately.
