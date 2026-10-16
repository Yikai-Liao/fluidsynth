#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {
constexpr long double kPi = 3.141592653589793238462643383279502884L;
constexpr uint64_t kPhaseOne = uint64_t{1} << 32;
constexpr uint32_t kPhaseTableSize = 4096;
constexpr uint32_t kPhaseLowMask = (uint32_t{1} << 20) - 1;
constexpr uint32_t kPhaseMid = uint32_t{1} << 31;

struct SampleMeta {
    std::string label;
    std::string raw_file;
    size_t frames{};
    size_t loop_start{};
    size_t loop_end{};
    int samplemode{};
    double base_step{};
};

struct RawFrame { double cubic{}, sinc25{}, ref2047{}, ref{}; };
struct Case {
    std::string name;
    double initial_phase{};
    bool looping{};
    bool has_looped{};
};

static std::vector<std::string> split_csv(const std::string &line)
{
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string field;
    while(std::getline(ss, field, ',')) fields.push_back(field);
    return fields;
}

static std::vector<SampleMeta> read_metadata(const std::string &path)
{
    std::ifstream in(path);
    std::string line;
    std::getline(in, line);
    std::vector<SampleMeta> out;
    while(std::getline(in, line))
    {
        const auto f = split_csv(line);
        if(f.size() < 7) continue;
        out.push_back({f[0], f[1], std::stoull(f[2]), std::stoull(f[3]),
                       std::stoull(f[4]), std::stoi(f[5]), std::stod(f[6])});
    }
    return out;
}

static std::vector<double> read_pcm16(const std::string &path, size_t frames)
{
    std::ifstream in(path, std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)), {});
    if(bytes.size() != frames * 2) throw std::runtime_error("bad sample size: " + path);
    std::vector<double> x(frames);
    for(size_t i = 0; i < frames; ++i)
    {
        const uint16_t u = static_cast<uint16_t>(bytes[2 * i])
                           | (static_cast<uint16_t>(bytes[2 * i + 1]) << 8);
        const int16_t s = static_cast<int16_t>(u);
        x[i] = static_cast<double>(s) / 32768.0;
    }
    return x;
}

static double beta_spline(int degree, double x)
{
    if(degree == 3)
    {
        const double a = std::abs(x);
        if(a < 1.0) return (4.0 - 6.0 * a * a + 3.0 * a * a * a) / 6.0;
        if(a < 2.0) { const double t = 2.0 - a; return t * t * t / 6.0; }
        return 0.0;
    }
    // Centered cardinal degree-5 B-spline, truncated-power form.
    static constexpr int binom[7] = {1, 6, 15, 20, 15, 6, 1};
    long double s = 0.0L;
    for(int k = 0; k <= 6; ++k)
    {
        const long double t = static_cast<long double>(x) + 3.0L - k;
        if(t > 0.0L) s += (k & 1 ? -1.0L : 1.0L) * binom[k] * t * t * t * t * t;
    }
    return static_cast<double>(s / 120.0L);
}

static std::vector<double> spline_weights(int degree)
{
    if(degree == 3) return {1.0, 4.0, 1.0};
    return {1.0, 26.0, 66.0, 26.0, 1.0};
}

