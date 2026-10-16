#define main compensated_c_quality_screen_main
#include "compensated_c_sf2_screen.cpp"
#undef main

#include <random>
#include <type_traits>

namespace {

using namespace std::chrono;

static std::vector<int16_t> read_pcm16_integer(const std::string &path, size_t frames)
{
    std::ifstream in(path, std::ios::binary);
    if(!in) throw std::runtime_error("cannot open " + path);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)), {});
    if(bytes.size() != frames * 2) throw std::runtime_error("bad PCM size: " + path);
    std::vector<int16_t> out(frames);
    for(size_t i = 0; i < frames; ++i)
    {
        const uint16_t u = static_cast<uint16_t>(bytes[2 * i])
                           | (static_cast<uint16_t>(bytes[2 * i + 1]) << 8);
        out[i] = static_cast<int16_t>(u);
    }
    return out;
}

template<typename T>
using SincTableT = std::vector<std::array<T, 25>>;

template<typename T>
using CubicTableT = std::vector<std::array<T, 4>>;

template<typename T>
static CubicTableT<T> make_production_cubic_table()
{
    CubicTableT<T> out(256);
    for(size_t row = 0; row < out.size(); ++row)
    {
        const T x = static_cast<T>(row) / T(256);
        out[row] = {{
            x * (T(-0.5) + x * (T(1) - T(0.5) * x)),
            T(1) + x * x * (T(1.5) * x - T(2.5)),
            x * (T(0.5) + x * (T(2) - T(1.5) * x)),
            T(0.5) * x * x * (x - T(1)),
        }};
    }
    return out;
}

template<typename T>
static SincTableT<T> convert_sinc25_table(const SincTable &source)
{
    SincTableT<T> out(source.size());
    for(size_t row = 0; row < source.size(); ++row)
        for(size_t tap = 0; tap < source[row].size(); ++tap)
            out[row][tap] = static_cast<T>(source[row][tap]);
    return out;
}

template<typename T>
static std::vector<double> render_sinc25_interior(const std::vector<int16_t> &pcm,
                                                  const SincTableT<T> &table,
                                                  const Case &c, double step,
                                                  size_t frames)
{
    constexpr uint32_t low_mask = (uint32_t{1} << 20) - 1;
    constexpr int half = 12;
    uint64_t phase = fixed_phase(c.initial_phase);
    const uint64_t increment = fixed_increment(step);
    std::vector<double> out(frames);
    for(size_t f = 0; f < frames; ++f)
    {
        const uint64_t shifted = phase + kPhaseMid;
        const long long center = static_cast<long long>(shifted >> 32);
        const uint32_t frac = static_cast<uint32_t>(shifted);
        const uint32_t row = frac >> 20;
        const T alpha = static_cast<T>(frac & low_mask) / T(1048576.0);
        T y = T(0);
        for(int i = 0; i < 25; ++i)
        {
            const T a = table[row][static_cast<size_t>(i)];
            const T b = table[row + 1][static_cast<size_t>(i)];
            const T coeff = a + (b - a) * alpha;
            const T sample = static_cast<T>(pcm[static_cast<size_t>(center + i - half)])
                             * T(1.0 / 32768.0);
            y += coeff * sample;
        }
        out[f] = static_cast<double>(y);
        phase += increment;
    }
    return out;
}

template<typename T>
static std::vector<double> render_production_cubic_interior(const std::vector<int16_t> &pcm,
                                                            const CubicTableT<T> &table,
                                                            const Case &c, double step,
                                                            size_t frames)
{
    uint64_t phase = fixed_phase(c.initial_phase);
    const uint64_t increment = fixed_increment(step);
    std::vector<double> out(frames);
    for(size_t f = 0; f < frames; ++f)
    {
        const size_t center = static_cast<size_t>(phase >> 32);
        const size_t row = static_cast<uint32_t>(phase) >> 24;
        const auto &coeff = table[row];
        const T s0 = static_cast<T>(pcm[center - 1]) / T(32768);
        const T s1 = static_cast<T>(pcm[center]) / T(32768);
        const T s2 = static_cast<T>(pcm[center + 1]) / T(32768);
        const T s3 = static_cast<T>(pcm[center + 2]) / T(32768);
        const T y = coeff[0] * s0 + coeff[1] * s1 + coeff[2] * s2 + coeff[3] * s3;
        out[f] = static_cast<double>(y);
        phase += increment;
    }
    return out;
}

