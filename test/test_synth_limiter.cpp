#include "test.h"
#include "fluid_synth.h"
#include "../src/rvoice/fluid_rvoice_mixer.cpp"
#include <cmath>

/* Inject a deterministic stereo signal at the final processing stage. This
 * exercises settings callbacks and mixer bypass without a SoundFont fixture. */
static void compare_audio(fluid_synth_t *synth, const fluid_limiter_settings_t &params)
{
    auto mixer = synth->eventhandler->mixer;
    fluid_synth_process_event_queue(synth);
#ifdef WITH_FLOAT
    /* BoxSum<float> accumulates history-dependent rounding errors, especially
     * with long hold times. Compare parameter routing from identical histories;
     * the hot output-limit change is checked separately below. */
    TEST_SUCCESS(fluid_settings_setint(synth->settings, "synth.limiter.active", 0));
    fluid_synth_process_event_queue(synth);
    fluid_real_t silence[2][FLUID_BUFSIZE] = {};
    fluid_limiter_run(mixer->limiter, silence[0], silence[1], 1);
    TEST_SUCCESS(fluid_settings_setint(synth->settings, "synth.limiter.active", 1));
    fluid_synth_process_event_queue(synth);
#endif
    auto settings = params;
    auto reference = new_fluid_limiter(synth->sample_rate, &settings, 1);
    TEST_ASSERT(reference != NULL);
    fluid_real_t *left, *right;
    fluid_rvoice_mixer_get_bufs(mixer, &left, &right);
    fluid_real_t reference_left[FLUID_BUFSIZE], reference_right[FLUID_BUFSIZE];

    /* Silence drains previous audio and lets the retained release/smoothing
     * state converge. A hot update deliberately preserves that state. */
    int blocks = (int)std::ceil(synth->sample_rate / FLUID_BUFSIZE);
    double settle_seconds = 1 + 0.032 * std::max(params.attack_ms,
                                                params.release_ms * params.smoothing_stages);
    int settle_blocks = (int)std::ceil(settle_seconds * synth->sample_rate / FLUID_BUFSIZE);
    for(int block = -settle_blocks; block < blocks; block++)
    {
        for(int i = 0; i < FLUID_BUFSIZE; i++)
        {
            int sample = block * FLUID_BUFSIZE + i;
            left[i] = reference_left[i] = block < 0 ? 0 : (sample % 997 < 137 ? 2 : 0.125);
            right[i] = reference_right[i] = block < 0 ? 0 : (sample % 761 < 83 ? -1.5 : 0.0625);
        }
        fluid_rvoice_mixer_process_fx(mixer, 1);
        fluid_limiter_run(reference, reference_left, reference_right, 1);
        if(block >= 0)
        {
            for(int i = 0; i < FLUID_BUFSIZE; i++)
            {
                TEST_ASSERT(std::isfinite(left[i]) && std::isfinite(right[i]));
                if(std::abs(left[i] - reference_left[i]) >= 1e-5 ||
                        std::abs(right[i] - reference_right[i]) >= 1e-5)
                {
                    fprintf(stderr, "limit=%g attack=%g hold=%g release=%g stages=%d link=%g block=%d sample=%d: %g/%g vs %g/%g\n",
                            (double)params.output_limit, (double)params.attack_ms,
                            (double)params.hold_ms, (double)params.release_ms,
                            params.smoothing_stages, (double)params.link_channels,
                            block, i, (double)left[i], (double)right[i],
                            (double)reference_left[i], (double)reference_right[i]);
                }
                TEST_ASSERT(std::abs(left[i] - reference_left[i]) < 1e-5);
                TEST_ASSERT(std::abs(right[i] - reference_right[i]) < 1e-5);
            }
        }
    }
    delete_fluid_limiter(reference);
}

static void check_bypass(fluid_synth_t *synth)
{
    fluid_real_t *left, *right;
    fluid_rvoice_mixer_get_bufs(synth->eventhandler->mixer, &left, &right);
    for(int i = 0; i < FLUID_BUFSIZE; i++)
    {
        left[i] = 2;
        right[i] = -2;
    }
    fluid_rvoice_mixer_process_fx(synth->eventhandler->mixer, 1);
    for(int i = 0; i < FLUID_BUFSIZE; i++)
    {
        TEST_ASSERT(left[i] == 2 && right[i] == -2);
    }
}

