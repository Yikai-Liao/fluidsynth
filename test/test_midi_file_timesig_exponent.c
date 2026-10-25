#include "test.h"
#include "fluidsynth.h"
#include "midi/fluid_midi.h"
#include <limits.h>

typedef struct
{
    int program_changes;
    int last_tick;
} captured_events_t;

static int capture_event(void *data, fluid_midi_event_t *event)
{
    captured_events_t *captured = data;
    if(fluid_midi_event_get_type(event) == PROGRAM_CHANGE)
    {
        captured->program_changes++;
    }
    return FLUID_OK;
}

static int capture_tick(void *data, int tick)
{
    captured_events_t *captured = data;
    TEST_ASSERT(tick >= captured->last_tick);
    captured->last_tick = tick;
    return FLUID_OK;
}

static void test_large_track_duration(int skip_meta_events)
{
    unsigned char midi[160] = {
        'M','T','h','d', 0,0,0,6, 0,0, 0,1, 0x7f,0xff,
        'M','T','r','k', 0,0,0,0
    };
    captured_events_t captured = {0};
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth = new_fluid_synth(settings);
    fluid_player_t *player = new_fluid_player(synth);
    uint64_t duration = UINT64_C(17) * 0x0fffffff;
    float left[8192], right[8192];
    int i, pos = 22;

    TEST_ASSERT(settings != NULL && synth != NULL && player != NULL);
    for(i = 0; i < 17; i++)
    {
        midi[pos++] = 0xff;
        midi[pos++] = 0xff;
        midi[pos++] = 0xff;
        midi[pos++] = 0x7f;
        if(skip_meta_events)
        {
            midi[pos++] = 0xff;
            midi[pos++] = 0x06; /* Marker is skipped, retaining its delta time. */
            midi[pos++] = 0;
        }
        else
        {
            midi[pos++] = 0xc0;
            midi[pos++] = i;
        }
    }
    midi[pos++] = 0;
    midi[pos++] = 0xc0;
    midi[pos++] = 42; /* This event lies beyond both INT_MAX and UINT_MAX. */
    midi[pos++] = 0;
    midi[pos++] = 0xff;
    midi[pos++] = 0x2f;
    midi[pos++] = 0;
    midi[21] = pos - 22;

    TEST_SUCCESS(fluid_player_add_mem(player, midi, pos));
    TEST_SUCCESS(fluid_player_set_playback_callback(player, capture_event, &captured));
    TEST_SUCCESS(fluid_player_set_tick_callback(player, capture_tick, &captured));
    TEST_SUCCESS(fluid_player_set_tempo(player, FLUID_PLAYER_TEMPO_EXTERNAL_MIDI, 1));
    TEST_SUCCESS(fluid_player_play(player));
    TEST_SUCCESS(fluid_synth_write_float(synth, 8192, left, 0, 1, right, 0, 1));

    TEST_ASSERT(player->ntracks == 1);
    TEST_ASSERT(player->track[0]->ticks == duration);
    TEST_ASSERT(fluid_track_eot(player->track[0]));
    TEST_ASSERT(captured.program_changes == (skip_meta_events ? 1 : 18));
    TEST_ASSERT(captured.last_tick == INT_MAX);
    TEST_ASSERT(fluid_player_get_current_tick(player) == INT_MAX);
    TEST_ASSERT(fluid_player_get_total_ticks(player) == INT_MAX);

    /* Complete the normal effect tail and ensure playback actually finishes. */
    for(i = 0; i < 16; i++)
    {
        TEST_SUCCESS(fluid_synth_write_float(synth, 8192, left, 0, 1, right, 0, 1));
    }
    TEST_ASSERT(fluid_player_get_status(player) == FLUID_PLAYER_DONE);

    delete_fluid_player(player);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
}

static void test_time_signature_and_tempo(void)
{
    static const unsigned char midi[] = {
        'M','T','h','d', 0,0,0,6, 0,0, 0,1, 0x7f,0xff,
        'M','T','r','k', 0,0,0,12,
        0,0xff,0x58,4,4,255,24,8,
        0,0xff,0x2f,0
    };
    float left[8192], right[8192];
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth = NULL;
    fluid_player_t *player = NULL;

    TEST_ASSERT(settings != NULL);
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);
    player = new_fluid_player(synth);
    TEST_ASSERT(player != NULL);
    TEST_SUCCESS(fluid_player_add_mem(player, midi, sizeof(midi)));
    TEST_SUCCESS(fluid_player_set_tempo(player, FLUID_PLAYER_TEMPO_EXTERNAL_MIDI, 1));
    TEST_SUCCESS(fluid_player_play(player));
    TEST_SUCCESS(fluid_synth_write_float(synth, 8192, left, 0, 1, right, 0, 1));

    /* This meta field is only logged, but its full byte range must not cause
     * an out-of-range floating-point to integer conversion. */
    TEST_ASSERT(player->ntracks == 1);
    TEST_ASSERT(player->cur_ticks > INT_MAX);
    TEST_ASSERT(fluid_player_get_current_tick(player) == INT_MAX);

    /* A zero tick duration supplied through the deprecated tempo API must
     * stop cleanly instead of dividing by zero in the playback callback. */
    TEST_SUCCESS(fluid_player_set_midi_tempo(player, 0));
    TEST_SUCCESS(fluid_player_set_tempo(player, FLUID_PLAYER_TEMPO_INTERNAL, 1.0));
    TEST_SUCCESS(fluid_synth_write_float(synth, 128, left, 0, 1, right, 0, 1));
    TEST_ASSERT(fluid_player_get_status(player) == FLUID_PLAYER_DONE);

    delete_fluid_player(player);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
}

int main(void)
{
    test_time_signature_and_tempo();
    test_large_track_duration(0);
    test_large_track_duration(1);
    return EXIT_SUCCESS;
}
