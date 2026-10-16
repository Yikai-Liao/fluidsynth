#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <utility>
#include <vector>

namespace {
constexpr int N = 8;
constexpr int BINS = 2048;
constexpr double PI = 3.1415926535897932384626433832795;
constexpr std::array<int, N> nominal_delay{601, 691, 773, 839, 919, 997, 1061, 1129};

int sign_at(unsigned mask, int index)
{
    return (mask & (1u << index)) ? 1 : -1;
}

int sign_dot(unsigned left, unsigned right)
{
    int dot = 0;
    for(int i = 0; i < N; ++i) dot += sign_at(left, i) * sign_at(right, i);
    return dot;
}

bool solve(std::array<std::array<std::complex<double>, N>, N> matrix,
           std::array<std::complex<double>, N> rhs,
           std::array<std::complex<double>, N>& result)
{
    for(int col = 0; col < N; ++col)
    {
        int pivot = col;
        for(int row = col + 1; row < N; ++row)
            if(std::abs(matrix[row][col]) > std::abs(matrix[pivot][col])) pivot = row;
        if(std::abs(matrix[pivot][col]) < 1e-20) return false;
        std::swap(matrix[pivot], matrix[col]);
        std::swap(rhs[pivot], rhs[col]);
        const auto diagonal = matrix[col][col];
        for(int j = col; j < N; ++j) matrix[col][j] /= diagonal;
        rhs[col] /= diagonal;
        for(int row = 0; row < N; ++row)
        {
            if(row == col) continue;
            const auto factor = matrix[row][col];
            for(int j = col; j < N; ++j) matrix[row][j] -= factor * matrix[col][j];
            rhs[row] -= factor * rhs[col];
        }
    }
    result = rhs;
    return true;
}

struct Pair
{
    unsigned left;
    unsigned right;
    size_t left_index;
    size_t right_index;
};

struct Metrics
{
    double mean_power_db;
    double flatness_db;
    double ripple_std_db;
    double p99_excess_db;
    double peak_excess_db;
    double lr_energy_db;
};

Metrics measure_pair(const std::vector<double>& powers, size_t offset,
                     double left_energy, double right_energy)
{
    constexpr int count = BINS - 1;
    long double sum_power = 0.0;
    double peak_power = 0.0;
    std::vector<double> ordered;
    ordered.reserve(count);
    for(int bin = 0; bin < count; ++bin)
    {
        const double power = powers[offset + bin];
        ordered.push_back(power);
        sum_power += power;
        peak_power = std::max(peak_power, power);
    }
    const double mean_power = static_cast<double>(sum_power / count);
    const double floor = std::max(mean_power * 1e-20, 1e-300);
    long double sum_log_power = 0.0;
    long double mean_db = 0.0;
    for(int bin = 0; bin < count; ++bin)
    {
        const double power = std::max(powers[offset + bin], floor);
        sum_log_power += std::log(power);
        mean_db += 10.0 * std::log10(power);
    }
    mean_db /= count;
    long double variance_db = 0.0;
    for(int bin = 0; bin < count; ++bin)
    {
        const double db = 10.0 * std::log10(std::max(powers[offset + bin], floor));
        const double delta = db - static_cast<double>(mean_db);
        variance_db += delta * delta;
    }
    const size_t p99_index = static_cast<size_t>(0.99 * (ordered.size() - 1));
    std::nth_element(ordered.begin(), ordered.begin() + p99_index, ordered.end());
    return {
        10.0 * std::log10(mean_power),
        10.0 * std::log10(std::exp(static_cast<double>(sum_log_power / count)) / mean_power),
        std::sqrt(static_cast<double>(variance_db / count)),
        10.0 * std::log10(std::max(ordered[p99_index], floor) / mean_power),
        10.0 * std::log10(std::max(peak_power, floor) / mean_power),
        10.0 * std::log10(left_energy / right_energy)
    };
}

void run(unsigned u_mask, double t60, const std::vector<unsigned>& outputs,
         const std::vector<Pair>& pairs)
{
    std::array<double, N> delay{};
    std::array<double, N> gain{};
    std::array<int, N> u{};
    std::array<int, N> pu{};
    for(int i = 0; i < N; ++i)
    {
        delay[i] = 2.0 * nominal_delay[i] + 5.0;
        gain[i] = std::pow(10.0, -3.0 * delay[i] / (44100.0 * t60));
        u[i] = sign_at(u_mask, i);
        pu[i] = u[(i + 1) % N];
    }

    std::vector<double> powers(pairs.size() * (BINS - 1));
    std::vector<double> left_energy(pairs.size(), 0.0);
    std::vector<double> right_energy(pairs.size(), 0.0);
    std::array<std::complex<double>, N> state{};
    std::array<std::complex<double>, 35> output{};

    for(int bin = 1; bin < BINS; ++bin)
    {
        const double omega = PI * bin / BINS;
        std::array<std::array<std::complex<double>, N>, N> matrix{};
        std::array<std::complex<double>, N> rhs{};
        for(int i = 0; i < N; ++i)
        {
            const auto diagonal = std::polar(1.0 / gain[i], omega * delay[i]);
            for(int j = 0; j < N; ++j)
            {
                const double p = (j == (i + 1) % N) ? 1.0 : 0.0;
                const double a = p - (2.0 / N) * pu[i] * u[j];
                matrix[i][j] = (i == j ? diagonal : std::complex<double>{}) - a;
            }
            rhs[i] = static_cast<double>(pu[i]);
        }
        if(!solve(matrix, rhs, state)) continue;

        for(size_t out = 0; out < outputs.size(); ++out)
        {
            std::complex<double> h{};
            for(int i = 0; i < N; ++i) h += static_cast<double>(sign_at(outputs[out], i)) * state[i];
            output[out] = h;
        }
        const size_t freq = static_cast<size_t>(bin - 1);
        for(size_t p = 0; p < pairs.size(); ++p)
        {
            const double lp = std::norm(output[pairs[p].left_index]);
            const double rp = std::norm(output[pairs[p].right_index]);
            powers[p * (BINS - 1) + freq] = 0.5 * (lp + rp);
            left_energy[p] += lp;
            right_energy[p] += rp;
        }
    }

    for(size_t p = 0; p < pairs.size(); ++p)
    {
        const auto m = measure_pair(powers, p * (BINS - 1), left_energy[p], right_energy[p]);
        std::printf("%u,%u,%u,%.1f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\n",
                    u_mask, pairs[p].left, pairs[p].right, t60,
                    m.mean_power_db, m.flatness_db, m.ripple_std_db,
                    m.p99_excess_db, m.peak_excess_db,
                    m.lr_energy_db);
    }
}
}

int main()
{
    std::vector<unsigned> outputs;
    for(unsigned mask = 128; mask < 256; ++mask)
        if(__builtin_popcount(mask) == 4) outputs.push_back(mask);
    std::vector<Pair> pairs;
    for(size_t i = 0; i < outputs.size(); ++i)
        for(size_t j = i + 1; j < outputs.size(); ++j)
            if(sign_dot(outputs[i], outputs[j]) == 0)
                pairs.push_back({outputs[i], outputs[j], i, j});

    std::puts("u_mask,left_mask,right_mask,t60,mean_power_db,flatness_db,ripple_std_db,p99_excess_db,peak_excess_db,lr_energy_db");
    constexpr std::array<double, 3> t60s{1.5, 5.0, 12.5};
    for(unsigned u = 128; u < 256; ++u)
        for(double t60 : t60s)
            run(u, t60, outputs, pairs);
}
