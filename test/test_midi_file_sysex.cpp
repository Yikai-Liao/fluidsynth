#include "test.h"
#include "fluid_midi.h"
#include "fluid_synth.h"
#include <vector>

static std::vector<int> types, keys;
static std::vector<unsigned int> ticks;
static std::vector<std::vector<unsigned char>> messages;
static int callback(void *data, fluid_midi_event_t *event)
{
    types.push_back(event->type);
    ticks.push_back(static_cast<fluid_player_t *>(data)->track[0]->ticks);
    if(event->type == NOTE_ON) keys.push_back(event->param1);
    if(event->type == MIDI_SYSEX)
    {
        auto bytes = static_cast<unsigned char *>(event->paramptr);
        messages.emplace_back(bytes, bytes + event->param1);
    }
    return FLUID_OK;
}

static void run(std::vector<unsigned char> track, bool valid = true)
{
    track.insert(track.end(), {0,0xFF,0x2F,0});
    std::vector<unsigned char> file = {'M','T','h','d',0,0,0,6,0,0,0,1,0,96,'M','T','r','k'};
    for(int shift = 24; shift >= 0; shift -= 8) file.push_back(track.size() >> shift);
    file.insert(file.end(), track.begin(), track.end());
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth = new_fluid_synth(settings);
    fluid_player_t *player = new_fluid_player(synth);
    types.clear(); keys.clear(); messages.clear(); ticks.clear();
    TEST_SUCCESS(fluid_player_set_playback_callback(player, callback, player));
    TEST_SUCCESS(fluid_player_add_mem(player, file.data(), file.size()));
    TEST_SUCCESS(fluid_player_play(player));
    TEST_SUCCESS(fluid_synth_process(synth, 44100, 0, nullptr, 0, nullptr));
    TEST_ASSERT((player->ntracks == 1) == valid);
    delete_fluid_player(player);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
}

int main()
{
    run({0,0xF0,3,0x43,0x12,0,10,0xF7,3,1,2,3,10,0xF7,2,4,0xF7});
    TEST_ASSERT(types == std::vector<int>({MIDI_SYSEX}));
    TEST_ASSERT(ticks == std::vector<unsigned int>({20}));
    TEST_ASSERT(messages == std::vector<std::vector<unsigned char>>({{0x43,0x12,0,1,2,3,4}}));
    run({0,0xF7,6,0x90,60,100,61,110,0xF8});
    TEST_ASSERT(types == std::vector<int>({NOTE_ON,NOTE_ON,MIDI_SYNC}));
    TEST_ASSERT(keys == std::vector<int>({60,61}));
    run({0,0xF0,0,10,0xF7,5,0x7E,0x7F,9,1,0xF7});
    TEST_ASSERT(messages == std::vector<std::vector<unsigned char>>({{0x7E,0x7F,9,1}}));
    std::vector<unsigned char> track = {0,0xF7,0x8F,0x52,0xF0}; /* 2002 escaped bytes */
    track.insert(track.end(), 2000, 0x55);
    track.push_back(0xF7);
    run(track);
    TEST_ASSERT(messages.size() == 1 && messages[0] == std::vector<unsigned char>(2000,0x55));
    run({0,0xF7,0x7F,0x90,60}, false); /* Reject a packet exceeding its track. */
    run({0,0xF0,2,0x43,0x12}); /* Discard an unfinished message on file cleanup. */
    TEST_ASSERT(messages.empty());
    return EXIT_SUCCESS;
}