template<typename T>
static void write_static_kernel_response(std::ofstream &out,
                                         const CubicTableT<T> &cubic_table,
                                         const SincTableT<T> &sinc_table)
{
    constexpr size_t source_frames = 65536;
    constexpr size_t output_frames = 32768;
    constexpr double step = 0.371239901;
    constexpr std::array<double, 9> frequencies{{0.0, 0.01, 0.05, 0.10, 0.20, 0.30, 0.40, 0.45, 0.49}};
    for(double frequency : frequencies)
    {
        std::vector<double> source(source_frames);
        for(size_t i = 0; i < source_frames; ++i)
            source[i] = frequency == 0.0 ? 1.0 : std::sin(2.0 * kPi * frequency * static_cast<double>(i));
        long double cubic_y2 = 0.0L, sinc_y2 = 0.0L, ref2 = 0.0L, cubic_e2 = 0.0L, sinc_e2 = 0.0L;
        for(size_t n = 0; n < output_frames; ++n)
        {
            const double pos = 8192.0 + static_cast<double>(n) * step;
            const double reference = frequency == 0.0 ? 1.0 : std::sin(2.0 * kPi * frequency * pos);
            const long long base = static_cast<long long>(std::floor(pos));
            const double fraction = pos - static_cast<double>(base);
            const size_t cubic_row = std::min<size_t>(255, static_cast<size_t>(fraction * 256.0));
            const auto &cw = cubic_table[cubic_row];
            const auto get = [&](long long i) { return static_cast<T>(source[static_cast<size_t>(i)]); };
            const T cubic = cw[0] * get(base - 1) + cw[1] * get(base)
                + cw[2] * get(base + 1) + cw[3] * get(base + 2);

            const double shifted = pos + 0.5;
            const long long center = static_cast<long long>(std::floor(shifted));
            const double shifted_fraction = shifted - static_cast<double>(center);
            const double row_coordinate = shifted_fraction * 4096.0;
            const size_t row = std::min<size_t>(4095, static_cast<size_t>(row_coordinate));
            const T alpha = static_cast<T>(row_coordinate - static_cast<double>(row));
            T sinc{};
            for(size_t i = 0; i < 25; ++i)
            {
                const T a = sinc_table[row][i], b = sinc_table[row + 1][i];
                const T weight = a + (b - a) * alpha;
                sinc += weight * get(center + static_cast<long long>(i) - 12);
            }
            const double yc = static_cast<double>(cubic), ys = static_cast<double>(sinc);
            cubic_y2 += yc * yc;
            sinc_y2 += ys * ys;
            ref2 += reference * reference;
            cubic_e2 += (yc - reference) * (yc - reference);
            sinc_e2 += (ys - reference) * (ys - reference);
        }
        const double count = static_cast<double>(output_frames);
        const double ref_rms = std::sqrt(static_cast<double>(ref2 / count));
        const double cubic_rms = std::sqrt(static_cast<double>(cubic_y2 / count));
        const double sinc_rms = std::sqrt(static_cast<double>(sinc_y2 / count));
        const double cubic_err = std::sqrt(static_cast<double>(cubic_e2 / count));
        const double sinc_err = std::sqrt(static_cast<double>(sinc_e2 / count));
        const std::string precision = std::is_same<T, float>::value ? "float" : "double";
        out << frequency << ",production-cubic-256row," << precision << ',' << ref_rms << ','
            << 20.0 * std::log10(cubic_rms / ref_rms) << ',' << 20.0 * std::log10(cubic_err / ref_rms) << '\n';
        out << frequency << ",lut-sinc25-4096row," << precision << ',' << ref_rms << ','
            << 20.0 * std::log10(sinc_rms / ref_rms) << ',' << 20.0 * std::log10(sinc_err / ref_rms) << '\n';
    }
}

