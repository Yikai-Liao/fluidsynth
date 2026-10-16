#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr uint64_t kPhaseOne = uint64_t{1} << 32;
constexpr uint64_t kPhaseMid = uint64_t{1} << 31;
constexpr size_t kWindowFrames = 882;
constexpr int kFilterTaps = 4095;
constexpr int kFilterHalf = kFilterTaps / 2;
constexpr double kCutoffCycles = 0.25;
constexpr double kKaiserBeta = 9.5;

struct Meta {
    std::string label, raw_file;
    size_t frames{}, loop_start{}, loop_end{};
    int samplemode{};
    double base_step{};
};
struct Case { std::string name; double initial_phase{}; bool looping{}, has_looped{}; };
struct Metrics { double rms{}, maximum{}; };

static std::vector<std::string> split_csv(const std::string &line)
{
    std::vector<std::string> result;
    size_t start = 0;
    while(true)
    {
        const size_t end = line.find(',', start);
        result.push_back(line.substr(start, end == std::string::npos ? end : end - start));
        if(end == std::string::npos) break;
        start = end + 1;
    }
    return result;
}

static std::vector<Meta> read_metadata(const std::string &path)
{
    std::ifstream in(path);
    if(!in) throw std::runtime_error("cannot open metadata " + path);
    std::string line;
    std::getline(in, line);
    std::vector<Meta> result;
    while(std::getline(in, line))
    {
        const auto f = split_csv(line);
        if(f.size() < 7) continue;
        result.push_back({f[0], f[1], std::stoull(f[2]), std::stoull(f[3]),
                          std::stoull(f[4]), std::stoi(f[5]), std::stod(f[6])});
    }
    return result;
}

static std::vector<double> read_pcm16(const std::string &path, size_t frames)
{
    std::ifstream in(path, std::ios::binary);
    if(!in) throw std::runtime_error("cannot open PCM " + path);
    const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)), {});
    if(bytes.size() != frames * 2) throw std::runtime_error("bad PCM size " + path);
    std::vector<double> result(frames);
    for(size_t i = 0; i < frames; ++i)
    {
        const uint16_t u = static_cast<uint16_t>(bytes[2 * i])
                           | (static_cast<uint16_t>(bytes[2 * i + 1]) << 8);
        result[i] = static_cast<double>(static_cast<int16_t>(u)) / 32768.0;
    }
    return result;
}

static bool has_loop(const Meta &m)
{
    return m.samplemode && m.loop_end > m.loop_start + 1024 && m.loop_end <= m.frames;
}

static std::vector<Case> make_cases(const Meta &m, double step)
{
    constexpr double offset = 0.271828;
    if(has_loop(m))
        return {{"attack", offset, true, false},
                {"sustain", static_cast<double>(m.loop_start) + 1024.0 + offset, true, true},
                {"loop_seam", static_cast<double>(m.loop_end) - step * (kWindowFrames / 2.0) + offset, true, true},
                {"release_exit", static_cast<double>(m.frames - 1) - step * (kWindowFrames - 1.0) + offset, false, true}};
    return {{"attack", offset, false, false},
            {"interior", 1024.0 + offset, false, false},
            {"tail", static_cast<double>(m.frames - 1) - step * (kWindowFrames - 1.0) + offset, false, false}};
}

static double source_at(const std::vector<double> &data, long long index,
                        size_t loop_start, size_t loop_end, bool looping, bool has_looped)
{
    if(looping && has_looped)
    {
        const long long length = static_cast<long long>(loop_end - loop_start);
        long long q = (index - static_cast<long long>(loop_start)) % length;
        if(q < 0) q += length;
        index = static_cast<long long>(loop_start) + q;
    }
    else if(looping && index >= static_cast<long long>(loop_end))
    {
        const long long length = static_cast<long long>(loop_end - loop_start);
        long long q = (index - static_cast<long long>(loop_start)) % length;
        if(q < 0) q += length;
        index = static_cast<long long>(loop_start) + q;
    }
    else index = std::max<long long>(0, std::min<long long>(static_cast<long long>(data.size()) - 1, index));
    return data[static_cast<size_t>(index)];
}

static double i0(double x)
{
    double sum = 1.0, term = 1.0;
    const double q = x * x / 4.0;
    for(int k = 1; k < 100; ++k)
    {
        term *= q / (static_cast<double>(k) * k);
        sum += term;
        if(term < sum * 1.0e-16) break;
    }
    return sum;
}

