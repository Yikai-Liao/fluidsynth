#include "test.h"
#include "fluid_synth.h"
#include "../src/rvoice/fluid_rvoice_mixer.cpp"
#include <cmath>
#include <array>
#include <new>
#include <cstdlib>
#if !OSAL_embedded
#include <atomic>
#include <thread>
#endif

/* Signalsmith uses C++ heap allocation. Neither allocation nor destruction
 * may migrate back into the audio path during future refactors. */
static thread_local bool checking_render = false;
static thread_local bool fail_control_allocation = false;
void *operator new(std::size_t size)
{
    TEST_ASSERT(!checking_render);
    if(fail_control_allocation)
    {
        fail_control_allocation = false;
        throw std::bad_alloc();
    }
    if(auto ptr = std::malloc(size ? size : 1))
    {
        return ptr;
    }
    throw std::bad_alloc();
}
void operator delete(void *ptr) noexcept
{
    TEST_ASSERT(!ptr || !checking_render);
    std::free(ptr);
}
void operator delete(void *ptr, std::size_t) noexcept
{
    ::operator delete(ptr);
}
void *operator new[](std::size_t size)
{
    return ::operator new(size);
}
void operator delete[](void *ptr) noexcept
{
    ::operator delete(ptr);
}
void operator delete[](void *ptr, std::size_t) noexcept
{
    ::operator delete(ptr);
}
void *operator new(std::size_t size, const std::nothrow_t &) noexcept
{
    try
    {
        return ::operator new(size);
    }
    catch(...)
    {
        return nullptr;
    }
}
void *operator new[](std::size_t size, const std::nothrow_t &tag) noexcept
{
    return ::operator new(size, tag);
}
void operator delete(void *ptr, const std::nothrow_t &) noexcept
{
    ::operator delete(ptr);
}
void operator delete[](void *ptr, const std::nothrow_t &) noexcept
{
    ::operator delete(ptr);
}

