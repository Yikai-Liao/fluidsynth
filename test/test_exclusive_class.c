#include "test.h"
#include "fluidsynth.h"

enum { Polyphony = 64 };

/* Keep a note's voices so subsequent exclusive-class cuts can be checked. */
static int remember_voices(fluid_synth_t *synth, fluid_voice_t **voices, int key)
{
    int i, count = 0;

    fluid_synth_get_voicelist(synth, voices, Polyphony, -1);
    for(i = 0; i < Polyphony && voices[i] != NULL; i++)
    {
        if(fluid_voice_get_key(voices[i]) == key)
        {
            fluid_voice_gen_set(voices[i], GEN_VOLENVRELEASE, 1200);
            fluid_voice_update_param(voices[i], GEN_VOLENVRELEASE);
            voices[count++] = voices[i];
        }
    }
    TEST_ASSERT(count > 0);
    return count;
}

/* Changing preset must preserve the old note; returning must still cut it. */
static void test_preset_scope(fluid_synth_t *synth, int first_id, int other_id,
                              int bank, int program, int use_start)
{
    fluid_voice_t *first[Polyphony], *other[Polyphony];
    fluid_preset_t *preset;
    int first_count, other_count, i;

    TEST_SUCCESS(fluid_synth_all_sounds_off(synth, 0));
    TEST_SUCCESS(fluid_synth_process(synth, 128, 0, NULL, 0, NULL));
    TEST_SUCCESS(fluid_synth_program_select(synth, 0, first_id, 0, 0));
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 100));
    first_count = remember_voices(synth, first, 60);

    if(use_start)
    {
        /* This preset is not selected on the channel. */
        preset = fluid_sfont_get_preset(fluid_synth_get_sfont_by_id(synth, other_id),
                                       bank, program);
        TEST_ASSERT(preset != NULL);
        TEST_SUCCESS(fluid_synth_start(synth, 1000, preset, 0, 0, 64, 100));
    }
    else
    {
        TEST_SUCCESS(fluid_synth_program_select(synth, 0, other_id, bank, program));
        TEST_SUCCESS(fluid_synth_noteon(synth, 0, 64, 100));
    }

    for(i = 0; i < first_count; i++)
    {
        TEST_ASSERT(fluid_voice_gen_get(first[i], GEN_VOLENVRELEASE) == 1200);
    }

    /* Give the other preset's voices the same release marker. */
    other_count = remember_voices(synth, other, 64);
    TEST_SUCCESS(fluid_synth_program_select(synth, 0, first_id, 0, 0));
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 67, 100));
    for(i = 0; i < first_count; i++)
    {
        TEST_ASSERT(fluid_voice_gen_get(first[i], GEN_VOLENVRELEASE) == -2000);
    }
    for(i = 0; i < other_count; i++)
    {
        TEST_ASSERT(fluid_voice_gen_get(other[i], GEN_VOLENVRELEASE) == 1200);
    }
}

int main(void)
{
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth;
    int first_id, second_id, use_start;

    TEST_ASSERT(settings != NULL);
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.polyphony", Polyphony));
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.reverb.active", 0));
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.chorus.active", 0));
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);
    TEST_SUCCESS(first_id = fluid_synth_sfload(synth, TEST_SOUNDFONT, 0));
    TEST_SUCCESS(second_id = fluid_synth_sfload(synth, TEST_SOUNDFONT, 0));
    TEST_SUCCESS(fluid_synth_set_gen(synth, 0, GEN_EXCLUSIVECLASS, 1));

    for(use_start = 0; use_start <= 1; use_start++)
    {
        test_preset_scope(synth, first_id, first_id, 0, 1, use_start);
        test_preset_scope(synth, first_id, first_id, 128, 0, use_start);
        test_preset_scope(synth, first_id, second_id, 0, 0, use_start);
    }

    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    return EXIT_SUCCESS;
}
