#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <set>
#include <string>
#include <vector>

constexpr int N = 16384;
constexpr double PI = 3.141592653589793238462643383279502884;

static std::vector<double> read(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    std::vector<double> v(N);
    in.read(reinterpret_cast<char *>(v.data()), sizeof(double) * N);
    if(in.gcount() != sizeof(double) * N) throw std::runtime_error("short file: " + path);
    return v;
}

static std::vector<std::complex<double>> fft(const std::vector<double> &x)
{
    std::vector<std::complex<double>> a(N);
    for(int i = 0; i < N; ++i) a[i] = {x[i], 0.0};
    for(int i = 1, j = 0; i < N; ++i)
    {
        int bit = N >> 1;
        for(; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if(i < j) std::swap(a[i], a[j]);
    }
    for(int len = 2; len <= N; len <<= 1)
    {
        const double angle = -2.0 * PI / len;
        const std::complex<double> root{std::cos(angle), std::sin(angle)};
        for(int start = 0; start < N; start += len)
        {
            std::complex<double> w{1.0, 0.0};
            for(int j = 0; j < len / 2; ++j)
            {
                const auto even = a[start + j];
                const auto odd = w * a[start + j + len / 2];
                a[start + j] = even + odd;
                a[start + j + len / 2] = even - odd;
                w *= root;
            }
        }
    }
    for(auto &v : a) v /= static_cast<double>(N);
    return a;
}

static std::complex<double> coefficient(const std::string &kind, int h)
{
    if(kind == "sine") return h == 1 ? std::complex<double>{0.0, -0.5}
                                     : std::complex<double>{0.0, 0.0};
    if(kind == "saw") return {0.0, 1.0 / (PI * h)};
    const double theta = 2.0 * PI * h * 0.25;
    return {std::sin(theta) / (PI * h), -(1.0 - std::cos(theta)) / (PI * h)};
}

static double mse(const std::vector<double> &a, const std::vector<double> &b)
{
    long double sum = 0.0;
    for(int i = 0; i < N; ++i) { const double d = a[i] - b[i]; sum += d * d; }
    return static_cast<double>(sum / N);
}

static double max_abs(std::vector<double> x)
{
    for(double &v : x) v = std::fabs(v);
    return *std::max_element(x.begin(), x.end());
}

int main(int argc, char **argv)
{
    if(argc < 2 || argc > 3) return 2;
    const std::string dir = argv[1];
    const std::string suffix = argc == 3 ? std::string("-") + argv[2] : "";
    std::cout << "waveform,fundamental_bin,harmonics,aa_error_power,sinc_error_power,error_gain_db,aa_compensated_error_power,compensated_gain_db,optimal_advance_samples,aa_fundamental_amp_error_db,aa_fundamental_phase_deg,aa_legal_amp_p99_db,aa_legal_amp_max_db,sinc_legal_amp_p99_db,aa_spur_dbc,sinc_spur_dbc\n";
    for(const std::string kind : {"sine", "saw", "pulse"})
    for(int bin : {128, 512, 2048, 4096, 6144, 7000})
    {
        const std::string tag = kind + "-bin" + std::to_string(bin) + suffix;
        auto aa = read(dir + "/" + tag + "-aa.f64");
        auto sinc = read(dir + "/" + tag + "-sinc.f64");
        auto ref = read(dir + "/" + tag + "-reference.f64");
        const double aa_mse = mse(aa, ref);
        const double sinc_mse = mse(sinc, ref);
        const double gain_db = 10.0 * std::log10(sinc_mse / aa_mse);
        const auto aa_fft = fft(aa);
        const auto sinc_fft = fft(sinc);
        const auto ref_fft = fft(ref);
        int max_h = (N / 2 - 1) / bin;
        std::set<int> legal;
        legal.insert(0);
        double aa_max_dev = 0.0, sinc_max_dev = 0.0;
        std::vector<double> aa_devs, sinc_devs;
        for(int h = 1; h <= max_h; ++h)
        {
            const int k = h * bin;
            const double target = std::abs(coefficient(kind, h));
            if(target < 1e-10) continue;
            legal.insert(k);
            const double aa_db = 20.0 * std::log10(std::max(std::abs(aa_fft[k]), 1e-30) / target);
            const double sinc_db = 20.0 * std::log10(std::max(std::abs(sinc_fft[k]), 1e-30) / target);
            aa_devs.push_back(std::fabs(aa_db));
            sinc_devs.push_back(std::fabs(sinc_db));
            aa_max_dev = std::max(aa_max_dev, std::fabs(aa_db));
            sinc_max_dev = std::max(sinc_max_dev, std::fabs(sinc_db));
        }
        auto percentile99 = [](std::vector<double> v) {
            if(v.empty()) return 0.0;
            const size_t i = static_cast<size_t>(0.99 * (v.size() - 1));
            std::nth_element(v.begin(), v.begin() + i, v.end());
            return v[i];
        };
        std::vector<double> aa_spurs, sinc_spurs;
        for(int k = 1; k < N / 2; ++k)
            if(!legal.contains(k))
            {
                aa_spurs.push_back(std::abs(aa_fft[k]));
                sinc_spurs.push_back(std::abs(sinc_fft[k]));
            }
        const double aa_fund = std::max(std::abs(aa_fft[bin]), 1e-30);
        const double sinc_fund = std::max(std::abs(sinc_fft[bin]), 1e-30);
        const double aa_spur = 20.0 * std::log10(max_abs(aa_spurs) / aa_fund);
        const double sinc_spur = 20.0 * std::log10(max_abs(sinc_spurs) / sinc_fund);
        const auto target_fund = coefficient(kind, 1);
        const auto aa_ratio = aa_fft[bin] / target_fund;
        const double aa_fund_db = 20.0 * std::log10(std::max(std::abs(aa_ratio), 1e-30));
        const double aa_phase_deg = std::arg(aa_ratio) * 180.0 / PI;
        double aa_power = 0.0, ref_power = 0.0;
        for(int k = 0; k <= N / 2; ++k)
        {
            const double weight = (k == 0 || k == N / 2) ? 1.0 : 2.0;
            aa_power += weight * std::norm(aa_fft[k]);
            ref_power += weight * std::norm(ref_fft[k]);
        }
        double best_error = std::numeric_limits<double>::infinity();
        double best_advance = 0.0;
        for(int step = -4000; step <= 4000; ++step)
        {
            const double advance = step / 100.0;
            std::complex<double> cross = aa_fft[0] * std::conj(ref_fft[0]);
            for(int k : legal)
            {
                if(k == 0) continue;
                const double angle = 2.0 * PI * k * advance / N;
                cross += 2.0 * aa_fft[k] * std::conj(ref_fft[k]) *
                         std::complex<double>{std::cos(angle), std::sin(angle)};
            }
            const double error = std::max(0.0, aa_power + ref_power - 2.0 * cross.real());
            if(error < best_error) { best_error = error; best_advance = advance; }
        }
        best_error = std::max(0.0, best_error);
        std::cout << kind << ',' << bin << ',' << max_h << ',' << std::setprecision(10)
                  << aa_mse << ',' << sinc_mse << ',' << gain_db << ','
                  << best_error << ',' << 10.0 * std::log10(sinc_mse / std::max(best_error, 1e-300)) << ',' << best_advance << ','
                  << aa_fund_db << ',' << aa_phase_deg << ','
                  << percentile99(aa_devs) << ',' << aa_max_dev << ','
                  << percentile99(sinc_devs) << ',' << aa_spur << ',' << sinc_spur << '\n';
    }
}
