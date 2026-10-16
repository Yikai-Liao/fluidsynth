# Reset mixer gains when reusing a voice

Reusing a finished rvoice currently retains its previous `current_amp` mixer gains. The next note's initial gain ramp can therefore depend on which voice slot the allocator selects. For example, sample-clock MIDI playback rendered with 64-frame and 8192-frame API requests can produce different note onsets, even though the note-on callbacks occur at the same synth ticks.

Reset the buffer record count in `fluid_rvoice_reset()`. The existing buffer mapping initialization then initializes the gains for the new note, giving a reused rvoice the same onset ramp as a fresh one. Gain smoothing during an active note continues to use the existing mixer path.

Add a regression test using a generated RAM sample. It compares fresh and reused voice output after a note with a different pan, and fails without the fix.

Validation:

- Release double: 41/41 enabled tests passed.
- Release float: 41/41 enabled tests passed.
- ASan/UBSan: the regression test and voice callback lifecycle test passed.
- Three 60-second MIDI excerpts with MuseScore General SF2: 64-frame and 8192-frame requests produce byte-identical PCM with the fix, both with reverb/chorus enabled and disabled.

These builds disable libsndfile and Signalsmith. SF3 and the external manual regression assets were not exercised. No realtime audio driver was tested.
