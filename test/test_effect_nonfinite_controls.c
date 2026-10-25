#include <math.h>

#include "test.h"
#include "fluidsynth.h"

enum { RENDER_FRAMES = 512 };

static int render_finite(fluid_synth_t *synth)
{
    float left[RENDER_FRAMES];
    float right[RENDER_FRAMES];
    int i;

    if(fluid_synth_write_float(synth, RENDER_FRAMES, left, 0, 1,
                               right, 0, 1) != FLUID_OK)
    {
        return 0;
    }

    for(i = 0; i < RENDER_FRAMES; i++)
    {
        if(!isfinite(left[i]) || !isfinite(right[i]))
        {
            return 0;
        }
    }

    return 1;
}

int main(void)
{
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth;
    int sfid;

    TEST_ASSERT(settings != NULL);
    TEST_ASSERT(fluid_settings_setnum(settings, "synth.chorus.level", NAN) == FLUID_FAILED);
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);

    sfid = fluid_synth_sfload(synth, TEST_SOUNDFONT, 1);
    TEST_ASSERT(sfid != FLUID_FAILED);
    TEST_SUCCESS(fluid_synth_program_change(synth, 0, 0));
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 100));
    TEST_ASSERT(render_finite(synth));

    /* Non-finite effect values pass ordinary range comparisons unless they
     * are rejected explicitly. A bad value must not poison later audio. */
    TEST_ASSERT(fluid_synth_set_chorus_group_level(synth, 0, NAN) == FLUID_FAILED);
    TEST_ASSERT(render_finite(synth));
    TEST_ASSERT(fluid_synth_set_reverb_group_level(synth, 0, NAN) == FLUID_FAILED);
    TEST_ASSERT(render_finite(synth));
    TEST_ASSERT(fluid_synth_set_chorus(synth, 3, NAN, 0.3, 8.0,
                                       FLUID_CHORUS_MOD_SINE) == FLUID_FAILED);
    TEST_ASSERT(render_finite(synth));
    TEST_ASSERT(fluid_synth_set_reverb(synth, NAN, 0.5, 50.0, 0.5) == FLUID_FAILED);
    TEST_ASSERT(render_finite(synth));

    fluid_synth_set_gain(synth, NAN);
    TEST_ASSERT(render_finite(synth));
    TEST_ASSERT(isfinite(fluid_synth_get_gain(synth)));

    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    return EXIT_SUCCESS;
}
