#include "test.h"
#include "fluidsynth.h"
#include "fluidsynth/voice.h"

#include <stdint.h>
#include <limits.h>
#include <float.h>
#include <math.h>

#include "fluid_voice.h"

int main(void)
{
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth;
    fluid_voice_t *voices[512];
    fluid_voice_t *voice;
    float attenuation;
    double pitches[128];
    float left[64], right[64];
    int i;

    TEST_ASSERT(settings != NULL);
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);
    TEST_ASSERT(fluid_synth_sfload(synth, TEST_SOUNDFONT, 1) != FLUID_FAILED);
    TEST_SUCCESS(fluid_synth_program_change(synth, 0, 0));
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 100));

    fluid_synth_get_voicelist(synth, voices, 512, -1);
    TEST_ASSERT(voices[0] != NULL);
    voice = voices[0];
    attenuation = fluid_voice_gen_get(voice, GEN_ATTENUATION);

    /* The public voice helpers must reject indices outside the generator
     * array instead of reading adjacent voice fields or writing past it. */
    fluid_voice_gen_set(voice, GEN_LAST, 12.0f);
    TEST_ASSERT(voice->callback == NULL);
    fluid_voice_gen_incr(voice, INT_MAX, 1.0f);
    TEST_ASSERT(voice->callback == NULL);
    TEST_ASSERT(fluid_voice_gen_get(voice, GEN_LAST) == 0.0f);
    TEST_ASSERT(fluid_voice_gen_get(voice, -1) == 0.0f);
    fluid_voice_update_param(voice, GEN_LAST);
    fluid_voice_update_param(voice, INT_MAX);
    TEST_ASSERT(fluid_voice_gen_get(voice, GEN_ATTENUATION) == attenuation);

    /* Root overrides reach tuning lookup through the public voice API, even
     * when the SoundFont itself contains only valid keys. */
    for(i = 0; i < 128; ++i)
    {
        pitches[i] = i * 100.0;
    }
    TEST_SUCCESS(fluid_synth_activate_key_tuning(synth, 0, 0, "root bounds", pitches, 1));
    TEST_SUCCESS(fluid_synth_activate_tuning(synth, 0, 0, 0, 1));
    fluid_voice_gen_set(voice, GEN_OVERRIDEROOTKEY, 200.0f);
    fluid_voice_update_param(voice, GEN_OVERRIDEROOTKEY);
    TEST_ASSERT(voice->root_key == 12700.0f);
    fluid_voice_gen_set(voice, GEN_OVERRIDEROOTKEY, FLT_MAX);
    fluid_voice_update_param(voice, GEN_OVERRIDEROOTKEY);
    TEST_ASSERT(voice->root_key == 12700.0f);
    TEST_SUCCESS(fluid_synth_write_float(synth, 64, left, 0, 1, right, 0, 1));
    for(i = 0; i < 64; ++i)
    {
        TEST_ASSERT(isfinite(left[i]) && isfinite(right[i]));
    }

    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    return EXIT_SUCCESS;
}
