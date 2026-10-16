#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr uint64_t kPhaseOne = uint64_t{1} << 32;
constexpr uint64_t kPhaseMid = uint64_t{1} << 31;
constexpr int kUpsampleTaps = 4095;
constexpr int kUpsampleHalf = kUpsampleTaps / 2;
constexpr size_t kFrames = 882;

struct Meta {
    std::string label, raw_file;
    size_t frames{}, loop_start{}, loop_end{};
    int samplemode{};
    double base_step{};
};
struct RawFrame { double cubic{}, sinc25{}, ref2047{}, ref4095{}; };
struct Case { std::string name; double initial_phase{}; bool looping{}, has_looped{}; };

static std::vector<std::string> split_csv(const std::string &line)
{
    std::vector<std::string> out;
    size_t start = 0;
    while(true)
    {
        const size_t end = line.find(',', start);
        out.push_back(line.substr(start, end == std::string::npos ? end : end - start));
        if(end == std::string::npos) break;
        start = end + 1;
    }
    return out;
}

static std::vector<Meta> read_metadata(const std::string &path)
{
    std::ifstream in(path);
    std::string line;
    std::getline(in, line);
    std::vector<Meta> out;
    while(std::getline(in, line))
    {
        auto f = split_csv(line);
        if(f.size() < 7) continue;
        out.push_back({f[0], f[1], std::stoull(f[2]), std::stoull(f[3]),
                       std::stoull(f[4]), std::stoi(f[5]), std::stod(f[6])});
    }
    return out;
}

static std::vector<double> read_pcm16(const std::string &path, size_t frames)
{
    std::ifstream in(path, std::ios::binary);
    if(!in) throw std::runtime_error("cannot open " + path);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)), {});
    if(bytes.size() != frames * 2) throw std::runtime_error("bad PCM size: " + path);
    std::vector<double> out(frames);
    for(size_t i = 0; i < frames; ++i)
    {
        const uint16_t u = static_cast<uint16_t>(bytes[2 * i])
                           | (static_cast<uint16_t>(bytes[2 * i + 1]) << 8);
        out[i] = static_cast<double>(static_cast<int16_t>(u)) / 32768.0;
    }
    return out;
}

static std::vector<RawFrame> read_raw(const std::string &path)
{
    std::ifstream in(path);
    if(!in) throw std::runtime_error("cannot open reference window: " + path);
    std::string line;
    std::getline(in, line);
    std::vector<RawFrame> out;
    while(std::getline(in, line))
    {
        const auto f = split_csv(line);
        if(f.size() < 6) continue;
        out.push_back({std::stod(f[1]), std::stod(f[2]), std::stod(f[4]), std::stod(f[5])});
    }
    return out;
}

static bool has_usable_loop(const Meta &m)
{
    return m.samplemode && m.loop_end > m.loop_start + 1024 && m.loop_end <= m.frames;
}

static std::vector<Case> make_cases(const Meta &m, double step)
{
    constexpr double offset = 0.271828;
    if(has_usable_loop(m))
        return {{"attack", offset, true, false},
                {"sustain", static_cast<double>(m.loop_start) + 1024.0 + offset, true, true},
                {"loop_seam", static_cast<double>(m.loop_end) - step * (kFrames / 2.0) + offset, true, true},
                {"release_exit", static_cast<double>(m.frames - 1) - step * (kFrames - 1.0) + offset, false, true}};
    return {{"attack", offset, false, false},
            {"interior", 1024.0 + offset, false, false},
            {"tail", static_cast<double>(m.frames - 1) - step * (kFrames - 1.0) + offset, false, false}};
}

static double source_at(const std::vector<double> &data, long long index,
                        size_t loop_start, size_t loop_end, bool looping, bool has_looped)
{
    if(looping && has_looped)
    {
        const long long n = static_cast<long long>(loop_end - loop_start);
        long long q = (index - static_cast<long long>(loop_start)) % n;
        if(q < 0) q += n;
        index = static_cast<long long>(loop_start) + q;
    }
    else if(looping && index >= static_cast<long long>(loop_end))
    {
        const long long n = static_cast<long long>(loop_end - loop_start);
        long long q = (index - static_cast<long long>(loop_start)) % n;
        if(q < 0) q += n;
        index = static_cast<long long>(loop_start) + q;
    }
    else index = std::max<long long>(0, std::min<long long>(static_cast<long long>(data.size()) - 1, index));
    return data[static_cast<size_t>(index)];
}