static std::vector<double> make_lowpass()
{
    std::vector<double> h(kFilterTaps);
    const double denom = i0(kKaiserBeta);
    double sum = 0.0;
    for(int j = -kFilterHalf; j <= kFilterHalf; ++j)
    {
        const double x = static_cast<double>(j);
        const double ideal = 2.0 * kCutoffCycles * (std::abs(x) < 1.0e-14
            ? 1.0 : std::sin(2.0 * kPi * kCutoffCycles * x) / (2.0 * kPi * kCutoffCycles * x));
        const double ratio = x / static_cast<double>(kFilterHalf);
        const double window = i0(kKaiserBeta * std::sqrt(std::max(0.0, 1.0 - ratio * ratio))) / denom;
        h[static_cast<size_t>(j + kFilterHalf)] = ideal * window;
        sum += ideal * window;
    }
    for(double &v : h) v /= sum;
    return h;
}

static double filter_sample(const std::vector<double> &pcm, long long center,
                            size_t loop_start, size_t loop_end, bool looping,
                            bool has_looped, const std::vector<double> &h)
{
    const long long first = center - kFilterHalf;
    const long long last = center + kFilterHalf;
    bool contiguous = first >= 0 && last < static_cast<long long>(pcm.size());
    if(looping && has_looped)
        contiguous = contiguous && first >= static_cast<long long>(loop_start)
                     && last < static_cast<long long>(loop_end);
    else if(looping)
        contiguous = contiguous && last < static_cast<long long>(loop_end);
    double value = 0.0;
    if(contiguous)
    {
        const size_t begin = static_cast<size_t>(first);
        for(size_t i = 0; i < h.size(); ++i) value += h[i] * pcm[begin + i];
    }
    else
    {
        for(int j = -kFilterHalf; j <= kFilterHalf; ++j)
            value += h[static_cast<size_t>(j + kFilterHalf)]
                     * source_at(pcm, center + j, loop_start, loop_end, looping, has_looped);
    }
    return value;
}

struct Layer {
    Meta meta;
    std::vector<double> pcm;
    std::vector<float> finite;
    std::vector<float> attack;
    std::vector<float> periodic;
    Meta reduced_meta;
    std::vector<float> reduced_finite;
    std::vector<float> reduced_attack;
    std::vector<float> reduced_periodic;
    bool reduced_eligible{};
    double prepare_ms{};
};

static Layer prepare(const Meta &m, const std::string &pcm_dir,
                     const std::vector<double> &h)
{
    const auto begin = std::chrono::steady_clock::now();
    Layer layer;
    layer.meta = m;
    layer.pcm = read_pcm16(pcm_dir + "/" + m.raw_file, m.frames);
    layer.finite.resize(m.frames);
    for(size_t i = 0; i < m.frames; ++i)
        layer.finite[i] = static_cast<float>(filter_sample(layer.pcm, static_cast<long long>(i),
            m.loop_start, m.loop_end, false, false, h));
    if(has_loop(m))
    {
        layer.attack = layer.finite;
        const size_t first_affected = m.loop_end > static_cast<size_t>(kFilterHalf)
            ? m.loop_end - static_cast<size_t>(kFilterHalf) : 0;
        for(size_t i = first_affected; i < m.loop_end; ++i)
            layer.attack[i] = static_cast<float>(filter_sample(layer.pcm, static_cast<long long>(i),
                m.loop_start, m.loop_end, true, false, h));
        const size_t length = m.loop_end - m.loop_start;
        layer.periodic.resize(length);
        for(size_t i = 0; i < length; ++i)
            layer.periodic[i] = static_cast<float>(filter_sample(layer.pcm,
                static_cast<long long>(m.loop_start + i), m.loop_start, m.loop_end, true, true, h));
    }
    else layer.attack = layer.finite;
    layer.reduced_eligible = !has_loop(m)
        || ((m.loop_start & 1U) == 0 && (m.loop_end & 1U) == 0);
    if(layer.reduced_eligible)
    {
        layer.reduced_meta = m;
        layer.reduced_meta.frames = (m.frames + 1) / 2;
        if(has_loop(m))
        {
            layer.reduced_meta.loop_start = m.loop_start / 2;
            layer.reduced_meta.loop_end = m.loop_end / 2;
        }
        layer.reduced_finite.resize(layer.reduced_meta.frames);
        for(size_t i = 0; i < layer.reduced_finite.size(); ++i)
            layer.reduced_finite[i] = layer.finite[2 * i];
        if(has_loop(m))
        {
            layer.reduced_attack.resize(layer.reduced_meta.frames);
            for(size_t i = 0; i < layer.reduced_attack.size(); ++i)
                layer.reduced_attack[i] = layer.attack[2 * i];
            const size_t reduced_loop_frames = layer.reduced_meta.loop_end - layer.reduced_meta.loop_start;
            layer.reduced_periodic.resize(reduced_loop_frames);
            for(size_t i = 0; i < reduced_loop_frames; ++i)
                layer.reduced_periodic[i] = layer.periodic[2 * i];
        }
        else layer.reduced_attack = layer.reduced_finite;
    }
    layer.prepare_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - begin).count();
    return layer;
}

