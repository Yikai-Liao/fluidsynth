#include "test.h"
#include "fluid_synth.h"
#include "../src/rvoice/fluid_rvoice_mixer.cpp"
#include <cmath>
#include <array>
#include <new>
#include <cstdlib>
#include <vector>
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
/* Race publication, retirement, helper exit/restart and destruction. */
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
                TEST_ASSERT(std::isfinite(out[0]) && std::abs(out[0]) <= 0.25);
                TEST_ASSERT(std::isfinite(out[1]) && std::abs(out[1]) <= 0.125);
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
        TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.attack", 1));
        TEST_SUCCESS(fluid_settings_setint(settings, "synth.limiter.active", 1));
        auto out = constant_output(synth, 500, 0.25, -0.125);
        TEST_ASSERT(std::abs(out[0] - 0.25) < 1e-5);
        TEST_ASSERT(std::abs(out[1] + 0.125) < 1e-5);
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
        TEST_SUCCESS(fluid_synth_noteoff(synth, 0, 60));
        delete_fluid_synth(synth);
        delete_fluid_settings(settings);
    }
}

/* Backends request arbitrary lengths through different public interfaces.
 * Their buffer partitioning must preserve the same stream, including limiter
 * updates that straddle callback boundaries. Compare with a single public
 * float request, rather than another copy of the limiter's transition logic. */
static void check_public_rendering()
{
    constexpr int frames = 32768;
    const int lengths[] = {1, 7, 63, 64, 65, 127, 257, 509};
    for(int rate : {44100, 48000, 96000})
    {
        std::vector<float> expected;
        for(int api = 0; api < 4; api++)
        {
            auto settings = new_fluid_settings();
            TEST_SUCCESS(fluid_settings_setnum(settings, "synth.sample-rate", rate));
            TEST_SUCCESS(fluid_settings_setint(settings, "synth.reverb.active", 0));
            TEST_SUCCESS(fluid_settings_setint(settings, "synth.chorus.active", 0));
            TEST_SUCCESS(fluid_settings_setint(settings, "synth.limiter.active", 1));
            TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.output-limit", 0.35));
            auto synth = new_fluid_synth(settings);
            TEST_ASSERT(synth != NULL);
            int font = fluid_synth_sfload(synth, TEST_SOUNDFONT, 1);
            TEST_ASSERT(font >= 0);
            TEST_SUCCESS(fluid_synth_program_select(synth, 0, font, 0, 5));
            for(int note : {48, 60, 67})
            {
                TEST_SUCCESS(fluid_synth_noteon(synth, 0, note, 100));
            }
            std::vector<float> left(frames), right(frames);
            std::vector<short> pcm(frames * 2);
            for(int phase = 0; phase < 5; phase++)
            {
                if(phase == 1)
                {
                    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.output-limit", 0.12));
                    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.hold", 250));
                }
                if(phase == 2 || phase == 3)
                {
                    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.attack", phase == 2 ? 250 : 1));
                }
                if(phase == 3 || phase == 4)
                {
                    TEST_SUCCESS(fluid_settings_setint(settings, "synth.limiter.active", phase == 4));
                }
                std::fill(left.begin(), left.end(), 0);
                std::fill(right.begin(), right.end(), 0);
                for(int offset = 0, request = 0; offset < frames; request++)
                {
                    int count = api == 0 ? frames : std::min(lengths[request % 8], frames - offset);
                    if(api == 2)
                    {
                        float *out[] = {left.data() + offset, right.data() + offset};
                        TEST_SUCCESS(fluid_synth_process(synth, count, 0, NULL, 2, out));
                    }
                    else if(api == 3)
                    {
                        TEST_SUCCESS(fluid_synth_write_s16(synth, count, pcm.data(), 2 * offset, 2, pcm.data(), 2 * offset + 1, 2));
                        for(int i = offset; i < offset + count; i++)
                        {
                            /* Legacy s16 uses 32766 headroom before dithering. */
                            left[i] = pcm[2 * i] / 32766.0f;
                            right[i] = pcm[2 * i + 1] / 32766.0f;
                        }
                    }
                    else
                    {
                        TEST_SUCCESS(fluid_synth_write_float(synth, count, left.data(), offset, 1, right.data(), offset, 1));
                    }
                    offset += count;
                }
                for(int i = 0; i < frames; i++)
                {
                    for(float sample : {left[i], right[i]})
                    {
                        TEST_ASSERT(std::isfinite(sample));
                    }
                    if(api == 0)
                    {
                        expected.push_back(left[i]);
                        expected.push_back(right[i]);
                    }
                    else
                    {
                        /* TPDF dither plus rounding is bounded by two LSBs. */
                        double tolerance = api == 3 ? 2.0 / 32766 : 1e-6;
                        size_t index = 2 * (phase * frames + i);
                        if(std::abs(left[i] - expected[index]) > tolerance ||
                           std::abs(right[i] - expected[index + 1]) > tolerance)
                        {
                            fprintf(stderr, "public limiter rate=%d api=%d phase=%d frame=%d left=%.9g expected=%.9g right=%.9g expected=%.9g\n",
                                    rate, api, phase, i, left[i], expected[index], right[i], expected[index + 1]);
                        }
                        TEST_ASSERT(std::abs(left[i] - expected[index]) <= tolerance);
                        TEST_ASSERT(std::abs(right[i] - expected[index + 1]) <= tolerance);
                    }
                }
            }
            delete_fluid_synth(synth);
            delete_fluid_settings(settings);
        }
    }
}

int main(void)
{
    check_public_rendering();
    check_runtime_reconfiguration();
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
