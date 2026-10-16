#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr unsigned kSampleRate = 44100;
constexpr size_t kSignalFrames = 131072;
constexpr size_t kBenchBlocks = 131072;
constexpr size_t kBlockFrames = 64;
constexpr int kPairs = 7;

struct Coefficients { float a1{}, a2{}, b02{}, b1{}; };

static double cents_to_hz(int cents)
{
    return 440.0 * std::exp2((static_cast<double>(cents) - 6900.0) / 1200.0);
}

static Coefficients make_lowpass(int cents, float q)
{
    cents = std::clamp(cents, 1500, 13500);
    const float hz = static_cast<float>(cents_to_hz(cents));
    const float period = static_cast<float>(2.0 * kPi / kSampleRate);
    const float omega = period * hz;
    const float sn = std::sin(omega);
    const float cs = std::cos(omega);
    const float alpha = sn / (2.0f * q);
    const float inv = 1.0f / (1.0f + alpha);
    const float gain = 1.0f / std::sqrt(q);
    const float b1 = (1.0f - cs) * inv * gain;
    return {-2.0f * cs * inv, (1.0f - alpha) * inv, b1 * 0.5f, b1};
}

template<typename T> struct State { T h1{}, h2{}; };

template<typename T>
struct BlockParams
{
    T b02{}, b1{};
    std::array<double, 4> p{}, u{}, v{};

    explicit BlockParams(Coefficients c)
        : b02(static_cast<T>(c.b02)), b1(static_cast<T>(c.b1))
    {
        const double aa = -static_cast<double>(c.a1);
        const double bb = -static_cast<double>(c.a2);
        p[0] = 1.0;
        p[1] = aa;
        p[2] = aa * p[1] + bb * p[0];
        p[3] = aa * p[2] + bb * p[1];
        u[0] = aa; v[0] = bb;
        u[1] = aa * u[0] + bb; v[1] = aa * v[0];
        u[2] = aa * u[1] + bb * u[0]; v[2] = aa * v[1] + bb * v[0];
        u[3] = aa * u[2] + bb * u[1]; v[3] = aa * v[2] + bb * v[1];
    }
};

template<typename T>
static inline T scalar_sample(const Coefficients &c, T x, State<T> &s)
{
    const T a1 = static_cast<T>(c.a1), a2 = static_cast<T>(c.a2);
    const T b02 = static_cast<T>(c.b02), b1 = static_cast<T>(c.b1);
    const T w = x - a1 * s.h1 - a2 * s.h2;
    const T y = b02 * (w + s.h2) + b1 * s.h1;
    s.h2 = s.h1;
    s.h1 = w;
    return y;
}

template<typename T>
static void scalar_run(const Coefficients &c, const T *input, T *output,
                       size_t count, State<T> &s)
{
    for(size_t i = 0; i < count; ++i) output[i] = scalar_sample(c, input[i], s);
}

template<int W, typename T>
static inline void block_run(const Coefficients &c, const BlockParams<T> &p,
                             const T *input, T *output, size_t count, State<T> &s)
{
    static_assert(W == 2 || W == 4);
    size_t i = 0;
    for(; i + W <= count; i += W)
    {
        const T h1 = s.h1, h2 = s.h2;
        std::array<T, W> w{};
        w[0] = static_cast<T>(static_cast<double>(input[i]) + p.u[0] * (double)h1 + p.v[0] * (double)h2);
        for(int k = 1; k < W; ++k)
        {
            double value = static_cast<double>(input[i + k]) + p.u[k] * (double)h1 + p.v[k] * (double)h2;
            for(int j = 0; j < k; ++j) value += p.p[k - j] * input[i + j];
            w[k] = static_cast<T>(value);
        }
        for(int k = 0; k < W; ++k)
        {
            const T prev1 = k == 0 ? h1 : w[k - 1];
            const T prev2 = k == 0 ? h2 : (k == 1 ? h1 : w[k - 2]);
            output[i + k] = p.b02 * (w[k] + prev2) + p.b1 * prev1;
        }
        s.h1 = w[W - 1];
        s.h2 = w[W - 2];
    }
    if(i < count) scalar_run(c, input + i, output + i, count - i, s);
}