template<typename T>
static std::vector<double> render_candidate_interior(const std::vector<float> &table,
                                                     size_t loop_start,
                                                     bool periodic_loop,
                                                     const Case &c, double step,
                                                     size_t frames)
{
    uint64_t phase = fixed_phase(c.initial_phase);
    const uint64_t increment = fixed_increment(step);
    std::vector<double> out(frames);
    for(size_t f = 0; f < frames; ++f)
    {
        const uint64_t doubled = phase << 1;
        const long long absolute_base = static_cast<long long>(doubled >> 32);
        const long long base = periodic_loop
            ? absolute_base - static_cast<long long>(2 * loop_start)
            : absolute_base;
        const double fraction = static_cast<double>(static_cast<uint32_t>(doubled)) / 4294967296.0;
        const auto get = [&](long long i) { return static_cast<T>(table[static_cast<size_t>(i)]); };
        const T y = optimal_6(get(base - 2), get(base - 1), get(base), get(base + 1),
                              get(base + 2), get(base + 3), fraction);
        out[f] = static_cast<double>(y);
        phase += increment;
    }
    return out;
}

static double checksum(const std::vector<double> &values)
{
    long double sum = 0.0L;
    for(size_t i = 0; i < values.size(); ++i)
        sum += static_cast<long double>(values[i]) * static_cast<long double>(1 + (i % 17));
    return static_cast<double>(sum);
}

struct TimedResult { int64_t ns{}; double check{}; };

template<typename F>
static TimedResult time_render(F &&render)
{
    const auto begin = steady_clock::now();
    const std::vector<double> values = render();
    const auto end = steady_clock::now();
    return {duration_cast<nanoseconds>(end - begin).count(), checksum(values)};
}

static double max_difference(const std::vector<double> &a, const std::vector<double> &b)
{
    if(a.size() != b.size()) throw std::runtime_error("parity vector size mismatch");
    double result = 0.0;
    for(size_t i = 0; i < a.size(); ++i) result = std::max(result, std::abs(a[i] - b[i]));
    return result;
}

template<typename T>
static void benchmark_threeway(std::ofstream &out, const Prepared &p,
                               const std::vector<int16_t> &pcm,
                               const SincTable &baseline_reference_table,
                               const SincTableT<T> &sinc_table,
                               const CubicTableT<T> &cubic_table,
                               const Case &c, const std::string &segment, double factor,
                               size_t frames, const std::string &precision,
                               int pairs, std::mt19937 &rng)
{
    const double step = p.meta.base_step * factor;
    const bool periodic = has_usable_loop(p.meta);
    const auto &candidate_table = periodic ? p.periodic_loop : p.full_finite;
    const double last_position = c.initial_phase + step * static_cast<double>(frames - 1);
    const double safe_end = periodic ? static_cast<double>(p.meta.loop_end)
                                     : static_cast<double>(p.meta.frames);
    if(last_position + 14.0 >= safe_end)
        throw std::runtime_error("three-way window approaches sample boundary: " + p.meta.label);

    auto cubic = [&] { return render_production_cubic_interior<T>(pcm, cubic_table, c, step, frames); };
    auto sixpoint = [&] { return render_candidate_interior<T>(candidate_table, p.meta.loop_start,
                                                               periodic, c, step, frames); };
    auto sinc = [&] { return render_sinc25_interior<T>(pcm, sinc_table, c, step, frames); };
    const auto warm_cubic = cubic();
    const auto warm_sixpoint = sixpoint();
    const auto warm_sinc = sinc();
    const auto model_sixpoint = render_optimal<T, 6>(p, c, step, frames);
    if(max_difference(warm_sixpoint, model_sixpoint) > (std::is_same<T, float>::value ? 1.0e-7 : 1.0e-12))
        throw std::runtime_error("six-point three-way model mismatch: " + p.meta.label);
    if(std::is_same<T, double>::value)
    {
        const auto model_sinc = render_sinc25(p, baseline_reference_table, c, step, frames);
        if(max_difference(warm_sinc, model_sinc) > 1.0e-12)
            throw std::runtime_error("sinc25 three-way model mismatch: " + p.meta.label);
    }

    // The production cubic table is truncated to 8 fractional bits. Verify against
    // the saved renderer output on the matching 882-frame sustain window.
    const std::string raw_path = "/root/code/fluidsynth-analysis-20261015/cubic-sinc25/sf2-raw/"
        + p.meta.label + "-sustain-r" + std::to_string(step) + ".csv";
    if(segment == "sustain")
    {
        const auto saved = read_raw(raw_path);
        const size_t checked = std::min(saved.size(), warm_cubic.size());
        double max_saved_diff = 0.0;
        for(size_t i = 0; i < checked; ++i)
            max_saved_diff = std::max(max_saved_diff, std::abs(warm_cubic[i] - saved[i].cubic));
        if(checked == 0 || max_saved_diff > 1.1e-6)
            throw std::runtime_error("production cubic table failed saved-output parity: " + raw_path
                                     + " diff=" + std::to_string(max_saved_diff));
    }

    std::vector<std::array<int, 3>> orders(static_cast<size_t>(pairs));
    for(auto &order : orders) order = {{0, 1, 2}};
    for(size_t pair = 0; pair < orders.size(); ++pair)
        std::shuffle(orders[pair].begin(), orders[pair].end(), rng);
    for(int pair = 0; pair < pairs; ++pair)
    {
        std::array<TimedResult, 3> times{};
        for(int arm : orders[static_cast<size_t>(pair)])
            times[static_cast<size_t>(arm)] = arm == 0 ? time_render(cubic)
                : arm == 1 ? time_render(sixpoint) : time_render(sinc);
        out << p.meta.label << ',' << segment << ',' << factor << ',' << precision << ',' << pair << ','
            << orders[static_cast<size_t>(pair)][0] << orders[static_cast<size_t>(pair)][1]
            << orders[static_cast<size_t>(pair)][2] << ','
            << times[0].ns << ',' << times[1].ns << ',' << times[2].ns << ','
            << std::setprecision(17) << times[0].check << ',' << times[1].check << ',' << times[2].check << ','
            << checksum(warm_cubic) << ',' << checksum(warm_sixpoint) << ',' << checksum(warm_sinc) << '\n';
    }
}

