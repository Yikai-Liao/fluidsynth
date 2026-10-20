#include "test.h"
#include "fluid_midi.h"
#include "fluid_synth.h"
#include <atomic>
#include <thread>

static int last_tick;
static bool check_monotonic;
static int tick_callback(void *, int tick)
{
    if(check_monotonic) TEST_ASSERT(tick >= last_tick);
    last_tick = tick;
    return FLUID_OK;
}

int main()
{
    const unsigned char file[] = {'M','T','h','d',0,0,0,6,0,0,0,1,1, 0xE0,
                                 'M','T','r','k',0,0,0,7,0xFF,0xFF,0xFF,0x7F,0xFF,0x2F,0};
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth = new_fluid_synth(settings);
    fluid_player_t *player = new_fluid_player(synth);
    TEST_SUCCESS(fluid_player_add_mem(player, file, sizeof(file)));
    TEST_SUCCESS(fluid_player_set_tick_callback(player, tick_callback, nullptr));
    TEST_SUCCESS(fluid_player_play(player));
    TEST_SUCCESS(fluid_synth_process(synth, 44100, 0, nullptr, 0, nullptr));
    int paused = fluid_player_get_current_tick(player);
    TEST_ASSERT(paused > 0);
    TEST_SUCCESS(fluid_player_stop(player));
    TEST_SUCCESS(fluid_synth_process(synth, 44100, 0, nullptr, 0, nullptr));
    TEST_ASSERT(fluid_player_get_current_tick(player) == paused);
    TEST_SUCCESS(fluid_player_play(player));
    TEST_SUCCESS(fluid_synth_process(synth, 44100, 0, nullptr, 0, nullptr));
    TEST_ASSERT(fluid_player_get_current_tick(player) > paused);
    TEST_ASSERT(fluid_player_get_current_tick(player) < paused * 3);
    TEST_ASSERT(fluid_player_set_midi_tempo(player, 0) == FLUID_FAILED);
    TEST_SUCCESS(fluid_player_set_tempo(player, FLUID_PLAYER_TEMPO_EXTERNAL_MIDI, 16777217));
    TEST_ASSERT(fluid_player_get_midi_tempo(player) == 16777217);
    TEST_SUCCESS(fluid_player_set_tempo(player, FLUID_PLAYER_TEMPO_INTERNAL, 0.5));
    TEST_ASSERT(fluid_player_get_midi_tempo(player) == 1000000);
    TEST_SUCCESS(fluid_player_set_midi_tempo(player, 2000000));
    TEST_ASSERT(fluid_player_get_midi_tempo(player) == 4000000);
    for(int phase = 0; phase < 2; phase++)
    {
        check_monotonic = true;
        std::atomic<bool> start{false};
        std::thread render([&]()
        {
            while(!start) {}
            for(int i = 0; i < 3000; i++) TEST_SUCCESS(fluid_synth_process(synth, 256, 0, nullptr, 0, nullptr));
        });
        start = true;
        for(int i = 0; i < 10000; i++)
        {
            TEST_SUCCESS(fluid_player_set_tempo(player, i % 3 == 0 ? FLUID_PLAYER_TEMPO_INTERNAL : FLUID_PLAYER_TEMPO_EXTERNAL_BPM,
                                               i % 3 == 0 ? 0.5 + (i % 4) : 20 + i % 300));
            TEST_ASSERT(fluid_player_get_current_tick(player) >= 0);
            TEST_ASSERT(fluid_player_get_total_ticks(player) == 0x0FFFFFFF);
            if(phase == 1)
            {
                TEST_SUCCESS(fluid_player_stop(player));
                TEST_SUCCESS(fluid_player_play(player));
            }
        }
        render.join();
        check_monotonic = false;
    }
    delete_fluid_player(player);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    return EXIT_SUCCESS;
}