int main(void)
{
    fluid_settings_t *settings = new_fluid_settings();
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.reverb.active", 0));
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.chorus.active", 0));
    /* A non-integral samples-per-millisecond rate also exercises rounding at
     * the maximum attack and hold values. */
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.sample-rate", 44101));
    fluid_synth_t *synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);
    check_bypass(synth);

    const char *names[] = {
        "synth.limiter.active", "synth.limiter.output-limit", "synth.limiter.attack",
        "synth.limiter.hold", "synth.limiter.release", "synth.limiter.smoothing-stages",
        "synth.limiter.link-channels"
    };
    for(auto name : names)
    {
        TEST_ASSERT(fluid_settings_is_realtime(settings, name));
    }

    /* Changes made before first activation are read when the DSP is created. */
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.output-limit", 0.8));
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.limiter.active", 1));
    fluid_synth_process_event_queue(synth);
    fluid_limiter_t *limiter = synth->eventhandler->mixer->limiter;
    TEST_ASSERT(limiter != NULL);
    fluid_limiter_settings_t params = {1, 0.8, 5, 15, 40, 1, 0.5};
    compare_audio(synth, params);

    /* Change the limit while processing a constant over-limit signal. */
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.output-limit", 0.3));
    params.output_limit = 0.3;
    fluid_synth_process_event_queue(synth);
    fluid_real_t *hot_left, *hot_right;
    fluid_rvoice_mixer_get_bufs(synth->eventhandler->mixer, &hot_left, &hot_right);
    for(int block = 0; block < 1000; block++)
    {
        std::fill_n(hot_left, FLUID_BUFSIZE, 2);
        std::fill_n(hot_right, FLUID_BUFSIZE, 2);
        fluid_rvoice_mixer_process_fx(synth->eventhandler->mixer, 1);
    }
    for(int i = 0; i < FLUID_BUFSIZE; i++)
    {
        TEST_ASSERT(std::abs(hot_left[i] - 0.3) < 1e-4);
        TEST_ASSERT(std::abs(hot_right[i] - 0.3) < 1e-4);
    }
    compare_audio(synth, params);
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.attack", 250));
    params.attack_ms = 250;
    compare_audio(synth, params);
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.hold", 250));
    params.hold_ms = 250;
    compare_audio(synth, params);
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.release", 0));
    params.release_ms = 0;
    compare_audio(synth, params);
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.limiter.smoothing-stages", 2));
    params.smoothing_stages = 2;
    compare_audio(synth, params);
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.link-channels", 1));
    params.link_channels = 1;
    compare_audio(synth, params);

    TEST_SUCCESS(fluid_settings_setint(settings, "synth.limiter.active", 0));
    fluid_synth_process_event_queue(synth);
    check_bypass(synth);
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.attack", 1));
    params.attack_ms = 1;
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.hold", 0));
    params.hold_ms = 0;
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.release", 250));
    params.release_ms = 250;
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.limiter.smoothing-stages", 3));
    params.smoothing_stages = 3;
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.link-channels", 0));
    params.link_channels = 0;
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.limiter.active", 1));
    fluid_synth_process_event_queue(synth);
    TEST_ASSERT(synth->eventhandler->mixer->limiter == limiter);

    /* Re-enabling must not play audio retained before bypass. */
    fluid_real_t *left, *right;
    fluid_rvoice_mixer_get_bufs(synth->eventhandler->mixer, &left, &right);
    std::fill_n(left, FLUID_BUFSIZE, 0);
    std::fill_n(right, FLUID_BUFSIZE, 0);
    fluid_rvoice_mixer_process_fx(synth->eventhandler->mixer, 1);
    for(int i = 0; i < FLUID_BUFSIZE; i++)
    {
        TEST_ASSERT(left[i] == 0 && right[i] == 0);
    }
    compare_audio(synth, params);

    delete_fluid_synth(synth);
    /* Callback removal is also covered by test_settings_unregister_callback. */
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.limiter.active", 1));
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.attack", 5));
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);
    TEST_ASSERT(synth->eventhandler->mixer->limiter != NULL);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    return EXIT_SUCCESS;
}