template<typename T>
static void benchmark_case(std::ofstream &out, const Prepared &p,
                           const std::vector<int16_t> &pcm,
                           const SincTable &baseline_reference_table,
                           const SincTableT<T> &table, const Case &c,
                           const std::string &segment, double factor,
                           size_t frames, const std::string &precision,
                           int pairs, std::mt19937 &rng)
{
    const double step = p.meta.base_step * factor;
    const bool periodic = has_usable_loop(p.meta);
    const auto &candidate_table = periodic ? p.periodic_loop : p.full_finite;
    const double last_position = c.initial_phase + step * static_cast<double>(frames - 1);
    const double safe_end = periodic ? static_cast<double>(p.meta.loop_end)
                                     : static_cast<double>(p.meta.frames);
    if(last_position + 14.0 >= safe_end)
        throw std::runtime_error("interior benchmark window approaches a boundary: " + p.meta.label);
    auto base = [&] { return render_sinc25_interior<T>(pcm, table, c, step, frames); };
    auto candidate = [&] { return render_candidate_interior<T>(candidate_table, p.meta.loop_start,
                                                               periodic, c, step, frames); };
    const auto warm_base = base();
    const auto warm_candidate = candidate();
    const auto reference_candidate = render_optimal<T, 6>(p, c, step, frames);
    const double candidate_model_diff = max_difference(warm_candidate, reference_candidate);
    if(candidate_model_diff > (std::is_same<T, float>::value ? 1.0e-7 : 1.0e-12))
        throw std::runtime_error("interior benchmark model mismatch: " + p.meta.label);
    if(std::is_same<T, double>::value)
    {
        const auto reference_base = render_sinc25(p, baseline_reference_table, c, step, frames);
        if(max_difference(warm_base, reference_base) > 1.0e-12)
            throw std::runtime_error("sinc25 interior benchmark differs from the validated screen path: " + p.meta.label);
    }
    const double base_check = checksum(warm_base);
    const double candidate_check = checksum(warm_candidate);

    std::vector<int> order(static_cast<size_t>(pairs));
    for(int i = 0; i < pairs; ++i) order[static_cast<size_t>(i)] = i & 1;
    std::shuffle(order.begin(), order.end(), rng);
    for(int pair = 0; pair < pairs; ++pair)
    {
        TimedResult b, cnd;
        if(order[static_cast<size_t>(pair)] == 0)
        {
            b = time_render(base);
            cnd = time_render(candidate);
        }
        else
        {
            cnd = time_render(candidate);
            b = time_render(base);
        }
        out << p.meta.label << ',' << segment << ',' << factor << ',' << precision << ','
            << pair << ',' << (order[static_cast<size_t>(pair)] == 0 ? "baseline-first" : "candidate-first")
            << ',' << b.ns << ',' << cnd.ns << ',' << std::setprecision(17)
            << b.check << ',' << cnd.check << ',' << base_check << ',' << candidate_check << '\n';
    }
}

