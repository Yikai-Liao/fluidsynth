#include "test.h"
#include "fluidsynth.h"

static int count_on_voices(fluid_synth_t *synth)
{
    fluid_voice_t *voices[64];
    int count = 0;
    int i;

    fluid_synth_get_voicelist(synth, voices, 64, -1);

    for(i = 0; i < 64 && voices[i] != NULL; ++i)
    {
        if(fluid_voice_is_on(voices[i]))
        {
            ++count;
        }
    }

    return count;
}

int main(void)
{
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth;
    double pitches[128];
    float left[1024], right[1024];
    int sfid;
    int i;

    TEST_ASSERT(settings != NULL);
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);
    sfid = fluid_synth_sfload(synth, TEST_SOUNDFONT, 1);
    TEST_ASSERT(sfid != FLUID_FAILED);
    TEST_SUCCESS(fluid_synth_program_select(synth, 0, sfid, 0, 123));
    TEST_SUCCESS(fluid_synth_cc(synth, 0, 126, 0));
    TEST_SUCCESS(fluid_synth_set_legato_mode(synth, 0, FLUID_CHANNEL_LEGATO_MODE_MULTI_RETRIGGER));

    for(i = 0; i < 128; ++i)
    {
        pitches[i] = i * 100.0;
    }
    pitches[71] = 6000.0; /* Tune note 71 into the sample zone for note 60. */
    TEST_SUCCESS(fluid_synth_activate_key_tuning(synth, 0, 0, "split", pitches, 1));

    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 70, 100));
    TEST_SUCCESS(fluid_synth_write_float(synth, 512, left, 0, 1, right, 0, 1));
    TEST_ASSERT(count_on_voices(synth) == 2);

    /* The tuned key selects a different sample zone. Old-zone voices should
     * enter release, leaving only the two voices from the selected zone on. */
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 71, 100));
    TEST_SUCCESS(fluid_synth_write_float(synth, 512, left, 0, 1, right, 0, 1));
    TEST_ASSERT(count_on_voices(synth) == 2);

    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    return EXIT_SUCCESS;
}
