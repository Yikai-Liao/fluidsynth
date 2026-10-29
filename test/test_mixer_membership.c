#include "test.h"
#include "rvoice/fluid_rvoice_event.h"
#include "rvoice/fluid_rev.h"
#include "synth/fluid_synth.h"
#include "synth/fluid_voice.h"

enum { VOICES = 16, EVENTS = 64 };
struct fixture
{
    fluid_rvoice_eventhandler_t *handler;
    fluid_rvoice_t voices[VOICES];
    int active[VOICES], count, polyphony;
    int pending[EVENTS], pending_count;
    int events[EVENTS], event_count, event_pos;
    int slow[VOICES], callback_seen[VOICES];
};

/* The original scan is the reference, including its j++ after a swap. */
static void reference_finish(struct fixture *f, int voice)
{
    int j;
    for(j = 0; j < f->count; ++j)
    {
        if(f->active[j] == voice)
        {
            --f->count;
            if(j < f->count) f->active[j] = f->active[f->count];
        }
    }
}

static void check_indices(struct fixture *f)
{
    int v, j;
    for(v = 0; v < VOICES; ++v)
    {
        int expected = -1;
        for(j = 0; j < f->count; ++j)
            if(f->active[j] == v) expected = j;
        if(f->slow[v]) expected = -2;
        TEST_ASSERT(f->voices[v].mixer_index == expected);
    }
}

static void finished(const fluid_voice_t *voice, int reason, void *data)
{
    struct fixture *f = data;
    int v = (const fluid_rvoice_t *)voice - f->voices;
    int next;
    TEST_ASSERT(reason == FLUID_VOICE_CALLBACK_FINISHED);
    do
    {
        TEST_ASSERT(f->event_pos < f->event_count);
        next = f->events[f->event_pos++];
        reference_finish(f, next);
    }
    while(f->callback_seen[next]);
    TEST_ASSERT(next == v);
    f->callback_seen[v] = 1;
    check_indices(f);
}

static void resize(struct fixture *f, int capacity)
{
    fluid_rvoice_param_t p[MAX_EVENT_PARAMS] = {{0}};
    p[0].i = capacity;
    fluid_rvoice_mixer_set_polyphony(f->handler->mixer, p);
    if(capacity >= f->count && capacity >= f->pending_count)
        f->polyphony = capacity;
    check_indices(f);
}

static void init(struct fixture *f, int capacity)
{
    int i;
    memset(f, 0, sizeof(*f));
    f->handler = new_fluid_rvoice_eventhandler(64, EVENTS, 1, 2, 1,
                    44100, 44100, FLUID_REVERB_TYPE_FDN, 0, 0);
    TEST_ASSERT(f->handler != NULL);
    for(i = 0; i < VOICES; ++i) f->voices[i].mixer_index = -1;
    resize(f, capacity);
}

/* Run the old add algorithm independently before the candidate operation. */
static void add(struct fixture *f, int v)
{
    fluid_rvoice_param_t p[MAX_EVENT_PARAMS] = {{0}};
    int slot = -1, i, present = 0;
    for(i = 0; i < f->count; ++i) present += f->active[i] == v;
    if(f->count < f->polyphony) slot = f->count++;
    else
    {
        for(i = 0; i < f->count; ++i)
        {
            if(f->active[i] == v) break;
            if(f->voices[f->active[i]].envlfo.volenv.section == FLUID_VOICE_ENVFINISHED)
            {
                TEST_ASSERT(f->pending_count < f->polyphony);
                f->pending[f->pending_count++] = f->active[i];
                slot = i;
                break;
            }
        }
    }
    if(slot >= 0)
    {
        if(present) f->slow[v] = 1;
        f->active[slot] = v;
    }
    p[0].ptr = &f->voices[v];
    fluid_rvoice_mixer_add_voice(f->handler->mixer, p);
    check_indices(f);
}