static std::vector<double> spline_prefilter(const std::vector<double> &samples,
                                            int degree, bool periodic,
                                            unsigned *iterations = nullptr)
{
    if(samples.empty()) return {};
    const std::vector<double> weights = spline_weights(degree);
    const int radius = static_cast<int>(weights.size() / 2);
    const double scale = degree == 3 ? 6.0 : 120.0;
    const size_t n = samples.size();
    std::vector<double> b(n), x = samples, ax(n), r(n), p(n), ap(n);
    for(size_t i = 0; i < n; ++i)
    {
        b[i] = scale * samples[i];
    }
    auto apply = [&](const std::vector<double> &v, std::vector<double> &out)
    {
        std::fill(out.begin(), out.end(), 0.0);
        for(size_t i = 0; i < n; ++i)
        {
            double acc = 0.0;
            for(int d = -radius; d <= radius; ++d)
            {
                long long j = static_cast<long long>(i) + d;
                if(periodic)
                {
                    const long long m = static_cast<long long>(n);
                    j %= m;
                    if(j < 0) j += m;
                    acc += weights[d + radius] * v[static_cast<size_t>(j)];
                }
                else
                {
                    j = std::max<long long>(0, std::min<long long>(static_cast<long long>(n) - 1, j));
                    acc += weights[d + radius] * v[static_cast<size_t>(j)];
                }
            }
            out[i] = acc;
        }
    };
    apply(x, ax);
    long double rr = 0.0L, bb = 0.0L;
    for(size_t i = 0; i < n; ++i) { r[i] = b[i] - ax[i]; p[i] = r[i]; rr += (long double)r[i] * r[i]; bb += (long double)b[i] * b[i]; }
    const long double initial = std::sqrt(rr);
    const long double tolerance = std::max(1.0e-25L, std::sqrt(bb) * 2.0e-14L);
    unsigned iter = 0;
    for(; iter < 400 && std::sqrt(rr) > tolerance; ++iter)
    {
        apply(p, ap);
        long double pap = 0.0L;
        for(size_t i = 0; i < n; ++i) pap += (long double)p[i] * ap[i];
        if(!(pap > 0.0L)) throw std::runtime_error("spline prefilter lost positive definiteness");
        const double alpha = static_cast<double>(rr / pap);
        long double next_rr = 0.0L;
        for(size_t i = 0; i < n; ++i)
        {
            x[i] += alpha * p[i];
            r[i] -= alpha * ap[i];
            next_rr += (long double)r[i] * r[i];
        }
        const double beta = static_cast<double>(next_rr / rr);
        for(size_t i = 0; i < n; ++i) p[i] = r[i] + beta * p[i];
        rr = next_rr;
    }
    if(iterations) *iterations = iter;
    if(std::sqrt(rr) > std::max(tolerance * 10.0L, initial * 1.0e-12L))
        throw std::runtime_error("spline prefilter did not converge");
    return x;
}

static double spline_reconstruct(const std::vector<double> &coeff, int degree,
                                 uint64_t phase, bool periodic,
                                 size_t loop_start, size_t loop_end)
{
    const long long base = static_cast<long long>(phase >> 32);
    const double frac = static_cast<double>(static_cast<uint32_t>(phase)) / 4294967296.0;
    const int radius = (degree + 1) / 2;
    double out = 0.0;
    for(int k = -radius; k <= radius; ++k)
    {
        long long idx = base + k;
        double c;
        if(periodic)
        {
            const long long n = static_cast<long long>(loop_end - loop_start);
            long long q = (idx - static_cast<long long>(loop_start)) % n;
            if(q < 0) q += n;
            c = coeff[static_cast<size_t>(q)];
        }
        else
        {
            idx = std::max<long long>(0, std::min<long long>(static_cast<long long>(coeff.size()) - 1, idx));
            c = coeff[static_cast<size_t>(idx)];
        }
        out += c * beta_spline(degree, frac - k);
    }
    return out;
}

static double sample_at(const std::vector<double> &data, long long idx,
                        size_t loop_start, size_t loop_end,
                        bool looping, bool has_looped)
{
    if(looping && has_looped)
    {
        const long long n = static_cast<long long>(loop_end - loop_start);
        long long q = (idx - static_cast<long long>(loop_start)) % n;
        if(q < 0) q += n;
        idx = static_cast<long long>(loop_start) + q;
    }
    else if(looping && idx >= static_cast<long long>(loop_end))
    {
        const long long n = static_cast<long long>(loop_end - loop_start);
        long long q = (idx - static_cast<long long>(loop_start)) % n;
        if(q < 0) q += n;
        idx = static_cast<long long>(loop_start) + q;
    }
    else idx = std::max<long long>(0, std::min<long long>(static_cast<long long>(data.size()) - 1, idx));
    return data[static_cast<size_t>(idx)];
}

using FirTable = std::vector<std::vector<double>>;

static std::vector<std::vector<long double>> invert(std::vector<std::vector<long double>> a)
{
    const size_t n = a.size();
    std::vector<std::vector<long double>> aug(n, std::vector<long double>(2 * n));
    for(size_t i = 0; i < n; ++i)
    {
        for(size_t j = 0; j < n; ++j) aug[i][j] = a[i][j];
        aug[i][n + i] = 1.0L;
    }
    for(size_t col = 0; col < n; ++col)
    {
        size_t pivot = col;
        for(size_t row = col + 1; row < n; ++row)
            if(std::abs(aug[row][col]) > std::abs(aug[pivot][col])) pivot = row;
        if(std::abs(aug[pivot][col]) < 1.0e-24L) throw std::runtime_error("singular FIR normal matrix");
        if(pivot != col) std::swap(aug[pivot], aug[col]);
        const long double div = aug[col][col];
        for(size_t j = 0; j < 2 * n; ++j) aug[col][j] /= div;
        for(size_t row = 0; row < n; ++row)
        {
            if(row == col) continue;
            const long double factor = aug[row][col];
            for(size_t j = 0; j < 2 * n; ++j) aug[row][j] -= factor * aug[col][j];
        }
    }
    std::vector<std::vector<long double>> inv(n, std::vector<long double>(n));
    for(size_t i = 0; i < n; ++i)
        for(size_t j = 0; j < n; ++j) inv[i][j] = aug[i][n + j];
    return inv;
}