static double layer_at(const Layer &layer, long long index, bool looping, bool has_looped)
{
    const Meta &m = layer.meta;
    if(looping && has_looped && has_loop(m))
    {
        const long long length = static_cast<long long>(m.loop_end - m.loop_start);
        long long q = (index - static_cast<long long>(m.loop_start)) % length;
        if(q < 0) q += length;
        return layer.periodic[static_cast<size_t>(q)];
    }
    if(looping && has_loop(m) && index >= static_cast<long long>(m.loop_end))
    {
        const long long length = static_cast<long long>(m.loop_end - m.loop_start);
        long long q = (index - static_cast<long long>(m.loop_start)) % length;
        if(q < 0) q += length;
        index = static_cast<long long>(m.loop_start) + q;
    }
    index = std::max<long long>(0, std::min<long long>(static_cast<long long>(m.frames) - 1, index));
    const auto &table = looping && has_loop(m) ? layer.attack : layer.finite;
    return table[static_cast<size_t>(index)];
}

static double reduced_layer_at(const Layer &layer, long long index, bool looping, bool has_looped)
{
    const Meta &m = layer.reduced_meta;
    const bool loop_layer = layer.reduced_eligible && has_loop(layer.meta);
    if(looping && has_looped && loop_layer)
    {
        const long long length = static_cast<long long>(m.loop_end - m.loop_start);
        long long q = (index - static_cast<long long>(m.loop_start)) % length;
        if(q < 0) q += length;
        return layer.reduced_periodic[static_cast<size_t>(q)];
    }
    if(looping && loop_layer && index >= static_cast<long long>(m.loop_end))
    {
        const long long length = static_cast<long long>(m.loop_end - m.loop_start);
        long long q = (index - static_cast<long long>(m.loop_start)) % length;
        if(q < 0) q += length;
        index = static_cast<long long>(m.loop_start) + q;
    }
    index = std::max<long long>(0, std::min<long long>(static_cast<long long>(m.frames) - 1, index));
    const auto &table = looping && loop_layer ? layer.reduced_attack : layer.reduced_finite;
    return table[static_cast<size_t>(index)];
}

using SincTable = std::vector<std::array<double, 25>>;
static SincTable make_sinc25_table()
{
    constexpr int table_size = 4096, half = 12;
    SincTable table(table_size + 1);
    for(int row = 0; row <= table_size; ++row)
    {
        const double phase = static_cast<double>(row) / table_size;
        const double center = half - 0.5 + phase;
        double sum = 0.0;
        for(int i = 0; i < 25; ++i)
        {
            const double shifted = i - center;
            const double arg = kPi * shifted;
            const double s = std::abs(arg) > 1.0e-6 ? std::sin(arg) / arg : 1.0;
            const double win = std::cos(arg / 25.0);
            table[static_cast<size_t>(row)][static_cast<size_t>(i)] = s * win * win;
            sum += s * win * win;
        }
        for(double &v : table[static_cast<size_t>(row)]) v /= sum;
    }
    return table;
}

static uint64_t fixed_phase(double position)
{
    return static_cast<uint64_t>(std::llround(position * 4294967296.0));
}
static uint64_t fixed_increment(double step)
{
    const uint64_t integer = static_cast<uint64_t>(step);
    const uint32_t fraction = static_cast<uint32_t>((step - static_cast<int>(step)) * 4294967296.0);
    return (integer << 32) | fraction;
}