static void render(struct fixture *f)
{
    int i, remaining = 0;
    f->event_pos = 0;
    f->event_count = f->pending_count;
    memcpy(f->events, f->pending, f->pending_count * sizeof(int));
    TEST_ASSERT(f->pending_count + f->count <= f->polyphony);
    for(i = 0; i < f->count; ++i)
    {
        if(f->voices[f->active[i]].dsp.sample == NULL)
            f->events[f->event_count++] = f->active[i];
        else
            ++remaining;
    }
    for(i = 0; i < VOICES; ++i)
    {
        f->callback_seen[i] = 0;
        f->voices[i].finished_cb = finished;
        f->voices[i].finished_cb_voice = &f->voices[i];
        f->voices[i].finished_cb_data = f;
    }
    /* Sample-less pointers finish; the partial case keeps silent delay voices. */
    TEST_ASSERT(fluid_rvoice_mixer_render(f->handler->mixer, 1) == 1);
    while(f->event_pos < f->event_count)
        reference_finish(f, f->events[f->event_pos++]);
    check_indices(f);
    for(i = 0; i < f->event_count; ++i)
        TEST_ASSERT(fluid_rvoice_eventhandler_get_finished_voice(f->handler) == &f->voices[f->events[i]]);
    TEST_ASSERT(fluid_rvoice_eventhandler_get_finished_voice(f->handler) == NULL);
    f->pending_count = 0;
    TEST_ASSERT(f->count == remaining);
}

static void ignore_error(int level, const char *message, void *data)
{
    int *errors = data;
    ++*errors;
}

static void test_unique_and_resize(void)
{
    struct fixture f;
    fluid_rvoice_param_t p[MAX_EVENT_PARAMS] = {{0}};
    int i;
    init(&f, 12);
    for(i = 0; i < 12; ++i) add(&f, i);
    resize(&f, 8); /* A rejected shrink must not disturb indices. */
    resize(&f, 16);
    fluid_rvoice_reset(&f.voices[5], p);
    check_indices(&f); /* DSP reset must never reset membership. */
    render(&f); /* Every callback checks the moved-last unique index. */
    resize(&f, 4);
    add(&f, 9);
    render(&f);
    delete_fluid_rvoice_eventhandler(f.handler);
}

static void test_partial_completion(void)
{
    struct fixture f;
    fluid_sample_t *sample = new_fluid_sample();
    int i;
    TEST_ASSERT(sample != NULL);
    init(&f, 8);
    for(i = 0; i < 6; ++i) add(&f, i);
    for(i = 3; i <= 5; i += 2)
    {
        fluid_rvoice_t *v = &f.voices[i];
        v->dsp.sample = sample;
        v->dsp.root_pitch_hz = 440;
        v->dsp.samplemode = FLUID_START_ON_RELEASE;
        v->envlfo.volenv.data[FLUID_VOICE_ENVDELAY].count = ~0u;
        v->envlfo.volenv.data[FLUID_VOICE_ENVDELAY].max = 1;
        v->envlfo.modenv.data[FLUID_VOICE_ENVDELAY].count = ~0u;
        v->envlfo.modenv.data[FLUID_VOICE_ENVDELAY].max = 1;
    }
    render(&f);
    TEST_ASSERT(f.count == 2 && f.voices[5].mixer_index == 0 && f.voices[3].mixer_index == 1);
    resize(&f, 2);
    resize(&f, 8);
    add(&f, 6); add(&f, 7);
    render(&f);
    f.voices[3].dsp.sample = NULL;
    f.voices[5].dsp.sample = NULL;
    render(&f);
    delete_fluid_rvoice_eventhandler(f.handler);
    delete_fluid_sample(sample);
}

static void test_duplicates(void)
{
    struct fixture f;
    int sequence[] = {0, 1, 2, 0, 3, 0};
    int i;
    init(&f, 8);
    for(i = 0; i < 6; ++i) add(&f, sequence[i]);
    render(&f);
    TEST_ASSERT(f.voices[0].mixer_index == -2);
    add(&f, 0); /* Legacy status remains sticky after complete removal. */
    add(&f, 4);
    render(&f);
    delete_fluid_rvoice_eventhandler(f.handler);
}