static long double sinc_integral(long double x, long double wmax)
{
    return std::abs(x) < 1.0e-18L ? wmax : std::sin(wmax * x) / x;
}

static FirTable design_fir_table(int taps)
{
    const int n = taps;
    const int half = taps / 2;
    const int dim = n + 1;
    // Unit-weight least squares over the preregistered passband 0..0.40 cycles/sample,
    // with an exact DC constraint sum(h)=1.
    const long double wmax = 2.0L * kPi * 0.40L;
    std::vector<std::vector<long double>> kkt(dim, std::vector<long double>(dim, 0.0L));
    for(int i = 0; i < n; ++i)
    {
        for(int j = 0; j < n; ++j) kkt[i][j] = sinc_integral((long double)(i - j), wmax);
        kkt[i][n] = 1.0L;
        kkt[n][i] = 1.0L;
    }
    auto inv = invert(kkt);
    FirTable table(kPhaseTableSize + 1, std::vector<double>(n));
    for(uint32_t row = 0; row <= kPhaseTableSize; ++row)
    {
        const long double x = (long double)row / kPhaseTableSize;
        const long double d = x - 0.5L;
        std::vector<long double> rhs(dim);
        for(int i = 0; i < n; ++i) rhs[i] = sinc_integral((long double)(i - half) - d, wmax);
        rhs[n] = 1.0L;
        for(int i = 0; i < n; ++i)
        {
            long double acc = 0.0L;
            for(int j = 0; j < dim; ++j) acc += inv[i][j] * rhs[j];
            table[row][i] = (double)acc;
        }
    }
    return table;
}

static void table_coeffs(const FirTable &table, uint32_t frac, std::vector<double> &out)
{
    const uint32_t row = frac >> 20;
    const double alpha = (double)(frac & kPhaseLowMask) / 1048576.0;
    for(size_t i = 0; i < out.size(); ++i)
        out[i] = table[row][i] + (table[row + 1][i] - table[row][i]) * alpha;
}

static FirTable make_current_sinc25_table()
{
    FirTable table(kPhaseTableSize + 1, std::vector<double>(25));
    constexpr int half = 12;
    for(uint32_t row = 0; row <= kPhaseTableSize; ++row)
    {
        const double x = (double)row / kPhaseTableSize;
        const double center = half - 0.5 + x;
        double sum = 0.0;
        for(int i = 0; i < 25; ++i)
        {
            const double shifted = i - center;
            const double arg = (double)kPi * shifted;
            double c = std::abs(arg) > 1.0e-6 ? std::sin(arg) / arg : 1.0;
            const double w = std::cos(arg / 25.0);
            c *= w * w;
            table[row][i] = c;
            sum += c;
        }
        for(double &c : table[row]) c /= sum;
    }
    return table;
}

static uint64_t set_phase(double value)
{
    return static_cast<uint64_t>(std::llround(value * 4294967296.0));
}

static uint64_t set_increment(double step)
{
    const uint64_t integer = static_cast<uint64_t>(step);
    const uint32_t fraction = static_cast<uint32_t>((step - (int)step) * 4294967296.0);
    return (integer << 32) | fraction;
}

static std::vector<RawFrame> read_raw_frames(const std::string &path)
{
    std::ifstream in(path);
    std::string line;
    std::getline(in, line);
    std::vector<RawFrame> frames;
    while(std::getline(in, line))
    {
        const auto f = split_csv(line);
        if(f.size() < 6) continue;
        frames.push_back({std::stod(f[1]), std::stod(f[2]), std::stod(f[4]), std::stod(f[5])});
    }
    return frames;
}

