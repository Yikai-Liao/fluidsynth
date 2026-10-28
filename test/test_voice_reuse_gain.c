#include "test.h"
#include "fluidsynth.h"
#include "sfloader/fluid_sfont.h"
#include "synth/fluid_chan.h"
#include "synth/fluid_synth.h"

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

static int empty_noteon(fluid_preset_t *preset, fluid_synth_t *synth,
                        int chan, int key, int vel)
{
    return FLUID_OK;
}

static void test_legato_fromkey(fluid_synth_t *synth, fluid_sample_t *sample)
{
    fluid_channel_t *channel = synth->channel[0];
    fluid_preset_t preset = {0}, *saved_preset = channel->preset;
    fluid_voice_t **voices = synth->voice, *voice;
    preset.noteon = empty_noteon;
    channel->preset = &preset;
    /* An invalid fromkey must not inspect any voice: char must not turn 255
     * into -1, which the INVALID_NOTE comparison would treat as valid. */
    synth->voice = NULL;
    TEST_SUCCESS(fluid_synth_noteon_monopoly_legato(synth, 0, INVALID_NOTE, 61, 100));
    synth->voice = voices;
    TEST_ASSERT(synth->fromkey_portamento == INVALID_NOTE);

    voice = start_voice(synth, sample, 0);
    fluid_channel_set_cc(channel, PORTAMENTO_CTRL, 60);
    TEST_SUCCESS(fluid_synth_noteon_monopoly_legato(synth, 0, INVALID_NOTE, 61, 100));
    TEST_ASSERT(!fluid_voice_is_on(voice));
    TEST_ASSERT(synth->fromkey_portamento == 60);
    TEST_ASSERT(fluid_channel_get_cc(channel, PORTAMENTO_CTRL) == INVALID_NOTE);
    channel->preset = saved_preset;
    stop_voice(synth);
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
    test_legato_fromkey(synth, sample);
    delete_fluid_synth(synth);
    delete_fluid_sample(sample);
    delete_fluid_settings(settings);
    return EXIT_SUCCESS;
}
