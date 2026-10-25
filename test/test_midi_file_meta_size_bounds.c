#include "test.h"
#include "fluidsynth.h"
#include <stddef.h>
#include <string.h>

static int record_large_allocations;
static unsigned int large_allocations;

void *__real_malloc(size_t size);

void *__wrap_malloc(size_t size)
{
    if(record_large_allocations && size >= 128)
    {
        large_allocations++;
        return NULL;
    }

    return __real_malloc(size);
}

static void check_no_large_allocations(const unsigned char *midi, int len)
{
    float left[128], right[128];
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth = NULL;
    fluid_player_t *player = NULL;

    TEST_ASSERT(settings != NULL);
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);
    player = new_fluid_player(synth);
    TEST_ASSERT(player != NULL);
    TEST_SUCCESS(fluid_player_add_mem(player, midi, len));

    /* The declared text payload is much larger than this MTrk. Reject it
     * before allocating a buffer based on that untrusted length. */
    record_large_allocations = 1;
    TEST_SUCCESS(fluid_player_play(player));
    TEST_SUCCESS(fluid_synth_write_float(synth, 128, left, 0, 1, right, 0, 1));
    record_large_allocations = 0;

    TEST_ASSERT(large_allocations == 0);
    TEST_ASSERT(fluid_player_get_status(player) == FLUID_PLAYER_DONE);

    delete_fluid_player(player);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
}

int main(void)
{
    static const unsigned char large_meta[] = {
        'M','T','h','d', 0,0,0,6, 0,0, 0,1, 0,96,
        'M','T','r','k', 0,0,0,7,
        0,0xff,0x01, 0xff,0xff,0xff,0x7f
    };
    static const unsigned char header[] = {
        'M','T','h','d', 0,0,0,6, 0,0, 0,1, 0,96
    };
    static const unsigned char event[] = { 0,0xff,0x01,0x82,0x2c };
    unsigned char track_overrun[sizeof(header) + 8 + sizeof(event) + 300];

    check_no_large_allocations(large_meta, sizeof(large_meta));

    /* Here the text length fits the remaining file bytes exactly, but not
     * the declared MTrk. The track bound must reject it before allocation. */
    memcpy(track_overrun, header, sizeof(header));
    memcpy(track_overrun + sizeof(header), "MTrk", 4);
    track_overrun[sizeof(header) + 4] = 0;
    track_overrun[sizeof(header) + 5] = 0;
    track_overrun[sizeof(header) + 6] = 0;
    track_overrun[sizeof(header) + 7] = sizeof(event);
    memcpy(track_overrun + sizeof(header) + 8, event, sizeof(event));
    memset(track_overrun + sizeof(header) + 8 + sizeof(event), 'x', 300);
    check_no_large_allocations(track_overrun, sizeof(track_overrun));

    return EXIT_SUCCESS;
}