static void test_full_capacity_order(void)
{
    struct fixture f;
    fluid_log_function_t old;
    int errors = 0;
    init(&f, 2);
    add(&f, 0); add(&f, 1);
    f.voices[0].envlfo.volenv.section = FLUID_VOICE_ENVFINISHED;
    add(&f, 1); /* Earlier finished hole wins over later duplicate. */
    resize(&f, 8);
    render(&f);
    delete_fluid_rvoice_eventhandler(f.handler);

    init(&f, 2);
    add(&f, 0); add(&f, 1);
    f.voices[1].envlfo.volenv.section = FLUID_VOICE_ENVFINISHED;
    old = fluid_set_log_function(FLUID_ERR, ignore_error, &errors);
    add(&f, 0); /* Earlier duplicate still rejects before later hole. */
    fluid_set_log_function(FLUID_ERR, old, NULL);
    TEST_ASSERT(errors == 1);
    render(&f);
    delete_fluid_rvoice_eventhandler(f.handler);
}

static void test_pending_readd(void)
{
    struct fixture f;
    init(&f, 2);
    add(&f, 0); add(&f, 1);
    f.voices[0].envlfo.volenv.section = FLUID_VOICE_ENVFINISHED;
    add(&f, 2); /* Pending completion of unbound 0. */
    f.voices[2].envlfo.volenv.section = FLUID_VOICE_ENVFINISHED;
    add(&f, 0); /* Rebound before its old completion is processed. */
    resize(&f, 8);
    render(&f); /* Completion queue must remain [0, 2, 0, 1]. */
    delete_fluid_rvoice_eventhandler(f.handler);
}

static void test_stale_fallback(void)
{
    struct fixture f;
    init(&f, 8);
    add(&f, 0); add(&f, 1); add(&f, 2);
    f.voices[0].mixer_index = 1; f.slow[0] = 1;
    f.voices[2].mixer_index = 99; f.slow[2] = 1;
    render(&f); /* Victim and swapped-last stale indices go conservative. */
    delete_fluid_rvoice_eventhandler(f.handler);
}

static void test_duplicate_patterns(void)
{
    struct fixture f;
    unsigned int state = 1;
    int round, i, count;
    init(&f, VOICES);
    for(round = 0; round < 256; ++round)
    {
        state = state * 1664525u + 1013904223u;
        count = 1 + (state >> 16) % VOICES;
        for(i = 0; i < count; ++i)
        {
            state = state * 1664525u + 1013904223u;
            add(&f, (state >> 16) % VOICES);
        }
        render(&f);
    }
    delete_fluid_rvoice_eventhandler(f.handler);
}

static void test_construction_and_public_duplicate(void)
{
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth;
    fluid_sample_t *sample = new_fluid_sample();
    fluid_voice_t *voice;
    short data[256] = {0};
    TEST_ASSERT(settings != NULL && sample != NULL);
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.polyphony", 8));
    TEST_SUCCESS(fluid_settings_setint(settings, "audio.realtime-prio", 0));
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);
    TEST_ASSERT(synth->voice[0]->rvoice->mixer_index == -1);
    TEST_ASSERT(synth->voice[0]->overflow_rvoice->mixer_index == -1);
    TEST_SUCCESS(fluid_sample_set_sound_data(sample, data, NULL, 256, 44100, 1));
    voice = fluid_synth_alloc_voice(synth, sample, 0, 60, 100);
    TEST_ASSERT(voice != NULL);
    fluid_synth_start_voice(synth, voice);
    fluid_synth_start_voice(synth, voice);
    fluid_rvoice_eventhandler_flush(synth->eventhandler);
    fluid_rvoice_eventhandler_dispatch_all(synth->eventhandler);
    TEST_ASSERT(voice->rvoice->mixer_index == -2);
    delete_fluid_synth(synth);
    delete_fluid_sample(sample);
    delete_fluid_settings(settings);
}

int main(void)
{
    test_unique_and_resize();
    test_partial_completion();
    test_duplicates();
    test_full_capacity_order();
    test_pending_readd();
    test_stale_fallback();
    test_duplicate_patterns();
    test_construction_and_public_duplicate();
    return EXIT_SUCCESS;
}
