#include "test.h"
#include "fluidsynth.h"

enum { Polyphony = 64 };

/* Loop the note's samples at full sustain to isolate exclusive-class cuts. */
static int hold_note(fluid_synth_t *synth, int key)
{
    fluid_voice_t *voices[Polyphony];
    int i, count = 0;

    fluid_synth_get_voicelist(synth, voices, Polyphony, -1);
    for(i = 0; i < Polyphony && voices[i] != NULL; i++)
    {
        if(fluid_voice_get_key(voices[i]) == key)
        {
            fluid_voice_gen_set(voices[i], GEN_SAMPLEMODE, 1);
            fluid_voice_gen_set(voices[i], GEN_VOLENVSUSTAIN, 0);
            fluid_voice_update_param(voices[i], GEN_VOLENVSUSTAIN);
            count++;
        }
    }
    TEST_ASSERT(count > 0);
    return count;
}

/* Count the voices still playing a note after rendering. */
static int count_voices(fluid_synth_t *synth, int key)
{
    fluid_voice_t *voices[Polyphony];
    int i, count = 0;
    fluid_synth_get_voicelist(synth, voices, Polyphony, -1);
    for(i = 0; i < Polyphony && voices[i] != NULL; i++)
    {
        count += fluid_voice_get_key(voices[i]) == key;
    }
    return count;
}

/* Changing preset must preserve the old note; returning must still cut it. */
static void test_preset_scope(fluid_synth_t *synth, int first_id, int other_id,
                              int bank, int program, int use_start)
{
    fluid_preset_t *preset;
    int first_count, other_count;

    TEST_SUCCESS(fluid_synth_all_sounds_off(synth, 0));
    TEST_SUCCESS(fluid_synth_process(synth, 128, 0, NULL, 0, NULL));
    TEST_SUCCESS(fluid_synth_program_select(synth, 0, first_id, 0, 0));
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 100));
    first_count = hold_note(synth, 60);

    if(use_start)
    {
        /* Start a preset independently of the channel selection. */
        preset = fluid_sfont_get_preset(fluid_synth_get_sfont_by_id(synth, other_id),
                                       bank, program);
        TEST_ASSERT(preset != NULL);
        TEST_SUCCESS(fluid_synth_start(synth, 1000, preset, 0, 0, 46, 100));
    }
    else
    {
        TEST_SUCCESS(fluid_synth_program_select(synth, 0, other_id, bank, program));
        TEST_SUCCESS(fluid_synth_noteon(synth, 0, 46, 100));
    }

    other_count = hold_note(synth, 46);
    TEST_SUCCESS(fluid_synth_process(synth, 44100, 0, NULL, 0, NULL));
    TEST_ASSERT(count_voices(synth, 60) == first_count);
    TEST_ASSERT(count_voices(synth, 46) == other_count);
    TEST_SUCCESS(fluid_synth_program_select(synth, 0, first_id, 0, 0));
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 67, 100));
    TEST_SUCCESS(fluid_synth_process(synth, 44100, 0, NULL, 0, NULL));
    TEST_ASSERT(count_voices(synth, 60) == 0);
    TEST_ASSERT(count_voices(synth, 46) == other_count);
}

int main(void)
{
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth;
    int first_id, second_id, use_start;

    TEST_ASSERT(settings != NULL);
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.polyphony", Polyphony));
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.sample-rate", 44100));
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