struct Metrics { double rms_error{}, max_error{}, rms_reference{}, rel_error_db{}, max_abs_dbfs{}; };

template<typename T>
static Metrics compare(const std::vector<T> &candidate, const std::vector<T> &reference)
{
    long double e2 = 0.0L, r2 = 0.0L;
    double mx = 0.0;
    for(size_t i = 0; i < candidate.size(); ++i)
    {
        const double e = static_cast<double>(candidate[i]) - static_cast<double>(reference[i]);
        const double r = static_cast<double>(reference[i]);
        e2 += (long double)e * e;
        r2 += (long double)r * r;
        mx = std::max(mx, std::abs(e));
    }
    const double erms = std::sqrt(static_cast<double>(e2 / candidate.size()));
    const double rrms = std::sqrt(static_cast<double>(r2 / reference.size()));
    const double rel = erms > 0.0 && rrms > 0.0 ? 20.0 * std::log10(erms / rrms) : -INFINITY;
    const double mxdb = mx > 0.0 ? 20.0 * std::log10(mx) : -INFINITY;
    return {erms, mx, rrms, rel, mxdb};
}

static std::vector<double> make_noise(size_t n, double scale)
{
    std::vector<double> x(n);
    uint64_t state = 0x9e3779b97f4a7c15ULL;
    for(double &v : x)
    {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        const uint64_t z = state * 0x2545F4914F6CDD1DULL;
        v = (static_cast<double>(z >> 11) / 9007199254740992.0 * 2.0 - 1.0) * scale;
    }
    return x;
}

