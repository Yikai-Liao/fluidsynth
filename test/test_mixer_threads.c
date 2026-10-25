#include "test.h"
#include "fluidsynth.h"
#include <math.h>

enum { SAMPLE_COUNT = 512, NOTE_COUNT = 32 };

static void render(int cores, float *left, float *right)
{
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth;
    int sfid, i;

    TEST_ASSERT(settings != NULL);
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.cpu-cores", cores));
    TEST_SUCCESS(fluid_settings_setint(settings, "audio.realtime-prio", 0));
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);

    sfid = fluid_synth_sfload(synth, TEST_SOUNDFONT, 1);
    TEST_ASSERT(sfid >= 0);
    TEST_SUCCESS(fluid_synth_program_select(synth, 0, sfid, 0, 127));

    for(i = 0; i < NOTE_COUNT; ++i)
    {
        TEST_SUCCESS(fluid_synth_noteon(synth, 0, 36 + i, 100));
    }

    TEST_ASSERT(fluid_synth_get_active_voice_count(synth) > 8);
    TEST_SUCCESS(fluid_synth_write_float(synth, SAMPLE_COUNT, left, 0, 1, right, 0, 1));

    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
}

int main(void)
{
#if ENABLE_MIXER_THREADS
    float single_left[SAMPLE_COUNT], single_right[SAMPLE_COUNT];
    float threaded_left[SAMPLE_COUNT], threaded_right[SAMPLE_COUNT];
    int i;

    render(1, single_left, single_right);
    render(4, threaded_left, threaded_right);

    for(i = 0; i < SAMPLE_COUNT; ++i)
    {
        TEST_ASSERT(fabsf(single_left[i] - threaded_left[i]) < 1e-3f);
        TEST_ASSERT(fabsf(single_right[i] - threaded_right[i]) < 1e-3f);
    }
#endif

    return EXIT_SUCCESS;
}
