#include "test.h"
#include "midi/fluid_midi.h"
#include <string.h>

/* Reject zero Set Tempo values without rejecting a valid MIDI file. */
static void test_file_tempo(void)
{
    unsigned char midi[] = {
        'M','T','h','d', 0,0,0,6, 0,0, 0,1, 0,96,
        'M','T','r','k', 0,0,0,11,
        0,0xFF,0x51,3, 0,0,0, 96,0xFF,0x2F,0
    };
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth;
    int valid;

    TEST_ASSERT(settings != NULL);
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);
    for(valid = 0; valid <= 1; valid++)
    {
        fluid_player_t *player = new_fluid_player(synth);
        TEST_ASSERT(player != NULL);
        TEST_SUCCESS(fluid_player_add_mem(player, midi, sizeof(midi)));
        TEST_SUCCESS(fluid_player_play(player));
        TEST_SUCCESS(fluid_synth_process(synth, 64, 0, NULL, 0, NULL));
        TEST_ASSERT(player->ntracks == valid);
        delete_fluid_player(player);

        /* 500000 microseconds per quarter note. */
        midi[26] = 0x07;
        midi[27] = 0xA1;
        midi[28] = 0x20;
    }
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
}

static void check_stream(const unsigned char *bytes, unsigned int length,
                         const unsigned int expected[][3], unsigned int event_count,
                         unsigned int realtime)
{
    static const unsigned char sysex[] = {0x7E, 0x7F, 0x09, 0x01};
    fluid_midi_parser_t *parser = new_fluid_midi_parser();
    unsigned int i, count = 0;
    TEST_ASSERT(parser != NULL);
    for(i = 0; i < length; i++)
    {
        fluid_midi_event_t *event = fluid_midi_parser_parse(parser, bytes[i] == MIDI_SYNC ? realtime : bytes[i]);
        if(event == NULL)
        {
            continue;
        }
        TEST_ASSERT(count < event_count);
        TEST_ASSERT(event->type == (expected[count][0] == MIDI_SYNC ? realtime : expected[count][0]));
        if(event->type < MIDI_SYSEX)
        {
            TEST_ASSERT(event->channel == 3 && event->param1 == expected[count][1]);
            if(event->type == NOTE_ON)
            {
                TEST_ASSERT(event->param2 == expected[count][2]);
            }
        }
        if(event->type == MIDI_SYSEX)
        {
            TEST_ASSERT(event->param1 == sizeof(sysex));
            TEST_ASSERT(memcmp(event->paramptr, sysex, sizeof(sysex)) == 0);
        }
        count++;
    }
    TEST_ASSERT(count == event_count);
    delete_fluid_midi_parser(parser);
}

static void test_realtime_interleaving(void)
{
    /* Interleave F8-FE with channel messages and SysEx. */
    static const unsigned char bytes[] = {
        0x93, 60, 0xF8, 100, 0xF8, 61, 101,
        0xC3, 0xF8, 12, 0xF8, 13, 0xF1, 14,
        0xF0, 0x7E, 0xF8, 0x7F, 0x09, 0x01, 0xF7
    };
    static const unsigned int expected[][3] = {
        {MIDI_SYNC, 0, 0}, {NOTE_ON, 60, 100},
        {MIDI_SYNC, 0, 0}, {NOTE_ON, 61, 101},
        {MIDI_SYNC, 0, 0}, {PROGRAM_CHANGE, 12, 0},
        {MIDI_SYNC, 0, 0}, {PROGRAM_CHANGE, 13, 0},
        {MIDI_SYNC, 0, 0}, {MIDI_SYSEX, 4, 0}
    };
    unsigned int realtime;
    for(realtime = 0xF8; realtime < MIDI_SYSTEM_RESET; realtime++)
    {
        check_stream(bytes, sizeof(bytes), expected, FLUID_N_ELEMENTS(expected), realtime);
    }
}

static void test_system_reset(void)
{
    /* Discard pending messages and running status on FF, then resume on a new status. */
    static const unsigned char bytes[] = {
        0x93, 60, 0xFF, 100,
        0x93, 61, 101, 0xFF, 62, 102,
        0xC3, 0xFF, 13,
        0xF0, 0x7E, 0xFF, 0x7F, 0xF7,
        0x93, 63, 103
    };
    static const unsigned int expected[][3] = {
        {MIDI_SYSTEM_RESET, 0, 0}, {NOTE_ON, 61, 101},
        {MIDI_SYSTEM_RESET, 0, 0}, {MIDI_SYSTEM_RESET, 0, 0},
        {MIDI_SYSTEM_RESET, 0, 0}, {NOTE_ON, 63, 103}
    };
    check_stream(bytes, sizeof(bytes), expected, FLUID_N_ELEMENTS(expected), MIDI_SYNC);
}

int main(void)
{
    test_realtime_interleaving();
    test_system_reset();
    test_file_tempo();
    return EXIT_SUCCESS;
}
