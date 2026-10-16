#include "fluid_sys.h"
#include "fluid_rvoice_dsp_sinc.hpp"
#include <adwt.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
constexpr double kRate = 44100.0;
constexpr int kTableLen = 4096;
constexpr int kFftLen = 16384;
constexpr int kHalfTaps = 12;
constexpr double kPi = 3.141592653589793238462643383279502884;

static double now_s()
{
    using clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

static double wrap01(double x)
{
    x -= std::floor(x);
    return x;
}

static float periodic_value(const std::string &kind, double phase)
{
    if(kind == "sine") return static_cast<float>(std::sin(2.0 * kPi * phase));
    if(kind == "saw") return static_cast<float>(2.0 * phase - 1.0);
    if(kind == "pulse") return phase < 0.25 ? 1.0f : -1.0f;
    std::abort();
}

static std::vector<float> make_table(const std::string &kind)
{
    std::vector<float> wave(kTableLen);
    for(int i = 0; i < kTableLen; ++i)
        wave[i] = periodic_value(kind, static_cast<double>(i) / kTableLen);
    return wave;
}

static double bandlimited_value(const std::string &kind, double phase, int max_harmonic)
{
    if(kind == "sine") return std::sin(2.0 * kPi * phase);
    double result = 0.0;
    if(kind == "saw")
    {
        for(int h = 1; h <= max_harmonic; ++h)
            result -= 2.0 / (kPi * h) * std::sin(2.0 * kPi * h * phase);
        return result;
    }
    const double duty = 0.25;
    result = 2.0 * duty - 1.0;
    for(int h = 1; h <= max_harmonic; ++h)
    {
        const double theta = 2.0 * kPi * h * duty;
        const double angle = 2.0 * kPi * h * phase;
        result += 2.0 / (kPi * h) *
                  (std::sin(theta) * std::cos(angle) + (1.0 - std::cos(theta)) * std::sin(angle));
    }
    return result;
}

template<typename T>
static void write_f64(const fs::path &path, const std::vector<T> &values, int start)
{
    std::ofstream out(path, std::ios::binary);
    for(int i = start; i < start + kFftLen; ++i)
    {
        const double x = static_cast<double>(values[i]);
        out.write(reinterpret_cast<const char *>(&x), sizeof(x));
    }
}

int main(int argc, char **argv)
{
    if(argc != 2)
    {
        std::cerr << "usage: aa_static_probe OUTPUT_DIR\n";
        return 2;
    }
    const fs::path output_dir(argv[1]);
    fs::create_directories(output_dir);
    const auto &sinc_table = fluid_interp_sinc_coeff_table<25>();
    std::ofstream meta(output_dir / "screen.csv");
    meta << "waveform,fundamental_bin,frequency_hz,max_harmonic,table_len,table_bytes,mipmap_tables,adwt_process_s,sinc_process_s\n";

    const std::array<int, 6> bins{128, 512, 2048, 4096, 6144, 7000};
    const std::array<std::string, 3> kinds{"sine", "saw", "pulse"};
    for(const auto &kind : kinds)
    {
        auto wave = make_table(kind);
        const auto build_start = now_s();
        auto wavetable = adwt::WavetableData::build(
            adwt::Span<const float>(wave.data(), wave.size()), 1, static_cast<float>(kRate));
        const double build_s = now_s() - build_start;
        if(!wavetable)
        {
            std::cerr << "WavetableData build failed for " << kind << "\n";
            return 3;
        }
        std::size_t table_bytes = 0;
        for(int m = 0; m < wavetable->numMipMapTables(); ++m)
        {
            table_bytes += (wavetable->m(0, m).size() + wavetable->q(0, m).size() +
                            wavetable->mDiff(0, m).size() + wavetable->qDiff(0, m).size() +
                            wavetable->phases(m).size()) * sizeof(float);
        }
        const int mipmap_tables = wavetable->numMipMapTables();
        adwt::Oscillator<adwt::FilterType::kType5> osc;
        if(osc.init(std::move(wavetable), {0.0f, 0.01f}) != 0)
        {
            std::cerr << "Oscillator init failed for " << kind << "\n";
            return 4;
        }

        for(int bin : bins)
        {
            const double frequency = kRate * bin / kFftLen;
            const double step = frequency / kRate;
            const int max_harmonic = static_cast<int>(std::floor((kRate * 0.5 - 1e-9) / frequency));
            std::vector<float> phases(2 * kFftLen);
            std::vector<float> aa(2 * kFftLen);
            std::vector<double> sinc(2 * kFftLen);
            std::vector<double> reference(kFftLen);
            for(int i = 0; i < 2 * kFftLen; ++i)
                phases[i] = static_cast<float>(wrap01((i + 1) * step));

            const double aa_start = now_s();
            osc.resetInternals(0.0f, static_cast<float>(step));
            osc.process<adwt::Direction::kForward>(
                adwt::Span<const float>(phases.data(), phases.size()),
                adwt::Span<float>(aa.data(), aa.size()));
            const double aa_s = now_s() - aa_start;

            const double sinc_start = now_s();
            for(int i = 0; i < 2 * kFftLen; ++i)
            {
                const double pos = wrap01((i + 1) * step) * kTableLen;
                int center = static_cast<int>(std::floor(pos + 0.5));
                double fraction = pos + 0.5 - center;
                if(fraction >= 1.0)
                {
                    fraction = 0.0;
                    ++center;
                }
                const uint32_t phase32 = static_cast<uint32_t>(fraction * 4294967296.0);
                std::array<fluid_real_t, 25> samples{};
                for(int tap = 0; tap < 25; ++tap)
                {
                    const int idx = (center + tap - kHalfTaps + kTableLen) & (kTableLen - 1);
                    samples[tap] = static_cast<fluid_real_t>(wave[idx]);
                }
                sinc[i] = fluid_interp_sinc_kernel<25>(samples, phase32, sinc_table);
            }
            const double sinc_s = now_s() - sinc_start;

            const int segment = kFftLen;
            for(int i = 0; i < kFftLen; ++i)
            {
                const double phase = wrap01((i + kFftLen + 1) * step);
                reference[i] = bandlimited_value(kind, phase, max_harmonic);
            }
            const std::string tag = kind + "-bin" + std::to_string(bin);
            write_f64(output_dir / (tag + "-aa.f64"), aa, segment);
            write_f64(output_dir / (tag + "-sinc.f64"), sinc, segment);
            write_f64(output_dir / (tag + "-reference.f64"), reference, 0);
            meta << kind << ',' << bin << ',' << std::setprecision(12) << frequency << ','
                 << max_harmonic << ',' << kTableLen << ',' << table_bytes << ','
                 << mipmap_tables << ',' << aa_s << ',' << sinc_s << '\n';
            std::cerr << tag << " table_build_s=" << build_s << " aa_s=" << aa_s
                      << " sinc_s=" << sinc_s << " table_bytes=" << table_bytes << '\n';
        }
    }
    return 0;
}
