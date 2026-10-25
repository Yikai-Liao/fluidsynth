#include "test.h"
#include "fluidsynth.h"
#include "fluidsynth/gen.h"
#include "fluidsynth/voice.h"
#include <float.h>
#include <math.h>

int main(void)
{
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth;
    float left[128], right[128];
    float old_attack;
    fluid_voice_t *voices[512];
    int dls_sfid;
    int i;
    static const int interp_methods[] =
    {
        FLUID_INTERP_NONE,
        FLUID_INTERP_LINEAR,
        FLUID_INTERP_4THORDER,
        FLUID_INTERP_MID,
        FLUID_INTERP_HIGH,
        FLUID_INTERP_HIGHEST
    };

    TEST_ASSERT(settings != NULL);
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);
    TEST_ASSERT(fluid_synth_sfload(synth, TEST_SOUNDFONT, 1) != FLUID_FAILED);
    TEST_SUCCESS(fluid_synth_program_change(synth, 0, 127));
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 100));
    TEST_ASSERT(fluid_synth_get_active_voice_count(synth) > 0);

    /* Extreme finite exclusive classes must not be cast to int until their
     * range is checked. */
    TEST_SUCCESS(fluid_synth_set_gen(synth, 0, GEN_EXCLUSIVECLASS, -FLT_MAX));
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 61, 100));
    TEST_SUCCESS(fluid_synth_set_gen(synth, 0, GEN_EXCLUSIVECLASS, FLT_MAX));
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 62, 100));

    TEST_ASSERT(fluid_synth_set_gen(synth, 0, GEN_VOLENVATTACK, NAN) == FLUID_FAILED);
    TEST_ASSERT(fluid_synth_set_gen(synth, 0, GEN_VOLENVATTACK, INFINITY) == FLUID_FAILED);
    TEST_ASSERT(fluid_synth_set_gen(synth, 0, GEN_VOLENVATTACK, -INFINITY) == FLUID_FAILED);

    TEST_SUCCESS(fluid_synth_set_gen(synth, 0, GEN_PITCH, FLT_MAX));
    TEST_SUCCESS(fluid_synth_write_float(synth, 128, left, 0, 1, right, 0, 1));
    TEST_ASSERT(fluid_synth_get_active_voice_count(synth) > 0);
    TEST_SUCCESS(fluid_synth_set_gen(synth, 0, GEN_PITCH, -FLT_MAX));
    TEST_SUCCESS(fluid_synth_write_float(synth, 128, left, 0, 1, right, 0, 1));
    TEST_ASSERT(fluid_synth_get_active_voice_count(synth) > 0);

    TEST_SUCCESS(fluid_synth_set_gen(synth, 0, GEN_STARTADDROFS, FLT_MAX));
    TEST_SUCCESS(fluid_synth_set_gen(synth, 0, GEN_STARTADDRCOARSEOFS, FLT_MAX));
    TEST_SUCCESS(fluid_synth_set_gen(synth, 0, GEN_ENDADDROFS, -FLT_MAX));
    TEST_SUCCESS(fluid_synth_set_gen(synth, 0, GEN_ENDADDRCOARSEOFS, -FLT_MAX));
    TEST_SUCCESS(fluid_synth_set_gen(synth, 0, GEN_STARTLOOPADDROFS, FLT_MAX));
    TEST_SUCCESS(fluid_synth_set_gen(synth, 0, GEN_STARTLOOPADDRCOARSEOFS, FLT_MAX));
    TEST_SUCCESS(fluid_synth_set_gen(synth, 0, GEN_ENDLOOPADDROFS, -FLT_MAX));
    TEST_SUCCESS(fluid_synth_set_gen(synth, 0, GEN_ENDLOOPADDRCOARSEOFS, -FLT_MAX));
    TEST_SUCCESS(fluid_synth_write_float(synth, 128, left, 0, 1, right, 0, 1));

    /* Extreme finite tuning and fixed-velocity offsets must stay within the
     * MIDI key and velocity ranges before conversion or mapping. */
    TEST_SUCCESS(fluid_synth_all_sounds_off(synth, -1));
    TEST_SUCCESS(fluid_synth_system_reset(synth));
    TEST_SUCCESS(fluid_synth_program_change(synth, 0, 0));
    TEST_SUCCESS(fluid_synth_set_gen(synth, 0, GEN_FINETUNE, FLT_MAX));
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 100));
    TEST_SUCCESS(fluid_synth_all_sounds_off(synth, -1));
    TEST_SUCCESS(fluid_synth_system_reset(synth));
    TEST_SUCCESS(fluid_synth_program_change(synth, 0, 0));
    TEST_SUCCESS(fluid_synth_set_gen(synth, 0, GEN_VELOCITY, FLT_MAX));
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 100));
    fluid_synth_get_voicelist(synth, voices, 512, -1);
    TEST_ASSERT(voices[0] != NULL);
    TEST_ASSERT(fluid_voice_get_actual_velocity(voices[0]) == 127);
    old_attack = fluid_voice_gen_get(voices[0], GEN_VOLENVATTACK);
    fluid_voice_gen_set(voices[0], GEN_VOLENVATTACK, NAN);
    fluid_voice_gen_incr(voices[0], GEN_VOLENVATTACK, INFINITY);
    TEST_ASSERT(fluid_voice_gen_get(voices[0], GEN_VOLENVATTACK) == old_attack);
    TEST_SUCCESS(fluid_synth_write_float(synth, 128, left, 0, 1, right, 0, 1));

    /* Both preset loaders use the tuned pitch for key-range selection. */
    TEST_SUCCESS(fluid_synth_all_sounds_off(synth, -1));
    TEST_SUCCESS(fluid_synth_system_reset(synth));
    TEST_SUCCESS(fluid_synth_program_change(synth, 0, 0));
    for(i = 0; i < (int)(sizeof(interp_methods) / sizeof(interp_methods[0])); i++)
    {
        TEST_SUCCESS(fluid_synth_set_interp_method(synth, 0, interp_methods[i]));
        TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 100));
        TEST_SUCCESS(fluid_synth_set_gen(synth, 0, GEN_FINETUNE, -194642.0f));
        TEST_SUCCESS(fluid_synth_write_float(synth, 128, left, 0, 1, right, 0, 1));
        TEST_SUCCESS(fluid_synth_all_sounds_off(synth, -1));
        TEST_SUCCESS(fluid_synth_system_reset(synth));
    }

    dls_sfid = fluid_synth_sfload(synth, TEST_DLS, 1);
    TEST_ASSERT(dls_sfid != FLUID_FAILED);
    TEST_SUCCESS(fluid_synth_program_select(synth, 0, dls_sfid, 0, 0));
    TEST_SUCCESS(fluid_synth_set_gen(synth, 0, GEN_FINETUNE, FLT_MAX));
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 100));

    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    return EXIT_SUCCESS;
}
