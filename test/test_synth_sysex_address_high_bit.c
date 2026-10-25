#include "test.h"
#include "fluidsynth.h"
#include "fluidsynth_priv.h"
#include "fluid_synth.h"
#include "fluid_chan.h"
#include <stdlib.h>

static int test_gs_dt1_large_checksum(fluid_synth_t *synth)
{
    const int payload_len = 17000000;
    const int len = payload_len + 9;
    char *message = malloc((size_t)len);
    int checksum = 0, i, result;

    if(message == NULL)
    {
        return EXIT_FAILURE;
    }

    message[0] = 0x41;
    message[1] = 0x7f;
    message[2] = 0x42;
    message[3] = 0x12;
    message[4] = 0x40;
    message[5] = 0x00;
    message[6] = 0x01;
    checksum = message[4] + message[5] + message[6];

    for(i = 7; i < len - 1; ++i)
    {
        message[i] = 0x7f;
        checksum = (checksum + 0x7f) & 0x7f;
    }

    message[len - 1] = (char)((0x80 - checksum) & 0x7f);
    result = fluid_synth_sysex(synth, message, len, NULL, NULL, NULL, 0);
    free(message);
    return result == FLUID_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}

int main(void)
{
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth;
    /* SysEx address bytes are 7-bit; 0x80 is malformed input. */
    const char gs_message[] = { 0x41, 0x7f, 0x42, 0x12, (char)0x80, 0, 0, 0, 0 };
    const char xg_message[] = { 0x43, 0x7f, 0x4c, (char)0x80, 0, 0, 0 };
    const char gs_mode_message[] = { 0x41, 0x7f, 0x42, 0x12, 0x40, 0, 0x7f, 0, 0x41 };
    const char gs_bad_rhythm_message[] = {
        0x41, 0x7f, 0x42, 0x12, 0x40, 0x10, 0x15, (char)0xff, 0x1c
    };

    TEST_ASSERT(settings != NULL);
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);
    TEST_ASSERT(fluid_synth_sysex(synth, gs_message, sizeof(gs_message), NULL, NULL,
                                  NULL, 0) == FLUID_FAILED);
    TEST_ASSERT(fluid_synth_sysex(synth, xg_message, sizeof(xg_message), NULL, NULL,
                                  NULL, 0) == FLUID_FAILED);
    TEST_SUCCESS(fluid_synth_sysex(synth, gs_mode_message, sizeof(gs_mode_message),
                                   NULL, NULL, NULL, 0));
    TEST_SUCCESS(fluid_synth_set_channel_type(synth, 9, CHANNEL_TYPE_MELODIC));
    TEST_ASSERT(fluid_synth_sysex(synth, gs_bad_rhythm_message,
                                  sizeof(gs_bad_rhythm_message), NULL, NULL,
                                  NULL, 0) == FLUID_FAILED);
    TEST_ASSERT(synth->channel[9]->channel_type == CHANNEL_TYPE_MELODIC);
    TEST_ASSERT(test_gs_dt1_large_checksum(synth) == EXIT_SUCCESS);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    return EXIT_SUCCESS;
}
