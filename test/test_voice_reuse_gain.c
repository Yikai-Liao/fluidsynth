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

struct batch_case {
    fluid_synth_t *synth;
    int kind, count, fast_seen, native_fast;
    unsigned int order[1024];
};
static void batch_callback(const fluid_voice_t *voice, int reason, void *data)
{
    struct batch_case *test = data;
    if(reason == FLUID_VOICE_CALLBACK_NOTEOFF)
    {
        TEST_ASSERT(test->count < 1024);
        test->order[test->count++] = fluid_voice_get_id(voice);
    }
}
/* Both arms use the same native timer boundary; the reference selects the
 * original array fallback. Compare every output sample and callback. */
static int run_batch_case(void *data, unsigned int msec)
{
    struct batch_case *test = data;
    fluid_synth_t *synth = test->synth;
    fluid_voice_t *voices[64] = {0};
    int i, j;
    if(!test->native_fast) synth->note_batch = 0;
    for(i = 0; i < 16; ++i)
    {
        TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 100));
        if(test->kind == 2 && i == 4) TEST_SUCCESS(fluid_synth_cc(synth, 0, 64, 127));
        TEST_SUCCESS(fluid_synth_noteoff(synth, 0, 60));
        test->fast_seen |= synth->note_batch == 2;
        if(test->kind == 1 && i == 0)
        {
            fluid_synth_get_voicelist(synth, voices, 64, -1);
            for(j = 0; j < 64 && voices[j]; ++j)
                fluid_voice_set_callback(voices[j], batch_callback, test);
        }
        if(i == 8)
        {
            if(test->kind == 2) TEST_SUCCESS(fluid_synth_cc(synth, 0, 64, 0));
            if(test->kind == 3) TEST_SUCCESS(fluid_synth_set_gen(synth, 0, GEN_VOLENVRELEASE, 1000));
            if(test->kind == 4) TEST_SUCCESS(fluid_synth_all_sounds_off(synth, 0));
            if(test->kind == 5) fluid_synth_process_event_queue(synth);
            if(test->kind == 6) TEST_SUCCESS(fluid_synth_set_polyphony(synth, 256));
        }
    }
    fluid_synth_release_voice_on_same_note_LOCAL(synth, 0, -1);
    return 0;
}
static void test_note_batches(void)
{
    int kind, mode, block;
    for(kind = 0; kind < 8; ++kind)
    {
        fluid_settings_t *settings = new_fluid_settings();
        struct batch_case test[2] = {{0}};
        float pcm[2][Channels][Frames];
        fluid_sample_timer_t *timer[2];
        TEST_SUCCESS(fluid_settings_setint(settings, "synth.polyphony", 128));
        TEST_SUCCESS(fluid_settings_setint(settings, "synth.reverb.active", 0));
        TEST_SUCCESS(fluid_settings_setint(settings, "synth.chorus.active", 0));
        TEST_SUCCESS(fluid_settings_setint(settings, "audio.realtime-prio", 0));
        for(mode = 0; mode < 2; ++mode)
        {
            test[mode].synth = new_fluid_synth(settings);
            test[mode].kind = kind;
            test[mode].native_fast = mode;
            TEST_ASSERT(test[mode].synth != NULL);
            TEST_ASSERT(fluid_synth_sfload(test[mode].synth, TEST_SOUNDFONT, 1) >= 0);
        }
        /* An already-progressing release must be rearmed on each later batch. */
        for(mode = 0; mode < 2; ++mode)
        {
            TEST_SUCCESS(fluid_synth_noteon(test[mode].synth, 0, 60, 100));
            TEST_SUCCESS(fluid_synth_process(test[mode].synth, 1024, 0, NULL, 0, NULL));
            TEST_SUCCESS(fluid_synth_noteoff(test[mode].synth, 0, 60));
            TEST_SUCCESS(fluid_synth_process(test[mode].synth, 128, 0, NULL, 0, NULL));
            if(kind == 7) TEST_SUCCESS(fluid_synth_set_polyphony(test[mode].synth, 64));
            timer[mode] = new_fluid_sample_timer(test[mode].synth, run_batch_case, &test[mode]);
            TEST_ASSERT(timer[mode] != NULL);
        }
        for(block = 0; block < 16; ++block)
        {
            for(mode = 0; mode < 2; ++mode) render(test[mode].synth, pcm[mode]);
            TEST_ASSERT(memcmp(pcm[0], pcm[1], sizeof(pcm[0])) == 0);
            if(block == 7)
                for(mode = 0; mode < 2; ++mode)
                {
                    delete_fluid_sample_timer(test[mode].synth, timer[mode]);
                    timer[mode] = new_fluid_sample_timer(test[mode].synth, run_batch_case, &test[mode]);
                    TEST_ASSERT(timer[mode] != NULL);
                }
        }
        TEST_ASSERT(test[0].count == test[1].count);
        TEST_ASSERT(memcmp(test[0].order, test[1].order, test[0].count * sizeof(unsigned int)) == 0);
        TEST_ASSERT(test[1].fast_seen == (kind != 7) && !test[0].fast_seen);
        for(mode = 0; mode < 2; ++mode)
        {
            delete_fluid_sample_timer(test[mode].synth, timer[mode]);
            delete_fluid_synth(test[mode].synth);
        }
        delete_fluid_settings(settings);
    }
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
    delete_fluid_sample(sample);
    delete_fluid_settings(settings);
    test_note_batches();
    return EXIT_SUCCESS;
}