template<typename Getter>
static std::vector<double> render_sinc25(Getter get, const SincTable &table,
                                          const Case &c, double step, size_t count,
                                          const Meta &m)
{
    constexpr uint32_t low_mask = (uint32_t{1} << 20) - 1;
    constexpr int half = 12;
    uint64_t phase = fixed_phase(c.initial_phase);
    const uint64_t increment = fixed_increment(step);
    bool has_looped = c.has_looped;
    std::vector<double> out(count);
    for(size_t f = 0; f < count; ++f)
    {
        const uint64_t shifted = phase + kPhaseMid;
        const long long center = static_cast<long long>(shifted >> 32);
        const uint32_t frac = static_cast<uint32_t>(shifted);
        const uint32_t row = frac >> 20;
        const double alpha = static_cast<double>(frac & low_mask) / 1048576.0;
        double y = 0.0;
        for(int i = 0; i < 25; ++i)
        {
            const double a = table[row][static_cast<size_t>(i)];
            const double b = table[row + 1][static_cast<size_t>(i)];
            const double coeff = a + (b - a) * alpha;
            y += coeff * get(center + i - half, c.looping, has_looped);
        }
        out[f] = y;
        phase += increment;
        if(c.looping && (phase >> 32) >= m.loop_end)
        {
            phase -= static_cast<uint64_t>(m.loop_end - m.loop_start) << 32;
            has_looped = true;
        }
    }
    return out;
}

static std::vector<double> make_reference_coefficients(double fraction, int taps, double cutoff)
{
    const int half = taps / 2;
    std::vector<double> coefficients(static_cast<size_t>(taps));
    double norm = 0.0;
    for(int j = -half; j <= half; ++j)
    {
        const double t = static_cast<double>(j) - fraction;
        const double arg = kPi * cutoff * t;
        const double sinc = std::abs(arg) < 1.0e-14 ? 1.0 : std::sin(arg) / arg;
        const double window = std::abs(t) <= half ? 0.5 * (1.0 + std::cos(kPi * t / half)) : 0.0;
        const double coefficient = cutoff * sinc * window;
        coefficients[static_cast<size_t>(j + half)] = coefficient;
        norm += coefficient;
    }
    for(double &coefficient : coefficients) coefficient /= norm;
    return coefficients;
}

static std::vector<double> render_reference(const Layer &layer, const Case &c,
                                            size_t count, const std::vector<double> &coefficients)
{
    const int half = static_cast<int>(coefficients.size() / 2);
    uint64_t phase = fixed_phase(c.initial_phase);
    const uint64_t increment = fixed_increment(2.0);
    bool has_looped = c.has_looped;
    std::vector<double> out(count);
    for(size_t f = 0; f < count; ++f)
    {
        const long long center = static_cast<long long>(phase >> 32);
        double sum = 0.0;
        for(int j = -half; j <= half; ++j)
        {
            sum += source_at(layer.pcm, center + j, layer.meta.loop_start,
                             layer.meta.loop_end, c.looping, has_looped)
                   * coefficients[static_cast<size_t>(j + half)];
        }
        out[f] = sum;
        phase += increment;
        if(c.looping && (phase >> 32) >= layer.meta.loop_end)
        {
            phase -= static_cast<uint64_t>(layer.meta.loop_end - layer.meta.loop_start) << 32;
            has_looped = true;
        }
    }
    return out;
}

static Metrics difference(const std::vector<double> &a, const std::vector<double> &b)
{
    if(a.size() != b.size()) throw std::runtime_error("metric length mismatch");
    long double e2 = 0.0L;
    double maximum = 0.0;
    for(size_t i = 0; i < a.size(); ++i)
    {
        const double e = a[i] - b[i];
        e2 += static_cast<long double>(e) * e;
        maximum = std::max(maximum, std::abs(e));
    }
    return {std::sqrt(static_cast<double>(e2 / a.size())), maximum};
}
static double db20(double x) { return x > 0.0 ? 20.0 * std::log10(x) : -INFINITY; }
static double rms_value(const std::vector<double> &x)
{
    long double sum = 0.0L;
    for(double value : x) sum += static_cast<long double>(value) * value;
    return std::sqrt(static_cast<double>(sum / x.size()));
}

