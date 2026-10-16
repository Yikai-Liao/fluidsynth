#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

#include "rvoice/fluid_rev_dattorro.h"

namespace
{
constexpr int frames_per_block = FLUID_BUFSIZE;
constexpr int low_frames_per_block = FLUID_BUFSIZE / 2;
constexpr int filter_length = 89;
constexpr double fs = 88200.0;
constexpr double pass_hz = 16500.0;
constexpr double stop_hz = 22050.0;
constexpr double beta = 0.1102 * (80.0 - 8.7);
constexpr double pi = 3.14159265358979323846;

double i0(double x)
{
    double term = 1.0, sum = 1.0;
    for(int k = 1; k < 100; ++k)
    {
        term *= (x * x / 4.0) / (k * k);
        double next = sum + term;
        if(next == sum) break;
        sum = next;
    }
    return sum;
}

std::vector<double> make_filter()
{
    int mid = filter_length / 2;
    double fc = (pass_hz + stop_hz) / (2.0 * fs);
    double denom = i0(beta);
    std::vector<double> h(filter_length);
    double sum = 0.0;
    for(int n = 0; n < filter_length; ++n)
    {
        double x = n - mid;
        double sinc = x == 0.0 ? 2.0 * fc : std::sin(2.0 * pi * fc * x) / (pi * x);
        double ratio = x / mid;
        double window = i0(beta * std::sqrt(std::max(0.0, 1.0 - ratio * ratio))) / denom;
        h[n] = sinc * window;
        sum += h[n];
    }
    for(double &v : h) v /= sum;
    return h;
}

struct FilterState
{
    std::array<fluid_real_t, filter_length> high{};
    int high_pos = 0;
    std::array<std::array<fluid_real_t, (filter_length + 1) / 2>, 2> low{};
    int low_pos = 0;
    int low_frames = 0;

    fluid_real_t get_high_delay(int delay) const
    {
        int i = high_pos - 1 - delay;
        if(i < 0) i += filter_length;
        return high[i];
    }