/* Mark only actual audio processing, not control calls between blocks. */
static void render_block(fluid_rvoice_mixer_t *mixer)
{
    checking_render = true;
    fluid_rvoice_mixer_process_fx(mixer, 1);
    checking_render = false;
}

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
                        left[i] = 0.2 * std::sin(2 * M_PI * 500 * position / rate);
                        right[i] = 0.1 * std::sin(2 * M_PI * 613 * position / rate);
                    }
                }
                render_block(mixer);
                for(int i = 0; i < FLUID_BUFSIZE; i++)
                {
                    double values[] = {left[i], right[i]};
                    for(int channel = 0; channel < 2; channel++)
                    {
                        double step = std::abs(values[channel] - previous[channel]);
                        double max_step = signal == 1
                            ? 2 * (channel ? 0.1 : 0.2) * (std::sin(M_PI * (channel ? 613 : 500) / rate) + 1 / (0.010 * rate)) + 1e-5
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
            {"synth.limiter.smoothing-stages", 2, true},
            {"synth.limiter.output-limit", signal == 2 ? 0.3 : 0.6, false},
            {"synth.limiter.output-limit", 0.9, false},
            {"synth.limiter.link-channels", 0, false}, {"synth.limiter.link-channels", 1, false},
            {"synth.limiter.smoothing-stages", 1, true},
            {"synth.limiter.smoothing-stages", 3, true},
            {"synth.limiter.link-channels", 0, false}, {"synth.limiter.link-channels", 1, false}
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

/* Run queued settings and constant input through the real final mixer stage. */
static std::array<double, 2> constant_output(fluid_synth_t *synth, int blocks,
                                           fluid_real_t left_value, fluid_real_t right_value)
{
    auto mixer = synth->eventhandler->mixer;
    fluid_synth_process_event_queue(synth);
    fluid_real_t *left, *right;
    fluid_rvoice_mixer_get_bufs(mixer, &left, &right);
    for(int block = 0; block < blocks; block++)
    {
        std::fill_n(left, FLUID_BUFSIZE, left_value);
        std::fill_n(right, FLUID_BUFSIZE, right_value);
        render_block(mixer);
    }
    return {left[FLUID_BUFSIZE - 1], right[FLUID_BUFSIZE - 1]};
}

#if !OSAL_embedded
/* Race publication, retirement, helper exit/restart and destruction while
 * keeping a below-threshold DC signal exactly unchanged. */
static void check_concurrent_updates()
{
    for(int round = 0; round < 4; round++)
    {
        auto settings = new_fluid_settings();
        TEST_SUCCESS(fluid_settings_setint(settings, "synth.reverb.active", 0));
        TEST_SUCCESS(fluid_settings_setint(settings, "synth.chorus.active", 0));
        TEST_SUCCESS(fluid_settings_setint(settings, "synth.limiter.active", 1));
        auto synth = new_fluid_synth(settings);
        TEST_ASSERT(synth != NULL);
        std::atomic<bool> started{false}, stop{false};
        std::thread audio([&]
        {
            constant_output(synth, 500, 0.25, -0.125);
            started.store(true);
            while(!stop.load())
            {
                auto out = constant_output(synth, 1, 0.25, -0.125);
                TEST_ASSERT(std::abs(out[0] - 0.25) < 1e-5);
                TEST_ASSERT(std::abs(out[1] + 0.125) < 1e-5);
            }
        });
        while(!started.load())
        {
            std::this_thread::yield();
        }
        for(int i = 0; i < 100; i++)
        {
            TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.attack", i % 2 ? 250 : 1));
            TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.hold", i % 2 ? 250 : 0));
            TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.output-limit", i % 2 ? 0.9 : 0.6));
            TEST_SUCCESS(fluid_settings_setint(settings, "synth.limiter.active", i % 3 != 0));
        }
        stop.store(true);
        audio.join();
        delete_fluid_synth(synth);
        delete_fluid_settings(settings);
    }
}
#endif

/* Measure gain recovery after a peak without bypassing the active DSP. */
static double pulse_response(fluid_synth_t *synth, double probe_ms)
{
    constant_output(synth, (int)std::ceil(0.7 * synth->sample_rate / FLUID_BUFSIZE), 0.1, 0.1);
    constant_output(synth, 1, 2, 2);
    return constant_output(synth, (int)std::ceil(probe_ms * 0.001 * synth->sample_rate / FLUID_BUFSIZE), 0.1, 0.1)[0];
}

/* Verify the applied lookahead with a single below-threshold impulse. */
static void check_latency(fluid_synth_t *synth, int milliseconds, int hold)
{
    TEST_SUCCESS(fluid_settings_setnum(synth->settings, "synth.limiter.attack", milliseconds));
    TEST_SUCCESS(fluid_settings_setnum(synth->settings, "synth.limiter.hold", hold));
    constant_output(synth, (int)std::ceil(0.7 * synth->sample_rate / FLUID_BUFSIZE), 0, 0);
    int latency = (int)std::ceil(milliseconds * 0.001 * synth->sample_rate);
    auto mixer = synth->eventhandler->mixer;
    fluid_real_t *left, *right;
    fluid_rvoice_mixer_get_bufs(mixer, &left, &right);
    for(int block = 0; block <= latency / FLUID_BUFSIZE + 1; block++)
    {
        std::fill_n(left, FLUID_BUFSIZE, 0);
        std::fill_n(right, FLUID_BUFSIZE, 0);
        if(block == 0)
        {
            left[0] = 0.2;
            right[0] = -0.1;
        }
        render_block(mixer);
        for(int i = 0; i < FLUID_BUFSIZE; i++)
        {
            bool impulse = block * FLUID_BUFSIZE + i == latency;
            TEST_ASSERT(std::abs(left[i] - (impulse ? 0.2 : 0)) < 1e-5);
            TEST_ASSERT(std::abs(right[i] - (impulse ? -0.1 : 0)) < 1e-5);
        }
    }
}

int main(void)
{
    check_live_transitions();
#if !OSAL_embedded
    check_concurrent_updates();
#endif
    auto settings = new_fluid_settings();
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.reverb.active", 0));
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.chorus.active", 0));
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.sample-rate", 44101));
    auto synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);
    int settling_blocks = (int)std::ceil(0.7 * synth->sample_rate / FLUID_BUFSIZE);
    auto output = constant_output(synth, 1, 2, -2);
    TEST_ASSERT(output[0] == 2 && output[1] == -2);
    const char *names[] = {
        "synth.limiter.active", "synth.limiter.output-limit", "synth.limiter.attack",
        "synth.limiter.hold", "synth.limiter.release", "synth.limiter.smoothing-stages",
        "synth.limiter.link-channels"
    };
    for(auto name : names)
    {
        TEST_ASSERT(fluid_settings_is_realtime(settings, name));
    }

    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.output-limit", 0.25));
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.attack", 1));
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.hold", 0));
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.release", 0));
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.link-channels", 0));
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.limiter.active", 1));
    output = constant_output(synth, settling_blocks, 2, 0.1);
    TEST_ASSERT(std::abs(output[0] - 0.25) < 1e-4);
    TEST_ASSERT(std::abs(output[1] - 0.1) < 1e-4);
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.link-channels", 1));
    output = constant_output(synth, settling_blocks, 2, 0.1);
    TEST_ASSERT(std::abs(output[1] - 0.0125) < 1e-4);

    double fast = pulse_response(synth, 120);
    TEST_ASSERT(fast > 0.095);
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.hold", 250));
    TEST_ASSERT(pulse_response(synth, 120) < 0.02);
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.hold", 0));
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.release", 250));
    TEST_ASSERT(pulse_response(synth, 120) < 0.06);

    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.attack", 50));
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.release", 0));
    double single_stage = pulse_response(synth, 20);
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.limiter.smoothing-stages", 2));
    TEST_ASSERT(pulse_response(synth, 20) > single_stage + 0.005);
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.limiter.smoothing-stages", 3));
    TEST_ASSERT(std::abs(pulse_response(synth, 20) - single_stage) < 1e-3);
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.limiter.smoothing-stages", 1));
    /* A failed preparation must restore the unconsumed request, not publish
     * a partial processor or lose the previous accepted target. */
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.output-limit", 0.5));
    fail_control_allocation = true;
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.output-limit", 0.25));
    fail_control_allocation = false;
    output = constant_output(synth, settling_blocks, 2, 0.1);
    TEST_ASSERT(std::abs(output[0] - 0.5) < 1e-3);
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.output-limit", 0.25));
    output = constant_output(synth, settling_blocks, 2, 0.1);
    TEST_ASSERT(std::abs(output[0] - 0.25) < 1e-3);
    check_latency(synth, 1, 0);
    check_latency(synth, 250, 250);

    TEST_SUCCESS(fluid_settings_setint(settings, "synth.limiter.active", 0));
    output = constant_output(synth, settling_blocks, 2, -2);
    TEST_ASSERT(output[0] == 2 && output[1] == -2);
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.limiter.active", 1));
    output = constant_output(synth, 1, 0, 0);
    TEST_ASSERT(output[0] == 0 && output[1] == 0);
    output = constant_output(synth, settling_blocks, 2, 0.1);
    TEST_ASSERT(std::abs(output[0] - 0.25) < 1e-3);
    delete_fluid_synth(synth);
    /* Settings outlive the synth, and can be reused for a new one. */
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.limiter.active", 1));
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.attack", 5));
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    return EXIT_SUCCESS;
}