template<typename T>
static void benchmark_aa(std::ofstream &out, const Prepared &p,
                         const std::vector<int16_t> &pcm,
                         const SincTableT<T> &table, const Case &c,
                         double factor, size_t frames, int pairs,
                         std::mt19937 &rng)
{
    const double step = p.meta.base_step * factor;
    auto base = [&] { return render_sinc25_interior<T>(pcm, table, c, step, frames); };
    (void)base();
    std::vector<int> order(static_cast<size_t>(pairs));
    for(int i = 0; i < pairs; ++i) order[static_cast<size_t>(i)] = i & 1;
    std::shuffle(order.begin(), order.end(), rng);
    for(int pair = 0; pair < pairs; ++pair)
    {
        TimedResult first, second;
        if(order[static_cast<size_t>(pair)] == 0)
        {
            first = time_render(base);
            second = time_render(base);
        }
        else
        {
            second = time_render(base);
            first = time_render(base);
        }
        out << p.meta.label << ",sustain-aa," << factor << ','
            << (std::is_same<T, float>::value ? "float-aa" : "double-aa") << ','
            << pair << ',' << (order[static_cast<size_t>(pair)] == 0 ? "first-first" : "second-first")
            << ',' << first.ns << ',' << second.ns << ',' << std::setprecision(17)
            << first.check << ',' << second.check << ',' << first.check << ',' << second.check << '\n';
    }
}

} // namespace

