#include "test.h"
#include "fluid_midi.h"
#include "fluid_synth.h"
#include <vector>

static void check(const std::vector<char> &track, bool valid, int duration, int tracks = 1)
{
    std::vector<char> file = {'M','T','h','d',0,0,0,6,0,0,0,1,0,96,'M','T','r','k'};
    file[9] = tracks > 1 ? 1 : 0;
    file[10] = tracks >> 8; file[11] = tracks;
    file.resize(14);
    for(int i = 0; i < tracks; i++)
    {
        file.insert(file.end(), {'M','T','r','k'});
        for(int shift = 24; shift >= 0; shift -= 8) file.push_back(track.size() >> shift);
        file.insert(file.end(), track.begin(), track.end());
    }
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth = new_fluid_synth(settings);
    fluid_player_t *player = new_fluid_player(synth);
    TEST_SUCCESS(fluid_player_add_mem(player, file.data(), file.size()));
    TEST_SUCCESS(fluid_player_play(player));
    TEST_SUCCESS(fluid_synth_process(synth, 64, 0, nullptr, 0, nullptr));
    TEST_ASSERT((player->ntracks == tracks) == valid);
    if(valid) TEST_ASSERT(fluid_player_get_total_ticks(player) == duration);
    delete_fluid_player(player);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
}

int main()
{
    check({0, char(0xFF), 0x2F, 0}, true, 0, 128);
    check({0, char(0xFF), 0x2F, 0}, false, 0, 129);
    return EXIT_SUCCESS;
}
