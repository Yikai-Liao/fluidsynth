#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#ifdef IMAGE_FLOAT
using Real = float;
static constexpr const char *precision_name = "float";
#else
using Real = double;
static constexpr const char *precision_name = "double";
#endif

using C = std::complex<double>;
constexpr double PI = 3.141592653589793238462643383279502884;
constexpr uint64_t PHASE_ONE = UINT64_C(1) << 32;
constexpr uint64_t HALF_PHASE = UINT64_C(1) << 31;
constexpr size_t FFT_N = 16384;
constexpr int SINC_TAPS = 25;

static uint64_t phase_increment(double requested)
{
    const Real rate = static_cast<Real>(requested);
    const int integer = static_cast<int>(rate);
    const uint32_t fraction = static_cast<uint32_t>((static_cast<double>(rate) - integer) * 4294967296.0);
    return (static_cast<uint64_t>(integer) << 32) | fraction;
}

static uint64_t fixed_phase(double position)
{
    return static_cast<uint64_t>(std::llround(position * 4294967296.0));
}

static std::vector<float> read_prefilter(const std::string &path)
{
    std::ifstream in(path);
    if(!in) throw std::runtime_error("cannot open prefilter CSV");
    std::string line;
    std::getline(in, line);
    std::vector<std::pair<size_t, float>> indexed;
    while(std::getline(in, line))
    {
        std::stringstream ss(line);
        std::string f[4];
        for(int i = 0; i < 4 && std::getline(ss, f[i], ','); ++i) {}
        if(f[0] == "6" && f[1] == "65") indexed.emplace_back(std::stoull(f[2]), std::stof(f[3]));
    }
    std::sort(indexed.begin(), indexed.end());
    std::vector<float> result;
    for(size_t i = 0; i < indexed.size(); ++i)
    {
        if(indexed[i].first != i) throw std::runtime_error("non-contiguous prefilter rows");
        result.push_back(indexed[i].second);
    }
    if(result.size() != 65) throw std::runtime_error("expected 65-tap prefilter");
    return result;
}

static std::vector<double> make_halfphase_kernel()
{
    constexpr int taps = 4095;
    constexpr int half = taps / 2;
    std::vector<double> out(taps);
    double sum = 0.0;
    for(int j = -half; j <= half; ++j)
    {
        const double t = static_cast<double>(j) - 0.5;
        const double a = PI * t;
        const double s = std::abs(a) < 1e-14 ? 1.0 : std::sin(a) / a;
        const double window = 0.5 * (1.0 + std::cos(PI * t / half));
        out[static_cast<size_t>(j + half)] = std::abs(t) <= half ? s * window : 0.0;
        sum += out[static_cast<size_t>(j + half)];
    }
    for(double &v : out) v /= sum;
    return out;
}

struct CandidateResponse { C pre; C half; C exp_delta[5]; double omega{}; };

static CandidateResponse make_candidate_response(double f, const std::vector<float> &prefilter,
                                                  const std::vector<double> &half_kernel)
{
    CandidateResponse r{};
    r.omega = 2.0 * PI * f;
    const long long ph = static_cast<long long>(prefilter.size() / 2);
    for(long long j = -ph; j <= ph; ++j)
        r.pre += static_cast<double>(prefilter[static_cast<size_t>(j + ph)])
            * std::polar(1.0, r.omega * static_cast<double>(j));
    const long long hh = static_cast<long long>(half_kernel.size() / 2);
    for(long long j = -hh; j <= hh; ++j)
        r.half += half_kernel[static_cast<size_t>(j + hh)]
            * std::polar(1.0, r.omega * static_cast<double>(j));
    for(int d = -2; d <= 2; ++d) r.exp_delta[d + 2] = std::polar(1.0, r.omega * d);
    return r;
}

static std::array<std::array<Real, 4>, 256> make_cubic_table()
{
    std::array<std::array<Real, 4>, 256> table{};
    for(int row = 0; row < 256; ++row)
    {
        const Real x = static_cast<Real>(row) / Real(256);
        table[row] = {{x * (Real(-0.5) + x * (Real(1) - Real(0.5) * x)),
                       Real(1) + x * x * (Real(1.5) * x - Real(2.5)),
                       x * (Real(0.5) + x * (Real(2) - Real(1.5) * x)),
                       Real(0.5) * x * x * (x - Real(1))}};
    }
    return table;
}

using SincRow = std::array<Real, SINC_TAPS>;
static std::vector<SincRow> make_sinc_table()
{
    constexpr int size = 4096, half = 12;
    std::vector<SincRow> table(size + 1);
    for(int row = 0; row <= size; ++row)
    {
        const double phase = static_cast<double>(row) / size;
        const double center = half - 0.5 + phase;
        double sum = 0.0;
        for(int i = 0; i < SINC_TAPS; ++i)
        {
            const double shifted = i - center;
            const double arg = PI * shifted;
            const double sinc = std::abs(arg) > 1e-6 ? std::sin(arg) / arg : 1.0;
            const double window = std::cos(arg / SINC_TAPS);
            const double value = sinc * window * window;
            table[row][static_cast<size_t>(i)] = static_cast<Real>(value);
            sum += value;
        }
        for(Real &v : table[row]) v = static_cast<Real>(static_cast<double>(v) / sum);
    }
    return table;
}

