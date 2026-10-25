#include "test.h"
#include "fluidsynth.h"
#include "midi/fluid_midi.h"

int main(void)
{
    fluid_midi_event_t *midi = new_fluid_midi_event();
    fluid_event_t *seq = new_fluid_event();
    int success = 0;

    if(midi == NULL || seq == NULL)
    {
        goto cleanup;
    }

    fluid_midi_event_set_type(midi, NOTE_OFF);
    fluid_midi_event_set_channel(midi, 3);
    fluid_midi_event_set_key(midi, 61);
    if(fluid_event_from_midi_event(seq, midi) != FLUID_OK ||
       fluid_event_get_type(seq) != FLUID_SEQ_NOTEOFF ||
       fluid_event_get_channel(seq) != 3 || fluid_event_get_key(seq) != 61)
    {
        goto cleanup;
    }

    fluid_midi_event_set_type(midi, NOTE_ON);
    fluid_midi_event_set_channel(midi, 4);
    fluid_midi_event_set_key(midi, 62);
    fluid_midi_event_set_velocity(midi, 100);
    if(fluid_event_from_midi_event(seq, midi) != FLUID_OK ||
       fluid_event_get_type(seq) != FLUID_SEQ_NOTEON ||
       fluid_event_get_channel(seq) != 4 || fluid_event_get_key(seq) != 62 ||
       fluid_event_get_velocity(seq) != 100)
    {
        goto cleanup;
    }

    fluid_midi_event_set_type(midi, CONTROL_CHANGE);
    fluid_midi_event_set_channel(midi, 5);
    fluid_midi_event_set_control(midi, 74);
    fluid_midi_event_set_value(midi, 53);
    if(fluid_event_from_midi_event(seq, midi) != FLUID_OK ||
       fluid_event_get_type(seq) != FLUID_SEQ_CONTROLCHANGE ||
       fluid_event_get_channel(seq) != 5 || fluid_event_get_control(seq) != 74 ||
       fluid_event_get_value(seq) != 53)
    {
        goto cleanup;
    }

    fluid_midi_event_set_type(midi, PROGRAM_CHANGE);
    fluid_midi_event_set_channel(midi, 6);
    fluid_midi_event_set_program(midi, 17);
    if(fluid_event_from_midi_event(seq, midi) != FLUID_OK ||
       fluid_event_get_type(seq) != FLUID_SEQ_PROGRAMCHANGE ||
       fluid_event_get_channel(seq) != 6 || fluid_event_get_program(seq) != 17)
    {
        goto cleanup;
    }

    fluid_midi_event_set_type(midi, PITCH_BEND);
    fluid_midi_event_set_channel(midi, 7);
    fluid_midi_event_set_pitch(midi, 12345);
    if(fluid_event_from_midi_event(seq, midi) != FLUID_OK ||
       fluid_event_get_type(seq) != FLUID_SEQ_PITCHBEND ||
       fluid_event_get_channel(seq) != 7 || fluid_event_get_pitch(seq) != 12345)
    {
        goto cleanup;
    }

    fluid_midi_event_set_type(midi, CHANNEL_PRESSURE);
    fluid_midi_event_set_channel(midi, 8);
    /* Channel pressure uses the first data byte, stored in the param1 slot. */
    fluid_midi_event_set_program(midi, 83);
    if(fluid_event_from_midi_event(seq, midi) != FLUID_OK ||
       fluid_event_get_type(seq) != FLUID_SEQ_CHANNELPRESSURE ||
       fluid_event_get_channel(seq) != 8 || fluid_event_get_value(seq) != 83)
    {
        goto cleanup;
    }

    fluid_midi_event_set_type(midi, KEY_PRESSURE);
    fluid_midi_event_set_channel(midi, 9);
    fluid_midi_event_set_key(midi, 70);
    fluid_midi_event_set_value(midi, 45);
    if(fluid_event_from_midi_event(seq, midi) != FLUID_OK ||
       fluid_event_get_type(seq) != FLUID_SEQ_KEYPRESSURE ||
       fluid_event_get_channel(seq) != 9 || fluid_event_get_key(seq) != 70 ||
       fluid_event_get_value(seq) != 45)
    {
        goto cleanup;
    }

    fluid_midi_event_set_type(midi, MIDI_SYSTEM_RESET);
    if(fluid_event_from_midi_event(seq, midi) != FLUID_OK ||
       fluid_event_get_type(seq) != FLUID_SEQ_SYSTEMRESET)
    {
        goto cleanup;
    }

    fluid_midi_event_set_type(midi, MIDI_SYSEX);
    if(fluid_event_from_midi_event(seq, midi) != FLUID_FAILED ||
       fluid_event_from_midi_event(seq, NULL) != FLUID_FAILED)
    {
        goto cleanup;
    }

    success = 1;

cleanup:
    if(midi != NULL)
    {
        delete_fluid_midi_event(midi);
    }

    if(seq != NULL)
    {
        delete_fluid_event(seq);
    }

    TEST_ASSERT(success);
    return EXIT_SUCCESS;
}