static std::vector<double> make_half_phase_kernel()
{
    std::vector<double> c(kUpsampleTaps);
    double sum = 0.0;
    for(int j = -kUpsampleHalf; j <= kUpsampleHalf; ++j)
    {
        const double t = static_cast<double>(j) - 0.5;
        double value = 0.0;
        if(std::abs(t) <= kUpsampleHalf)
        {
            const double a = kPi * t;
            const double sinc = std::abs(a) < 1.0e-14 ? 1.0 : std::sin(a) / a;
            const double window = 0.5 * (1.0 + std::cos(kPi * t / kUpsampleHalf));
            value = sinc * window;
        }
        c[static_cast<size_t>(j + kUpsampleHalf)] = value;
        sum += value;
    }
    for(double &v : c) v /= sum;
    return c;
}

static double reconstruct_half(const std::vector<double> &data, long long left,
                               size_t loop_start, size_t loop_end,
                               bool looping, bool has_looped,
                               const std::vector<double> &kernel)
{
    double value = 0.0;
    const long long first = left - kUpsampleHalf;
    const long long last = left + kUpsampleHalf;
    bool contiguous = first >= 0 && last < static_cast<long long>(data.size());
    if(looping && has_looped)
        contiguous = contiguous && first >= static_cast<long long>(loop_start)
                     && last < static_cast<long long>(loop_end);
    else if(looping)
        contiguous = contiguous && last < static_cast<long long>(loop_end);
    if(contiguous)
    {
        const size_t begin = static_cast<size_t>(first);
        for(size_t j = 0; j < kernel.size(); ++j)
            value += kernel[j] * data[begin + j];
        return value;
    }
    for(int j = -kUpsampleHalf; j <= kUpsampleHalf; ++j)
        value += kernel[static_cast<size_t>(j + kUpsampleHalf)]
                 * source_at(data, left + j, loop_start, loop_end, looping, has_looped);
    return value;
}

struct Prepared {
    Meta meta;
    std::vector<double> pcm;
    std::vector<float> full_finite;
    std::vector<float> full_attack;
    std::vector<float> periodic_loop;
    size_t attack_rebuilt_midpoints{};
    size_t generated_midpoints{};
    double prepare_ms{};
};

static Prepared prepare(const Meta &m, const std::string &pcm_dir,
                        const std::vector<double> &half_kernel)
{
    const auto begin = std::chrono::steady_clock::now();
    Prepared p;
    p.meta = m;
    p.pcm = read_pcm16(pcm_dir + "/" + m.raw_file, m.frames);
    p.full_finite.resize(2 * m.frames);
    for(size_t i = 0; i < m.frames; ++i)
    {
        p.full_finite[2 * i] = static_cast<float>(p.pcm[i]);
        p.full_finite[2 * i + 1] = static_cast<float>(reconstruct_half(
            p.pcm, static_cast<long long>(i), m.loop_start, m.loop_end, false, false, half_kernel));
    }
    p.generated_midpoints += m.frames;
    if(has_usable_loop(m))
    {
        p.full_attack = p.full_finite;
        const size_t first_affected = m.loop_end > static_cast<size_t>(kUpsampleHalf)
            ? m.loop_end - static_cast<size_t>(kUpsampleHalf) : 0;
        for(size_t i = first_affected; i < m.loop_end; ++i)
        {
            p.full_attack[2 * i + 1] = static_cast<float>(reconstruct_half(
                p.pcm, static_cast<long long>(i), m.loop_start, m.loop_end, true, false, half_kernel));
            ++p.attack_rebuilt_midpoints;
        }
        const size_t length = m.loop_end - m.loop_start;
        p.periodic_loop.resize(2 * length);
        for(size_t i = 0; i < length; ++i)
        {
            const long long source_index = static_cast<long long>(m.loop_start + i);
            p.periodic_loop[2 * i] = static_cast<float>(p.pcm[static_cast<size_t>(source_index)]);
            p.periodic_loop[2 * i + 1] = static_cast<float>(reconstruct_half(
                p.pcm, source_index, m.loop_start, m.loop_end, true, true, half_kernel));
        }
        p.generated_midpoints += length;
    }
    else p.full_attack = p.full_finite;
    const auto end = std::chrono::steady_clock::now();
    p.prepare_ms = std::chrono::duration<double, std::milli>(end - begin).count();
    return p;
}