template<typename T, int W>
static void write_quality_for_type(const std::string &path)
{
    std::ofstream out(path, std::ios::app);
    const std::array<std::pair<int, float>, 6> designs{{
        {1500, 0.70710678f}, {1500, 20.0f}, {6000, 10.0f},
        {9000, 40.0f}, {12000, 10.0f}, {13500, 40.0f}
    }};
    const auto noise = make_noise(kSignalFrames, 0.25);
    for(size_t d = 0; d < designs.size(); ++d)
    {
        const auto c = make_lowpass(designs[d].first, designs[d].second);
        const BlockParams<T> p(c);
        std::vector<T> input(kSignalFrames), ref(kSignalFrames), cand(kSignalFrames);
        for(size_t i = 0; i < input.size(); ++i) input[i] = static_cast<T>(noise[i]);
        State<T> sr{}, sc{};
        scalar_run(c, input.data(), ref.data(), input.size(), sr);
        block_run<W>(c, p, input.data(), cand.data(), input.size(), sc);
        const auto active = compare(cand, ref);
        const double state_diff = std::max(std::abs((double)(sr.h1 - sc.h1)), std::abs((double)(sr.h2 - sc.h2)));
        bool finite = std::isfinite((double)sc.h1) && std::isfinite((double)sc.h2);
        for(size_t i = 0; i < cand.size(); ++i) finite = finite && std::isfinite((double)cand[i]);
        out << "noise," << (sizeof(T) == sizeof(float) ? "float" : "double") << ',' << W << ','
            << designs[d].first << ',' << designs[d].second << ",," << 20.0 * std::log10(active.rms_reference) << ",," << active.rel_error_db << ','
            << active.max_abs_dbfs << ',' << state_diff << ',' << (finite ? 1 : 0) << ",,\n";

        for(double hz : {20.0, 100.0, 1000.0, 6000.0, 0.30 * kSampleRate, 0.45 * kSampleRate})
        {
            std::vector<T> sine(kSignalFrames), ys(kSignalFrames), yc(kSignalFrames);
            for(size_t i = 0; i < sine.size(); ++i)
                sine[i] = static_cast<T>(0.2 * std::sin(2.0 * kPi * hz * i / kSampleRate));
            State<T> ss{}, cs{};
            scalar_run(c, sine.data(), ys.data(), sine.size(), ss);
            block_run<W>(c, p, sine.data(), yc.data(), sine.size(), cs);
            long double a2 = 0.0L, b2 = 0.0L, e2 = 0.0L;
            double max_error = 0.0;
            const size_t begin = sine.size() - 32768;
            for(size_t i = begin; i < sine.size(); ++i)
            {
                const double a = ys[i], b = yc[i], e = b - a;
                a2 += (long double)a * a; b2 += (long double)b * b; e2 += (long double)e * e;
                max_error = std::max(max_error, std::abs(e));
            }
            const double ar = std::sqrt((double)(a2 / 32768));
            const double br = std::sqrt((double)(b2 / 32768));
            const double er = std::sqrt((double)(e2 / 32768));
            const double gain_db = 20.0 * std::log10(br / ar);
            const double rel_db = 20.0 * std::log10(er / ar);
            const double state_diff_sine = std::max(std::abs((double)(ss.h1 - cs.h1)), std::abs((double)(ss.h2 - cs.h2)));
            const bool sine_finite = std::isfinite((double)cs.h1) && std::isfinite((double)cs.h2)
                                     && std::isfinite(gain_db) && std::isfinite(rel_db);
            out << "sine," << (sizeof(T) == sizeof(float) ? "float" : "double") << ',' << W << ','
                << designs[d].first << ',' << designs[d].second << ',' << hz << ',' << 20.0 * std::log10(ar) << ',' << gain_db << ','
                << rel_db << ',' << (max_error > 0.0 ? 20.0 * std::log10(max_error) : -INFINITY) << ','
                << state_diff_sine << ',' << (sine_finite ? 1 : 0) << ",,\n";
        }

        std::vector<T> impulse(kSignalFrames, T{}), impulse_ref(kSignalFrames), impulse_cand(kSignalFrames);
        impulse[0] = T{1};
        State<T> is{}, ic{};
        scalar_run(c, impulse.data(), impulse_ref.data(), impulse.size(), is);
        block_run<W>(c, p, impulse.data(), impulse_cand.data(), impulse.size(), ic);
        long double tail2 = 0.0L;
        double tailmax = 0.0;
        for(size_t i = 65536; i < impulse.size(); ++i)
        {
            const double e = (double)impulse_cand[i] - (double)impulse_ref[i];
            tail2 += (long double)e * e;
            tailmax = std::max(tailmax, std::abs(e));
        }
        const double tail_rms_dbfs = tail2 > 0.0L ? 20.0 * std::log10(std::sqrt((double)(tail2 / 65536.0L))) : -INFINITY;
        const double tail_max_dbfs = tailmax > 0.0 ? 20.0 * std::log10(tailmax) : -INFINITY;
        const auto impulse_metrics = compare(impulse_cand, impulse_ref);
        const double impulse_state_diff = std::max(std::abs((double)(is.h1 - ic.h1)), std::abs((double)(is.h2 - ic.h2)));
        const bool impulse_finite = std::isfinite((double)ic.h1) && std::isfinite((double)ic.h2);
        out << "impulse_tail," << (sizeof(T) == sizeof(float) ? "float" : "double") << ',' << W << ','
            << designs[d].first << ',' << designs[d].second << ",," << 20.0 * std::log10(impulse_metrics.rms_reference) << ",," << impulse_metrics.rel_error_db << ','
            << impulse_metrics.max_abs_dbfs << ',' << impulse_state_diff << ',' << (impulse_finite ? 1 : 0) << ','
            << tail_rms_dbfs << ',' << tail_max_dbfs << '\n';
    }
}

template<typename T, int W>
static void write_dynamic_for_type(const std::string &path)
{
    std::ofstream out(path, std::ios::app);
    const auto noise = make_noise(kSignalFrames, 0.25);
    std::vector<T> input(kSignalFrames), ref(kSignalFrames), cand(kSignalFrames);
    for(size_t i = 0; i < input.size(); ++i) input[i] = static_cast<T>(noise[i]);
    for(size_t run_length : {1u, 2u, 3u, 4u, 5u, 31u, 64u, 127u})
    {
        State<T> sr{}, sc{};
        size_t pos = 0, segment = 0;
        while(pos < input.size())
        {
            const size_t count = std::min(run_length, input.size() - pos);
            const size_t mod_index = (segment++ * run_length) % 1200;
            const int cents = 6000 + static_cast<int>(mod_index);
            const float q = 4.0f + static_cast<float>((mod_index * 5) % 1000) * 0.0001f;
            const Coefficients coeff = make_lowpass(cents, q);
            const BlockParams<T> p(coeff);
            scalar_run(coeff, input.data() + pos, ref.data() + pos, count, sr);
            block_run<W>(coeff, p, input.data() + pos, cand.data() + pos, count, sc);
            pos += count;
        }
        const auto m = compare(cand, ref);
        const double state_diff = std::max(std::abs((double)(sr.h1 - sc.h1)), std::abs((double)(sr.h2 - sc.h2)));
        out << "dynamic," << (sizeof(T) == sizeof(float) ? "float" : "double") << ',' << W << ','
            << run_length << ',' << m.rel_error_db << ',' << m.max_abs_dbfs << ',' << state_diff << '\n';
    }
}