int main(int argc, char **argv)
{
    if(argc != 3)
    {
        std::cerr << "usage: compensated_c_perf_screen <data-root> <output-dir>\n";
        return 2;
    }
    try
    {
        const std::string data_root = argv[1];
        const std::string output_dir = argv[2];
        const std::string samples_csv = data_root + "/fractional-delay-ab/samples.csv";
        const std::string pcm_dir = data_root + "/cubic-sinc25/sf2-samples";
        const std::vector<float> prefilter = read_prefilter("prefilter_coefficients.csv", 6);
        const auto kernel = make_half_phase_kernel();
        const auto table_begin = steady_clock::now();
        const auto sinc25 = make_sinc25_table();
        const auto float_table = convert_sinc25_table<float>(sinc25);
        const auto double_table = convert_sinc25_table<double>(sinc25);
        const auto cubic_float_table = make_production_cubic_table<float>();
        const auto cubic_double_table = make_production_cubic_table<double>();
        const auto table_end = steady_clock::now();
        const double sinc_table_ms = duration<double, std::milli>(table_end - table_begin).count();

        std::ofstream out(output_dir + "/perf-raw.csv");
        std::ofstream threeway(output_dir + "/threeway-raw.csv");
        std::ofstream static_kernel(output_dir + "/static-threeway-kernel.csv");
        static_kernel << "frequency_cycles_per_original_sample,candidate,precision,reference_rms,gain_error_db,relative_error_db\n";
        write_static_kernel_response<float>(static_kernel, cubic_float_table, float_table);
        write_static_kernel_response<double>(static_kernel, cubic_double_table, double_table);
        static_kernel.close();
        std::ofstream setup(output_dir + "/setup.csv");
        out << "sample,segment,factor,precision,pair,order,baseline_ns,candidate_ns,baseline_checksum,candidate_checksum,baseline_warm_checksum,candidate_warm_checksum\n";
        threeway << "sample,segment,factor,precision,pair,order,cubic_ns,sixpoint_ns,sinc25_ns,cubic_checksum,sixpoint_checksum,sinc25_checksum,cubic_warm_checksum,sixpoint_warm_checksum,sinc25_warm_checksum\n";
        setup << "sample,frames,loop_frames,base_load_ms,candidate_prepare_ms,original_pcm_bytes,candidate_unique_table_bytes,candidate_prototype_allocated_table_bytes,combined_storage_bytes,combined_storage_ratio,generated_midpoints,attack_rebuilt_midpoints\n"
              << std::setprecision(12);
        std::cout << "sinc25_shared_table_ms=" << std::setprecision(9) << sinc_table_ms << '\n';

        std::mt19937 rng(20261016);
        constexpr int pairs = 30;
        constexpr size_t frames = 8192;
        const std::vector<Meta> metas = read_metadata(samples_csv);
        for(const std::string label : {"piano-mid", "strings-mid", "cymbal"})
        {
            const auto it = std::find_if(metas.begin(), metas.end(), [&](const Meta &m) { return m.label == label; });
            if(it == metas.end()) throw std::runtime_error("missing representative sample: " + label);

            const auto load_begin = steady_clock::now();
            const auto pcm = read_pcm16_integer(pcm_dir + "/" + it->raw_file, it->frames);
            const auto load_end = steady_clock::now();
            const double load_ms = duration<double, std::milli>(load_end - load_begin).count();

            Prepared p = prepare(*it, pcm_dir, kernel, prefilter);
            const size_t full_bytes = p.full_finite.size() * sizeof(float);
            const size_t attack_bytes = has_usable_loop(*it) ? p.full_attack.size() * sizeof(float) : 0;
            const size_t loop_bytes = p.periodic_loop.size() * sizeof(float);
            const size_t unique_table_bytes = full_bytes + attack_bytes + loop_bytes;
            const size_t allocated_table_bytes = (p.full_finite.size() + p.full_attack.size()
                                                  + p.periodic_loop.size()) * sizeof(float);
            const size_t original_bytes = it->frames * sizeof(int16_t);
            setup << it->label << ',' << it->frames << ','
                  << (has_usable_loop(*it) ? it->loop_end - it->loop_start : 0) << ','
                  << load_ms << ',' << p.prepare_ms << ',' << original_bytes << ',' << unique_table_bytes << ','
                  << allocated_table_bytes << ',' << (original_bytes + unique_table_bytes) << ','
                  << static_cast<double>(original_bytes + unique_table_bytes) / static_cast<double>(original_bytes) << ','
                  << p.generated_midpoints << ',' << p.attack_rebuilt_midpoints << '\n';
            std::cout << "prepared " << it->label << " load_ms=" << load_ms
                      << " candidate_ms=" << p.prepare_ms << " unique_table_bytes=" << unique_table_bytes
                      << " prototype_allocated_table_bytes=" << allocated_table_bytes << '\n';

            std::vector<Case> cases;
            if(has_usable_loop(*it))
            {
                cases.push_back({"sustain", static_cast<double>(it->loop_start) + 1024.271828, true, true});
                for(double factor : {0.5, 1.0})
                {
                    benchmark_case<float>(out, p, pcm, sinc25, float_table, cases.back(), "sustain", factor,
                                          frames, "float", pairs, rng);
                    benchmark_threeway<float>(threeway, p, pcm, sinc25, float_table, cubic_float_table,
                                              cases.back(), "sustain", factor, frames, "float", pairs, rng);
                    benchmark_case<double>(out, p, pcm, sinc25, double_table, cases.back(), "sustain", factor,
                                          frames, "double", pairs, rng);
                    benchmark_threeway<double>(threeway, p, pcm, sinc25, double_table, cubic_double_table,
                                               cases.back(), "sustain", factor, frames, "double", pairs, rng);
                }
                if(it->label == "strings-mid")
                {
                    benchmark_aa<float>(out, p, pcm, float_table, cases.back(), 0.5, frames, pairs, rng);
                    benchmark_aa<double>(out, p, pcm, double_table, cases.back(), 0.5, frames, pairs, rng);
                }
            }
            else
            {
                const Case c{"interior", 1024.271828, false, false};
                benchmark_case<float>(out, p, pcm, sinc25, float_table, c, "interior", 1.0,
                                      frames, "float", pairs, rng);
                benchmark_threeway<float>(threeway, p, pcm, sinc25, float_table, cubic_float_table,
                                          c, "interior", 1.0, frames, "float", pairs, rng);
                benchmark_case<double>(out, p, pcm, sinc25, double_table, c, "interior", 1.0,
                                      frames, "double", pairs, rng);
                benchmark_threeway<double>(threeway, p, pcm, sinc25, double_table, cubic_double_table,
                                           c, "interior", 1.0, frames, "double", pairs, rng);
            }
        }
        out.close();
        threeway.close();
        setup.close();
        std::cout << "completed pairs=" << pairs << " frames_per_render=" << frames
                  << " sinc25_shared_table_ms=" << sinc_table_ms << '\n';
    }
    catch(const std::exception &e)
    {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