enum class GridMode { Finite, AttackLoop, PeriodicLoop };

template<typename T>
static T grid_value(const Prepared &p, long long index, GridMode mode)
{
    const Meta &m = p.meta;
    if(mode == GridMode::PeriodicLoop && has_usable_loop(m))
    {
        const long long length = static_cast<long long>(2 * (m.loop_end - m.loop_start));
        long long q = (index - static_cast<long long>(2 * m.loop_start)) % length;
        if(q < 0) q += length;
        return static_cast<T>(p.periodic_loop[static_cast<size_t>(q)]);
    }
    if(mode == GridMode::AttackLoop && has_usable_loop(m))
    {
        if(index < 0) index = 0;
        const long long loop_begin = static_cast<long long>(2 * m.loop_start);
        const long long loop_end = static_cast<long long>(2 * m.loop_end);
        const long long length = loop_end - loop_begin;
        if(index >= loop_end)
        {
            long long q = (index - loop_begin) % length;
            if(q < 0) q += length;
            index = loop_begin + q;
        }
        index = std::min<long long>(index, static_cast<long long>(p.full_attack.size()) - 1);
        return static_cast<T>(p.full_attack[static_cast<size_t>(index)]);
    }
    index = std::max<long long>(0, std::min<long long>(static_cast<long long>(p.full_finite.size()) - 1, index));
    return static_cast<T>(p.full_finite[static_cast<size_t>(index)]);
}

template<typename T>
static T optimal_4(T ym1, T y0, T y1, T y2, double fraction)
{
    const T even1 = y1 + y0, odd1 = y1 - y0;
    const T even2 = y2 + ym1, odd2 = y2 - ym1;
    const T c0 = even1 * T(0.45645918406487612) + even2 * T(0.04354173901996461);
    const T c1 = odd1 * T(0.47236675362442071) + odd2 * T(0.17686613581136501);
    const T c2 = even1 * T(-0.253674794204558521) + even2 * T(0.25371918651882464);
    const T c3 = odd1 * T(-0.37917091811631082) + odd2 * T(0.11952965967158000);
    const T c4 = even1 * T(0.04252164479749607) + even2 * T(-0.04289144034653719);
    const T z = T(fraction - 0.5);
    return (((c4 * z + c3) * z + c2) * z + c1) * z + c0;
}