template<typename T, int W, bool CANDIDATE>
static double run_bench(const Coefficients &c, const BlockParams<T> &p,
                        const std::vector<T> &input)
{
    State<T> s{};
    std::array<T, kBlockFrames> output{};
    T checksum{};
    const auto start = std::chrono::steady_clock::now();
    for(size_t block = 0; block < kBenchBlocks; ++block)
    {
        const T *src = input.data() + (block % (input.size() / kBlockFrames)) * kBlockFrames;
        if constexpr(CANDIDATE) block_run<W>(c, p, src, output.data(), output.size(), s);
        else scalar_run(c, src, output.data(), output.size(), s);
        checksum += output[0] + output[kBlockFrames - 1];
    }
    const auto end = std::chrono::steady_clock::now();
    volatile double sink = static_cast<double>(checksum + s.h1 + s.h2);
    (void)sink;
    return std::chrono::duration<double, std::nano>(end - start).count();
}

template<typename T, int W>
static void bench_for_type(const std::string &path)
{
    std::ofstream out(path, std::ios::app);
    const Coefficients c = make_lowpass(9000, 10.0f);
    const BlockParams<T> p(c);
    const auto noise = make_noise(kBlockFrames * 128, 0.2);
    std::vector<T> input(noise.size());
    for(size_t i = 0; i < input.size(); ++i) input[i] = static_cast<T>(noise[i]);
    for(int pair = 0; pair < kPairs; ++pair)
    {
        double base, candidate;
        if(pair & 1)
        {
            base = run_bench<T, W, false>(c, p, input);
            candidate = run_bench<T, W, true>(c, p, input);
        }
        else
        {
            candidate = run_bench<T, W, true>(c, p, input);
            base = run_bench<T, W, false>(c, p, input);
        }
        out << (sizeof(T) == sizeof(float) ? "float" : "double") << ',' << W << ',' << pair << ','
            << base << ',' << candidate << ',' << candidate / base << '\n';
    }
}

} // namespace

int main(int argc, char **argv)
{
    if(argc != 2) { std::cerr << "usage: iir_block_screen <output-dir>\n"; return 2; }
    const std::string root = argv[1];
    const std::string quality_path = root + "/quality.csv";
    const std::string dynamic_path = root + "/dynamic.csv";
    const std::string bench_path = root + "/bench.csv";
    { std::ofstream(quality_path) << "case,precision,width,cutoff_cents,q,frequency_hz,reference_rms_dbfs,gain_delta_db,relative_error_db,max_abs_error_dbfs,final_state_diff,finite,quiet_tail_rms_dbfs,quiet_tail_max_dbfs\n"; }
    { std::ofstream(dynamic_path) << "case,precision,width,constant_run_length,relative_error_db,max_abs_error_dbfs,final_state_diff\n"; }
    { std::ofstream(bench_path) << "precision,width,pair,scalar_ns,block_ns,block_over_scalar\n"; }
    write_quality_for_type<float, 2>(quality_path);
    write_quality_for_type<float, 4>(quality_path);
    write_quality_for_type<double, 2>(quality_path);
    write_quality_for_type<double, 4>(quality_path);
    write_dynamic_for_type<float, 2>(dynamic_path);
    write_dynamic_for_type<float, 4>(dynamic_path);
    write_dynamic_for_type<double, 2>(dynamic_path);
    write_dynamic_for_type<double, 4>(dynamic_path);
    bench_for_type<float, 2>(bench_path);
    bench_for_type<float, 4>(bench_path);
    bench_for_type<double, 2>(bench_path);
    bench_for_type<double, 4>(bench_path);
    std::cout << "completed quality, state-transition, and 7-pair fixed-coefficient screens\n";
    return 0;
}