static void write_windows(const std::string &path, const std::string &reduced_path,
                          const std::vector<Meta> &metas,
                          const std::vector<Layer> &layers, const SincTable &table)
{
    std::ofstream out(path);
    if(!out) throw std::runtime_error("cannot create window output");
    std::ofstream reduced_out(reduced_path);
    if(!reduced_out) throw std::runtime_error("cannot create reduced-layer window output");
    out << "sample,segment,step,frames,reference_taps,reference_rms_dbfs,baseline_error_dbfs,candidate_error_dbfs,candidate_max_error_dbfs,error_change_vs_sinc25_db,reference_margin_db,quiet_reference,finite\n" << std::setprecision(12);
    reduced_out << "sample,segment,mode,step,frames,reference_taps,reference_rms_dbfs,baseline_error_dbfs,candidate_error_dbfs,candidate_max_error_dbfs,error_change_vs_sinc25_db,reference_margin_db,quiet_reference,finite\n" << std::setprecision(12);
    constexpr double step = 2.0;
    for(size_t s = 0; s < metas.size(); ++s)
    {
        const Meta &m = metas[s];
        const Layer &layer = layers[s];
        for(const Case &c : make_cases(m, step))
        {
            const double fraction = static_cast<double>(static_cast<uint32_t>(fixed_phase(c.initial_phase)))
                                    / 4294967296.0;
            auto coeff32767 = make_reference_coefficients(fraction, 32767, 0.5);
            auto coeff65535 = make_reference_coefficients(fraction, 65535, 0.5);
            auto ref = render_reference(layer, c, kWindowFrames, coeff65535);
            auto ref32767 = render_reference(layer, c, kWindowFrames, coeff32767);
            const auto get_raw = [&](long long i, bool loop, bool done) {
                return source_at(layer.pcm, i, m.loop_start, m.loop_end, loop, done);
            };
            const auto get_layer = [&](long long i, bool loop, bool done) {
                return layer_at(layer, i, loop, done);
            };
            const auto baseline = render_sinc25(get_raw, table, c, step, kWindowFrames, m);
            const auto candidate = render_sinc25(get_layer, table, c, step, kWindowFrames, m);
            const bool use_reduced = layer.reduced_eligible;
            Case reduced_case = c;
            double reduced_step = step;
            const Meta &reduced_meta = use_reduced ? layer.reduced_meta : m;
            if(use_reduced)
            {
                reduced_case.initial_phase *= 0.5;
                reduced_step = 1.0;
            }
            const auto get_reduced = [&](long long i, bool loop, bool done) {
                return use_reduced ? reduced_layer_at(layer, i, loop, done)
                                   : layer_at(layer, i, loop, done);
            };
            const auto reduced_candidate = render_sinc25(get_reduced, table, reduced_case,
                reduced_step, kWindowFrames, reduced_meta);
            Metrics be = difference(baseline, ref);
            Metrics ce = difference(candidate, ref);
            Metrics reduced_ce = difference(reduced_candidate, ref);
            Metrics convergence = difference(ref32767, ref);
            bool finite = true;
            for(double y : candidate) finite = finite && std::isfinite(y);
            for(double y : reduced_candidate) finite = finite && std::isfinite(y);
            int reference_taps = 65535;
            double reference_rms_dbfs = db20(rms_value(ref));
            double reference_margin_db = db20(convergence.rms) - db20(ce.rms);
            double reduced_reference_margin_db = db20(convergence.rms) - db20(reduced_ce.rms);
            if(reference_rms_dbfs >= -60.0
               && (reference_margin_db > -20.0 || reduced_reference_margin_db > -20.0))
            {
                const auto coeff262143 = make_reference_coefficients(fraction, 262143, 0.5);
                const auto coeff524287 = make_reference_coefficients(fraction, 524287, 0.5);
                ref32767 = render_reference(layer, c, kWindowFrames, coeff262143);
                ref = render_reference(layer, c, kWindowFrames, coeff524287);
                be = difference(baseline, ref);
                ce = difference(candidate, ref);
                reduced_ce = difference(reduced_candidate, ref);
                convergence = difference(ref32767, ref);
                reference_taps = 524287;
                reference_rms_dbfs = db20(rms_value(ref));
                reference_margin_db = db20(convergence.rms) - db20(ce.rms);
                reduced_reference_margin_db = db20(convergence.rms) - db20(reduced_ce.rms);
            }
            out << m.label << ',' << c.name << ',' << step << ',' << kWindowFrames << ','
                << reference_taps << ',' << reference_rms_dbfs << ','
                << db20(be.rms) << ',' << db20(ce.rms) << ',' << db20(ce.maximum) << ','
                << db20(ce.rms) - db20(be.rms) << ','
                << reference_margin_db << ','
                << (reference_rms_dbfs < -60.0 ? 1 : 0) << ',' << (finite ? 1 : 0) << '\n';
            reduced_out << m.label << ',' << c.name << ',' << (use_reduced ? "decimated-2x" : "full-rate-fallback")
                << ',' << step << ',' << kWindowFrames << ',' << reference_taps << ',' << reference_rms_dbfs << ','
                << db20(be.rms) << ',' << db20(reduced_ce.rms) << ',' << db20(reduced_ce.maximum) << ','
                << db20(reduced_ce.rms) - db20(be.rms) << ',' << reduced_reference_margin_db << ','
                << (reference_rms_dbfs < -60.0 ? 1 : 0) << ',' << (finite ? 1 : 0) << '\n';
        }
    }
}

