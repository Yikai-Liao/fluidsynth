#include "test.h"
#include <fluidsynth.h>
#include <math.h>

enum { SAMPLE_COUNT = 4096 };

static fluid_synth_t *new_test_synth(fluid_settings_t **settings)
{
    fluid_synth_t *synth;
    int sfont_id;

    *settings = new_fluid_settings();
    TEST_ASSERT(*settings != NULL);
    TEST_SUCCESS(fluid_settings_setint(*settings, "synth.reverb.active", 0));
    TEST_SUCCESS(fluid_settings_setint(*settings, "synth.chorus.active", 0));
    synth = new_fluid_synth(*settings);
    TEST_ASSERT(synth != NULL);
    sfont_id = fluid_synth_sfload(synth, TEST_SOUNDFONT, 1);
    TEST_ASSERT(sfont_id != FLUID_FAILED);
    TEST_SUCCESS(fluid_synth_program_select(synth, 0, sfont_id, 0, 0));
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 100));
    return synth;
}

int main(void)
{
    static const int lengths[] = {1, 63, 128, 511, 2048, 1345};
    fluid_settings_t *reference_settings, *planar_settings;
    fluid_synth_t *reference = new_test_synth(&reference_settings);
    fluid_synth_t *planar = new_test_synth(&planar_settings);
    float reference_left[SAMPLE_COUNT];
    float reference_right[SAMPLE_COUNT];
    float planar_left[SAMPLE_COUNT];
    float planar_right[SAMPLE_COUNT];
    float effect_left_0[SAMPLE_COUNT], effect_right_0[SAMPLE_COUNT];
    float effect_left_1[SAMPLE_COUNT], effect_right_1[SAMPLE_COUNT];
    float *left[] = {planar_left};
    float *right[] = {planar_right};
    float *effect_left[] = {effect_left_0, effect_left_1};
    float *effect_right[] = {effect_right_0, effect_right_1};
    int offset = 0;
    int i, j;

    for(i = 0; i < (int)(sizeof(lengths) / sizeof(lengths[0])); i++)
    {
        int len = lengths[i];
        TEST_SUCCESS(fluid_synth_write_float(reference, len,
                     reference_left + offset, 0, 1,
                     reference_right + offset, 0, 1));
        TEST_SUCCESS(fluid_synth_nwrite_float(planar, len,
                     left, right, effect_left, effect_right));

        for(j = 0; j < len; j++)
        {
            TEST_ASSERT(fabsf(reference_left[offset + j] - planar_left[offset + j]) < 1e-6f);
            TEST_ASSERT(fabsf(reference_right[offset + j] - planar_right[offset + j]) < 1e-6f);
        }

        offset += len;
        left[0] = planar_left + offset;
        right[0] = planar_right + offset;
        effect_left[0] = effect_left_0 + offset;
        effect_right[0] = effect_right_0 + offset;
        effect_left[1] = effect_left_1 + offset;
        effect_right[1] = effect_right_1 + offset;
    }

    TEST_ASSERT(offset == SAMPLE_COUNT);
    TEST_ASSERT(fabsf(planar_left[128]) > 1e-6f);

    delete_fluid_synth(reference);
    delete_fluid_settings(reference_settings);
    delete_fluid_synth(planar);
    delete_fluid_settings(planar_settings);
    return EXIT_SUCCESS;
}