    fluid_real_t get_low_delay(int channel, int delay) const
    {
        int i = low_pos - 1 - delay;
        if(i < 0) i += static_cast<int>(low[channel].size());
        return low[channel][i];
    }
};

double run_high(fluid_revmodel_dattorro &rev, const std::vector<fluid_real_t> &input,
                int blocks, int repeats)
{
    std::array<fluid_real_t, frames_per_block> out_l{}, out_r{};
    double checksum = 0.0;
    auto start = std::chrono::steady_clock::now();
    for(int repeat = 0; repeat < repeats; ++repeat)
    {
        rev.reset();
        for(int b = 0; b < blocks; ++b)
        {
            const fluid_real_t *in = input.data() + b * frames_per_block;
            rev.processmix(in, out_l.data(), out_r.data());
            checksum += out_l[repeat % frames_per_block] + out_r[(repeat + 3) % frames_per_block];
        }
    }
    auto end = std::chrono::steady_clock::now();
    std::printf("high-only checksum %.9g\n", checksum);
    return std::chrono::duration<double>(end - start).count() / repeats;
}

double run_multirate(fluid_revmodel_dattorro &rev, const std::vector<fluid_real_t> &input,
                     const std::vector<double> &h, int blocks, int repeats, bool process_reverb = true)
{
    std::array<fluid_real_t, low_frames_per_block> low_in{}, low_l{}, low_r{};
    std::array<fluid_real_t, frames_per_block> up_l{}, up_r{};
    double checksum = 0.0;
    auto start = std::chrono::steady_clock::now();
    for(int repeat = 0; repeat < repeats; ++repeat)
    {
        rev.reset();
        FilterState st{};
        for(int b = 0; b < blocks; ++b)
        {
            const fluid_real_t *in = input.data() + b * frames_per_block;
            int low_count = 0;
            for(int n = 0; n < frames_per_block; ++n)
            {
                st.high[st.high_pos] = in[n];
                st.high_pos = (st.high_pos + 1) % filter_length;
                if((n & 1) == 0)
                {
                    int mid = filter_length / 2;
                    fluid_real_t value = static_cast<fluid_real_t>(h[mid] * st.get_high_delay(mid));
                    for(int k = 0; k < mid; ++k)
                    {
                        value += static_cast<fluid_real_t>(h[k] *
                            (st.get_high_delay(k) + st.get_high_delay(filter_length - 1 - k)));
                    }
                    low_in[low_count++] = value;
                }
            }
            if(low_count != low_frames_per_block) std::abort();
            if(process_reverb)
            {
                rev.process_n(low_in.data(), low_l.data(), low_r.data(), low_frames_per_block);
            }
            else
            {
                low_l = low_in;
                low_r = low_in;
            }
            for(int m = 0; m < low_frames_per_block; ++m)
            {
                st.low[0][st.low_pos] = low_l[m];
                st.low[1][st.low_pos] = low_r[m];
                st.low_pos = (st.low_pos + 1) % static_cast<int>(st.low[0].size());
                ++st.low_frames;

                for(int channel = 0; channel < 2; ++channel)
                {
                    fluid_real_t even = static_cast<fluid_real_t>(2.0 * h[filter_length / 2] *
                        st.get_low_delay(channel, (filter_length - 1) / 4));
                    for(int j = 0; j < (filter_length - 1) / 4; ++j)
                    {
                        fluid_real_t pair = st.get_low_delay(channel, j) +
                            st.get_low_delay(channel, (filter_length - 1) / 2 - j);
                        even += static_cast<fluid_real_t>(2.0 * h[2 * j] * pair);
                    }

                    fluid_real_t odd = 0;
                    for(int j = 0; j < (filter_length - 1) / 4; ++j)
                    {
                        fluid_real_t pair = st.get_low_delay(channel, j) +
                            st.get_low_delay(channel, (filter_length - 3) / 2 - j);
                        odd += static_cast<fluid_real_t>(2.0 * h[2 * j + 1] * pair);
                    }
                    if(channel == 0)
                    {
                        up_l[2 * m] = even;
                        up_l[2 * m + 1] = odd;
                    }
                    else
                    {
                        up_r[2 * m] = even;
                        up_r[2 * m + 1] = odd;
                    }
                }
            }
            checksum += up_l[repeat % frames_per_block] + up_r[(repeat + 3) % frames_per_block];
        }
    }
    auto end = std::chrono::steady_clock::now();
    std::printf("multirate checksum %.9g\n", checksum);
    return std::chrono::duration<double>(end - start).count() / repeats;
}
}

int main()
{
    constexpr int blocks = 1024;
    constexpr int repeats = 5;
    std::vector<fluid_real_t> input(blocks * frames_per_block);
    for(size_t i = 0; i < input.size(); ++i)
        input[i] = static_cast<fluid_real_t>(0.3 * std::sin(0.0013 * i) + 0.1 * std::sin(0.057 * i));

    auto h = make_filter();
    fluid_revmodel_dattorro high(88200.0);
    fluid_revmodel_dattorro low(44100.0);
    high.set(FLUID_REVMODEL_SET_ALL, 0.7, 0.35, 1.0, 0.8);
    low.set(FLUID_REVMODEL_SET_ALL, 0.7, 0.35, 1.0, 0.8);

    double high_s = run_high(high, input, blocks, repeats);
    double low_s = run_multirate(low, input, h, blocks, repeats, false);
    double multi_s = run_multirate(low, input, h, blocks, repeats, true);
    std::printf("frames %.0f blocks %d repeats %d high_s %.6f converter_only_s %.6f multirate_s %.6f ratios %.4f %.4f\n",
                blocks * static_cast<double>(frames_per_block), blocks, repeats, high_s, low_s, multi_s,
                low_s / high_s, multi_s / high_s);
}