template<typename T>
static T optimal_6(T ym2, T ym1, T y0, T y1, T y2, T y3, double fraction)
{
    const T even1 = y1 + y0, odd1 = y1 - y0;
    const T even2 = y2 + ym1, odd2 = y2 - ym1;
    const T even3 = y3 + ym2, odd3 = y3 - ym2;
    const T c0 = even1 * T(0.37484203669443822) + even2 * T(0.11970939637439368)
                 + even3 * T(0.00544862268096358);
    const T c1 = odd1 * T(0.19253897284651597) + odd2 * T(0.22555179040018719)
                 + odd3 * T(0.02621377625620669);
    const T c2 = even1 * T(-0.154026006475653071) + even2 * T(0.10546111301131367)
                 + even3 * T(0.04856757454258609);
    const T c3 = odd1 * T(-0.06523685579716083) + odd2 * T(-0.04867197815057284)
                 + odd3 * T(0.04200764942718964);
    const T c4 = even1 * T(0.03134095684084392) + even2 * T(-0.04385804833432710)
                 + even3 * T(0.01249475765486819);
    const T z = T(fraction - 0.5);
    return (((c4 * z + c3) * z + c2) * z + c1) * z + c0;
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

template<typename T, int Points>
static std::vector<double> render_optimal(const Prepared &p, const Case &c, double step, size_t count)
{
    uint64_t phase = fixed_phase(c.initial_phase);
    const uint64_t increment = fixed_increment(step);
    const Meta &m = p.meta;
    std::vector<double> out(count);
    bool has_looped = c.has_looped;
    for(size_t f = 0; f < count; ++f)
    {
        const GridMode mode = c.looping ? (has_looped ? GridMode::PeriodicLoop : GridMode::AttackLoop)
                                        : GridMode::Finite;
        const uint64_t doubled = phase << 1;
        const long long base = static_cast<long long>(doubled >> 32);
        const double fraction = static_cast<double>(static_cast<uint32_t>(doubled)) / 4294967296.0;
        T y{};
        if constexpr(Points == 4)
            y = optimal_4(grid_value<T>(p, base - 1, mode), grid_value<T>(p, base, mode),
                          grid_value<T>(p, base + 1, mode), grid_value<T>(p, base + 2, mode), fraction);
        else
            y = optimal_6(grid_value<T>(p, base - 2, mode), grid_value<T>(p, base - 1, mode),
                          grid_value<T>(p, base, mode), grid_value<T>(p, base + 1, mode),
                          grid_value<T>(p, base + 2, mode), grid_value<T>(p, base + 3, mode), fraction);
        out[f] = static_cast<double>(y);
        phase += increment;
        if(c.looping && (phase >> 32) >= m.loop_end)
        {
            phase -= static_cast<uint64_t>(m.loop_end - m.loop_start) << 32;
            has_looped = true;
        }
    }
    return out;
}

struct Metrics { double rms_error{}, max_error{}, rms_reference{}; };
static Metrics errors(const std::vector<double> &x, const std::vector<double> &ref)
{
    if(x.size() != ref.size()) throw std::runtime_error("metric vector size mismatch");
    long double e2 = 0.0L, r2 = 0.0L;
    double mx = 0.0;
    for(size_t i = 0; i < x.size(); ++i)
    {
        const double e = x[i] - ref[i];
        e2 += static_cast<long double>(e) * e;
        r2 += static_cast<long double>(ref[i]) * ref[i];
        mx = std::max(mx, std::abs(e));
    }
    return {std::sqrt(static_cast<double>(e2 / x.size())), mx,
            std::sqrt(static_cast<double>(r2 / ref.size()))};
}
static double db20(double x) { return x > 0.0 ? 20.0 * std::log10(x) : -INFINITY; }

static double sinc(double x)
{
    return std::abs(x) < 1.0e-14 ? 1.0 : std::sin(kPi * x) / (kPi * x);
}

static std::vector<double> render_long_reference(const Prepared &p, const Case &c,
                                                 double step, size_t frames, int taps)
{
    const int half = taps / 2;
    const double cutoff = std::min(1.0, 1.0 / step);
    uint64_t phase = fixed_phase(c.initial_phase);
    const uint64_t increment = fixed_increment(step);
    bool has_looped = c.has_looped;
    std::vector<double> out(frames);
    for(size_t f = 0; f < frames; ++f)
    {
        const double pos = static_cast<double>(phase >> 32)
                           + static_cast<double>(static_cast<uint32_t>(phase)) / 4294967296.0;
        const long long center = static_cast<long long>(std::floor(pos));
        double sum = 0.0, norm = 0.0;
        for(int j = -half; j <= half; ++j)
        {
            const double t = static_cast<double>(j) - (pos - static_cast<double>(center));
            const double window = std::abs(t) <= half ? 0.5 * (1.0 + std::cos(kPi * t / half)) : 0.0;
            const double coeff = cutoff * sinc(cutoff * t) * window;
            sum += source_at(p.pcm, center + j, p.meta.loop_start, p.meta.loop_end,
                            c.looping, has_looped) * coeff;
            norm += coeff;
        }
        out[f] = sum / norm;
        phase += increment;
        if(c.looping && (phase >> 32) >= p.meta.loop_end)
        {
            phase -= static_cast<uint64_t>(p.meta.loop_end - p.meta.loop_start) << 32;
            has_looped = true;
        }
    }
    return out;
}

using SincTable = std::vector<std::array<double, 25>>;
static SincTable make_sinc25_table()
{
    constexpr int table_size = 4096;
    constexpr int half = 12;
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

static std::vector<double> render_sinc25(const Prepared &p, const SincTable &table,
                                         const Case &c, double step, size_t frames)
{
    constexpr uint32_t table_size = 4096;
    constexpr uint32_t low_mask = (uint32_t{1} << 20) - 1;
    constexpr int half = 12;
    uint64_t phase = fixed_phase(c.initial_phase);
    const uint64_t increment = fixed_increment(step);
    bool has_looped = c.has_looped;
    std::vector<double> out(frames);
    for(size_t f = 0; f < frames; ++f)
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
            y += coeff * source_at(p.pcm, center + i - half, p.meta.loop_start,
                                   p.meta.loop_end, c.looping, has_looped);
        }
        out[f] = y;
        phase += increment;
        if(c.looping && (phase >> 32) >= p.meta.loop_end)
        {
            phase -= static_cast<uint64_t>(p.meta.loop_end - p.meta.loop_start) << 32;
            has_looped = true;
        }
    }
    (void)table_size;
    return out;
}

