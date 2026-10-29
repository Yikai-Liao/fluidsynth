#include "test.h"
#include "fluidsynth.h"
#include "sfloader/fluid_sfont.h"
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

struct owner_callback_state { fluid_voice_t *expected; int calls; };
static void owner_finished_callback(const fluid_voice_t *voice, int reason, void *data)
{
    struct owner_callback_state *state = data;
    TEST_ASSERT(reason == FLUID_VOICE_CALLBACK_FINISHED);
    TEST_ASSERT(voice == state->expected);
    state->calls++;
}

/* Render without an intervening API completion drain, so delayed FIFO records
 * and the physical primary/overflow role can be checked separately. */
static void owner_render(fluid_synth_t *synth)
{
    fluid_rvoice_eventhandler_flush(synth->eventhandler);
    TEST_ASSERT(fluid_synth_render_blocks(synth, 1) == 1);
}

static fluid_voice_t *owner_start(fluid_synth_t *synth, fluid_sample_t *sample)
{
    fluid_voice_t *v = fluid_synth_alloc_voice(synth, sample, 0, 60, 127);
    TEST_ASSERT(v != NULL);
    fluid_voice_gen_set(v, GEN_SAMPLEMODE, 1);
    fluid_synth_start_voice(synth, v);
    return v;
}

#if FLUID_THREAD_SAFE_CAPABLE
struct owner_reader { fluid_rvoice_t *rvoice; fluid_voice_t *owner; int ready, done; };
static fluid_thread_return_t read_immutable_owner(void *data)
{
    struct owner_reader *test = data;
    fluid_atomic_int_set(&test->ready, 1);
    do { TEST_ASSERT(test->rvoice->finished_cb_voice == test->owner); }
    while(!fluid_atomic_int_get(&test->done));
    return FLUID_THREAD_RETURN_VALUE;
}
#endif

static void test_finished_owner_immutable(void)
{
    fluid_voice_t *v = new_fluid_voice(NULL, 44100, NULL);
    fluid_rvoice_param_t p[MAX_EVENT_PARAMS] = {{0}};
    int i;
#if FLUID_THREAD_SAFE_CAPABLE
    struct owner_reader test = {0};
    fluid_thread_t *thread;
    TEST_ASSERT(v != NULL);
    test.rvoice = v->rvoice;
    test.owner = v;
    thread = new_fluid_thread("owner-read", read_immutable_owner,
                              &test, 0, FALSE);
    TEST_ASSERT(thread != NULL);
    while(!fluid_atomic_int_get(&test.ready)) fluid_msleep(1);
#endif
    TEST_ASSERT(v != NULL && v->slot_index == -1 &&
                v->rvoice->finished_cb_voice == v &&
                v->overflow_rvoice->finished_cb_voice == v);
    for(i = 0; i < 10000; ++i)
    {
        /* Slot1 is now redundant: private raw-rvoice arbitrary callback args
         * are not the constructed-owner contract. It must never rewrite owner. */
        p[1].ptr = NULL;
        p[2].ptr = v;
        fluid_rvoice_set_finished_callback(v->rvoice, p);
        TEST_ASSERT(v->rvoice->finished_cb_voice == v);
        fluid_rvoice_reset(v->rvoice, p);
        TEST_ASSERT(v->rvoice->finished_cb_voice == v);
        TEST_ASSERT(v->rvoice->finished_cb == NULL && v->rvoice->finished_cb_data == NULL);
    }
#if FLUID_THREAD_SAFE_CAPABLE
    fluid_atomic_int_set(&test.done, 1);
    fluid_thread_join(thread);
    delete_fluid_thread(thread);
#endif
    delete_fluid_voice(v);
}