static double source_sample(int64_t i, double f)
{
    return std::sin(2.0 * PI * f * static_cast<double>(i));
}

static double cubic_sample(uint64_t phase, double f,
                           const std::array<std::array<Real, 4>, 256> &table)
{
    const int64_t center = static_cast<int64_t>(phase >> 32);
    const size_t row = static_cast<uint32_t>(phase) >> 24;
    const auto &c = table[row];
    const Real y = c[0] * static_cast<Real>(source_sample(center - 1, f))
        + c[1] * static_cast<Real>(source_sample(center, f))
        + c[2] * static_cast<Real>(source_sample(center + 1, f))
        + c[3] * static_cast<Real>(source_sample(center + 2, f));
    return static_cast<double>(y);
}

static double sinc_sample(uint64_t phase, double f, const std::vector<SincRow> &table)
{
    constexpr uint32_t low_mask = (UINT32_C(1) << 20) - 1;
    const uint64_t shifted = phase + HALF_PHASE;
    const int64_t center = static_cast<int64_t>(shifted >> 32);
    const uint32_t frac = static_cast<uint32_t>(shifted);
    const size_t row = frac >> 20;
    const Real alpha = static_cast<Real>(frac & low_mask) / Real(1048576.0);
    Real y = Real(0);
    for(int i = 0; i < SINC_TAPS; ++i)
    {
        const Real a = table[row][static_cast<size_t>(i)];
        const Real b = table[row + 1][static_cast<size_t>(i)];
        const Real c = a + (b - a) * alpha;
        y += c * static_cast<Real>(source_sample(center + i - 12, f));
    }
    return static_cast<double>(y);
}

static int64_t floor_half(int64_t x)
{
    return x >= 0 ? x / 2 : (x - 1) / 2;
}

static std::complex<Real> optimal6(const std::array<std::complex<Real>, 6> &y, Real fraction)
{
    const auto even1 = y[3] + y[2], odd1 = y[3] - y[2];
    const auto even2 = y[4] + y[1], odd2 = y[4] - y[1];
    const auto even3 = y[5] + y[0], odd3 = y[5] - y[0];
    const auto c0 = even1 * Real(0.37484203669443822) + even2 * Real(0.11970939637439368) + even3 * Real(0.00544862268096358);
    const auto c1 = odd1 * Real(0.19253897284651597) + odd2 * Real(0.22555179040018719) + odd3 * Real(0.02621377625620669);
    const auto c2 = even1 * Real(-0.154026006475653071) + even2 * Real(0.10546111301131367) + even3 * Real(0.04856757454258609);
    const auto c3 = odd1 * Real(-0.06523685579716083) + odd2 * Real(-0.04867197815057284) + odd3 * Real(0.04200764942718964);
    const auto c4 = even1 * Real(0.03134095684084392) + even2 * Real(-0.04385804833432710) + even3 * Real(0.01249475765486819);
    const Real z = fraction - Real(0.5);
    return (((c4 * z + c3) * z + c2) * z + c1) * z + c0;
}

static double compensated_sample(uint64_t phase, const CandidateResponse &response)
{
    const uint64_t doubled = phase << 1;
    const int64_t base = static_cast<int64_t>(doubled >> 32);
    const Real fraction = static_cast<Real>(static_cast<uint32_t>(doubled)) / Real(4294967296.0);
    const C base_phase = std::polar(1.0, response.omega * static_cast<double>(floor_half(base)));
    std::array<std::complex<Real>, 6> y{};
    for(int k = -2; k <= 3; ++k)
    {
        const int64_t gi = base + k;
        const int64_t si = floor_half(gi);
        const int64_t bi = floor_half(base);
        const int delta = static_cast<int>(si - bi);
        const bool odd = gi - 2 * si != 0;
        const C z = response.pre * (odd ? response.half : C(1.0, 0.0))
            * base_phase * response.exp_delta[delta + 2];
        y[static_cast<size_t>(k + 2)] = {static_cast<Real>(static_cast<float>(z.real())),
                                          static_cast<Real>(static_cast<float>(z.imag()))};
    }
    return static_cast<double>(optimal6(y, fraction).imag());
}

