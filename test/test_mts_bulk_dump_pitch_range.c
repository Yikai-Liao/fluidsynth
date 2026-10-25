#include <float.h>

#include "test.h"
#include "fluidsynth.h"

int main(void)
{
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth;
    int keys[] = { 60, 61, 62, 63 };
    double pitches[] = { DBL_MAX, -DBL_MAX, 12800.0, 12799.999 };
    char request[] = { 0x7e, 0x7f, 0x08, 0x00, 0x00 };
    char response[406];
    int response_len = sizeof(response);

    TEST_ASSERT(settings != NULL);
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);

    /* Pitch values are accepted by the tuning API and must be saturated
     * safely when encoded in the finite range of an MTS bulk dump. */
    TEST_SUCCESS(fluid_synth_tune_notes(synth, 0, 0, 4, keys, pitches, 0));
    TEST_SUCCESS(fluid_synth_sysex(synth, request, sizeof(request), response,
                                  &response_len, NULL, 0));
    TEST_ASSERT(response_len == (int)sizeof(response));
    TEST_ASSERT((unsigned char)response[21 + keys[0] * 3] == 127);
    TEST_ASSERT((unsigned char)response[22 + keys[0] * 3] == 127);
    TEST_ASSERT((unsigned char)response[23 + keys[0] * 3] == 126);
    TEST_ASSERT((unsigned char)response[21 + keys[1] * 3] == 0);
    TEST_ASSERT((unsigned char)response[22 + keys[1] * 3] == 0);
    TEST_ASSERT((unsigned char)response[23 + keys[1] * 3] == 0);
    /* Values near or above the top of the range must never encode the
     * reserved "no change" word instead of a saturated frequency. */
    TEST_ASSERT((unsigned char)response[21 + keys[2] * 3] == 127);
    TEST_ASSERT((unsigned char)response[22 + keys[2] * 3] == 127);
    TEST_ASSERT((unsigned char)response[23 + keys[2] * 3] == 126);
    TEST_ASSERT((unsigned char)response[21 + keys[3] * 3] == 127);
    TEST_ASSERT((unsigned char)response[22 + keys[3] * 3] == 127);
    TEST_ASSERT((unsigned char)response[23 + keys[3] * 3] == 126);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    return EXIT_SUCCESS;
}
