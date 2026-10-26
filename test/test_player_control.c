#include "test.h"
#include "midi/fluid_midi.h"

typedef struct
{
    int noteons;
    int mutes;
} captured_events_t;

/* Count file restarts and stop requests without creating sounding voices. */
static int capture_event(void *data, fluid_midi_event_t *event)
{
    captured_events_t *captured = data;
    if(event->type == NOTE_ON && event->param2 != 0)
    {
        captured->noteons++;
    }
    if(event->type == CONTROL_CHANGE && event->param1 == ALL_SOUND_OFF)
    {
        captured->mutes++;
    }
    return FLUID_OK;
}

/* Allow one 64-sample callback of rounding at 32000 Hz. */
static void check_progress(fluid_synth_t *synth, fluid_player_t *player,
                           int samples, int ticks)
{
    int before = fluid_player_get_current_tick(player);
    int elapsed;
    TEST_SUCCESS(fluid_synth_process(synth, samples, 0, NULL, 0, NULL));
    elapsed = fluid_player_get_current_tick(player) - before;
    TEST_ASSERT(abs(elapsed - ticks) <= 4);
}

/* Exercise clock changes, pause/resume, and replay through the sample timer. */
int main(void)
{
    const unsigned char midi[] = {
        'M','T','h','d', 0,0,0,6, 0,0, 0,1, 1,0xE0,
        'M','T','r','k', 0,0,0,17,
        0,0x90,60,100,
        0x83,0x60,0xFF,0x51,3, 3,0xD0,0x90, /* Tick 480: 240 BPM. */
        0xA1,0x60,0xFF,0x2F,0 /* End at tick 4800. */
    };
    captured_events_t captured = {0};
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth;
    fluid_player_t *player;
    int paused;

    TEST_ASSERT(settings != NULL);
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.sample-rate", 32000));
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);
    player = new_fluid_player(synth);
    TEST_ASSERT(player != NULL);
    TEST_SUCCESS(fluid_player_add_mem(player, midi, sizeof(midi)));
    TEST_SUCCESS(fluid_player_set_playback_callback(player, capture_event, &captured));
    TEST_SUCCESS(fluid_player_play(player));
    TEST_SUCCESS(fluid_synth_process(synth, 32000, 0, NULL, 0, NULL));
    TEST_ASSERT(captured.noteons == 1);
    TEST_ASSERT(fluid_player_get_division(player) == 480);
    TEST_ASSERT(fluid_player_get_total_ticks(player) == 4800);
    TEST_ASSERT(fluid_player_get_midi_tempo(player) == 250000);

    /* A rapid stop/play still has to deliver the pending mute. */
    TEST_SUCCESS(fluid_player_stop(player));
    TEST_SUCCESS(fluid_player_play(player));
    TEST_SUCCESS(fluid_synth_process(synth, 64, 0, NULL, 0, NULL));
    TEST_ASSERT(captured.mutes == 1);

    /* A tempo event has moved the clock anchor before this pause. */
    paused = fluid_player_get_current_tick(player);
    TEST_ASSERT(paused > 480);
    TEST_SUCCESS(fluid_player_stop(player));
    TEST_SUCCESS(fluid_synth_process(synth, 32000, 0, NULL, 0, NULL));
    TEST_ASSERT(fluid_player_get_current_tick(player) == paused);
    TEST_SUCCESS(fluid_player_play(player));
    check_progress(synth, player, 32000, 1920);
    TEST_ASSERT(captured.noteons == 1);

    TEST_ASSERT(fluid_player_set_midi_tempo(player, 0) == FLUID_FAILED);
    TEST_SUCCESS(fluid_player_set_tempo(player, FLUID_PLAYER_TEMPO_EXTERNAL_MIDI, 16777217));
    TEST_ASSERT(fluid_player_get_midi_tempo(player) == 16777217);
    check_progress(synth, player, 64, 0);
    TEST_SUCCESS(fluid_player_set_tempo(player, FLUID_PLAYER_TEMPO_INTERNAL, 0.5));
    TEST_ASSERT(fluid_player_get_midi_tempo(player) == 500000);
    check_progress(synth, player, 16000, 480);
    TEST_SUCCESS(fluid_player_set_midi_tempo(player, 1000000));
    TEST_ASSERT(fluid_player_get_midi_tempo(player) == 2000000);
    check_progress(synth, player, 32000, 240);
    TEST_SUCCESS(fluid_player_set_tempo(player, FLUID_PLAYER_TEMPO_EXTERNAL_BPM, 120));
    TEST_SUCCESS(fluid_player_set_midi_tempo(player, 250000));
    TEST_ASSERT(fluid_player_get_midi_tempo(player) == 500000);
    check_progress(synth, player, 8000, 240);

    /* Finish quickly, including the effect tail, then replay the same player. */
    TEST_SUCCESS(fluid_player_set_tempo(player, FLUID_PLAYER_TEMPO_EXTERNAL_MIDI, 1000));
    TEST_SUCCESS(fluid_synth_process(synth, 96000, 0, NULL, 0, NULL));
    TEST_ASSERT(fluid_player_get_status(player) == FLUID_PLAYER_DONE);
    TEST_SUCCESS(fluid_player_join(player));
    TEST_SUCCESS(fluid_player_play(player));
    TEST_SUCCESS(fluid_synth_process(synth, 96000, 0, NULL, 0, NULL));
    TEST_ASSERT(captured.noteons == 2);
    TEST_ASSERT(fluid_player_get_status(player) == FLUID_PLAYER_DONE);

    delete_fluid_player(player);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    return EXIT_SUCCESS;
}
