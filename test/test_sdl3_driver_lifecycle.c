#include "test.h"
#include <SDL3/SDL.h>
#include <fluidsynth.h>

#ifdef FLUID_TEST_WRAP_SDL_DESTROY
static int destroyed_streams;

void __real_SDL_DestroyAudioStream(SDL_AudioStream *stream);

void __wrap_SDL_DestroyAudioStream(SDL_AudioStream *stream)
{
    TEST_ASSERT(stream != NULL);
    destroyed_streams++;
    __real_SDL_DestroyAudioStream(stream);
}
#endif

int main(void)
{
    fluid_settings_t *settings;
    fluid_synth_t *synth;
    SDL_AudioDeviceID *devices;
    int count, i;

    TEST_ASSERT(SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy"));
    TEST_ASSERT(SDL_Init(SDL_INIT_AUDIO));
    devices = SDL_GetAudioPlaybackDevices(&count);
    TEST_ASSERT(devices != NULL && count > 0);
    SDL_free(devices);

    settings = new_fluid_settings();
    TEST_ASSERT(settings != NULL);
    TEST_SUCCESS(fluid_settings_setstr(settings, "audio.driver", "sdl3"));
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);

    for(i = 0; i < 3; i++)
    {
        fluid_audio_driver_t *driver = new_fluid_audio_driver(settings, synth);
        TEST_ASSERT(driver != NULL);
        delete_fluid_audio_driver(driver);
#ifdef FLUID_TEST_WRAP_SDL_DESTROY
        /* SDL_Quit must not be responsible for freeing the driver's stream. */
        TEST_ASSERT(destroyed_streams == i + 1);
#endif
    }

    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    SDL_Quit();
    return EXIT_SUCCESS;
}
