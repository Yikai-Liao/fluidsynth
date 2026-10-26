#include "test.h"
#include "fluidsynth.h"
#include "sfloader/fluid_defsfont.h"
#include "sfloader/fluid_sffile.h"
#include <string.h>

enum { SAMPLE_COUNT = 512, AUDIO_COUNT = 1024 };

static void render_sample(unsigned char original_pitch, float *audio)
{
    SFSample header = {0};
    fluid_sample_t sample = {0};
    fluid_sfont_t sfont = {0};
    fluid_defsfont_t defsfont = {0};
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth;
    fluid_voice_t *voice;
    short pcm[SAMPLE_COUNT];
    double pitches[128];
    int i;

    TEST_ASSERT(settings != NULL);
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.reverb.active", 0));
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.chorus.active", 0));
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);

    for(i = 0; i < SAMPLE_COUNT; ++i)
    {
        pcm[i] = (i % 32 - 16) * 1000;
    }
    header.end = SAMPLE_COUNT;
    header.loopstart = 8;
    header.loopend = SAMPLE_COUNT - 8;
    header.samplerate = 44100;
    header.origpitch = original_pitch;
    header.sampletype = FLUID_SAMPLETYPE_MONO;
    defsfont.sfont = &sfont;
    defsfont.samplesize = sizeof(pcm);
    sample.data = pcm;
    TEST_SUCCESS(fluid_sample_import_sfont(&sample, &header, &defsfont));

    for(i = 0; i < 128; ++i)
    {
        pitches[i] = i * 100.0;
    }
    pitches[60] += 37.0;
    TEST_SUCCESS(fluid_synth_activate_key_tuning(synth, 0, 1, "sample root", pitches, 0));
    TEST_SUCCESS(fluid_synth_activate_tuning(synth, 0, 0, 1, 1));
    voice = fluid_synth_alloc_voice(synth, &sample, 0, 64, 100);
    TEST_ASSERT(voice != NULL);
    fluid_synth_start_voice(synth, voice);
    TEST_SUCCESS(fluid_synth_write_float(synth, SAMPLE_COUNT, audio, 0, 2, audio, 1, 2));

    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
}

int main(void)
{
    float unpitched[AUDIO_COUNT], pitched[AUDIO_COUNT];
    int i, audible = 0;

    /* SF2 originalPitch=255 must sound like root key 60 with tuning active. */
    render_sample(255, unpitched);
    render_sample(60, pitched);
    TEST_ASSERT(memcmp(unpitched, pitched, sizeof(unpitched)) == 0);
    for(i = 0; i < AUDIO_COUNT; ++i)
    {
        audible |= pitched[i] != 0.0f;
    }
    TEST_ASSERT(audible);
    return EXIT_SUCCESS;
}
