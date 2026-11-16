/* Feed an analytic signal through the production final mixer and a real driver.
 * See limiter-audio.md: the expected output is computed independently in Python. */
#define NOMINMAX
#include "test.h"
#include "fluid_synth.h"
#include "../src/rvoice/fluid_rvoice_mixer.cpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <stdexcept>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#ifdef PROBE_PIPEWIRE
#include <pipewire/pipewire.h>
#endif

struct PlatformAudio
{
    PlatformAudio()
    {
#ifdef PROBE_PIPEWIRE
        pw_init(nullptr, nullptr);
#endif
    }
    ~PlatformAudio()
    {
#ifdef PROBE_PIPEWIRE
        pw_deinit();
#endif
    }
};

using Clock = std::chrono::steady_clock;
static long long nanoseconds(Clock::duration value)
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(value).count();
}

struct Callback
{
    size_t frame;
    int length, status;
    long long render_ns, capture_ns, interval_ns;
};

struct Probe
{
    fluid_synth_t *synth = nullptr;
    fluid_rvoice_mixer_t *mixer = nullptr;
    bool dc = false, sine = false;
    double gain = 0;
    int rate = 0, cursor = FLUID_BUFSIZE;
    size_t generated = 0;
    fluid_real_t *left = nullptr, *right = nullptr;
    std::vector<float> audio;
    std::vector<Callback> callbacks;
    std::atomic<size_t> frames{0};
    size_t count = 0;
    bool overflow = false;
    Clock::time_point previous{};
};

/* One rendering thread writes the preallocated storage. The control thread
 * reads only frames, and inspects the storage after the driver has stopped. */
static int render(void *data, int length, int, float *[], int nout, float *out[])
{
    auto &p = *static_cast<Probe *>(data);
    auto start = Clock::now();
    if(nout != 2 || length <= 0)
    {
        p.overflow = true;
        return FLUID_FAILED;
    }
    for(int i = 0; i < length; ++i)
    {
        if(p.cursor == FLUID_BUFSIZE)
        {
            for(int j = 0; j < FLUID_BUFSIZE; ++j, ++p.generated)
            {
                double t = double(p.generated) / p.rate;
                constexpr double tau = 6.2831853071795864769;
                p.left[j] = p.dc ? 2 * p.gain : p.sine ? p.gain * 0.2 * std::sin(tau * 500 * t) : p.gain *
                    (0.2 * std::sin(tau * 223 * t) + 0.1 * std::sin(tau * 617 * t));
                p.right[j] = p.dc ? -p.gain : p.sine ? p.gain * 0.15 * std::sin(tau * 500 * t) : p.gain *
                    (0.18 * std::sin(tau * 331 * t) + 0.12 * std::sin(tau * 997 * t));
            }
            fluid_rvoice_mixer_process_fx(p.mixer, 1);
            p.cursor = 0;
        }
        out[0][i] = p.left[p.cursor];
        out[1][i] = p.right[p.cursor++];
    }
    int status = FLUID_OK;
    auto rendered = Clock::now();
    size_t frame = p.frames.load(std::memory_order_relaxed);
    if(frame + length <= p.audio.size() / 2 && p.count < p.callbacks.size())
    {
        for(int i = 0; i < length; ++i)
        {
            p.audio[2 * (frame + i)] = out[0][i];
            p.audio[2 * (frame + i) + 1] = out[1][i];
        }
        auto copied = Clock::now();
        p.callbacks[p.count++] = {frame, length, status,
            nanoseconds(rendered - start), nanoseconds(copied - start),
            p.previous == Clock::time_point{} ? 0 : nanoseconds(start - p.previous)};
    }
    else
    {
        p.overflow = true;
    }
    p.previous = start;
    p.frames.store(frame + length, std::memory_order_relaxed);
    return status;
}

static void require(int result)
{
    if(result == FLUID_FAILED)
    {
        throw std::runtime_error("FluidSynth call failed; check driver log");
    }
}

static FILE *open_output(const std::string &path)
{
    auto file = std::fopen(path.c_str(), "wb");
    if(!file)
    {
        throw std::runtime_error("Cannot write " + path);
    }
    return file;
}

