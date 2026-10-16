#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <type_traits>
#include <vector>

#include "rvoice/fluid_rev_dattorro.h"
#include "hiir/Downsampler2xF64Fpu.h"
#include "hiir/PolyphaseIir2Designer.h"
#include "hiir/Upsampler2xF64Fpu.h"

namespace
{
constexpr int high_frames = FLUID_BUFSIZE;
constexpr int low_frames = FLUID_BUFSIZE / 2;
constexpr int coef_count = 6;
constexpr int blocks = 16384;
constexpr int measured_repeats = 7;
constexpr int warmup_repeats = 1;
using sample_t = fluid_real_t;
using down_t = hiir::Downsampler2xF64Fpu<coef_count>;
using up_t = hiir::Upsampler2xF64Fpu<coef_count>;
static_assert(std::is_same<sample_t, double>::value, "Build this screen in double precision");

struct converter_t
{
    down_t down;
    std::array<up_t, 2> up;
    std::array<double, low_frames> low{};
    std::array<std::array<double, high_frames>, 2> high{};

    explicit converter_t(const double *coefs)
    {
        down.set_coefs(coefs);
        for(int ch = 0; ch < 2; ++ch)
            up[ch].set_coefs(coefs);
    }

    void reset()
    {
        down.clear_buffers();
        for(int ch = 0; ch < 2; ++ch)
            up[ch].clear_buffers();
    }

    void downsample(const std::array<double, high_frames> &mono)
    {
        down.process_block(low.data(), mono.data(), low_frames);
    }

    void upsample(const std::array<double, low_frames> &mono)
    {
        for(int ch = 0; ch < 2; ++ch)
            up[ch].process_block(high[ch].data(), mono.data(), low_frames);
    }

    void convert(const std::array<double, high_frames> &mono)
    {
        downsample(mono);
        upsample(low);
    }
};

struct input_t
{
    std::array<double, high_frames> left{};
    std::array<double, high_frames> right{};
    std::array<double, low_frames> low_left{};
    std::array<double, low_frames> low_right{};

    input_t()
    {
        for(int i = 0; i < high_frames; ++i)
        {
            left[i] = 0.3 * std::sin(0.0013 * i) + 0.1 * std::sin(0.057 * i);
            right[i] = 0.2 * std::sin(0.0021 * i) - 0.07 * std::sin(0.041 * i);
        }
        for(int i = 0; i < low_frames; ++i)
        {
            low_left[i] = left[2 * i];
            low_right[i] = right[2 * i];
        }
    }
};

template<class Fn>
double timed_trial(Fn &&fn)
{
    const auto start = std::chrono::steady_clock::now();
    const double checksum = fn();
    const auto end = std::chrono::steady_clock::now();
    if(!std::isfinite(checksum)) std::abort();
    return std::chrono::duration<double>(end - start).count();
}

template<class Fn>
std::vector<double> measure(Fn &&fn)
{
    for(int i = 0; i < warmup_repeats; ++i) fn();
    std::vector<double> values;
    values.reserve(measured_repeats);
    for(int i = 0; i < measured_repeats; ++i) values.push_back(timed_trial(fn));
    std::sort(values.begin(), values.end());
    return values;
}

double run_full(fluid_revmodel_dattorro &rev, const input_t &input)
{
    std::array<double, high_frames> out_l{}, out_r{};
    double checksum = 0.0;
    rev.reset();
    for(int b = 0; b < blocks; ++b)
    {
        rev.processmix(input.left.data(), out_l.data(), out_r.data());
        checksum += out_l[b % high_frames] + out_r[(b + 3) % high_frames];
    }
    return checksum;
}

double run_core(fluid_revmodel_dattorro &rev, const input_t &input)
{
    std::array<double, low_frames> out_l{}, out_r{};
    double checksum = 0.0;
    rev.reset();
    for(int b = 0; b < blocks; ++b)
    {
        rev.process_n(input.low_left.data(), out_l.data(), out_r.data(), low_frames);
        checksum += out_l[b % low_frames] + out_r[(b + 3) % low_frames];
    }
    return checksum;
}

double run_converter(converter_t &converter, const input_t &input)
{
    double checksum = 0.0;
    converter.reset();
    for(int b = 0; b < blocks; ++b)
    {
        converter.convert(input.left);
        checksum += converter.high[0][b % high_frames] + converter.high[1][(b + 3) % high_frames];
    }
    return checksum;
}

double run_multirate(fluid_revmodel_dattorro &rev, converter_t &converter, const input_t &input)
{
    std::array<double, low_frames> wet_l{}, wet_r{};
    double checksum = 0.0;
    rev.reset();
    converter.reset();
    for(int b = 0; b < blocks; ++b)
    {
        converter.downsample(input.left);
        rev.process_n(converter.low.data(), wet_l.data(), wet_r.data(), low_frames);
        converter.up[0].process_block(converter.high[0].data(), wet_l.data(), low_frames);
        converter.up[1].process_block(converter.high[1].data(), wet_r.data(), low_frames);
        checksum += converter.high[0][b % high_frames] + converter.high[1][(b + 3) % high_frames];
    }
    return checksum;
}

void print_times(const char *label, const std::vector<double> &samples)
{
    std::printf("%s_ms", label);
    for(double value : samples) std::printf(" %.6f", value * 1000.0);
    std::printf("\n");
}
}

int main()
{
    const double transition = (22050.0 - 16500.0) / 88200.0;
    double coefs[coef_count]{};
    const int designed = hiir::PolyphaseIir2Designer::compute_coefs(coefs, 80.0, transition);
    if(designed != coef_count) return 2;

    input_t input;
    converter_t converter(coefs);
    fluid_revmodel_dattorro full(88200.0), half(44100.0);
    full.set(FLUID_REVMODEL_SET_ALL, 0.7, 0.35, 1.0, 0.8);
    half.set(FLUID_REVMODEL_SET_ALL, 0.7, 0.35, 1.0, 0.8);

    const auto full_times = measure([&] { return run_full(full, input); });
    const auto core_times = measure([&] { return run_core(half, input); });
    const auto converter_times = measure([&] { return run_converter(converter, input); });
    const auto combo_times = measure([&] { return run_multirate(half, converter, input); });

    std::printf("frames %d blocks %d repeats %d taps_per_halfband_path %d transition %.9f attenuation_db %.6f\n",
                blocks * high_frames, blocks, measured_repeats, designed, transition,
                hiir::PolyphaseIir2Designer::compute_atten_from_order_tbw(designed, transition));
    print_times("full_rate_core", full_times);
    print_times("half_rate_core_direct", core_times);
    print_times("converter_without_reverb", converter_times);
    print_times("complete_multirate", combo_times);
}
