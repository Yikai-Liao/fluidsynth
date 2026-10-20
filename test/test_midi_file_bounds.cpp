#include "test.h"
#include "fluid_midi.h"
#include "fluid_synth.h"
#include <vector>
#include <climits>

static void varlen(std::vector<char> &data, unsigned int ticks)
{
    char bytes[4];
    int n = 0;
    bytes[n++] = ticks & 127;
    while((ticks >>= 7) != 0) bytes[n++] = (ticks & 127) | 128;
    while(n) data.push_back(bytes[--n]);
}

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
    if(valid)
    {
        TEST_ASSERT(fluid_player_get_total_ticks(player) == duration);
        TEST_ASSERT(player->track[0]->ticks == 0);
    }
    delete_fluid_player(player);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
}

int main()
{
    for(int count : {8, 9})
    {
        std::vector<char> track;
        for(int i = 0; i < count; i++)
        {
            varlen(track, 0x0FFFFFFF);
            track.insert(track.end(), {char(0xFF), 1, 0});
        }
        /* Eight maximum deltas plus seven ticks reach INT_MAX exactly. */
        track.insert(track.end(), {7, char(0xFF), 0x2F, 0});
        check(track, count == 8, INT_MAX);
    }
    return EXIT_SUCCESS;
}
