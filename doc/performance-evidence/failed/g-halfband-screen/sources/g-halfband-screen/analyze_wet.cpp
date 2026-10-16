#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <vector>

constexpr double fs = 88200.0;
constexpr int window = 1764;
constexpr int max_lag = 88;
using Frame = std::array<double, 2>;

std::vector<Frame> read_pcm(const char *path)
{
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    auto bytes = in.tellg();
    if(bytes <= 0 || bytes % static_cast<std::streamoff>(sizeof(Frame)) != 0) return {};
    std::vector<Frame> data(static_cast<size_t>(bytes) / sizeof(Frame));
    in.seekg(0);
    in.read(reinterpret_cast<char *>(data.data()), bytes);
    return data;
}

double fit_decay(const std::vector<double> &edc, double high_db, double low_db)
{
    if(edc.empty() || edc[0] <= 0.0) return 0.0;
    double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;
    int count = 0;
    for(size_t b = 0; b < edc.size(); ++b)
    {
        if(edc[b] <= 0.0) continue;
        double db = 10.0 * std::log10(edc[b] / edc[0]);
        if(db <= high_db && db >= low_db)
        {
            double t = b * window / fs;
            sx += t; sy += db; sxx += t * t; sxy += t * db; ++count;
        }
    }
    double denom = count * sxx - sx * sx;
    if(count < 8 || denom == 0.0) return 0.0;
    double slope = (count * sxy - sx * sy) / denom;
    return slope < 0.0 ? -60.0 / slope : 0.0;
}

struct BandResult
{
    double energy = 0.0;
    double t20 = 0.0;
    double edt = 0.0;
};

BandResult analyze_band(const std::vector<Frame> &x, double center)
{
    constexpr double q = 4.318;
    double w = 2.0 * M_PI * center / fs;
    double alpha = std::sin(w) / (2.0 * q);
    double a0 = 1.0 + alpha;
    double b0 = alpha / a0, b1 = 0.0, b2 = -alpha / a0;
    double a1 = -2.0 * std::cos(w) / a0, a2 = (1.0 - alpha) / a0;
    std::array<double, 2> x1{}, x2{}, y1{}, y2{};
    size_t blocks = (x.size() + window - 1) / window;
    std::vector<double> energy(blocks, 0.0);
    for(size_t i = 0; i < x.size(); ++i)
    {
        double e = 0.0;
        for(int ch = 0; ch < 2; ++ch)
        {
            double input = x[i][ch];
            double output = b0 * input + b1 * x1[ch] + b2 * x2[ch] - a1 * y1[ch] - a2 * y2[ch];
            x2[ch] = x1[ch]; x1[ch] = input; y2[ch] = y1[ch]; y1[ch] = output;
            e += output * output;
        }
        energy[i / window] += e;
    }
    BandResult r;
    for(double e : energy) r.energy += e;
    for(size_t b = energy.size(); b-- > 0;)
        energy[b] += (b + 1 < energy.size()) ? energy[b + 1] : 0.0;
    r.t20 = fit_decay(energy, -5.0, -25.0);
    r.edt = fit_decay(energy, 0.0, -10.0);
    return r;
}

double iacc(const std::vector<Frame> &x, size_t start, size_t end)
{
    if(end <= start + 2 * max_lag) return 0.0;
    double best = 0.0;
    size_t lo = start + max_lag, hi = end - max_lag;
    for(int lag = -max_lag; lag <= max_lag; ++lag)
    {
        double lr = 0.0, el = 0.0, er = 0.0;
        for(size_t i = lo; i < hi; ++i)
        {
            double l = x[i][0], r = x[static_cast<size_t>(static_cast<long long>(i) + lag)][1];
            lr += l * r; el += l * l; er += r * r;
        }
        double c = (el > 0.0 && er > 0.0) ? lr / std::sqrt(el * er) : 0.0;
        best = std::max(best, std::abs(c));
    }
    return best;
}

double total_energy(const std::vector<Frame> &x)
{
    double e = 0.0;
    for(const auto &f : x) e += f[0] * f[0] + f[1] * f[1];
    return e;
}

int main(int argc, char **argv)
{
    if(argc != 3) return 2;
    auto f4 = read_pcm(argv[1]);
    auto f8 = read_pcm(argv[2]);
    if(f4.empty() || f8.empty() || f4.size() != f8.size()) return 3;
    double e4 = total_energy(f4), e8 = total_energy(f8);
    double norm_factor = std::sqrt(e8 / e4);
    std::printf("frames %zu total_energy_full %.12g total_energy_halfband %.12g raw_delta_db %.6f gain_to_match %.9f\n",
                f4.size(), e4, e8, 10.0 * std::log10(e4 / e8), norm_factor);
    double lr4 = 0.0, rr4 = 0.0, lr8 = 0.0, rr8 = 0.0;
    for(size_t i = 0; i < f4.size(); ++i)
    {
        lr4 += f4[i][0] * f4[i][0]; rr4 += f4[i][1] * f4[i][1];
        lr8 += f8[i][0] * f8[i][0]; rr8 += f8[i][1] * f8[i][1];
    }
    std::printf("left_right_ratio_db full %.6f halfband %.6f delta %.6f\n",
                10.0 * std::log10(lr4 / rr4), 10.0 * std::log10(lr8 / rr8),
                10.0 * std::log10((lr4 / rr4) / (lr8 / rr8)));
    for(double center = 31.5; center <= 40000.0; center *= std::pow(2.0, 1.0 / 3.0))
    {
        auto a = analyze_band(f4, center);
        auto b = analyze_band(f8, center);
        double raw = 10.0 * std::log10(a.energy / b.energy);
        double normalized = 10.0 * std::log10((a.energy / e4) / (b.energy / e8));
        double t20_pct = (a.t20 > 0.0 && b.t20 > 0.0) ? 100.0 * (a.t20 / b.t20 - 1.0) : 0.0;
        std::printf("band %.2fHz raw_level_delta %.3fdB normalized_color_delta %.3fdB t20_full %.3fs t20_halfband %.3fs t20_delta %.2f%% edt_full %.3fs edt_halfband %.3fs\n",
                    center, raw, normalized, a.t20, b.t20, t20_pct, a.edt, b.edt);
    }
    for(const auto &range : {std::array<double, 2>{0.0, 0.1}, {0.1, 0.5}, {0.5, 2.0}, {2.0, 8.0}, {8.0, 12.0}})
    {
        size_t s = static_cast<size_t>(range[0] * fs);
        size_t e = std::min(f4.size(), static_cast<size_t>(range[1] * fs));
        std::printf("iacc %.2f-%.2fs full %.5f halfband %.5f delta %.5f\n",
                    range[0], range[1], iacc(f4, s, e), iacc(f8, s, e),
                    iacc(f4, s, e) - iacc(f8, s, e));
    }
}
