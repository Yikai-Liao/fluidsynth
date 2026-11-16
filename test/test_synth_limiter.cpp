#include "test.h"
#include "fluid_synth.h"
#include "../src/rvoice/fluid_rvoice_mixer.cpp"
#include <cmath>

static void check_bypass(fluid_synth_t *synth);

/* Keep audio running across updates: silence before an assertion would hide a
 * cleared delay line. Bound steps by the input slope plus a 10 ms fade (and
 * the permitted 1 ms attack under limiting). An unchanged DC signal must
 * never drop out. */
static void check_live_transitions()
{
    constexpr double rate = 48000;
    for(int signal = 0; signal < 3; signal++)
    {
        auto settings = new_fluid_settings();
        TEST_SUCCESS(fluid_settings_setnum(settings, "synth.sample-rate", rate));
        TEST_SUCCESS(fluid_settings_setint(settings, "synth.reverb.active", 0));
        TEST_SUCCESS(fluid_settings_setint(settings, "synth.chorus.active", 0));
        TEST_SUCCESS(fluid_settings_setint(settings, "synth.limiter.active", 1));
        TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.output-limit", 0.9));
        auto synth = new_fluid_synth(settings);
        TEST_ASSERT(synth != NULL);
        auto mixer = synth->eventhandler->mixer;
        fluid_real_t *left, *right;
        fluid_rvoice_mixer_get_bufs(mixer, &left, &right);
        double previous[2] = {};
        int position = 0;
        const char *change = "initial state";
        auto render = [&](int blocks, bool check)
        {
            fluid_synth_process_event_queue(synth);
            for(int block = 0; block < blocks; block++)
            {
                for(int i = 0; i < FLUID_BUFSIZE; i++, position++)
                {
                    left[i] = signal == 2 ? 2 : 0.5;
                    right[i] = signal == 2 ? -0.75 : -0.25;
                    if(signal == 1)
                    {
                        left[i] = 0.3 + 0.2 * std::sin(2 * M_PI * 437 * position / rate);
                        right[i] += 0.1 * std::sin(2 * M_PI * 613 * position / rate);
                    }
                }
                fluid_rvoice_mixer_process_fx(mixer, 1);
                for(int i = 0; i < FLUID_BUFSIZE; i++)
                {
                    double values[] = {left[i], right[i]};
                    for(int channel = 0; channel < 2; channel++)
                    {
                        double step = std::abs(values[channel] - previous[channel]);
                        double max_step = signal == 1
                            ? 2 * (channel ? 0.1 : 0.2) * (std::sin(M_PI * (channel ? 613 : 437) / rate) + 1 / (0.010 * rate)) + 1e-5
                            : signal == 2 ? 2 / (0.001 * rate) + 2 / (0.010 * rate) + 1e-5 : 0.01;
                        if(check && step > max_step)
                        {
                            fprintf(stderr, "signal=%d change=%s channel=%d sample=%d step=%g limit=%g\n",
                                    signal, change, channel, position - FLUID_BUFSIZE + i, step, max_step);
                        }
                        TEST_ASSERT(std::isfinite(values[channel]));
                        TEST_ASSERT(!check || step <= max_step);
                        if(check && signal == 0)
                        {
                            TEST_ASSERT(std::abs(values[channel] - (channel ? -0.25 : 0.5)) < 1e-5);
                        }
                        previous[channel] = values[channel];
                    }
                }
            }
        };
        render(400, false);
        const struct { const char *name; double value; bool integer; } changes[] = {
            {"synth.limiter.attack", 250, false}, {"synth.limiter.attack", 1, false},
            {"synth.limiter.hold", 250, false}, {"synth.limiter.hold", 0, false},
            {"synth.limiter.active", 0, true}, {"synth.limiter.active", 1, true},
            {"synth.limiter.release", 250, false}, {"synth.limiter.release", 0, false},
            {"synth.limiter.smoothing-stages", 2, true}, {"synth.limiter.smoothing-stages", 1, true},
            {"synth.limiter.smoothing-stages", 3, true},
            {"synth.limiter.link-channels", 0, false}, {"synth.limiter.link-channels", 1, false},
            {"synth.limiter.output-limit", signal == 2 ? 0.3 : 0.6, false},
            {"synth.limiter.output-limit", 0.9, false}
        };
        for(const auto &event : changes)
        {
            change = event.name;
            if(event.integer)
            {
                TEST_SUCCESS(fluid_settings_setint(settings, event.name, (int)event.value));
            }
            else
            {
                TEST_SUCCESS(fluid_settings_setnum(settings, event.name, event.value));
            }
            render(240, true);
        }
        /* New requests during warm-up/fading must not discard audible state. */
        change = "rapid updates";
        for(int i = 0; i < 100; i++)
        {
            TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.attack", i % 2 ? 250 : 1));
            TEST_SUCCESS(fluid_settings_setint(settings, "synth.limiter.active", i % 3 != 0));
            render(1, true);
        }
        TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.attack", 3));
        TEST_SUCCESS(fluid_settings_setint(settings, "synth.limiter.active", 1));
        TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.output-limit", signal == 2 ? 0.4 : 0.8));
        render(500, true);
        if(signal == 2)
        {
            TEST_ASSERT(std::abs(previous[0] - 0.4) < 1e-3);
        }
        delete_fluid_synth(synth);
        delete_fluid_settings(settings);
    }
}

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
    check_bypass(synth);
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
    /* Drain any in-flight replacement, then finish the fade to bypass. */
    int blocks = (int)std::ceil(0.6 * synth->sample_rate / FLUID_BUFSIZE);
    for(int block = 0; block < blocks; block++)
    {
        std::fill_n(left, FLUID_BUFSIZE, 2);
        std::fill_n(right, FLUID_BUFSIZE, -2);
        fluid_rvoice_mixer_process_fx(synth->eventhandler->mixer, 1);
    }
    for(int i = 0; i < FLUID_BUFSIZE; i++)
    {
        TEST_ASSERT(left[i] == 2 && right[i] == -2);
    }
}

int main(void)
{
    check_live_transitions();
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