static void test_finished_owner(fluid_settings_t *settings)
{
    fluid_synth_t *synth, *foreign;
    fluid_sample_t *sample = new_fluid_sample();
    short data[SampleFrames];
    fluid_voice_t *v, *voices[4];
    fluid_rvoice_t *old, raw = {0};
    struct owner_callback_state callback = {NULL, 0}, replaced = {NULL, 0};
    int i;
    TEST_ASSERT(sample != NULL);
    for(i = 0; i < SampleFrames; ++i) data[i] = i % 32 < 16 ? 10000 : -10000;
    TEST_SUCCESS(fluid_sample_set_sound_data(sample, data, NULL, SampleFrames, 44100, 1));
    TEST_SUCCESS(fluid_sample_set_pitch(sample, 60, 0));
    TEST_SUCCESS(fluid_sample_set_loop(sample, 8, 240));
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.polyphony", 8));
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL && sample->refcount == 0);
    for(i = 0; i < synth->nvoice; ++i)
    {
        TEST_ASSERT(synth->voice[i]->slot_index == i);
        TEST_ASSERT(synth->voice[i]->rvoice->finished_cb_voice == synth->voice[i]);
        TEST_ASSERT(synth->voice[i]->overflow_rvoice->finished_cb_voice == synth->voice[i]);
    }
    TEST_SUCCESS(fluid_synth_set_polyphony(synth, 1));
    v = owner_start(synth, sample);
    owner_render(synth);
    fluid_voice_off(v);
    owner_render(synth);
    TEST_ASSERT(!v->can_access_rvoice && sample->refcount == 1);
    TEST_ASSERT(fluid_synth_get_active_voice_count(synth) == 0);
    TEST_ASSERT(v->can_access_rvoice && sample->refcount == 0);

    /* A real overflow keeps one owner, but completion acts on the old physical
     * rvoice's CURRENT role, not the role at callback registration. */
    TEST_ASSERT(owner_start(synth, sample) == v);
    callback.expected = replaced.expected = v;
    fluid_voice_set_callback(v, owner_finished_callback, &replaced);
    fluid_voice_set_callback(v, owner_finished_callback, &callback);
    owner_render(synth);
    old = v->rvoice;
    TEST_ASSERT(owner_start(synth, sample) == v);
    TEST_ASSERT(v->overflow_rvoice == old && v->rvoice != old);
    TEST_ASSERT(old->finished_cb_voice == v && v->rvoice->finished_cb_voice == v);
    owner_render(synth);
    TEST_ASSERT(callback.calls == 1 && replaced.calls == 0);
    TEST_ASSERT(!v->can_access_overflow_rvoice && sample->refcount == 2);
    TEST_ASSERT(fluid_synth_get_active_voice_count(synth) == 1);
    TEST_ASSERT(v->can_access_overflow_rvoice && !v->can_access_rvoice);
    TEST_ASSERT(sample->refcount == 1 && v->sample == sample && v->overflow_sample == NULL);
    TEST_ASSERT(v->rvoice->finished_cb == NULL && v->rvoice->finished_cb_data == NULL);
    fluid_voice_set_callback(v, owner_finished_callback, &callback);
    fluid_voice_set_callback(v, NULL, NULL);
    fluid_voice_off(v);
    owner_render(synth);
    TEST_ASSERT(callback.calls == 1 && fluid_synth_get_active_voice_count(synth) == 0);
    TEST_ASSERT(sample->refcount == 0);

    /* NULL and live foreign owners were unmatched in the old scan as well. */
    foreign = new_fluid_synth(settings);
    TEST_ASSERT(foreign != NULL);
    fluid_rvoice_eventhandler_finished_voice_callback(synth->eventhandler, &raw);
    fluid_rvoice_eventhandler_finished_voice_callback(synth->eventhandler, foreign->voice[0]->rvoice);
    TEST_ASSERT(fluid_synth_get_active_voice_count(synth) == 0);
    TEST_ASSERT(foreign->active_voice_count == 0 && foreign->voice[0]->status == FLUID_VOICE_CLEAN);
    delete_fluid_synth(foreign);

    /* Growth may relocate the POINTER array, never either physical owner.
     * Shrink then regrow before publication: completions are now in range. */
    TEST_SUCCESS(fluid_synth_set_polyphony(synth, 4));
    for(i = 0; i < 4; ++i) voices[i] = owner_start(synth, sample);
    owner_render(synth);
    TEST_SUCCESS(fluid_synth_set_polyphony(synth, 2));
    TEST_SUCCESS(fluid_synth_set_polyphony(synth, 12));
    owner_render(synth);
    TEST_ASSERT(fluid_synth_get_active_voice_count(synth) == 2 && sample->refcount == 2);
    for(i = 0; i < synth->nvoice; ++i)
    {
        TEST_ASSERT(synth->voice[i]->slot_index == i);
        TEST_ASSERT(synth->voice[i]->rvoice->finished_cb_voice == synth->voice[i]);
        TEST_ASSERT(synth->voice[i]->overflow_rvoice->finished_cb_voice == synth->voice[i]);
    }
    TEST_SUCCESS(fluid_synth_all_sounds_off(synth, 0)); owner_render(synth);
    TEST_ASSERT(fluid_synth_get_active_voice_count(synth) == 0 && sample->refcount == 0);

    /* Preserve old logical-shrink behavior, even its retained API accounting:
     * an already-finished rvoice outside logical P is ignored, not stopped. */
    TEST_SUCCESS(fluid_synth_set_polyphony(synth, 4));
    for(i = 0; i < 4; ++i) voices[i] = owner_start(synth, sample);
    owner_render(synth);
    for(i = 0; i < 3; ++i) fluid_voice_off(voices[i]);
    owner_render(synth);
    TEST_ASSERT(fluid_synth_get_active_voice_count(synth) == 1);
    TEST_SUCCESS(fluid_synth_set_polyphony(synth, 2));
    owner_render(synth);
    TEST_ASSERT(fluid_synth_get_active_voice_count(synth) == 1);
    TEST_ASSERT(!voices[3]->can_access_rvoice && voices[3]->sample == sample);
    TEST_ASSERT(sample->refcount == 1);
    TEST_SUCCESS(fluid_synth_set_polyphony(synth, 12));
    TEST_ASSERT(!voices[3]->can_access_rvoice); /* ignored FIFO entry isn't replayed */
    delete_fluid_synth(synth);
    TEST_ASSERT(sample->refcount == 0);
    delete_fluid_sample(sample);
    test_finished_owner_immutable();
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
    test_finished_owner(settings);
    delete_fluid_sample(sample);
    delete_fluid_settings(settings);
    return EXIT_SUCCESS;
}
