#include "test.h"
#include "fluidsynth.h"

/* Two blocks cover the gain ramp and its steady state. */
enum { Frames = 128, SampleFrames = 256, Channels = 2 };

static fluid_voice_t *start_voice(fluid_synth_t *synth, fluid_sample_t *sample,
                                 float pan)
{
    fluid_voice_t *voice = fluid_synth_alloc_voice(synth, sample, 0, 60, 127);
    TEST_ASSERT(voice != NULL);

    fluid_voice_gen_set(voice, GEN_PAN, pan);
    fluid_synth_start_voice(synth, voice);
    return voice;
}

static void render(fluid_synth_t *synth, float output[Channels][Frames])
{
    TEST_SUCCESS(fluid_synth_write_float(synth, Frames, output[0], 0, 1,
                                        output[1], 0, 1));
}

static void stop_voice(fluid_synth_t *synth)
{
    TEST_SUCCESS(fluid_synth_all_sounds_off(synth, 0));
    TEST_SUCCESS(fluid_synth_process(synth, Frames, 0, NULL, 0, NULL));
    TEST_ASSERT(fluid_synth_get_active_voice_count(synth) == 0);
}

/* Render a held looping note through a period of zero master gain. */
static void test_zero_gain(fluid_synth_t *synth, fluid_sample_t *sample, int muted_at_start)
{
    fluid_voice_t *voice;
    float output[Channels][Frames];
    int channel, frame, audible = 0;

    fluid_synth_set_gain(synth, muted_at_start ? 0.0f : 0.2f);
    voice = fluid_synth_alloc_voice(synth, sample, 0, 60, 127);
    TEST_ASSERT(voice != NULL);
    fluid_voice_gen_set(voice, GEN_SAMPLEMODE, 1);
    fluid_synth_start_voice(synth, voice);
    render(synth, output);

    fluid_synth_set_gain(synth, 0.0f);
    TEST_SUCCESS(fluid_synth_process(synth, 4096, 0, NULL, 0, NULL));
    render(synth, output);
    for(channel = 0; channel < Channels; channel++)
    {
        for(frame = 0; frame < Frames; frame++)
        {
            TEST_ASSERT(output[channel][frame] == 0.0f);
        }
    }
    TEST_ASSERT(fluid_synth_get_active_voice_count(synth) == 1);

    fluid_synth_set_gain(synth, 0.2f);
    render(synth, output);
    for(channel = 0; channel < Channels; channel++)
    {
        for(frame = 0; frame < Frames; frame++)
        {
            audible += output[channel][frame] != 0;
        }
    }
    TEST_ASSERT(audible > 0);

    fluid_synth_set_gain(synth, 0.0f);
    TEST_SUCCESS(fluid_synth_all_notes_off(synth, 0));
    TEST_SUCCESS(fluid_synth_process(synth, 4096, 0, NULL, 0, NULL));
    TEST_ASSERT(fluid_synth_get_active_voice_count(synth) == 0);
}

int main(void)
{
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth;
    fluid_sample_t *sample = new_fluid_sample();
    fluid_voice_t *voice, *reused_voice;
    short data[SampleFrames];
    float reference[Channels][Frames], reused[Channels][Frames];
    int channel, frame;

    TEST_ASSERT(settings != NULL);
    TEST_ASSERT(sample != NULL);
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.sample-rate", 44100));
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.reverb.active", 0));
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.chorus.active", 0));
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.polyphony", 1));
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);

    for(frame = 0; frame < SampleFrames; frame++)
    {
        data[frame] = (frame % 32 < 16) ? 10000 : -10000;
    }
    TEST_SUCCESS(fluid_sample_set_sound_data(sample, data, NULL, SampleFrames, 44100, 1));
    TEST_SUCCESS(fluid_sample_set_pitch(sample, 60, 0));

    voice = start_voice(synth, sample, -200);
    render(synth, reference);
    stop_voice(synth);

    /* A previous note's pan must not affect the next note's onset. */
    reused_voice = start_voice(synth, sample, 400);
    TEST_ASSERT(reused_voice == voice);
    render(synth, reused);
    stop_voice(synth);

    reused_voice = start_voice(synth, sample, -200);
    TEST_ASSERT(reused_voice == voice);
    render(synth, reused);

    /* A reused voice must have the same onset ramp as a fresh voice. */
    for(channel = 0; channel < Channels; channel++)
    {
        int audible_samples = 0;
        for(frame = 0; frame < Frames; frame++)
        {
            TEST_ASSERT(reference[channel][frame] == reused[channel][frame]);
            audible_samples += reference[channel][frame] != 0;
        }
        TEST_ASSERT(audible_samples > 0);
    }

    stop_voice(synth);
    delete_fluid_synth(synth);

    /* Finished voices exceeding the (later reduced) polyphony limit must still be retired. */
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.polyphony", 4));
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);
    for(frame = 0; frame < 4; frame++)
    {
        start_voice(synth, sample, 0);
    }
    TEST_ASSERT(fluid_synth_get_active_voice_count(synth) == 4);
    TEST_SUCCESS(fluid_synth_set_polyphony(synth, 1));
    render(synth, reused);
    stop_voice(synth);

    /* Test held notes and natural sample completion with zero master gain. */
    TEST_SUCCESS(fluid_sample_set_loop(sample, 32, SampleFrames - 32));
    TEST_SUCCESS(fluid_voice_optimize_sample(sample));
    test_zero_gain(synth, sample, 0);
    test_zero_gain(synth, sample, 1);
    start_voice(synth, sample, 0);
    TEST_SUCCESS(fluid_synth_process(synth, 512, 0, NULL, 0, NULL));
    TEST_ASSERT(fluid_synth_get_active_voice_count(synth) == 0);

    delete_fluid_synth(synth);
    delete_fluid_sample(sample);
    delete_fluid_settings(settings);
    return EXIT_SUCCESS;
}