struct Fit { double amplitude{}; };
static Fit sinusoid_amplitude(const std::vector<double> &signal, double frequency)
{
    long double ss = 0.0L, cc = 0.0L, sc = 0.0L, sy = 0.0L, cy = 0.0L;
    for(size_t n = 0; n < signal.size(); ++n)
    {
        const double phase = 2.0 * kPi * frequency * static_cast<double>(n);
        const double s = std::sin(phase), c = std::cos(phase);
        ss += s * s; cc += c * c; sc += s * c;
        sy += s * signal[n]; cy += c * signal[n];
    }
    const long double det = ss * cc - sc * sc;
    if(std::abs(static_cast<double>(det)) < 1.0e-12) return {0.0};
    const double a = static_cast<double>((sy * cc - cy * sc) / det);
    const double b = static_cast<double>((cy * ss - sy * sc) / det);
    return {std::hypot(a, b)};
}

static double filter_gain(const std::vector<double> &h, double frequency)
{
    long double response = 0.0L;
    for(int j = -kFilterHalf; j <= kFilterHalf; ++j)
        response += h[static_cast<size_t>(j + kFilterHalf)]
                    * std::cos(2.0 * kPi * frequency * static_cast<double>(j));
    return static_cast<double>(response);
}

static void write_static(const std::string &path, const std::vector<double> &h,
                         const SincTable &table)
{
    constexpr size_t length = 65536, output_count = 16384;
    const std::vector<double> frequencies = {
        0.01, 0.05, 0.10, 0.15, 0.20, 0.22, 0.23, 0.2395,
        0.245, 0.249, 0.251, 0.255, 0.261, 0.30, 0.35, 0.40, 0.45, 0.49
    };
    std::ofstream out(path);
    if(!out) throw std::runtime_error("cannot create static output");
    out << "frequency_cycles_per_source_sample,filter_gain_db,source_phase,configuration,candidate_gain_error_db,alias_input_db,band\n" << std::setprecision(12);
    for(double requested : frequencies)
    {
        const size_t bin = static_cast<size_t>(std::llround(requested * length));
        const double frequency = static_cast<double>(bin) / length;
        const double hgain = filter_gain(h, frequency);
        std::vector<double> filtered(length);
        for(size_t i = 0; i < length; ++i)
            filtered[i] = hgain * std::sin(2.0 * kPi * frequency * static_cast<double>(i));
        for(double start : {0.0, 0.25, 0.5, 0.75})
        {
            const Case c{"static", start, false, false};
            const auto periodic_get = [&](long long index, bool, bool) {
                long long q = index % static_cast<long long>(length);
                if(q < 0) q += static_cast<long long>(length);
                return filtered[static_cast<size_t>(q)];
            };
            const auto y = render_sinc25(periodic_get, table, c, 2.0, output_count, Meta{});
            Case down_case = c;
            down_case.initial_phase *= 0.5;
            const auto reduced_get = [&](long long index, bool, bool) {
                long long q = index % static_cast<long long>(length / 2);
                if(q < 0) q += static_cast<long long>(length / 2);
                return filtered[static_cast<size_t>(2 * q)];
            };
            const auto y_reduced = render_sinc25(reduced_get, table, down_case, 1.0,
                                                  output_count, Meta{});
            const double output_frequency = 2.0 * frequency;
            double folded = std::fmod(output_frequency, 1.0);
            if(folded < 0.0) folded += 1.0;
            if(folded > 0.5) folded = 1.0 - folded;
            const double amplitude = sinusoid_amplitude(y, folded).amplitude;
            const double reduced_amplitude = sinusoid_amplitude(y_reduced, folded).amplitude;
            const char *band = frequency <= 0.2400001 ? "passband"
                : (frequency < 0.26 ? "transition" : "stopband");
            for(const auto &[configuration, candidate_amplitude] :
                {std::pair<const char *, double>{"full-rate", amplitude},
                 std::pair<const char *, double>{"decimated-2x", reduced_amplitude}})
            {
                const double gain_db = db20(candidate_amplitude);
                out << frequency << ',' << db20(std::abs(hgain)) << ',' << start << ',' << configuration << ','
                    << (frequency < 0.26 ? gain_db : 0.0) << ','
                    << (frequency >= 0.26 ? gain_db : 0.0) << ',' << band << '\n';
            }
        }
    }
}

