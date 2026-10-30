#include "test.h"
#include "fluid_synth.h"
#include "fluid_chan.h"
#include "fluid_voice.h"
#include "fluid_mod.h"

static fluid_voice_t *start(fluid_synth_t *synth, fluid_sample_t *sample,
                            int channel, float exclusive, unsigned int id)
{
    fluid_voice_t *voice = fluid_synth_alloc_voice(synth, sample, channel, 60, 100);
    TEST_ASSERT(voice != NULL);
    voice->id = id;
    if(exclusive != 0)
        fluid_voice_gen_set(voice, GEN_EXCLUSIVECLASS, exclusive);
    fluid_synth_start_voice(synth, voice);
    return voice;
}

int main(void)
{
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth;
    fluid_sample_t *sample = new_fluid_sample();
    fluid_voice_t *voice, *fractional, *sibling;
    fluid_mod_t *mod = new_fluid_mod();
    short data[256] = {0};
    unsigned int id = 1;
    int i, access, overflow_access;

    TEST_SUCCESS(fluid_settings_setint(settings, "synth.polyphony", 64));
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.midi-channels", 256));
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL && sample != NULL && mod != NULL);
    TEST_SUCCESS(fluid_sample_set_sound_data(sample, data, NULL, 256, 44100, 1));
    TEST_SUCCESS(fluid_sample_set_pitch(sample, 60, 0));

    /* Cross an empty bitmap word without skipping a later candidate. */
    for(i = 0; i < 33; i++) start(synth, sample, 0, 0, id++);
    fractional = start(synth, sample, 0, 7.5f, id++);
    voice = start(synth, sample, 0, 7, id++);
    sibling = start(synth, sample, 0, 7, voice->id);
    TEST_ASSERT(fluid_voice_gen_get(voice, GEN_EXCLUSIVECLASS) == 7);
    start(synth, sample, 0, 7, id++);
    TEST_ASSERT(fluid_voice_gen_get(voice, GEN_EXCLUSIVECLASS) == 0);
    TEST_ASSERT(fluid_voice_gen_get(sibling, GEN_EXCLUSIVECLASS) == 0);
    TEST_ASSERT(fluid_voice_gen_get(fractional, GEN_EXCLUSIVECLASS) == 7.5f);

    /* Mutating an initially zero voice must enroll it. */
    voice = start(synth, sample, 0, 0, id++);
    fluid_voice_gen_incr(voice, GEN_EXCLUSIVECLASS, 9);
    start(synth, sample, 0, 9, id++);
    TEST_ASSERT(fluid_voice_gen_get(voice, GEN_EXCLUSIVECLASS) == 0);
    fluid_voice_set_param(voice, GEN_EXCLUSIVECLASS, 10);
    fluid_voice_gen_set(voice, GEN_VOLENVRELEASE, 0);
    start(synth, sample, 0, 10, id++);
    TEST_ASSERT(fluid_voice_gen_get(voice, GEN_VOLENVRELEASE) == -2000);
    TEST_ASSERT(fluid_voice_gen_value(voice, GEN_EXCLUSIVECLASS) == 10);

    voice = start(synth, sample, 0, 0, id++);
    fluid_mod_set_source1(mod, 0, FLUID_MOD_GC | FLUID_MOD_LINEAR);
    fluid_mod_set_source2(mod, 0, FLUID_MOD_GC | FLUID_MOD_LINEAR);
    fluid_mod_set_dest(mod, GEN_EXCLUSIVECLASS);
    fluid_mod_set_amount(mod, 11);
    fluid_voice_add_mod(voice, mod, FLUID_VOICE_ADD);
    fluid_voice_modulate_all(voice);
    start(synth, sample, 0, 11, id++);
    TEST_ASSERT(fluid_voice_gen_value(voice, GEN_EXCLUSIVECLASS) == 11);
    TEST_ASSERT(fluid_voice_gen_get(voice, GEN_VOLENVRELEASE) == -2000);

    /* Failed reuse keeps old membership; successful reuse resets it. */
    access = voice->can_access_rvoice;
    overflow_access = voice->can_access_overflow_rvoice;
    voice->can_access_rvoice = voice->can_access_overflow_rvoice = FALSE;
    TEST_ASSERT(fluid_voice_init(voice, sample, NULL, synth->channel[0],
                               60, 100, id++, 0, 0.2f) == FLUID_FAILED);
    TEST_ASSERT(synth->exclusive_candidates[voice->slot / 32] &
                ((uint32_t)1 << (voice->slot % 32)));
    voice->can_access_rvoice = access;
    voice->can_access_overflow_rvoice = overflow_access;
    TEST_SUCCESS(fluid_voice_init(voice, sample, NULL, synth->channel[0],
                                 60, 100, id++, 0, 0.2f));
    TEST_ASSERT(!(synth->exclusive_candidates[voice->slot / 32] &
                  ((uint32_t)1 << (voice->slot % 32))));

    voice = start(synth, sample, 255, 12, id++);
    start(synth, sample, 255, 12, id++);
    TEST_ASSERT(fluid_voice_gen_get(voice, GEN_EXCLUSIVECLASS) == 0);
    TEST_SUCCESS(fluid_synth_set_polyphony(synth, 65));
    TEST_ASSERT(synth->voice[64]->slot == 64);
    TEST_ASSERT(fluid_synth_set_polyphony(synth, 65536) == FLUID_FAILED);

    delete_fluid_mod(mod);
    delete_fluid_synth(synth);
    delete_fluid_sample(sample);
    delete_fluid_settings(settings);
    return EXIT_SUCCESS;
}
