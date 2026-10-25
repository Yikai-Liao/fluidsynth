#include "test.h"
#include "fluidsynth.h"

enum { RenderFrames = 128, RenderBlocksBeforeUpdate = 10, LoopOffsetUpdates = 3 };

static void render_block(fluid_synth_t *synth, float *left, float *right)
{
    TEST_SUCCESS(fluid_synth_write_float(synth, RenderFrames, left, 0, 1,
                                         right, 0, 1));
}

int main(void)
{
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth;
    float left[RenderFrames];
    float right[RenderFrames];
    int i;

    TEST_ASSERT(settings != NULL);
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.reverb.active", 0));
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.chorus.active", 0));
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);
    TEST_ASSERT(fluid_synth_sfload(synth, TEST_SOUNDFONT, 1) != FLUID_FAILED);
    TEST_SUCCESS(fluid_synth_program_change(synth, 0, 0));
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 96));
    TEST_ASSERT(fluid_synth_get_active_voice_count(synth) > 0);

    for(i = 0; i < RenderBlocksBeforeUpdate; ++i)
    {
        render_block(synth, left, right);
    }
    /* Shrink the loop out of range while the voice is already inside it. */
    for(i = 0; i < LoopOffsetUpdates; ++i)
    {
        TEST_SUCCESS(fluid_synth_set_gen(synth, 0, GEN_ENDLOOPADDRCOARSEOFS,
                                         -65536.0f));
        render_block(synth, left, right);
    }

    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    return 0;
}
