#include "test.h"
#include "fluid_midi.h"
#include "fluid_synth.h"
#include <vector>

static void check(const std::vector<char> &track, bool valid, int duration)
{
    std::vector<char> file = {'M','T','h','d',0,0,0,6,0,0,0,1,0,96,'M','T','r','k'};
    for(int shift = 24; shift >= 0; shift -= 8) file.push_back(track.size() >> shift);
    file.insert(file.end(), track.begin(), track.end());
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth = new_fluid_synth(settings);
    fluid_player_t *player = new_fluid_player(synth);
    TEST_SUCCESS(fluid_player_add_mem(player, file.data(), file.size()));
    TEST_SUCCESS(fluid_player_play(player));
    TEST_SUCCESS(fluid_synth_process(synth, 64, 0, nullptr, 0, nullptr));
    TEST_ASSERT((player->ntracks == 1) == valid);
    if(valid) TEST_ASSERT(fluid_player_get_total_ticks(player) == duration);
    delete_fluid_player(player);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
}

int main()
{
    check({0, char(0xFF), 0x51, 3, 0, 0, 0, 0, char(0xFF), 0x2F, 0}, false, 0);
    check({0, char(0xFF), 0x51, 3, 7, char(0xA1), 0x20, 96, char(0xFF), 0x2F, 0}, true, 96);
    return EXIT_SUCCESS;
}
