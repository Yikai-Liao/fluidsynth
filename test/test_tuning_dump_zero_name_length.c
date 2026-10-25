#include "test.h"
#include "fluidsynth.h"
#include <string.h>

int main(void)
{
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth;
    char name[1];
    const char expected_name[] = "0123456789ABCDEF";
    double pitches[128];
    char full_name[sizeof(expected_name)];
    int i;

    TEST_ASSERT(settings != NULL);
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);

    /* A zero-capacity output buffer must be rejected before formatting. */
    TEST_ASSERT(fluid_synth_tuning_dump(synth, 0, 0, name, 0, NULL) == FLUID_FAILED);
    TEST_SUCCESS(fluid_synth_tuning_dump(synth, 0, 0, name, sizeof(name), NULL));
    TEST_ASSERT(name[0] == '\0');

    for(i = 0; i < 128; ++i)
    {
        pitches[i] = i * 100.0;
    }

    TEST_SUCCESS(fluid_synth_activate_key_tuning(synth, 0, 1, expected_name, pitches, 0));
    TEST_SUCCESS(fluid_synth_tuning_dump(synth, 0, 1, full_name, sizeof(full_name), NULL));
    TEST_ASSERT(memcmp(full_name, expected_name, sizeof(expected_name)) == 0);

    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    return EXIT_SUCCESS;
}