static std::vector<Case> make_cases(const SampleMeta &m, double step)
{
    constexpr double offset = 0.271828;
    const bool use_loop = m.samplemode && m.loop_end > m.loop_start + 1024 && m.loop_end <= m.frames;
    if(use_loop)
    {
        return {{"attack", offset, true, false},
                {"sustain", m.loop_start + 1024 + offset, true, true},
                {"loop_seam", m.loop_end - step * (882 / 2.0) + offset, true, true},
                {"release_exit", (m.frames - 1) - step * (882 - 1.0) + offset, false, true}};
    }
    return {{"attack", offset, false, false},
            {"interior", 1024 + offset, false, false},
            {"tail", (m.frames - 1) - step * (882 - 1.0) + offset, false, false}};
}

static double db20(double x)
{
    return x > 0.0 ? 20.0 * std::log10(x) : -std::numeric_limits<double>::infinity();
}

static double sinc(double x)
{
    return std::abs(x) < 1.0e-14 ? 1.0 : std::sin((double)kPi * x) / ((double)kPi * x);
}

static std::vector<double> render_long_reference(const std::vector<double> &data,
                                                 double initial_phase, double step,
                                                 size_t loop_start, size_t loop_end,
                                                 bool looping, bool has_looped,
                                                 size_t frames, int taps)
{
    const int half = taps / 2;
    const double cutoff = std::min(1.0, 1.0 / step);
    uint64_t phase = set_phase(initial_phase);
    const uint64_t increment = set_increment(step);
    std::vector<double> out;
    out.reserve(frames);
    for(size_t f = 0; f < frames; ++f)
    {
        const double pos = (double)(phase >> 32) + (double)(uint32_t)phase / 4294967296.0;
        const long long center = (long long)std::floor(pos);
        double sum = 0.0, norm = 0.0;
        for(int j = -half; j <= half; ++j)
        {
            const long long idx = center + j;
            const double t = (double)idx - pos;
            const double window = std::abs(t) <= half ? 0.5 * (1.0 + std::cos((double)kPi * t / half)) : 0.0;
            const double c = cutoff * sinc(cutoff * t) * window;
            sum += sample_at(data, idx, loop_start, loop_end, looping, has_looped) * c;
            norm += c;
        }
        out.push_back(sum / norm);
        phase += increment;
        if(looping && (phase >> 32) >= loop_end)
        {
            phase -= (static_cast<uint64_t>(loop_end - loop_start) << 32);
            has_looped = true;
        }
    }
    return out;
}

static std::vector<double> render_fir(const std::vector<double> &data, const FirTable &table,
                                      int taps, double initial_phase, double step,
                                      size_t loop_start, size_t loop_end, bool looping,
                                      bool has_looped, size_t frames)
{
    const int half = taps / 2;
    uint64_t phase = set_phase(initial_phase);
    const uint64_t increment = set_increment(step);
    std::vector<double> coeff(taps), out;
    out.reserve(frames);
    for(size_t f = 0; f < frames; ++f)
    {
        const uint64_t shifted = phase + kPhaseMid;
        const long long center = static_cast<long long>(shifted >> 32);
        const uint32_t frac = static_cast<uint32_t>(shifted);
        table_coeffs(table, frac, coeff);
        double y = 0.0;
        for(int i = 0; i < taps; ++i)
            y += coeff[i] * sample_at(data, center + i - half, loop_start, loop_end, looping, has_looped);
        out.push_back(y);
        phase += increment;
        if(looping && (phase >> 32) >= loop_end)
        {
            phase -= (static_cast<uint64_t>(loop_end - loop_start) << 32);
            has_looped = true;
        }
    }
    return out;
}

static std::vector<double> render_spline(const std::vector<double> &data,
                                         const std::vector<double> &finite3,
                                         const std::vector<double> &finite5,
                                         const std::vector<double> &loop3,
                                         const std::vector<double> &loop5,
                                         int degree, double initial_phase, double step,
                                         size_t loop_start, size_t loop_end, bool looping,
                                         bool has_looped, size_t frames)
{
    uint64_t phase = set_phase(initial_phase);
    const uint64_t increment = set_increment(step);
    std::vector<double> out;
    out.reserve(frames);
    for(size_t f = 0; f < frames; ++f)
    {
        const bool periodic = looping && has_looped;
        const auto &coeff = periodic ? (degree == 3 ? loop3 : loop5)
                                     : (degree == 3 ? finite3 : finite5);
        out.push_back(spline_reconstruct(coeff, degree, phase, periodic, loop_start, loop_end));
        phase += increment;
        if(looping && (phase >> 32) >= loop_end)
        {
            phase -= (static_cast<uint64_t>(loop_end - loop_start) << 32);
            has_looped = true;
        }
    }
    (void)data;
    return out;
}

