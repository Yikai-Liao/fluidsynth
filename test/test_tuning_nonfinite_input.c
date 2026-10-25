#include <math.h>

#include "test.h"
#include "fluidsynth.h"

int main(void)
{
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth;
    double key_pitch[128];
    double octave_pitch[12] = { 0.0 };
    int key = 60;
    int i;
    double pitch = NAN;

    TEST_ASSERT(settings != NULL);
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);

    for(i = 0; i < 128; ++i)
    {
        key_pitch[i] = i * 100.0;
    }

    key_pitch[key] = NAN;
    TEST_ASSERT(fluid_synth_activate_key_tuning(synth, 0, 1, "bad key tuning",
               key_pitch, 0) == FLUID_FAILED);

    octave_pitch[5] = INFINITY;
    TEST_ASSERT(fluid_synth_activate_octave_tuning(synth, 0, 2, "bad octave tuning",
               octave_pitch, 0) == FLUID_FAILED);

    TEST_ASSERT(fluid_synth_tune_notes(synth, 0, 3, 1, &key, &pitch, 0) == FLUID_FAILED);

    key_pitch[key] = 6050.0;
    octave_pitch[5] = -12.5;
    pitch = 6050.0;
    TEST_SUCCESS(fluid_synth_activate_key_tuning(synth, 0, 1, "finite key tuning",
                  key_pitch, 0));
    TEST_SUCCESS(fluid_synth_activate_octave_tuning(synth, 0, 2, "finite octave tuning",
                  octave_pitch, 0));
    TEST_SUCCESS(fluid_synth_tune_notes(synth, 0, 3, 1, &key, &pitch, 0));

    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    return EXIT_SUCCESS;
}