static void fft(std::vector<C> &a)
{
    const size_t n = a.size();
    for(size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for(; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if(i < j) std::swap(a[i], a[j]);
    }
    for(size_t len = 2; len <= n; len <<= 1)
    {
        const double angle = -2.0 * PI / static_cast<double>(len);
        const C wl(std::cos(angle), std::sin(angle));
        for(size_t i = 0; i < n; i += len)
        {
            C w(1.0, 0.0);
            for(size_t j = 0; j < len / 2; ++j)
            {
                const C u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

static void run(const std::string &out_path, const std::string &prefilter_path)
{
    const auto prefilter = read_prefilter(prefilter_path);
    const auto half_kernel = make_halfphase_kernel();
    const auto cubic_table = make_cubic_table();
    const auto sinc_table = make_sinc_table();
    const double rates[] = {0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 4.0, 1.41421356237};
    const double output_freqs[] = {0.03, 0.08, 0.16, 0.24, 0.32, 0.40, 0.46, 0.54, 0.62, 0.72,
                                   0.88, 0.96, 1.08, 1.28, 1.72, 1.88};
    const uint64_t start_phase = fixed_phase(0.2718281828);
    std::ofstream out(out_path);
    out << "precision,requested_rate,actual_rate,input_cycles_per_sample,output_cycles_per_output,folded_output_frequency,band,method,fundamental_dbfs,residual_rms_dbfs,max_spur_dbfs,max_spur_dbc,analytic_error_rms_dbfs\n";
    for(double requested : rates)
    {
        const uint64_t step = phase_increment(requested);
        const double rate = static_cast<double>(step) / 4294967296.0;
        for(double g : output_freqs)
        {
            if(g >= rate * 0.499) continue;
            const double f = g / rate;
            double alias = std::abs(g - std::round(g));
            if(alias > 0.5) alias = 1.0 - alias;
            if(alias < 1e-6 || std::abs(alias - 0.5) < 1e-6) continue;
            const char *band = requested <= 1.0 ? "pass" : (g <= 0.45 ? "pass" : (g >= 0.55 ? "stop" : "transition"));
            const CandidateResponse response = make_candidate_response(f, prefilter, half_kernel);
            std::array<std::vector<double>, 3> samples;
            for(auto &v : samples) v.resize(FFT_N);
            uint64_t phase = start_phase;
            for(size_t n = 0; n < FFT_N; ++n)
            {
                samples[0][n] = cubic_sample(phase, f, cubic_table);
                samples[1][n] = sinc_sample(phase, f, sinc_table);
                samples[2][n] = compensated_sample(phase, response);
                phase += step;
            }
            const char *methods[] = {"production_cubic", "lut_sinc25", "compensated_6pt"};
            for(size_t m = 0; m < samples.size(); ++m)
            {
                const double omega = 2.0 * PI * alias;
                double cs = 0.0, ss = 0.0, cc = 0.0, sch = 0.0, cross = 0.0;
                for(size_t n = 0; n < FFT_N; ++n)
                {
                    const double c = std::cos(omega * n), s = std::sin(omega * n);
                    cs += samples[m][n] * c; ss += samples[m][n] * s;
                    cc += c * c; sch += s * s; cross += c * s;
                }
                const double det = cc * sch - cross * cross;
                const double a = (cs * sch - ss * cross) / det;
                const double b = (ss * cc - cs * cross) / det;
                const double fundamental = std::hypot(a, b);
                std::vector<C> spec(FFT_N);
                double residual2 = 0.0, analytic2 = 0.0, window_sum = 0.0;
                for(size_t n = 0; n < FFT_N; ++n)
                {
                    const double expected = std::sin(2.0 * PI * f *
                        (static_cast<double>(start_phase) / 4294967296.0 + static_cast<double>(n) * rate));
                    const double residual = samples[m][n] - (a * std::cos(omega * n) + b * std::sin(omega * n));
                    const double w = 0.5 * (1.0 - std::cos(2.0 * PI * n / (FFT_N - 1.0)));
                    spec[n] = C(residual * w, 0.0);
                    residual2 += residual * residual;
                    analytic2 += (samples[m][n] - expected) * (samples[m][n] - expected);
                    window_sum += w;
                }
                fft(spec);
                double max_spur = 0.0;
                const size_t main_bin = static_cast<size_t>(std::llround(alias * FFT_N));
                for(size_t k = 1; k < FFT_N / 2; ++k)
                {
                    if(k + 1 >= main_bin && k <= main_bin + 1) continue;
                    max_spur = std::max(max_spur, 2.0 * std::abs(spec[k]) / window_sum);
                }
                const double residual_rms = std::sqrt(residual2 / FFT_N);
                const double analytic_rms = std::sqrt(analytic2 / FFT_N);
                const double fundamental_db = 20.0 * std::log10(std::max(fundamental, 1e-300));
                const double residual_db = 20.0 * std::log10(std::max(residual_rms / std::sqrt(0.5), 1e-300));
                const double spur_db = 20.0 * std::log10(std::max(max_spur, 1e-300));
                const double spur_dbc = 20.0 * std::log10(std::max(max_spur / std::max(fundamental, 1e-300), 1e-300));
                const double analytic_db = 20.0 * std::log10(std::max(analytic_rms / std::sqrt(0.5), 1e-300));
                out << precision_name << ',' << requested << ',' << rate << ',' << f << ',' << g << ',' << alias << ','
                    << band << ',' << methods[m] << ',' << fundamental_db << ',' << residual_db << ',' << spur_db << ','
                    << spur_dbc << ',' << analytic_db << '\n';
            }
        }
    }
}

int main(int argc, char **argv)
{
    if(argc != 3)
    {
        std::cerr << "usage: image_threeway <output.csv> <prefilter_coefficients.csv>\n";
        return 2;
    }
    try { run(argv[1], argv[2]); }
    catch(const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
    return 0;
}