static void write_main_quality(const std::string &out_path, const std::string &baseline_path,
                               const std::string &samples_csv,
                               const std::vector<Prepared> &prepared, const std::string &raw_dir,
                               const SincTable &sinc25)
{
    const auto metas = read_metadata(samples_csv);
    if(metas.size() != prepared.size()) throw std::runtime_error("metadata/prepared sample count mismatch");
    std::ofstream out(out_path);
    std::ofstream baseline(baseline_path);
    out << "sample,segment,step,transpose_factor,candidate,precision,frames,reference_rms_dbfs,error_rms_dbfs,error_max_dbfs,error_vs_sinc25_db,error_vs_cubic_db,reference_margin_db,quiet_reference,finite\n" << std::setprecision(12);
    baseline << "sample,segment,step,frames,sinc25_rms_diff,sinc25_max_diff,long_ref_rms_diff,long_ref_max_diff\n" << std::setprecision(12);
    for(size_t sample_index = 0; sample_index < metas.size(); ++sample_index)
    {
        const Meta &m = metas[sample_index];
        const Prepared &p = prepared[sample_index];
        for(double factor : {0.5, 1.0, 2.0})
        {
            const double step = m.base_step * factor;
            for(const Case &c : make_cases(m, step))
            {
                const std::string raw_path = raw_dir + "/" + m.label + "-" + c.name + "-r" + std::to_string(step) + ".csv";
                const auto raw = read_raw(raw_path);
                std::vector<double> ref(raw.size()), ref2047(raw.size()), sinc(raw.size()), cubic(raw.size());
                for(size_t i = 0; i < raw.size(); ++i)
                {
                    ref[i] = raw[i].ref4095;
                    ref2047[i] = raw[i].ref2047;
                    sinc[i] = raw[i].sinc25;
                    cubic[i] = raw[i].cubic;
                }
                const auto reproduced_sinc = render_sinc25(p, sinc25, c, step, raw.size());
                const auto reproduced_reference = render_long_reference(p, c, step, raw.size(), 4095);
                const Metrics sinc_reproduction = errors(reproduced_sinc, sinc);
                const Metrics reference_reproduction = errors(reproduced_reference, ref);
                baseline << m.label << ',' << c.name << ',' << step << ',' << raw.size() << ','
                         << db20(sinc_reproduction.rms_error) << ',' << db20(sinc_reproduction.max_error) << ','
                         << db20(reference_reproduction.rms_error) << ',' << db20(reference_reproduction.max_error) << '\n';
                const Metrics base_sinc = errors(sinc, ref), base_cubic = errors(cubic, ref);
                const Metrics convergence = errors(ref2047, ref);
                for(int points : {4, 6})
                {
                    for(int precision = 0; precision < 2; ++precision)
                    {
                        const std::vector<double> y = points == 4
                            ? (precision == 0 ? render_optimal<float, 4>(p, c, step, raw.size())
                                              : render_optimal<double, 4>(p, c, step, raw.size()))
                            : (precision == 0 ? render_optimal<float, 6>(p, c, step, raw.size())
                                              : render_optimal<double, 6>(p, c, step, raw.size()));
                        const Metrics e = errors(y, ref);
                        bool finite = true;
                        for(double v : y) finite = finite && std::isfinite(v);
                        out << m.label << ',' << c.name << ',' << step << ',' << factor << ',' << points << "pt," << (precision == 0 ? "float" : "double") << ',' << raw.size() << ','
                            << db20(e.rms_reference) << ',' << db20(e.rms_error) << ',' << db20(e.max_error) << ','
                            << db20(e.rms_error) - db20(base_sinc.rms_error) << ','
                            << db20(e.rms_error) - db20(base_cubic.rms_error) << ','
                            << db20(convergence.rms_error) - db20(e.rms_error) << ','
                            << (db20(e.rms_reference) < -60.0 ? 1 : 0) << ',' << (finite ? 1 : 0) << '\n';
                    }
                }
            }
        }
    }
}

