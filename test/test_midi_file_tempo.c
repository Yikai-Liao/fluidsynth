#include "test.h"
#include "midi/fluid_midi.h"

struct timing_result
{
    unsigned int now;
    unsigned int notes[2];
    int count;
};

static int timing_callback(void *data, fluid_midi_event_t *event)
{
    struct timing_result *result = data;
    if(fluid_midi_event_get_type(event) == NOTE_ON)
    {
        TEST_ASSERT(result->count < 2);
        result->notes[result->count++] = result->now;
    }
    return FLUID_OK;
}

static void test_timing(fluid_synth_t *synth, int ppq, int changing,
                        int external, double multiplier, int seek, int loop)
{
    unsigned char midi[1024] = {
        'M','T','h','d', 0,0,0,6, 0,0, 0,1, 0,0,
        'M','T','r','k', 0,0,0,0
    };
    int size = 22, i, tempo;
    double duration = 0;
    struct timing_result result = {0, {0, 0}, 0};
    fluid_player_t *player = new_fluid_player(synth);
    TEST_ASSERT(player != NULL);
    midi[12] = ppq >> 8;
    midi[13] = ppq & 255;
    for(i = 0; i < 100; i++)
    {
        tempo = changing && i % 2 ? 1000000 : 500000;
        midi[size++] = i ? 3 : 0;
        midi[size++] = 0xFF;
        midi[size++] = 0x51;
        midi[size++] = 3;
        midi[size++] = tempo >> 16;
        midi[size++] = (tempo >> 8) & 255;
        midi[size++] = tempo & 255;
        duration += 3.0 * (external ? 750000 : tempo) / ppq / 1000 / multiplier;
    }
    midi[size++] = 3;
    midi[size++] = 0x90;
    midi[size++] = 60;
    midi[size++] = 100;
    midi[size++] = 1;
    midi[size++] = 0xFF;
    midi[size++] = 0x2F;
    midi[size++] = 0;
    midi[20] = (size - 22) >> 8;
    midi[21] = (size - 22) & 255;
    TEST_SUCCESS(fluid_player_add_mem(player, midi, size));
    if(loop == 2)
    {
        midi[12] = (ppq * 2) >> 8;
        midi[13] = (ppq * 2) & 255;
        TEST_SUCCESS(fluid_player_add_mem(player, midi, size));
    }
    TEST_SUCCESS(fluid_player_set_playback_callback(player, timing_callback, &result));
    TEST_SUCCESS(fluid_player_set_tempo(player, external ? FLUID_PLAYER_TEMPO_EXTERNAL_MIDI
                                                        : FLUID_PLAYER_TEMPO_INTERNAL,
                                        external ? 750000 : multiplier));
    TEST_SUCCESS(fluid_player_set_loop(player, loop == 1 ? 2 : 1));
    TEST_SUCCESS(fluid_player_play(player));
    for(result.now = 0; result.now < 6000 && result.count < (loop ? 2 : 1); result.now += 8)
    {
        if((seek == 1 || seek == 2) && result.now == 96)
        {
            TEST_SUCCESS(fluid_player_seek(player, 150));
        }
        if(seek == 2 && result.now == 192)
        {
            TEST_SUCCESS(fluid_player_seek(player, 0));
        }
        TEST_SUCCESS(fluid_synth_process(synth, 64, 0, NULL, 0, NULL));
        if(seek == 3 && (result.now == 96 || result.now == 192))
        {
            double before = player->start_ticks
                + (player->cur_msec - player->start_msec) / player->deltatime;
            double after;
            TEST_SUCCESS(fluid_player_set_tempo(player,
                result.now == 96 ? FLUID_PLAYER_TEMPO_EXTERNAL_MIDI : FLUID_PLAYER_TEMPO_INTERNAL,
                result.now == 96 ? 750000 : 1));
            after = player->start_ticks
                + (player->cur_msec - player->start_msec) / player->deltatime;
            TEST_ASSERT(fabs(after - before) < 0.000001);
        }
    }
    if(seek == 1)
    {
        duration = 96 + duration / 2;
    }
    else if(seek == 2)
    {
        duration += 192;
    }
    else if(seek == 3)
    {
        duration = 192 + (300 - (96 / 5.0 + 96 / 7.5)) * 5;
    }
    TEST_ASSERT(result.count == (loop ? 2 : 1));
    TEST_ASSERT(result.notes[0] >= duration - 0.001);
    TEST_ASSERT(result.notes[0] <= duration + 8);
    if(loop)
    {
        double end = duration / (loop == 2 ? 2 : 1) + (changing ? 1000000.0 : 500000.0) / ppq / 1000;
        TEST_ASSERT(result.notes[1] - result.notes[0] >= end - 8);
        TEST_ASSERT(result.notes[1] - result.notes[0] <= end + 8);
    }
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
        TEST_SUCCESS(fluid_synth_process(synth, 64, 0, NULL, 0, NULL));
        TEST_ASSERT(player->ntracks == valid);
        delete_fluid_player(player);

        /* 500000 microseconds per quarter note. */
        midi[26] = 0x07;
        midi[27] = 0xA1;
        midi[28] = 0x20;
    }
    test_timing(synth, 100, 0, 0, 1, 0, 0);
    test_timing(synth, 100, 1, 0, 1, 0, 0);
    test_timing(synth, 10000, 1, 0, 1, 0, 0);
    test_timing(synth, 100, 1, 1, 1, 0, 0);
    test_timing(synth, 100, 1, 0, 2, 0, 0);
    test_timing(synth, 100, 1, 0, 1, 1, 0);
    test_timing(synth, 100, 1, 0, 1, 2, 0);
    test_timing(synth, 100, 1, 0, 1, 0, 1);
    test_timing(synth, 100, 0, 0, 1, 3, 0);
    test_timing(synth, 100, 1, 0, 1, 0, 2);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    return EXIT_SUCCESS;
}