int main(int argc, char **argv)
try
{
    if(argc != 9)
    {
        std::fprintf(stderr, "Usage: %s DRIVER PREFIX RATE PERIOD SECONDS static|controls|delay|rapid multitone|sine|dc GAIN\n", argv[0]);
        return 2;
    }
    PlatformAudio platform;
    std::printf("FluidSynth %s\n", fluid_version_str());
    fluid_set_log_function(FLUID_DBG, nullptr, nullptr);
    std::string prefix = argv[2], mode = argv[6], source = argv[7];
    int rate = std::stoi(argv[3]), period = std::stoi(argv[4]);
    double seconds = std::stod(argv[5]), gain = std::stod(argv[8]);
    if(rate < 8000 || rate > 192000 || period < 16 || period > 8192 ||
       seconds < 4 || seconds > 300 || gain <= 0 || gain > 0.3 ||
       (mode != "static" && mode != "controls" && mode != "delay" && mode != "rapid") ||
       (source != "multitone" && source != "sine" && source != "dc") || (source == "dc" && mode == "rapid"))
    {
        throw std::runtime_error("Invalid probe arguments");
    }
    std::unique_ptr<fluid_settings_t, decltype(&delete_fluid_settings)>
        owned_settings(new_fluid_settings(), delete_fluid_settings);
    auto settings = owned_settings.get();
    if(!settings)
    {
        throw std::bad_alloc();
    }
    require(fluid_settings_setstr(settings, "audio.driver", argv[1]));
    require(fluid_settings_setnum(settings, "synth.sample-rate", rate));
    require(fluid_settings_setint(settings, "audio.period-size", period));
    require(fluid_settings_setint(settings, "audio.periods", 2));
    require(fluid_settings_setnum(settings, "synth.gain", gain));
    require(fluid_settings_setint(settings, "synth.reverb.active", 0));
    require(fluid_settings_setint(settings, "synth.chorus.active", 0));
    require(fluid_settings_setint(settings, "synth.limiter.active", 1));
    require(fluid_settings_setnum(settings, "synth.limiter.output-limit", 0.35));
    require(fluid_settings_setnum(settings, "synth.limiter.link-channels", 1));
    Probe p;
    if(!p.frames.is_lock_free())
    {
        throw std::runtime_error("Frame counter is not lock-free on this platform");
    }
    p.dc = source == "dc";
    p.sine = source == "sine";
    p.gain = gain;
    p.rate = rate;
    p.synth = new_fluid_synth(settings);
    std::unique_ptr<fluid_synth_t, decltype(&delete_fluid_synth)>
        owned_synth(p.synth, delete_fluid_synth);
    if(!p.synth)
    {
        throw std::runtime_error("Cannot create synth");
    }
    p.mixer = p.synth->eventhandler->mixer;
    fluid_rvoice_mixer_get_bufs(p.mixer, &p.left, &p.right);
    p.audio.resize(size_t((seconds + 4) * rate) * 2);
    p.callbacks.resize(size_t((seconds + 4) * rate / 16) + 1024);
    auto events = open_output(prefix + "-events.csv");
    std::fprintf(events, "before_frame,after_frame,setting,value,setter_ns\n");
    std::unique_ptr<fluid_audio_driver_t, decltype(&delete_fluid_audio_driver)>
        driver(new_fluid_audio_driver2(settings, render, &p), delete_fluid_audio_driver);
    if(!driver)
    {
        throw std::runtime_error("Cannot open audio driver");
    }
    const auto start = Clock::now();
    auto next = start + std::chrono::seconds(2);
    int update = 0;
    {
        auto set = [&](const char *name, double value, bool integer)
        {
            auto frame = p.frames.load(std::memory_order_relaxed);
            auto begin = Clock::now();
            require(integer ? fluid_settings_setint(settings, name, int(value))
                            : fluid_settings_setnum(settings, name, value));
            auto elapsed = nanoseconds(Clock::now() - begin);
            std::fprintf(events, "%zu,%zu,%s,%.9g,%lld\n", frame,
                         p.frames.load(std::memory_order_relaxed), name, value, elapsed);
            std::printf("%.3fs %s = %.9g\n", std::chrono::duration<double>(begin - start).count(), name, value);
            std::fflush(stdout);
        };
        while(std::chrono::duration<double>(Clock::now() - start).count() < seconds)
        {
            if(mode != "static" && Clock::now() >= next &&
               std::chrono::duration<double>(Clock::now() - start).count() < seconds - (mode == "rapid" ? 1.5 : 1))
            {
                bool alternate = (++update % 2) != 0;
                if(mode == "controls" || mode == "rapid")
                {
                    set("synth.limiter.output-limit", alternate ? 0.12 : 0.35, false);
                    set("synth.limiter.hold", alternate ? 250 : 0, false);
                    set("synth.limiter.release", alternate ? 250 : 0, false);
                    set("synth.limiter.smoothing-stages", update % 3 + 1, true);
                    set("synth.limiter.link-channels", alternate ? 0 : 1, false);
                }
                if(mode == "delay")
                {
                    switch((update - 1) % 4)
                    {
                    case 0: set("synth.limiter.attack", 250, false); break;
                    case 1: set("synth.limiter.attack", 1, false); break;
                    case 2: set("synth.limiter.active", 0, true); break;
                    case 3: set("synth.limiter.active", 1, true); break;
                    }
                }
                if(mode == "rapid")
                {
                    set("synth.limiter.attack", alternate ? 250 : 1, false);
                    set("synth.limiter.active", update % 3 != 0, true);
                }
                next += std::chrono::milliseconds(mode == "rapid" ? 10 : 750);
                /* Preparation can be slow; don't run an unbounded catch-up burst. */
                next = std::max(next, Clock::now());
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    driver.reset();
    std::fclose(events);
    size_t frames = std::min(p.frames.load(), p.audio.size() / 2);
    auto audio = open_output(prefix + "-rendered.f32");
    bool written = std::fwrite(p.audio.data(), sizeof(float) * 2, frames, audio) == frames;
    std::fclose(audio);
    auto csv = open_output(prefix + "-callbacks.csv");
    std::fprintf(csv, "frame,frames,status,render_ns,capture_ns,interval_ns\n");
    for(size_t i = 0; i < p.count; ++i)
    {
        const auto &c = p.callbacks[i];
        std::fprintf(csv, "%zu,%d,%d,%lld,%lld,%lld\n", c.frame, c.length,
                     c.status, c.render_ns, c.capture_ns, c.interval_ns);
    }
    std::fclose(csv);
    if(!written || p.overflow || frames == 0)
    {
        throw std::runtime_error("Probe capture incomplete or no callbacks received");
    }
    return 0;
}
catch(const std::exception &error)
{
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
}
