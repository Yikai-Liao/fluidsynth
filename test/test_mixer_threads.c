#include "test.h"
#include "fluidsynth.h"
#include <math.h>

enum { SAMPLE_COUNT = 512, NOTE_COUNT = 32, CHANGING_SAMPLE_COUNT = 16384 };

/* Cross the parallel-rendering threshold, finish voices, and submit more work
 * to sleeping workers. Compare every block with single-threaded synthesis. */
static void render_changing_voices(int cores, float *left, float *right)
{
    static const int block_sizes[] = {64, 192, 256, 512};
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth;
    int sfid, frame = 0, round, note;

    TEST_ASSERT(settings != NULL);
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.cpu-cores", cores));
    TEST_SUCCESS(fluid_settings_setint(settings, "audio.realtime-prio", 0));
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);
    sfid = fluid_synth_sfload(synth, TEST_SOUNDFONT, 1);
    TEST_ASSERT(sfid >= 0);
    TEST_SUCCESS(fluid_synth_program_select(synth, 0, sfid, 0, 127));

    for(round = 0; round < 64; ++round)
    {
        int block = block_sizes[round % 4];

        if(round % 8 == 0)
        {
            int notes = round % 16 == 0 ? 7 : 65;

            TEST_SUCCESS(fluid_synth_all_sounds_off(synth, 0));
            for(note = 0; note < notes; ++note)
            {
                TEST_SUCCESS(fluid_synth_noteon(synth, 0, 32 + note, 100));
            }
        }
        else if(round % 8 == 4)
        {
            TEST_SUCCESS(fluid_synth_all_notes_off(synth, 0));
        }
        else if(round % 8 == 6)
        {
            TEST_SUCCESS(fluid_synth_all_sounds_off(synth, 0));
        }

        TEST_SUCCESS(fluid_synth_write_float(synth, block, left, frame, 1,
                                             right, frame, 1));
        frame += block;
    }

    TEST_ASSERT(frame == CHANGING_SAMPLE_COUNT);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
}

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

    {
        float reference_left[CHANGING_SAMPLE_COUNT], reference_right[CHANGING_SAMPLE_COUNT];
        float actual_left[CHANGING_SAMPLE_COUNT], actual_right[CHANGING_SAMPLE_COUNT];
        int cores;

        render_changing_voices(1, reference_left, reference_right);
        for(cores = 2; cores <= 8; cores *= 2)
        {
            render_changing_voices(cores, actual_left, actual_right);
            for(i = 0; i < CHANGING_SAMPLE_COUNT; ++i)
            {
                TEST_ASSERT(fabsf(reference_left[i] - actual_left[i]) < 1e-3f);
                TEST_ASSERT(fabsf(reference_right[i] - actual_right[i]) < 1e-3f);
            }
        }
    }
#endif

    return EXIT_SUCCESS;
}