static void write_loop_entry(const std::string &path, const std::string &samples_csv,
                             const std::vector<Prepared> &prepared,
                             const SincTable &sinc25)
{
    std::ofstream out(path);
    const auto metas = read_metadata(samples_csv);
    if(metas.size() != prepared.size()) throw std::runtime_error("metadata/prepared sample count mismatch");
    out << "sample,step,transpose_factor,candidate,precision,frames,error_rms_dbfs,error_max_dbfs,error_vs_sinc25_db,reference_margin_db,finite\n" << std::setprecision(12);
    for(size_t sample_index = 0; sample_index < metas.size(); ++sample_index)
    {
        const Meta &m = metas[sample_index];
        if(!has_usable_loop(m)) continue;
        const Prepared &p = prepared[sample_index];
        for(double factor : {0.5, 1.0, 2.0})
        {
            const double step = m.base_step * factor;
            Case c{"loop_entry", static_cast<double>(m.loop_end) - step * 64.0 + 0.271828, true, false};
            constexpr size_t count = 128;
            const auto ref = render_long_reference(p, c, step, count, 4095);
            const auto ref2047 = render_long_reference(p, c, step, count, 2047);
            const auto sinc = render_sinc25(p, sinc25, c, step, count);
            const Metrics base_sinc = errors(sinc, ref), convergence = errors(ref2047, ref);
            for(int points : {4, 6})
            {
                for(int precision = 0; precision < 2; ++precision)
                {
                    const auto y = points == 4
                        ? (precision == 0 ? render_optimal<float, 4>(p, c, step, count)
                                          : render_optimal<double, 4>(p, c, step, count))
                        : (precision == 0 ? render_optimal<float, 6>(p, c, step, count)
                                          : render_optimal<double, 6>(p, c, step, count));
                    const Metrics e = errors(y, ref);
                    bool finite = true;
                    for(double v : y) finite = finite && std::isfinite(v);
                    out << m.label << ',' << step << ',' << factor << ',' << points << "pt," << (precision == 0 ? "float" : "double") << ',' << count << ','
                        << db20(e.rms_error) << ',' << db20(e.max_error) << ','
                        << db20(e.rms_error) - db20(base_sinc.rms_error) << ','
                        << db20(convergence.rms_error) - db20(e.rms_error) << ',' << (finite ? 1 : 0) << '\n';
                }
            }
        }
    }
}

template<typename T, int Points>
static T eval_synthetic(const std::vector<float> &table, double q)
{
    const long long base = static_cast<long long>(std::floor(q));
    const double fraction = q - static_cast<double>(base);
    auto get = [&](long long i) { i = std::max<long long>(0, std::min<long long>(static_cast<long long>(table.size()) - 1, i)); return static_cast<T>(table[static_cast<size_t>(i)]); };
    if constexpr(Points == 4) return optimal_4(get(base - 1), get(base), get(base + 1), get(base + 2), fraction);
    else return optimal_6(get(base - 2), get(base - 1), get(base), get(base + 1), get(base + 2), get(base + 3), fraction);
}

static void write_static_response(const std::string &path)
{
    constexpr size_t source_frames = 65536;
    constexpr size_t output_frames = 32768;
    constexpr double step = 0.371239901;
    std::ofstream out(path);
    out << "frequency_cycles_per_original_sample,candidate,precision,reference_rms,gain_error_db,relative_error_db\n" << std::setprecision(12);
    for(double frequency : {0.0, 0.01, 0.05, 0.10, 0.20, 0.30, 0.40, 0.45, 0.49})
    {
        std::vector<float> table(2 * source_frames);
        for(size_t i = 0; i < source_frames; ++i)
        {
            const double t = static_cast<double>(i);
            const double x0 = frequency == 0.0 ? 1.0 : std::sin(2.0 * kPi * frequency * t);
            const double xh = frequency == 0.0 ? 1.0 : std::sin(2.0 * kPi * frequency * (t + 0.5));
            table[2 * i] = static_cast<float>(x0);
            table[2 * i + 1] = static_cast<float>(xh);
        }
        for(int points : {4, 6})
        {
            for(int precision = 0; precision < 2; ++precision)
            {
                long double y2 = 0.0L, r2 = 0.0L, e2 = 0.0L;
                for(size_t n = 0; n < output_frames; ++n)
                {
                    const double pos = 8192.0 + static_cast<double>(n) * step;
                    const double reference = frequency == 0.0 ? 1.0 : std::sin(2.0 * kPi * frequency * pos);
                    const double candidate = points == 4
                        ? (precision == 0 ? static_cast<double>(eval_synthetic<float, 4>(table, pos * 2.0))
                                          : static_cast<double>(eval_synthetic<double, 4>(table, pos * 2.0)))
                        : (precision == 0 ? static_cast<double>(eval_synthetic<float, 6>(table, pos * 2.0))
                                          : static_cast<double>(eval_synthetic<double, 6>(table, pos * 2.0)));
                    y2 += static_cast<long double>(candidate) * candidate;
                    r2 += static_cast<long double>(reference) * reference;
                    e2 += static_cast<long double>(candidate - reference) * (candidate - reference);
                }
                const double yrms = std::sqrt(static_cast<double>(y2 / output_frames));
                const double rrms = std::sqrt(static_cast<double>(r2 / output_frames));
                const double erms = std::sqrt(static_cast<double>(e2 / output_frames));
                out << frequency << ',' << points << "pt," << (precision == 0 ? "float" : "double") << ','
                    << rrms << ',' << db20(yrms / rrms) << ',' << db20(erms / rrms) << '\n';
            }
        }
    }
}