struct ErrorMetrics { double rms{}, max{}, ref_rms{}; };
static ErrorMetrics errors(const std::vector<double> &candidate, const std::vector<double> &reference)
{
    const size_t n = std::min(candidate.size(), reference.size());
    long double sum = 0.0L, refsum = 0.0L;
    double mx = 0.0;
    for(size_t i = 0; i < n; ++i)
    {
        const double e = candidate[i] - reference[i];
        sum += (long double)e * e;
        refsum += (long double)reference[i] * reference[i];
        mx = std::max(mx, std::abs(e));
    }
    return {n ? std::sqrt((double)(sum / n)) : 0.0, mx,
            n ? std::sqrt((double)(refsum / n)) : 0.0};
}

static void write_spline_static(const std::string &path)
{
    std::ofstream out(path);
    out << "degree,frequency_cycles_per_sample,phase,gain_error_db,phase_error_deg\n" << std::setprecision(12);
    constexpr int phase_count = 256;
    for(int degree : {3, 5})
    {
        const int radius = (degree + 1) / 2;
        for(int fi = 0; fi <= 180; ++fi)
        {
            const double f = fi * 0.0025;
            const double w = 2.0 * (double)kPi * f;
            double den_re = 0.0, den_im = 0.0;
            for(int k = -radius; k <= radius; ++k)
            {
                const double b = beta_spline(degree, k);
                den_re += b * std::cos(w * k);
                den_im += b * std::sin(w * k);
            }
            const double den = std::hypot(den_re, den_im);
            for(int pi = 0; pi <= phase_count; ++pi)
            {
                const double x = (double)pi / phase_count;
                double re = 0.0, im = 0.0;
                for(int k = -radius; k <= radius; ++k)
                {
                    const double b = beta_spline(degree, x - k);
                    re += b * std::cos(w * k);
                    im += b * std::sin(w * k);
                }
                const double truth_phase = w * x;
                const double rel_re = (re * std::cos(truth_phase) + im * std::sin(truth_phase)) / den;
                const double rel_im = (im * std::cos(truth_phase) - re * std::sin(truth_phase)) / den;
                out << degree << ',' << f << ',' << x << ',' << db20(std::hypot(rel_re, rel_im)) << ','
                    << std::atan2(rel_im, rel_re) * 180.0 / (double)kPi << '\n';
            }
        }
    }
}

static void write_fir_static(const std::string &path, const std::vector<std::pair<int, FirTable>> &tables,
                             const std::string &coeff_path)
{
    std::ofstream out(path), coeffout(coeff_path);
    out << "taps,frequency_cycles_per_sample,max_abs_gain_error_db,max_abs_phase_error_deg\n" << std::setprecision(12);
    coeffout << "taps,max_l1_norm,max_abs_coeff,min_coeff,max_coeff,max_dc_error\n" << std::setprecision(12);
    constexpr int phase_count = 8192;
    for(const auto &[taps, table] : tables)
    {
        double maxl1 = 0.0, maxabs = 0.0, minc = std::numeric_limits<double>::infinity();
        double maxc = -std::numeric_limits<double>::infinity(), maxdc = 0.0;
        for(const auto &row : table)
        {
            double l1 = 0.0, dc = 0.0;
            for(double x : row) { l1 += std::abs(x); dc += x; maxabs = std::max(maxabs, std::abs(x)); minc = std::min(minc, x); maxc = std::max(maxc, x); }
            maxl1 = std::max(maxl1, l1);
            maxdc = std::max(maxdc, std::abs(dc - 1.0));
        }
        coeffout << taps << ',' << maxl1 << ',' << maxabs << ',' << minc << ',' << maxc << ',' << maxdc << '\n';
        for(int fi = 0; fi <= 200; ++fi)
        {
            const double f = fi * 0.0025;
            const double w = 2.0 * (double)kPi * f;
            double max_gain = 0.0, max_phase = 0.0;
            for(int pi = 0; pi < phase_count; ++pi)
            {
                const uint64_t original = static_cast<uint64_t>(pi) * kPhaseOne / phase_count;
                const uint64_t shifted = original + kPhaseMid;
                const uint32_t frac = static_cast<uint32_t>(shifted);
                const double d = (double)frac / 4294967296.0 - 0.5;
                std::vector<double> c(taps);
                table_coeffs(table, frac, c);
                double re = 0.0, im = 0.0;
                const int half = taps / 2;
                for(int j = 0; j < taps; ++j)
                {
                    const int k = j - half;
                    re += c[j] * std::cos(w * k);
                    im += c[j] * std::sin(w * k);
                }
                const double err_phase = std::atan2(im, re) - w * d;
                const double gain_db = db20(std::hypot(re, im));
                max_gain = std::max(max_gain, std::abs(gain_db));
                max_phase = std::max(max_phase, std::abs(err_phase) * 180.0 / (double)kPi);
            }
            out << taps << ',' << f << ',' << max_gain << ',' << max_phase << '\n';
        }
    }
}

