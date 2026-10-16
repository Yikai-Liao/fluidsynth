#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <vector>

#include "hiir/PolyphaseIir2Designer.h"

namespace
{
constexpr int coef_count = 6;
constexpr double fs = 88200.0;
constexpr double pass_hz = 16500.0;
constexpr double stop_hz_legacy = 22050.0;
constexpr double stop_hz_halfband_feasible = 27600.0;
constexpr double pi = 3.14159265358979323846264338327950288;

std::complex<double> response(const double *coefs, double f)
{
    const std::complex<double> z2 = std::polar(1.0, -4.0 * pi * f);
    std::complex<double> a0 = 1.0;
    std::complex<double> a1 = std::polar(1.0, -2.0 * pi * f);
    for(int i = 0; i < coef_count; ++i)
    {
        const std::complex<double> term = (coefs[i] + z2) / (1.0 + coefs[i] * z2);
        if((i & 1) == 0) a0 *= term;
        else a1 *= term;
    }
    return (a0 + a1) * 0.5;
}

double db(double magnitude)
{
    return 20.0 * std::log10(std::max(magnitude, 1e-300));
}
}

int main()
{
    const double transition = (stop_hz_legacy - pass_hz) / fs;
    double coefs[coef_count]{};
    const int n = hiir::PolyphaseIir2Designer::compute_coefs(coefs, 80.0, transition);
    if(n != coef_count) return 2;

    constexpr int bins = 1 << 20;
    const double pass_edge = pass_hz / fs;
    const double legacy_stop_edge = stop_hz_legacy / fs;
    const double halfband_stop_edge = stop_hz_halfband_feasible / fs;
    double legacy_stop_worst_alias_db = -1e9;
    double feasible_stop_worst_alias_db = -1e9;
    double single_pass_min_db = 0.0;
    double single_pass_max_db = -1e9;
    double combined_pass_min_db = 0.0;
    double combined_pass_max_db = -1e9;

    std::vector<double> pass_phase;
    std::vector<double> pass_freq;
    for(int i = 0; i <= bins / 2; ++i)
    {
        const double f = 0.5 * i / (bins / 2.0);
        const auto h = response(coefs, f);
        const double mag = std::abs(h);
        if(f <= pass_edge)
        {
            const double sdb = db(mag);
            const double cdb = db(mag * mag);
            single_pass_min_db = std::min(single_pass_min_db, sdb);
            single_pass_max_db = std::max(single_pass_max_db, sdb);
            combined_pass_min_db = std::min(combined_pass_min_db, cdb);
            combined_pass_max_db = std::max(combined_pass_max_db, cdb);
            pass_phase.push_back(std::arg(h * h));
            pass_freq.push_back(f);
        }
        if(f >= legacy_stop_edge)
        {
            const double alias = 0.5 - f;
            const double alias_mag = mag * std::abs(response(coefs, alias));
            legacy_stop_worst_alias_db = std::max(legacy_stop_worst_alias_db, db(alias_mag));
            if(f >= halfband_stop_edge)
                feasible_stop_worst_alias_db = std::max(feasible_stop_worst_alias_db, db(alias_mag));
        }
    }

    double delay_min_samples = 1e300;
    double delay_max_samples = -1e300;
    for(size_t i = 1; i + 1 < pass_phase.size(); ++i)
    {
        double p0 = pass_phase[i - 1];
        double p1 = pass_phase[i];
        double p2 = pass_phase[i + 1];
        while(p1 - p0 > pi) p0 += 2.0 * pi;
        while(p1 - p0 < -pi) p0 -= 2.0 * pi;
        while(p2 - p1 > pi) p2 -= 2.0 * pi;
        while(p2 - p1 < -pi) p2 += 2.0 * pi;
        const double dw = 2.0 * pi * (pass_freq[i + 1] - pass_freq[i - 1]);
        const double gd = -(p2 - p0) / dw;
        delay_min_samples = std::min(delay_min_samples, gd);
        delay_max_samples = std::max(delay_max_samples, gd);
    }

    std::printf("filter_order %d coefficient_count %d transition_width_hz %.6f designed_stopband_db %.6f\n",
                coef_count * 2 + 1, coef_count, stop_hz_legacy - pass_hz,
                hiir::PolyphaseIir2Designer::compute_atten_from_order_tbw(coef_count, transition));
    std::printf("passband_hz 0 %.6f single_stage_db %.9g %.9g two_stage_db %.9g %.9g\n",
                pass_hz, single_pass_min_db, single_pass_max_db,
                combined_pass_min_db, combined_pass_max_db);
    std::printf("alias_worst_22050_to_44100_db %.9g alias_worst_27600_to_44100_db %.9g\n",
                legacy_stop_worst_alias_db, feasible_stop_worst_alias_db);
    std::printf("two_stage_group_delay_passband_samples %.9g %.9g milliseconds %.9g %.9g\n",
                delay_min_samples, delay_max_samples,
                delay_min_samples / fs * 1000.0, delay_max_samples / fs * 1000.0);
}
