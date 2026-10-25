#include "test.h"
#include <SDL3/SDL.h>
#include <fluidsynth.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

static int asprintf_calls;
static int poison_next_audio_device_list;
static int playback_device_list_calls;
static int fake_enumeration;

void *__real_SDL_malloc(size_t size);
SDL_AudioDeviceID *__real_SDL_GetAudioPlaybackDevices(int *count);
const char *__real_SDL_GetAudioDeviceName(SDL_AudioDeviceID devid);

int __wrap_SDL_asprintf(char **strp, const char *fmt, ...)
{
    va_list args;
    int result;

    if(fake_enumeration && ++asprintf_calls == 2)
    {
        /* SDL leaves this output undefined on failure. It must not be freed. */
        *strp = (char *)(uintptr_t)1;
        return -1;
    }

    va_start(args, fmt);
    result = SDL_vasprintf(strp, fmt, args);
    va_end(args);
    return result;
}

void *__wrap_SDL_malloc(size_t size)
{
    void *memory = __real_SDL_malloc(size);

    if(fake_enumeration && memory != NULL && poison_next_audio_device_list)
    {
        memset(memory, 0xA5, size);
        poison_next_audio_device_list = 0;
    }

    return memory;
}

SDL_AudioDeviceID *__wrap_SDL_GetAudioPlaybackDevices(int *count)
{
    SDL_AudioDeviceID *devices;

    if(!fake_enumeration)
    {
        return __real_SDL_GetAudioPlaybackDevices(count);
    }

    devices = __real_SDL_malloc(2 * sizeof(*devices));

    playback_device_list_calls++;

    if(devices != NULL)
    {
        devices[0] = 1;
        devices[1] = 2;
        *count = 2;
        poison_next_audio_device_list = 1;
    }
    else
    {
        *count = 0;
    }

    return devices;
}

const char *__wrap_SDL_GetAudioDeviceName(SDL_AudioDeviceID devid)
{
    return fake_enumeration ? (devid == 1 ? "first" : "second")
                            : __real_SDL_GetAudioDeviceName(devid);
}

int main(void)
{
    fluid_settings_t *settings;
    fluid_settings_t *failed_enumeration_settings;
    fluid_synth_t *synth;
    fluid_audio_driver_t *driver;

    TEST_ASSERT(SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy"));
    TEST_ASSERT(SDL_Init(SDL_INIT_AUDIO));
    settings = new_fluid_settings();
    TEST_ASSERT(settings != NULL);
    TEST_SUCCESS(fluid_settings_setstr(settings, "audio.driver", "sdl3"));
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);

    /* Settings enumeration must also ignore an undefined failed output. */
    fake_enumeration = 1;
    asprintf_calls = 0;
    playback_device_list_calls = 0;
    failed_enumeration_settings = new_fluid_settings();
    TEST_ASSERT(failed_enumeration_settings != NULL);
    TEST_ASSERT(playback_device_list_calls == 1);
    TEST_ASSERT(asprintf_calls == 2);
    delete_fluid_settings(failed_enumeration_settings);

    /* Isolate the enumeration performed by the audio-driver constructor from
       any SDL device enumeration during settings initialization. */
    fake_enumeration = 1;
    asprintf_calls = 0;
    playback_device_list_calls = 0;
    driver = new_fluid_audio_driver(settings, synth);
    TEST_ASSERT(driver != NULL);
    TEST_ASSERT(playback_device_list_calls == 1);
    TEST_ASSERT(asprintf_calls == 2);
    delete_fluid_audio_driver(driver);

    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    SDL_Quit();
    return 0;
}