static void write_reconstruction(const std::string &path,
                                const std::vector<SampleMeta> &metadata,
                                const std::string &pcm_dir)
{
    std::ofstream out(path);
    out << "sample,degree,representation,max_integer_error,cg_iterations\n" << std::setprecision(12);
    for(const auto &m : metadata)
    {
        auto x = read_pcm16(pcm_dir + "/" + m.raw_file, m.frames);
        for(int degree : {3, 5})
        {
            unsigned iterations = 0;
            auto coeff = spline_prefilter(x, degree, false, &iterations);
            double mx = 0.0;
            for(size_t i = 0; i < x.size(); ++i)
                mx = std::max(mx, std::abs(spline_reconstruct(coeff, degree, (uint64_t)i << 32, false, 0, x.size()) - x[i]));
            out << m.label << ',' << degree << ",finite-clamped," << mx << ',' << iterations << '\n';
            if(m.samplemode && m.loop_end > m.loop_start + 1024 && m.loop_end <= m.frames)
            {
                std::vector<double> loop(x.begin() + m.loop_start, x.begin() + m.loop_end);
                iterations = 0;
                auto lp = spline_prefilter(loop, degree, true, &iterations);
                mx = 0.0;
                for(size_t i = m.loop_start; i < m.loop_end; ++i)
                    mx = std::max(mx, std::abs(spline_reconstruct(lp, degree,
                        (uint64_t)(i - m.loop_start) << 32, true, 0, loop.size()) - x[i]));
                out << m.label << ',' << degree << ",periodic-loop," << mx << ',' << iterations << '\n';
            }
        }
    }
    for(int degree : {3, 5})
    {
        std::vector<double> dc(128, 1.0);
        for(bool periodic : {false, true})
        {
            auto coeff = spline_prefilter(dc, degree, periodic);
            double mx = 0.0;
            for(size_t i = 0; i < dc.size(); ++i)
                mx = std::max(mx, std::abs(spline_reconstruct(coeff, degree,
                    (uint64_t)i << 32, periodic, 0, dc.size()) - 1.0));
            out << "unit-dc," << degree << ',' << (periodic ? "periodic" : "finite-clamped") << ',' << mx << ",0\n";
        }
        std::vector<double> impulse(128, 0.0); impulse[64] = 1.0;
        auto coeff = spline_prefilter(impulse, degree, false);
        double mx = 0.0;
        for(size_t i = 0; i < impulse.size(); ++i)
            mx = std::max(mx, std::abs(spline_reconstruct(coeff, degree, (uint64_t)i << 32, false, 0, impulse.size()) - impulse[i]));
        out << "unit-impulse," << degree << ",finite-clamped," << mx << ",0\n";
    }
}

