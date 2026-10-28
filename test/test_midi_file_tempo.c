#include "test.h"
#include "midi/fluid_midi.h"

static int timing_callback(void *data, fluid_midi_event_t *event)
{
    unsigned int *timing = data;
    if(fluid_midi_event_get_type(event) == NOTE_ON)
    {
        /* 50 segments each at 500000 and 1000000 us/quarter: 2250 ms. */
        TEST_ASSERT(timing[0] >= 2250 && timing[0] <= 2258);
        timing[1]++;
    }
    return FLUID_OK;
}

static void test_timing(fluid_synth_t *synth)
{
    static const unsigned char tempo[][7] = {
        {3, 0xFF, MIDI_SET_TEMPO, 3, 0x07, 0xA1, 0x20},
        {3, 0xFF, MIDI_SET_TEMPO, 3, 0x0F, 0x42, 0x40}
    };
    static const unsigned char ending[] = {3, NOTE_ON, 60, 100, 1, 0xFF, MIDI_EOT, 0};
    unsigned char midi[22 + 100 * sizeof(tempo[0]) + sizeof(ending)] = {
        'M','T','h','d', 0,0,0,6, 0,0, 0,1, 0,100,
        'M','T','r','k', 0,0,0,0
    };
    unsigned int timing[2] = {0, 0}; /* Elapsed milliseconds, received notes. */
    int i;
    fluid_player_t *player = new_fluid_player(synth);
    TEST_ASSERT(player != NULL);
    midi[20] = (sizeof(midi) - 22) >> 8;
    midi[21] = (sizeof(midi) - 22) & 255;
    for(i = 0; i < 100; i++)
    {
        FLUID_MEMCPY(midi + 22 + i * sizeof(tempo[0]), tempo[i % 2], sizeof(tempo[0]));
    }
    midi[22] = 0;
    FLUID_MEMCPY(midi + sizeof(midi) - sizeof(ending), ending, sizeof(ending));
    TEST_SUCCESS(fluid_player_add_mem(player, midi, sizeof(midi)));
    TEST_SUCCESS(fluid_player_set_playback_callback(player, timing_callback, timing));
    TEST_SUCCESS(fluid_player_play(player));
    for(timing[0] = 0; timing[0] < 3000 && !timing[1]; timing[0] += 8)
    {
        TEST_SUCCESS(fluid_synth_process(synth, FLUID_BUFSIZE, 0, NULL, 0, NULL));
    }
    TEST_ASSERT(timing[1] == 1);
    delete_fluid_player(player);
}

/* Reject a zero Set Tempo value and accept a valid one through file loading. */
int main(void)
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
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.sample-rate", 8000));
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);
    for(valid = 0; valid <= 1; valid++)
    {
        fluid_player_t *player = new_fluid_player(synth);
        TEST_ASSERT(player != NULL);
        TEST_SUCCESS(fluid_player_add_mem(player, midi, sizeof(midi)));
        TEST_SUCCESS(fluid_player_play(player));
        TEST_SUCCESS(fluid_synth_process(synth, FLUID_BUFSIZE, 0, NULL, 0, NULL));
        TEST_ASSERT(player->ntracks == valid);
        delete_fluid_player(player);

        /* 500000 microseconds per quarter note. */
        midi[26] = 0x07;
        midi[27] = 0xA1;
        midi[28] = 0x20;
    }
    test_timing(synth);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    return EXIT_SUCCESS;
}
