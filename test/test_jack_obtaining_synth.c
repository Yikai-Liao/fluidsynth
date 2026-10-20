
#include "test.h"
#include "fluidsynth.h"
#include "fluid_adriver.h"
#if JACK_SUPPORT
#include "fluid_jack.h"
#endif

// The jack driver may need the synth instance to adjust the sample-rate in case it mismatches with
// the sample-rate of the jack driver. However, new_fluid_audio_driver2() does not receive a synth pointer.
// Thus looking up the synth instance must be done via the settings object.
int main(void)
{
#if JACK_SUPPORT
    fluid_jack_client_t client = {0};
    fluid_synth_t *obtained_synth;
    fluid_synth_t *expected_synth;
    fluid_settings_t *settings = new_fluid_settings();
    TEST_ASSERT(settings != NULL);

    fluid_atomic_int_set(&client.audio_sample_rate, 48000);
    TEST_ASSERT(fluid_jack_driver_srate(48000, &client) == 0);
    TEST_ASSERT(fluid_atomic_int_get(&client.sample_rate) == 48000);
    TEST_ASSERT(fluid_jack_driver_srate(44100, &client) == 0);
    TEST_SUCCESS(fluid_jack_driver_process(64, &client));
    /* Publish an opaque audio pointer only after the rate callback, as activation does. */
    fluid_atomic_pointer_set(&client.audio_driver, (fluid_jack_audio_driver_t *)&client);
    TEST_ASSERT(fluid_jack_driver_process(64, &client) == FLUID_FAILED);
    fluid_atomic_pointer_set(&client.audio_driver, NULL);

    expected_synth = new_fluid_synth(settings);
    TEST_ASSERT(expected_synth != NULL);

    TEST_SUCCESS(fluid_jack_obtain_synth(settings, &obtained_synth));
    TEST_ASSERT(obtained_synth == expected_synth);

    delete_fluid_synth(obtained_synth);
    delete_fluid_settings(settings);

    obtained_synth = expected_synth = NULL;

    settings = new_fluid_settings();
    TEST_ASSERT(settings != NULL);
    TEST_ASSERT(fluid_jack_obtain_synth(settings, &obtained_synth) == FLUID_FAILED);
    delete_fluid_settings(settings);
#endif
    return EXIT_SUCCESS;
}
