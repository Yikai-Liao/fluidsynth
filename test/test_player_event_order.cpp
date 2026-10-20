#include "test.h"
#include "fluid_midi.h"
#include "fluid_synth.h"
#include <vector>

static std::vector<int> received, notes;
static int callback(void *, fluid_midi_event_t *event)
{
    if(event->type == NOTE_ON) notes.push_back(event->param1);
    if(event->type == CONTROL_CHANGE && event->param1 == 1) received.push_back(event->param2);
    return FLUID_OK;
}

int main()
{
    /* Dispatch earlier tracks first; break equal ticks by track index. */
    const unsigned char file[] = {
        'M','T','h','d',0,0,0,6,0,1,0,3,0x7F,0xFF,
        'M','T','r','k',0,0,0,12,2,0xB0,1,2,0,0xB0,1,5,0,0xFF,0x2F,0,
        'M','T','r','k',0,0,0,20,1,0xB0,1,1,0,0x90,60,100,1,0xB0,1,3,0,0x90,61,100,0,0xFF,0x2F,0,
        'M','T','r','k',0,0,0,8,4,0xB0,1,4,0,0xFF,0x2F,0};
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth = new_fluid_synth(settings);
    fluid_player_t *player = new_fluid_player(synth);
    TEST_SUCCESS(fluid_player_set_playback_callback(player, callback, nullptr));
    TEST_SUCCESS(fluid_player_add_mem(player, file, sizeof(file)));
    TEST_SUCCESS(fluid_player_play(player));
    TEST_SUCCESS(fluid_synth_process(synth, 256, 0, nullptr, 0, nullptr));
    TEST_ASSERT(received == std::vector<int>({1,2,5,3,4}));
    TEST_ASSERT(notes == std::vector<int>({60,61}));
    received.clear();
    notes.clear();
    TEST_SUCCESS(fluid_player_seek(player, 2));
    TEST_SUCCESS(fluid_synth_process(synth, 64, 0, nullptr, 0, nullptr));
    TEST_ASSERT(received == std::vector<int>({1,2,5,3}));
    TEST_ASSERT(notes == std::vector<int>({61}));
    TEST_SUCCESS(fluid_synth_process(synth, 256, 0, nullptr, 0, nullptr));
    received.clear();
    notes.clear();
    /* Rewind every track, including tracks whose last event precedes the seek target. */
    TEST_SUCCESS(fluid_player_seek(player, 3));
    TEST_SUCCESS(fluid_synth_process(synth, 64, 0, nullptr, 0, nullptr));
    TEST_ASSERT(received == std::vector<int>({1,2,5,3}));
    TEST_ASSERT(notes.empty());
    received.clear();
    notes.clear();
    /* Seeking forward dispatches the remaining controller at its absolute tick. */
    TEST_SUCCESS(fluid_player_seek(player, 4));
    TEST_SUCCESS(fluid_synth_process(synth, 64, 0, nullptr, 0, nullptr));
    TEST_ASSERT(received == std::vector<int>({4}));
    delete_fluid_player(player);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    return EXIT_SUCCESS;
}