static void write_storage(const std::string &path, const std::vector<Layer> &layers)
{
    std::ofstream out(path);
    if(!out) throw std::runtime_error("cannot create storage output");
    out << "sample,frames,loop_frames,reduced_eligible,original_int16_bytes,finite_float_bytes,attack_float_bytes,periodic_float_bytes,first_stage_total_bytes,first_stage_ratio_vs_original,reduced_finite_float_bytes,reduced_attack_float_bytes,reduced_periodic_float_bytes,original_plus_reduced_total_bytes,reduced_ratio_vs_original,prepare_ms\n" << std::setprecision(12);
    for(const Layer &layer : layers)
    {
        const Meta &m = layer.meta;
        const uint64_t original = static_cast<uint64_t>(m.frames) * 2;
        const uint64_t finite = static_cast<uint64_t>(layer.finite.size()) * sizeof(float);
        const uint64_t attack = has_loop(m) ? static_cast<uint64_t>(layer.attack.size()) * sizeof(float) : 0;
        const uint64_t periodic = static_cast<uint64_t>(layer.periodic.size()) * sizeof(float);
        const uint64_t first_stage_total = original + finite + attack + periodic;
        const uint64_t reduced_finite = static_cast<uint64_t>(layer.reduced_finite.size()) * sizeof(float);
        const uint64_t reduced_attack = static_cast<uint64_t>(layer.reduced_attack.size()) * sizeof(float);
        const uint64_t reduced_periodic = static_cast<uint64_t>(layer.reduced_periodic.size()) * sizeof(float);
        const uint64_t reduced_total = original + reduced_finite + reduced_attack + reduced_periodic;
        out << m.label << ',' << m.frames << ',' << (has_loop(m) ? m.loop_end - m.loop_start : 0) << ','
            << (layer.reduced_eligible ? 1 : 0) << ',' << original << ',' << finite << ',' << attack << ',' << periodic << ','
            << first_stage_total << ',' << static_cast<double>(first_stage_total) / static_cast<double>(original) << ','
            << reduced_finite << ',' << reduced_attack << ',' << reduced_periodic << ',' << reduced_total << ','
            << static_cast<double>(reduced_total) / static_cast<double>(original) << ',' << layer.prepare_ms << '\n';
    }
}

} // namespace

int main(int argc, char **argv)
{
    if(argc != 2) { std::cerr << "usage: mipmap_d_screen <analysis-directory>\n"; return 2; }
    try
    {
        const std::string root = argv[1];
        const std::string samples_csv = "/root/code/fluidsynth-analysis-20261015/fractional-delay-ab/samples.csv";
        const std::string pcm_dir = "/root/code/fluidsynth-analysis-20261015/cubic-sinc25/sf2-samples";
        const auto h = make_lowpass();
        const auto table = make_sinc25_table();
        std::vector<Meta> metas = read_metadata(samples_csv);
        std::vector<Layer> layers;
        for(const Meta &m : metas)
        {
            Layer layer = prepare(m, pcm_dir, h);
            std::cout << "prepared " << m.label << " in " << layer.prepare_ms << " ms\n";
            layers.push_back(std::move(layer));
        }
        write_windows(root + "/windows.csv", root + "/decimated-windows.csv", metas, layers, table);
        write_static(root + "/static.csv", h, table);
        write_storage(root + "/storage.csv", layers);
        std::cout << "completed fixed-step-2 prefilter screen without decimation\n";
    }
    catch(const std::exception &e)
    {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
