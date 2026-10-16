#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <vector>

#include "rvoice/fluid_rev_fdn.h"

namespace
{
constexpr int block_size = FLUID_BUFSIZE;
constexpr int impulse_blocks = 8270; // 12.00 s at 44.1 kHz, rounded to a block
constexpr int bench_blocks = 2048;
constexpr int repeats = 7;
constexpr double sample_rate = 44100.0;

void write_impulse(const char *path, std::vector<fluid_real_t> &left, std::vector<fluid_real_t> &right)
{
    fluid_revmodel_fdn rev(sample_rate, sample_rate);
    rev.set(FLUID_REVMODEL_SET_ALL, 0.7, 0.35, 1.0, 0.8);
    std::array<fluid_real_t, block_size> in{}, out_l{}, out_r{};
    left.resize(impulse_blocks * block_size);
    right.resize(impulse_blocks * block_size);
    in[0] = 1.0;
    for(int b = 0; b < impulse_blocks; ++b)
    {
        rev.processmix(in.data(), out_l.data(), out_r.data());
        std::copy(out_l.begin(), out_l.end(), left.begin() + b * block_size);
        std::copy(out_r.begin(), out_r.end(), right.begin() + b * block_size);
        in.fill(0.0);
    }
    std::ofstream out(path, std::ios::binary);
    for(size_t i = 0; i < left.size(); ++i)
    {
        double lr[2] = {static_cast<double>(left[i]), static_cast<double>(right[i])};
        out.write(reinterpret_cast<const char *>(lr), sizeof(lr));
    }
    out.close();
    double energy_l = 0.0, energy_r = 0.0, peak = 0.0;
    for(size_t i = 0; i < left.size(); ++i)
    {
        energy_l += static_cast<double>(left[i]) * left[i];
        energy_r += static_cast<double>(right[i]) * right[i];
        peak = std::max(peak, std::max(std::abs(static_cast<double>(left[i])), std::abs(static_cast<double>(right[i]))));
    }
    std::printf("impulse frames %zu energy_l %.12g energy_r %.12g peak %.12g\n",
                left.size(), energy_l, energy_r, peak);
}

double benchmark()
{
    fluid_revmodel_fdn rev(sample_rate, sample_rate);
    rev.set(FLUID_REVMODEL_SET_ALL, 0.7, 0.35, 1.0, 0.8);
    std::vector<fluid_real_t> input(bench_blocks * block_size);
    for(size_t i = 0; i < input.size(); ++i)
        input[i] = static_cast<fluid_real_t>(0.25 * std::sin(0.0017 * i) + 0.08 * std::sin(0.073 * i));
    std::array<fluid_real_t, block_size> out_l{}, out_r{};
    std::vector<double> samples;
    samples.reserve(repeats);
    double checksum = 0.0;
    for(int repeat = 0; repeat < repeats; ++repeat)
    {
        rev.reset();
        auto start = std::chrono::steady_clock::now();
        for(int b = 0; b < bench_blocks; ++b)
        {
            rev.processmix(input.data() + b * block_size, out_l.data(), out_r.data());
            checksum += out_l[repeat] + out_r[repeat + 4];
        }
        auto end = std::chrono::steady_clock::now();
        samples.push_back(std::chrono::duration<double>(end - start).count());
    }
    std::sort(samples.begin(), samples.end());
    std::printf("bench blocks %d frames %d repeats %d median_s %.8f all_s",
                bench_blocks, bench_blocks * block_size, repeats, samples[repeats / 2]);
    for(double v : samples) std::printf(" %.8f", v);
    std::printf(" checksum %.12g\n", checksum);
    return samples[repeats / 2];
}
}

int main(int argc, char **argv)
{
    if(argc != 2) return 2;
    std::vector<fluid_real_t> left, right;
    write_impulse(argv[1], left, right);
    benchmark();
}
