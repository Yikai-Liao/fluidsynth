#include "test.h"
#include "fluidsynth.h"
#include "synth/fluid_synth.h"
#include "synth/fluid_chan.h"

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

/* Candidate order/membership must match the old raw-channel array scan. */
static void check_channel_index(fluid_synth_t *synth)
{
    int chan, i, next;
    for(chan = 0; chan < synth->midi_channels; chan++)
    {
        next = fluid_channel_next_voice(synth->channel[chan], 0);
        for(i = 0; i < synth->polyphony; i++)
        {
            if(chan == NO_CHANNEL || synth->voice[i]->chan == chan)
            {
                TEST_ASSERT(next == i);
                next = fluid_channel_next_voice(synth->channel[chan], i + 1);
            }
        }
        TEST_ASSERT(next == -1);
    }
}

struct modulation_probe { fluid_synth_t *synth; int calls; };
static double shrink_on_modulation(const fluid_mod_t *mod, int value, int range,
                                   int is_src1, void *data)
{
    struct modulation_probe *probe = data;
    TEST_ASSERT(fluid_mod_get_amount(mod) == ++probe->calls);
    TEST_SUCCESS(fluid_synth_set_polyphony(probe->synth, 2));
    return 0;
}

static void test_channel_index(fluid_settings_t *settings, fluid_sample_t *sample)
{
    fluid_synth_t *synth;
    fluid_voice_t *voice;
    int i;
    fluid_mod_t mod = {0};
    struct modulation_probe probe;
    TEST_SUCCESS(fluid_sample_set_loop(sample, 8, 240));
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.polyphony", 96));
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.midi-channels", 256));
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);
    voice = fluid_synth_alloc_voice(synth, sample, 2, -1, 100);
    TEST_ASSERT(voice != NULL && fluid_synth_get_active_voice_count(synth) == 0);
    TEST_SUCCESS(fluid_synth_pitch_bend(synth, 2, 12288)); /* unstarted, invalid key */
    TEST_ASSERT(voice->gen[GEN_FINETUNE].mod != 0);
    TEST_SUCCESS(fluid_synth_cc(synth, 2, 121, 0));
    TEST_ASSERT(voice->gen[GEN_FINETUNE].mod == 0);
    TEST_SUCCESS(fluid_synth_set_polyphony(synth, 129)); /* grow with live membership */
    check_channel_index(synth);
    for(i = 0; i < 65; i++) /* sparse channels crossing word boundaries */
    {
        voice = fluid_synth_alloc_voice(synth, sample, i % 3, 60, 100);
        TEST_ASSERT(voice != NULL);
        fluid_voice_gen_set(voice, GEN_SAMPLEMODE, 1);
        fluid_synth_start_voice(synth, voice);
    }
    TEST_SUCCESS(fluid_synth_pitch_bend(synth, 1, 12288));
    for(i = 0; i < 65; i++)
        TEST_ASSERT((synth->voice[i]->gen[GEN_FINETUNE].mod != 0) == (i % 3 == 1));
    /* A custom mapping callback may shrink the logical pool mid-iteration. */
    probe.synth = synth;
    probe.calls = 0;
    fluid_mod_set_source1(&mod, FLUID_MOD_PITCHWHEEL, FLUID_MOD_CUSTOM);
    fluid_mod_set_dest(&mod, GEN_PAN);
    fluid_mod_set_custom_mapping(&mod, shrink_on_modulation, &probe);
    for(i = 0; i < 2; i++)
    {
        fluid_mod_set_amount(&mod, i + 1);
        fluid_voice_add_mod(synth->voice[1 + 3 * i], &mod, FLUID_VOICE_ADD);
    }
    TEST_SUCCESS(fluid_synth_pitch_bend(synth, 1, 9000));
    TEST_ASSERT(probe.calls == 1); /* slot1 first; slot4 now outside polyphony */
    synth->voice[1]->mod_count--;
    synth->voice[4]->mod_count--;
    TEST_SUCCESS(fluid_synth_set_polyphony(synth, 33));
    check_channel_index(synth);
    TEST_SUCCESS(fluid_synth_set_polyphony(synth, 65));
    check_channel_index(synth);
    TEST_SUCCESS(fluid_synth_system_reset(synth));
    check_channel_index(synth); /* reset must not erase pending membership */
    TEST_SUCCESS(fluid_synth_process(synth, 256, 0, NULL, 0, NULL));
    TEST_ASSERT(fluid_synth_get_active_voice_count(synth) == 0);
    check_channel_index(synth);
    TEST_SUCCESS(fluid_synth_set_polyphony(synth, 1));
    synth->voice[0]->gen[GEN_FINETUNE].mod = 1;
    TEST_SUCCESS(fluid_synth_pitch_bend(synth, 255, 12288)); /* NO_CHANNEL fallback */
    TEST_ASSERT(synth->voice[0]->gen[GEN_FINETUNE].mod == 0);
    delete_fluid_synth(synth);
}

static void test_exclusive_channels(fluid_settings_t *settings, fluid_sample_t *sample)
{
    fluid_synth_t *synth;
    fluid_voice_t *voices[5], *voice, *sentinel;
    int i;
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.polyphony", 16));
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.midi-channels", 256));
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);
    for(i = 0; i < 5; i++)
    {
        voices[i] = fluid_synth_alloc_voice(synth, sample, i == 1, 60, 100);
        TEST_ASSERT(voices[i] != NULL);
        voices[i]->id = i == 3 ? 2 : 1;
        fluid_synth_start_voice(synth, voices[i]); /* start before assigning class */
        fluid_voice_gen_set(voices[i], GEN_EXCLUSIVECLASS, i == 2 ? 7.5f : 7);
    }
    fluid_voice_gen_set(voices[4], GEN_EXCLUSIVECLASS, 0);
    voices[4]->gen[GEN_EXCLUSIVECLASS].mod = 7; /* effective class, not just val */
    fluid_voice_noteoff(voices[4]); /* released but still playing must be visited */
    voice = fluid_synth_alloc_voice(synth, sample, 0, 61, 100);
    TEST_ASSERT(voice != NULL);
    voice->id = 2;
    fluid_voice_gen_set(voice, GEN_EXCLUSIVECLASS, 7.5f); /* new class truncates to 7 */
    fluid_synth_start_voice(synth, voice);
    for(i = 0; i < 5; i++)
        TEST_ASSERT((voices[i]->gen[GEN_VOLENVRELEASE].val == -2000) == (i == 0 || i == 4));
    TEST_ASSERT(voices[4]->gen[GEN_EXCLUSIVECLASS].mod == 7);

    sentinel = fluid_synth_alloc_voice(synth, sample, 255, 62, 100);
    TEST_ASSERT(sentinel != NULL);
    sentinel->id = 3;
    fluid_synth_start_voice(synth, sentinel);
    fluid_voice_gen_set(sentinel, GEN_EXCLUSIVECLASS, 9);
    voice = fluid_synth_alloc_voice(synth, sample, 255, 63, 100);
    TEST_ASSERT(voice != NULL);
    voice->id = 4;
    fluid_voice_gen_set(voice, GEN_EXCLUSIVECLASS, 9);
    fluid_synth_start_voice(synth, voice);
    TEST_ASSERT(sentinel->gen[GEN_EXCLUSIVECLASS].val == 0); /* raw255 fallback */
    delete_fluid_synth(synth);
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

    delete_fluid_synth(synth);
    test_channel_index(settings, sample);
    test_exclusive_channels(settings, sample);
    delete_fluid_sample(sample);
    delete_fluid_settings(settings);
    return EXIT_SUCCESS;
}