static void write_window_errors(const std::string &path,
                                const std::string &check_path,
                                const std::vector<SampleMeta> &metadata,
                                const std::string &pcm_dir, const std::string &raw_dir,
                                const std::vector<std::pair<int, FirTable>> &tables)
{
    std::ofstream out(path);
    out << "sample,segment,step,transpose_factor,kind,frames,reference_rms_dbfs,reference_delta_dbfs,ref_margin_db,error_rms_dbfs,error_max_dbfs,error_vs_sinc25_db,error_vs_cubic_db,quiet_reference\n" << std::setprecision(12);
    const FirTable current_sinc = make_current_sinc25_table();
    std::ofstream check(check_path);
    check << "sample,segment,step,frames,max_abs_error,rms_error,rms_error_dbfs\n" << std::setprecision(12);
    for(const auto &m : metadata)
    {
        auto data = read_pcm16(pcm_dir + "/" + m.raw_file, m.frames);
        std::vector<double> finite3 = spline_prefilter(data, 3, false);
        std::vector<double> finite5 = spline_prefilter(data, 5, false);
        std::vector<double> loop3, loop5;
        const bool use_loop = m.samplemode && m.loop_end > m.loop_start + 1024 && m.loop_end <= m.frames;
        if(use_loop)
        {
            std::vector<double> loop(data.begin() + m.loop_start, data.begin() + m.loop_end);
            loop3 = spline_prefilter(loop, 3, true);
            loop5 = spline_prefilter(loop, 5, true);
        }
        for(double factor : {0.5, 1.0, 2.0})
        {
            const double step = m.base_step * factor;
            const auto cases = make_cases(m, step);
            for(const auto &c : cases)
            {
                const std::string raw_path = raw_dir + "/" + m.label + "-" + c.name + "-r" + std::to_string(step) + ".csv";
                auto raw = read_raw_frames(raw_path);
                if(raw.empty()) throw std::runtime_error("missing/empty raw window: " + raw_path);
                const size_t frames = raw.size();
                const bool looping = c.looping;
                auto sinc_repro = render_fir(data, current_sinc, 25, c.initial_phase, step,
                                             m.loop_start, m.loop_end, looping, c.has_looped, frames);
                std::vector<double> sinc_recorded(frames);
                for(size_t i = 0; i < frames; ++i) sinc_recorded[i] = raw[i].sinc25;
                const auto reproduc = errors(sinc_repro, sinc_recorded);
                check << m.label << ',' << c.name << ',' << step << ',' << frames << ','
                      << reproduc.max << ',' << reproduc.rms << ',' << db20(reproduc.rms) << '\n';
                for(int degree : {3, 5})
                {
                    auto candidate = render_spline(data, finite3, finite5, loop3, loop5, degree,
                                                   c.initial_phase, step, m.loop_start, m.loop_end,
                                                   looping, c.has_looped, frames);
                    std::vector<double> ref(frames), ref2047(frames), sinc(frames), cubic(frames);
                    for(size_t i = 0; i < frames; ++i) { ref[i] = raw[i].ref; ref2047[i] = raw[i].ref2047; sinc[i] = raw[i].sinc25; cubic[i] = raw[i].cubic; }
                    const auto e = errors(candidate, ref), es = errors(sinc, ref), ec = errors(cubic, ref);
                    const auto conv = errors(ref2047, ref);
                    out << m.label << ',' << c.name << ',' << step << ',' << factor << ",spline" << degree << ',' << frames << ','
                        << db20(e.ref_rms) << ',' << db20(conv.rms) << ',' << db20(conv.rms) - db20(e.rms) << ','
                        << db20(e.rms) << ',' << db20(e.max) << ','
                        << db20(e.rms) - db20(es.rms) << ',' << db20(e.rms) - db20(ec.rms) << ','
                        << (db20(e.ref_rms) < -60.0 ? 1 : 0) << '\n';
                }
                for(const auto &[taps, table] : tables)
                {
                    auto candidate = render_fir(data, table, taps, c.initial_phase, step,
                                                m.loop_start, m.loop_end, looping, c.has_looped, frames);
                    std::vector<double> ref(frames), ref2047(frames), sinc(frames), cubic(frames);
                    for(size_t i = 0; i < frames; ++i) { ref[i] = raw[i].ref; ref2047[i] = raw[i].ref2047; sinc[i] = raw[i].sinc25; cubic[i] = raw[i].cubic; }
                    const auto e = errors(candidate, ref), es = errors(sinc, ref), ec = errors(cubic, ref);
                    const auto conv = errors(ref2047, ref);
                    out << m.label << ',' << c.name << ',' << step << ',' << factor << ",fir" << taps << ',' << frames << ','
                        << db20(e.ref_rms) << ',' << db20(conv.rms) << ',' << db20(conv.rms) - db20(e.rms) << ','
                        << db20(e.rms) << ',' << db20(e.max) << ','
                        << db20(e.rms) - db20(es.rms) << ',' << db20(e.rms) - db20(ec.rms) << ','
                        << (db20(e.ref_rms) < -60.0 ? 1 : 0) << '\n';
                }
            }
        }
    }
}