static void write_storage(const std::string &path, const std::string &samples_csv,
                          const std::vector<Prepared> &prepared)
{
    std::ofstream out(path);
    const auto metas = read_metadata(samples_csv);
    if(metas.size() != prepared.size()) throw std::runtime_error("metadata/prepared sample count mismatch");
    out << "sample,frames,loop_frames,original_int16_bytes,full_2x_float_bytes,attack_2x_float_bytes,periodic_loop_2x_float_bytes,screen_total_bytes,screen_ratio_vs_original,minimal_full_plus_loop_total_bytes,minimal_ratio_vs_original,prepare_ms,generated_half_points,attack_rebuilt_half_points\n" << std::setprecision(12);
    for(size_t sample_index = 0; sample_index < metas.size(); ++sample_index)
    {
        const Meta &m = metas[sample_index];
        const Prepared &p = prepared[sample_index];
        const uint64_t original = static_cast<uint64_t>(m.frames) * 2;
        const uint64_t full = static_cast<uint64_t>(2 * m.frames) * sizeof(float);
        const uint64_t attack = has_usable_loop(m) ? full : 0;
        const uint64_t loop = static_cast<uint64_t>(p.periodic_loop.size()) * sizeof(float);
        const uint64_t screen = original + full + attack + loop;
        const uint64_t minimal = original + full + loop;
        out << m.label << ',' << m.frames << ',' << (has_usable_loop(m) ? m.loop_end - m.loop_start : 0) << ','
            << original << ',' << full << ',' << attack << ',' << loop << ',' << screen << ','
            << static_cast<double>(screen) / static_cast<double>(original) << ',' << minimal << ','
            << static_cast<double>(minimal) / static_cast<double>(original) << ',' << p.prepare_ms << ','
            << p.generated_midpoints << ',' << p.attack_rebuilt_midpoints << '\n';
    }
}

} // namespace

int main(int argc, char **argv)
{
    if(argc != 2) { std::cerr << "usage: oversampled_c_screen <analysis-directory>\n"; return 2; }
    try
    {
        const std::string root = argv[1];
        const std::string samples_csv = "/root/code/fluidsynth-analysis-20261015/fractional-delay-ab/samples.csv";
        const std::string pcm_dir = "/root/code/fluidsynth-analysis-20261015/cubic-sinc25/sf2-samples";
        const std::string raw_dir = "/root/code/fluidsynth-analysis-20261015/cubic-sinc25/sf2-raw";
        const auto kernel = make_half_phase_kernel();
        const auto sinc25 = make_sinc25_table();
        std::vector<Prepared> prepared;
        for(const Meta &m : read_metadata(samples_csv))
        {
            Prepared p = prepare(m, pcm_dir, kernel);
            std::cout << "prepared " << m.label << " in " << p.prepare_ms << " ms\n";
            prepared.push_back(std::move(p));
        }
        write_main_quality(root + "/quality.csv", root + "/baseline-reproduction.csv", samples_csv,
                           prepared, raw_dir, sinc25);
        write_loop_entry(root + "/loop-entry.csv", samples_csv, prepared, sinc25);
        write_static_response(root + "/static.csv");
        write_storage(root + "/storage.csv", samples_csv, prepared);
        std::cout << "completed 2x precomputed 4/6-point SF2 and loop-boundary screen\n";
    }
    catch(const std::exception &e)
    {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
