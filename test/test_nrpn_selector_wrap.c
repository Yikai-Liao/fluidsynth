#include "test.h"
#include <fluidsynth.h>
#include <fluidsynth/synth.h>

static void exercise_awe32_nrpn(fluid_synth_t *synth)
{
    int gen;
    int pitch_bend;

    TEST_SUCCESS(fluid_synth_sfload(synth, TEST_SOUNDFONT, 1));
    TEST_SUCCESS(fluid_synth_program_change(synth, 0, 0));
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 100));
    TEST_SUCCESS(fluid_synth_cc(synth, 0, 99, 127)); /* AWE32 NRPN selector */

    /* Exercise the conversion and realtime update for each valid AWE32 target. */
    for(gen = 0; gen <= 26; ++gen)
    {
        TEST_SUCCESS(fluid_synth_cc(synth, 0, 98, gen));
        TEST_SUCCESS(fluid_synth_cc(synth, 0, 6, 64));
        TEST_SUCCESS(fluid_synth_cc(synth, 0, 38, 127));
    }

    TEST_SUCCESS(fluid_synth_get_pitch_bend(synth, 0, &pitch_bend));
    TEST_ASSERT(pitch_bend == 8319);
}

int main(void)
{
    fluid_settings_t *settings;
    fluid_synth_t *synth;
    float filter_cutoff;
    int i;

    settings = new_fluid_settings();
    TEST_ASSERT(settings != NULL);
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);

    exercise_awe32_nrpn(synth);

    TEST_SUCCESS(fluid_synth_cc(synth, 0, 99, 120)); /* SoundFont NRPN selector */

    /* Advance the unsigned generator selector to 2^32 + 8. */
    for(i = 0; i < 429496; ++i)
    {
        TEST_SUCCESS(fluid_synth_cc(synth, 0, 98, 102)); /* +10000 */
    }
    for(i = 0; i < 73; ++i)
    {
        TEST_SUCCESS(fluid_synth_cc(synth, 0, 98, 100)); /* +100 */
    }
    TEST_SUCCESS(fluid_synth_cc(synth, 0, 98, 4)); /* +4; generator 8 after wrap */
    TEST_SUCCESS(fluid_synth_cc(synth, 0, 6, 127));  /* apply to selected generator */

    filter_cutoff = fluid_synth_get_gen(synth, 0, 8);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);

    TEST_ASSERT(filter_cutoff == 0.0f); /* invalid oversized NRPN must not select generator 8 */
    return EXIT_SUCCESS;
}
