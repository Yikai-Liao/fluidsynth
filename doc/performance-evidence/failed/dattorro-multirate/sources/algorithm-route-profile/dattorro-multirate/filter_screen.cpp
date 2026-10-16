#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

constexpr double fs = 88200.0;
constexpr double stop = 22050.0;
constexpr double beta = 0.1102 * (80.0 - 8.7);
constexpr double pi = 3.14159265358979323846;

double bessel_i0(double x)
{
    double term = 1.0, sum = 1.0;
    for(int k = 1; k < 100; ++k)
    {
        term *= (x * x / 4.0) / (k * k);
        double next = sum + term;
        if(next == sum) break;
        sum = next;
    }
    return sum;
}

std::vector<double> design(int length, double pass)
{
    int mid = length / 2;
    double fc = (pass + stop) / (2.0 * fs);
    double denom = bessel_i0(beta);
    std::vector<double> h(length);
    for(int n = 0; n < length; ++n)
    {
        double x = n - mid;
        double sinc = x == 0.0 ? 2.0 * fc : std::sin(2.0 * pi * fc * x) / (pi * x);
        double ratio = x / mid;
        double window = bessel_i0(beta * std::sqrt(std::max(0.0, 1.0 - ratio * ratio))) / denom;
        h[n] = sinc * window;
    }
    double sum = 0.0;
    for(double v : h) sum += v;
    for(double &v : h) v /= sum;
    return h;
}

double response(const std::vector<double> &h, double f)
{
    int mid = static_cast<int>(h.size()) / 2;
    double value = h[mid];
    for(int k = 1; k <= mid; ++k)
        value += 2.0 * h[mid - k] * std::cos(2.0 * pi * f / fs * k);
    return std::abs(value);
}

bool assess(const std::vector<double> &h, double pass, double &ripple, double &attenuation)
{
    double pmin = 1e30, pmax = 0.0, smax = 0.0;
    constexpr int bins = 4096;
    for(int i = 0; i <= bins; ++i)
    {
        double fp = pass * i / bins;
        double rp = response(h, fp);
        pmin = std::min(pmin, rp);
        pmax = std::max(pmax, rp);
        double fsb = stop + (fs / 2.0 - stop) * i / bins;
        smax = std::max(smax, response(h, fsb));
    }
    ripple = 20.0 * std::log10(pmax / pmin);
    attenuation = -20.0 * std::log10(smax);
    return ripple <= 0.1 && attenuation >= 80.0;
}

int main()
{
    for(double pass : {16000.0, 16500.0, 17000.0, 17500.0, 18000.0, 18500.0, 19000.0, 20000.0})
    {
        double dw = 2.0 * pi * (stop - pass) / fs;
        int estimate = static_cast<int>(std::ceil((80.0 - 8.0) / (2.285 * dw)));
        if((estimate & 1) != 0) ++estimate; // even order, odd length
        int found = 0;
        double ripple = 0.0, attenuation = 0.0;
        for(int order = std::max(30, estimate - 20); order <= estimate + 30; order += 2)
        {
            if(assess(design(order + 1, pass), pass, ripple, attenuation))
            {
                found = order + 1;
                break;
            }
        }
        if(found == 0)
            std::printf("pass %.0f Hz: no candidate in order window around estimate %d\n", pass, estimate);
        else
            std::printf("pass %.0f Hz: taps %d, ripple %.4f dB, stop %.2f dB, FIR delay pair %.4f ms, down+up MAC estimate %.3f million/s\n",
                        pass, found, ripple, attenuation, (found - 1) / fs * 1000.0,
                        ((found + 1) / 2.0 * 44100.0 * 3.0) / 1e6);
    }
}