static void write_loop_entry_errors(const std::string &path,
                                    const std::vector<SampleMeta> &metadata,
                                    const std::string &pcm_dir,
                                    const std::vector<std::pair<int, FirTable>> &tables)
{
    std::ofstream out(path);
    out << "sample,step,kind,frames,error_rms_dbfs,error_max_dbfs,error_vs_sinc25_db,ref_margin_db\n" << std::setprecision(12);
    const FirTable current_sinc = make_current_sinc25_table();
    constexpr size_t frames = 128;
    for(const auto &m : metadata)
    {
        if(!(m.samplemode && m.loop_end > m.loop_start + 1024 && m.loop_end <= m.frames)) continue;
        const auto data = read_pcm16(pcm_dir + "/" + m.raw_file, m.frames);
        std::vector<double> finite3 = spline_prefilter(data, 3, false);
        std::vector<double> finite5 = spline_prefilter(data, 5, false);
        std::vector<double> loop(data.begin() + m.loop_start, data.begin() + m.loop_end);
        std::vector<double> loop3 = spline_prefilter(loop, 3, true);
        std::vector<double> loop5 = spline_prefilter(loop, 5, true);
        for(double factor : {0.5, 1.0, 2.0})
        {
            const double step = m.base_step * factor;
            const double start = m.loop_end - step * (frames / 2.0) + 0.271828;
            const auto ref = render_long_reference(data, start, step, m.loop_start, m.loop_end,
                                                   true, false, frames, 4095);
            const auto ref_short = render_long_reference(data, start, step, m.loop_start, m.loop_end,
                                                         true, false, frames, 2047);
            const auto current = render_fir(data, current_sinc, 25, start, step,
                                            m.loop_start, m.loop_end, true, false, frames);
            const auto ecurrent = errors(current, ref);
            const auto convergence = errors(ref_short, ref);
            for(int degree : {3, 5})
            {
                const auto candidate = render_spline(data, finite3, finite5, loop3, loop5,
                                                     degree, start, step, m.loop_start, m.loop_end,
                                                     true, false, frames);
                const auto e = errors(candidate, ref);
                out << m.label << ',' << step << ",spline" << degree << ',' << frames << ','
                    << db20(e.rms) << ',' << db20(e.max) << ',' << db20(e.rms) - db20(ecurrent.rms)
                    << ',' << db20(convergence.rms) - db20(e.rms) << '\n';
            }
            for(const auto &[taps, table] : tables)
            {
                const auto candidate = render_fir(data, table, taps, start, step,
                                                  m.loop_start, m.loop_end, true, false, frames);
                const auto e = errors(candidate, ref);
                out << m.label << ',' << step << ",fir" << taps << ',' << frames << ','
                    << db20(e.rms) << ',' << db20(e.max) << ',' << db20(e.rms) - db20(ecurrent.rms)
                    << ',' << db20(convergence.rms) - db20(e.rms) << '\n';
            }
        }
    }
}

} // namespace

int main(int argc, char **argv)
{
    if(argc != 2)
    {
        std::cerr << "usage: fractional_delay_screen <analysis-root>\n";
        return 2;
    }
    try
    {
        const std::string root = argv[1];
        const auto metadata = read_metadata(root + "/samples.csv");
        if(metadata.size() != 5) throw std::runtime_error("expected five sample cases");
        std::vector<std::pair<int, FirTable>> tables;
        for(int taps : {13, 17, 21, 25}) tables.push_back({taps, design_fir_table(taps)});
        write_spline_static(root + "/spline-static.csv");
        write_fir_static(root + "/fir-static.csv", tables, root + "/fir-coefficients.csv");
        write_reconstruction(root + "/spline-reconstruction.csv", metadata,
                             "/root/code/fluidsynth-analysis-20261015/cubic-sinc25/sf2-samples");
        write_window_errors(root + "/candidate-windows.csv", root + "/baseline-reproduction.csv", metadata,
                            "/root/code/fluidsynth-analysis-20261015/cubic-sinc25/sf2-samples",
                            "/root/code/fluidsynth-analysis-20261015/cubic-sinc25/sf2-raw", tables);
        write_loop_entry_errors(root + "/loop-entry-windows.csv", metadata,
                                "/root/code/fluidsynth-analysis-20261015/cubic-sinc25/sf2-samples", tables);
        std::cout << "completed: " << metadata.size() << " samples; 2 spline degrees; 4 weighted-LS FIR lengths\n";
    }
    catch(const std::exception &e)
    {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
