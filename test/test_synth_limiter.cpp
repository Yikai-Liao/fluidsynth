#include "test.h"
#include "fluid_synth.h"
#include "../src/rvoice/fluid_rvoice_mixer.cpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <array>
#include <new>
#include <cstdlib>
#if !OSAL_embedded
#include <thread>
#endif

/* Check that rendering neither allocates nor destroys DSP storage. */
static std::atomic<int> live_allocations{0};
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
        ++live_allocations;
        return ptr;
    }
    throw std::bad_alloc();
}
void operator delete(void *ptr) noexcept
{
    TEST_ASSERT(!ptr || !checking_render);
    if(ptr)
    {
        --live_allocations;
    }
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

/* Reconfiguration preserves the synth, but starts a fresh limiter delay. */
static void check_runtime_reconfiguration()
{
    for(int rate : {44101, 48000, 96000})
    {
        auto settings = new_fluid_settings();
        TEST_SUCCESS(fluid_settings_setnum(settings, "synth.sample-rate", rate));
        TEST_SUCCESS(fluid_settings_setint(settings, "synth.reverb.active", 0));
        TEST_SUCCESS(fluid_settings_setint(settings, "synth.chorus.active", 0));
        TEST_SUCCESS(fluid_settings_setint(settings, "synth.limiter.active", 1));
        auto synth = new_fluid_synth(settings);
        TEST_ASSERT(synth != NULL);
        int font = fluid_synth_sfload(synth, TEST_SOUNDFONT, 1);
        TEST_ASSERT(font >= 0);
        TEST_SUCCESS(fluid_synth_program_select(synth, 0, font, 0, 5));
        TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 100));
        int voices = fluid_synth_get_active_voice_count(synth);
        TEST_ASSERT(voices > 0);
        auto sfont = fluid_synth_get_sfont_by_id(synth, font);
        auto mixer = synth->eventhandler->mixer;
        auto limiter = mixer->limiter;
        constant_output(synth, 500, 0.25, -0.125);

        TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.attack", 250));
        auto output = constant_output(synth, 1, 0.25, -0.125);
        TEST_ASSERT(output[0] == 0 && output[1] == 0);
        output = constant_output(synth, 500, 0.25, -0.125);
        TEST_ASSERT(std::abs(output[0] - 0.25) < 1e-5);
        TEST_ASSERT(std::abs(output[1] + 0.125) < 1e-5);

        TEST_SUCCESS(fluid_settings_setint(settings, "synth.limiter.active", 0));
        output = constant_output(synth, 1, 0.5, -0.25);
        TEST_ASSERT(output[0] == 0.5 && output[1] == -0.25);
        TEST_SUCCESS(fluid_settings_setint(settings, "synth.limiter.active", 1));
        output = constant_output(synth, 1, 0.5, -0.25);
        TEST_ASSERT(output[0] == 0 && output[1] == 0);

        TEST_ASSERT(mixer->limiter == limiter);
        TEST_ASSERT(fluid_synth_get_sfont_by_id(synth, font) == sfont);
        TEST_ASSERT(fluid_synth_get_active_voice_count(synth) == voices);
        /* Repeated updates must keep retired DSP storage bounded. */
        for(int i = 0; i < 2; i++)
        {
            TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.attack", i ? 250 : 1));
            constant_output(synth, 1, 0.25, -0.125);
        }
        int allocated = live_allocations.load();
        for(int i = 0; i < 100; i++)
        {
            TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.attack", i % 2 ? 250 : 1));
            constant_output(synth, 1, 0.25, -0.125);
            TEST_ASSERT(live_allocations.load() <= allocated);
        }
        TEST_SUCCESS(fluid_synth_noteoff(synth, 0, 60));
        delete_fluid_synth(synth);
        delete_fluid_settings(settings);
    }
}

#if !OSAL_embedded
/* Concurrent callbacks must converge to all final settings together. */
static void check_concurrent_updates()
{
    auto settings = new_fluid_settings();
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.reverb.active", 0));
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.chorus.active", 0));
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.limiter.active", 1));
    auto synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);
    std::atomic<bool> stop{false};
    std::thread audio([&]
    {
        while(!stop.load())
        {
            auto out = constant_output(synth, 1, 1, 0.1);
            TEST_ASSERT(std::isfinite(out[0]) && std::isfinite(out[1]));
        }
    });
    std::thread control([&]
    {
        for(int i = 0; i < 100; i++)
        {
            TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.link-channels", i % 2 ? 0 : 1));
        }
    });
    for(int i = 0; i < 100; i++)
    {
        TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.output-limit", i % 2 ? 0.25 : 0.75));
    }
    control.join();
    stop.store(true);
    audio.join();
    auto out = constant_output(synth, 500, 1, 0.1);
    TEST_ASSERT(std::abs(out[0] - 0.25) < 1e-4 && std::abs(out[1] - 0.1) < 1e-4);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
}
#endif

int main(void)
{
#if !OSAL_embedded
    check_concurrent_updates();
#endif
    check_runtime_reconfiguration();
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
