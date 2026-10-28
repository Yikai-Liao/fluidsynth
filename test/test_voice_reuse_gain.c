#include "test.h"
#include "fluidsynth.h"
#include "synth/fluid_synth.h"

/* Compare every selection, including full-pool ties/failure, to the old scan. */
static fluid_voice_t *alloc_voice_checked(fluid_synth_t *synth, fluid_sample_t *sample)
{
    fluid_voice_t *expected = NULL, *voice;
    float best = OVERFLOW_PRIO_CANNOT_KILL - 1, score;
    int i;
    fluid_synth_get_active_voice_count(synth); /* drain completions before oracle */
    for(i = 0; i < synth->polyphony; i++)
    {
        voice = synth->voice[i];
        if(_AVAILABLE(voice)) { expected = voice; break; }
        score = fluid_voice_get_overflow_prio(voice, &synth->overflow, fluid_synth_get_ticks(synth));
        if(score < best) { best = score; expected = voice; }
    }
    voice = fluid_synth_alloc_voice(synth, sample, 0, 60, 127);
    TEST_ASSERT(voice == expected);
    return voice;
}

/* Two blocks cover the gain ramp and its steady state. */
enum { Frames = 128, SampleFrames = 256, Channels = 2 };

static fluid_voice_t *start_voice(fluid_synth_t *synth, fluid_sample_t *sample,
                                 float pan)
{
    fluid_voice_t *voice = alloc_voice_checked(synth, sample);
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

static void test_free_voice_hint(fluid_synth_t *synth, fluid_sample_t *sample)
{
    fluid_voice_t *voices[4], *voice;
    int i;
    stop_voice(synth);
    TEST_SUCCESS(fluid_synth_set_polyphony(synth, 4));
    TEST_SUCCESS(fluid_sample_set_loop(sample, 8, 240));
    for(i = 0; i < 4; i++)
    {
        voices[i] = alloc_voice_checked(synth, sample);
        TEST_ASSERT(alloc_voice_checked(synth, sample) == voices[i]); /* not started */
        fluid_voice_gen_set(voices[i], GEN_SAMPLEMODE, 1);
        fluid_synth_start_voice(synth, voices[i]);
    }
    TEST_SUCCESS(fluid_synth_process(synth, Frames, 0, NULL, 0, NULL));
    fluid_voice_off(voices[1]);
    fluid_synth_get_active_voice_count(synth); /* publish the private off request */
    TEST_SUCCESS(fluid_synth_process(synth, Frames, 0, NULL, 0, NULL));
    TEST_ASSERT(alloc_voice_checked(synth, sample) == voices[1]); /* lower hole */
    fluid_synth_start_voice(synth, voices[1]);
    TEST_SUCCESS(fluid_synth_set_polyphony(synth, 2));
    TEST_SUCCESS(fluid_synth_set_polyphony(synth, 12));
    voice = alloc_voice_checked(synth, sample);
    TEST_ASSERT(voice != NULL);
    fluid_synth_start_voice(synth, voice);
    stop_voice(synth);
    TEST_SUCCESS(fluid_synth_set_polyphony(synth, 1));
    start_voice(synth, sample, 0);
    fluid_synth_start_voice(synth, alloc_voice_checked(synth, sample)); /* overflow */
    TEST_ASSERT(alloc_voice_checked(synth, sample) == NULL); /* both rvoices busy */
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
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.polyphony", 8));
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);
    TEST_SUCCESS(fluid_synth_set_polyphony(synth, 1)); /* retain a sufficient event FIFO */

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

    test_free_voice_hint(synth, sample);
    delete_fluid_synth(synth);
    delete_fluid_sample(sample);
    delete_fluid_settings(settings);
    return EXIT_SUCCESS;
}
