/* FluidSynth - stable-coefficient standard IIR block recurrence test */

#include "test.h"
#include "fluid_conv.h"
#include "rvoice/fluid_iir_filter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>

namespace
{
constexpr unsigned kMaxFrames = 64;
constexpr std::array<fluid_real_t, 4> kSampleRates{{44100.0, 48000.0, 88200.0, 96000.0}};
constexpr std::array<fluid_real_t, 4> kCutoffRatios{{0.00045, 0.02, 0.18, 0.45}};
constexpr std::array<fluid_real_t, 4> kQs{{0.001, 0.7071067811865476, 20.0, 44.7}};

enum class SignalKind
{
    Noise,
    QuietNoise,
    Impulse,
    Tail
};

struct FilterFixture
{
    fluid_iir_filter_t filter{};
    fluid_iir_filter_t custom{};
};

double next_noise(uint64_t &state)
{
    state ^= state >> 12;
    state ^= state << 25;
    state ^= state >> 27;
    const uint64_t value = state * 0x2545F4914F6CDD1DULL;
    return (static_cast<double>(value >> 11) / 9007199254740992.0) * 2.0 - 1.0;
}

std::array<fluid_real_t, kMaxFrames> make_signal(SignalKind kind)
{
    std::array<fluid_real_t, kMaxFrames> result{};
    uint64_t state = 0x9e3779b97f4a7c15ULL;
    for(unsigned i = 0; i < kMaxFrames; ++i)
    {
        switch(kind)
        {
        case SignalKind::Noise:
            result[i] = static_cast<fluid_real_t>(0.25 * next_noise(state));
            break;
        case SignalKind::QuietNoise:
            result[i] = static_cast<fluid_real_t>(1.0e-5 * next_noise(state));
            break;
        case SignalKind::Impulse:
            result[i] = i == 0 ? 1.0 : 0.0;
            break;
        case SignalKind::Tail:
            result[i] = static_cast<fluid_real_t>(0.2 * std::exp(-0.04 * i)
                         * std::sin(0.21 * i));
            break;
        }
    }
    return result;
}

void initialize_filter(fluid_iir_filter_t &filter,
                       fluid_iir_filter_t &custom,
                       fluid_iir_sincos_t *table,
                       fluid_real_t sample_rate,
                       fluid_real_t cutoff_hz,
                       fluid_real_t q,
                       fluid_real_t hist1,
                       fluid_real_t hist2)
{
    filter = {};
    custom = {};
    filter.type = FLUID_IIR_LOWPASS;
    filter.flags = static_cast<fluid_iir_filter_flags>(0);
    filter.sincos_table = table;
    filter.filter_startup = 1;
    filter.fres = fluid_hz2ct(cutoff_hz);
    filter.last_fres = -1.0;
    filter.last_q = q;
    filter.hist1 = hist1;
    filter.hist2 = hist2;
    filter.amp = 0.75;
    filter.amp_incr = 1.0e-5;
    custom.type = FLUID_IIR_DISABLED;
    custom.last_q = 0.0;
    fluid_iir_filter_calc(&filter, fluid_hz2ct(0.45 * sample_rate), 0.0);
}

void apply_stable_scalar(fluid_iir_filter_t &filter, fluid_real_t *buffer, unsigned count)
{
    if(filter.type == FLUID_IIR_DISABLED || filter.last_q < Q_MIN)
    {
        return;
    }

    fluid_real_t hist1 = filter.hist1;
    fluid_real_t hist2 = filter.hist2;
    const IIR_COEFF_T a1 = filter.a1;
    const IIR_COEFF_T a2 = filter.a2;
    const IIR_COEFF_T b02 = filter.b02;
    const IIR_COEFF_T b1 = filter.b1;
    const IIR_COEFF_T fres = static_cast<IIR_COEFF_T>(filter.last_fres);
    const IIR_COEFF_T q = static_cast<IIR_COEFF_T>(filter.last_q);
    fluid_real_t amp = filter.amp;
    const fluid_real_t amp_incr = filter.amp_incr;

    for(unsigned i = 0; i < count; ++i)
    {
        const fluid_real_t center = buffer[i] - a1 * hist1 - a2 * hist2;
        const fluid_real_t sample = b02 * (center + hist2) + b1 * hist1;
        hist2 = hist1;
        hist1 = center;
        buffer[i] = amp * sample;
        amp += amp_incr;
    }

    if(FLUID_FABS(hist1) < 1e-20f) hist1 = 0.0f;
    if(FLUID_FABS(hist2) < 1e-20f) hist2 = 0.0f;
    filter.hist1 = hist1;
    filter.hist2 = hist2;
    filter.last_fres = fres;
    filter.last_q = q;
    filter.amp = amp;
}

double rms(const fluid_real_t *samples, unsigned count)
{
    long double sum = 0.0L;
    for(unsigned i = 0; i < count; ++i)
        sum += static_cast<long double>(samples[i]) * samples[i];
    return count == 0 ? 0.0 : std::sqrt(static_cast<double>(sum / count));
}

void test_counts_and_parameter_range(void)
{
    for(fluid_real_t sample_rate : kSampleRates)
    {
        std::array<fluid_iir_sincos_t, SINCOS_TAB_SIZE> table{};
        fluid_iir_filter_init_table(table.data(), sample_rate);
        const fluid_real_t minimum_cutoff_hz = fluid_ct2hz(static_cast<fluid_real_t>(FRES_MIN));
        for(unsigned cutoff_index = 0; cutoff_index <= kCutoffRatios.size(); ++cutoff_index)
        {
            const fluid_real_t cutoff_hz = cutoff_index == 0
                ? minimum_cutoff_hz
                : kCutoffRatios[cutoff_index - 1] * sample_rate;
            for(fluid_real_t q : kQs)
            {
                for(SignalKind kind : {SignalKind::Noise, SignalKind::QuietNoise,
                                       SignalKind::Impulse, SignalKind::Tail})
                {
                    const auto raw = make_signal(kind);
                    for(unsigned count = 0; count <= kMaxFrames; ++count)
                    {
                        FilterFixture reference{};
                        initialize_filter(reference.filter, reference.custom, table.data(),
                                          sample_rate, cutoff_hz, q, 0.03125, -0.015625);
                        FilterFixture candidate = reference;
                        auto expected = raw;
                        auto actual = raw;

                        apply_stable_scalar(reference.filter, expected.data(), count);
                        fluid_iir_filter_apply(&candidate.filter, &candidate.custom,
                                               actual.data(), count);

                        for(unsigned i = 0; i < count; ++i)
                            TEST_ASSERT(std::isfinite(actual[i]));
                        TEST_ASSERT(std::isfinite(candidate.filter.hist1));
                        TEST_ASSERT(std::isfinite(candidate.filter.hist2));
                        TEST_ASSERT(std::isfinite(candidate.filter.amp));
                        TEST_ASSERT(std::isfinite(candidate.filter.last_fres));
                        TEST_ASSERT(std::isfinite(candidate.filter.last_q));

                        std::array<fluid_real_t, kMaxFrames> error{};
                        for(unsigned i = 0; i < count; ++i)
                            error[i] = actual[i] - expected[i];
                        const double ref_rms = rms(expected.data(), count);
                        const double err_rms = rms(error.data(), count);
                        if(kind == SignalKind::QuietNoise)
                        {
                            TEST_ASSERT(err_rms <= 1.0e-4); // -80 dBFS
                        }
                        else if(ref_rms > 0.0)
                        {
                            TEST_ASSERT(20.0 * std::log10(err_rms / ref_rms) <= -60.0);
                        }

#if !defined(WITH_FLOAT)
                        TEST_ASSERT(std::abs(candidate.filter.hist1 - reference.filter.hist1) <= 1.0e-8);
                        TEST_ASSERT(std::abs(candidate.filter.hist2 - reference.filter.hist2) <= 1.0e-8);
#endif
                        TEST_ASSERT(std::abs(candidate.filter.amp - reference.filter.amp) <= 1.0e-12);
                        TEST_ASSERT(candidate.filter.fres_incr_count == 0);
                        TEST_ASSERT(candidate.filter.q_incr_count == 0);
                    }
                }
            }
        }
    }
}

void test_custom_filter_precedes_stable_standard_filter(void)
{
    constexpr fluid_real_t sample_rate = 48000.0;
    std::array<fluid_iir_sincos_t, SINCOS_TAB_SIZE> table{};
    fluid_iir_filter_init_table(table.data(), sample_rate);

    FilterFixture actual{};
    initialize_filter(actual.filter, actual.custom, table.data(), sample_rate,
                      4200.0, 2.5, 0.03125, -0.015625);
    actual.custom.type = FLUID_IIR_HIGHPASS;
    actual.custom.flags = FLUID_IIR_NO_GAIN_AMP;
    actual.custom.sincos_table = table.data();
    actual.custom.filter_startup = 1;
    actual.custom.fres = fluid_hz2ct(800.0);
    actual.custom.last_fres = -1.0;
    actual.custom.last_q = 0.7071067811865476;
    fluid_iir_filter_calc(&actual.custom, fluid_hz2ct(0.45 * sample_rate), 0.0);

    FilterFixture reference = actual;
    auto actual_samples = make_signal(SignalKind::Noise);
    auto expected_samples = actual_samples;

    fluid_iir_filter_apply(&actual.filter, &actual.custom, actual_samples.data(), 64);

    fluid_iir_filter_t disabled_standard{};
    disabled_standard.type = FLUID_IIR_DISABLED;
    fluid_iir_filter_apply(&disabled_standard, &reference.custom, expected_samples.data(), 64);
    apply_stable_scalar(reference.filter, expected_samples.data(), 64);

    const double ref_rms = rms(expected_samples.data(), 64);
    std::array<fluid_real_t, 64> error{};
    for(unsigned i = 0; i < 64; ++i)
    {
        TEST_ASSERT(std::isfinite(actual_samples[i]));
        error[i] = actual_samples[i] - expected_samples[i];
    }
    TEST_ASSERT(ref_rms > 0.0);
    TEST_ASSERT(20.0 * std::log10(rms(error.data(), 64) / ref_rms) <= -60.0);
}

void test_real_smoothing_fallback_then_stable_block(void)
{
    constexpr fluid_real_t sample_rate = 44100.0;
    std::array<fluid_iir_sincos_t, SINCOS_TAB_SIZE> table{};
    fluid_iir_filter_init_table(table.data(), sample_rate);

    FilterFixture fixture{};
    initialize_filter(fixture.filter, fixture.custom, table.data(), sample_rate,
                      1800.0, 0.7071067811865476, 0.03125, -0.015625);

    // Exercise the same queued generator updates used by live voices. They
    // schedule real per-sample smoothing through fluid_iir_filter_calc().
    fluid_rvoice_param_t param[MAX_EVENT_PARAMS]{};
    const fluid_real_t old_fres = fixture.filter.last_fres;
    const fluid_real_t old_q = fixture.filter.last_q;
    param[0].real = fluid_hz2ct(2600.0);
    fluid_iir_filter_set_fres(&fixture.filter, param);
    param[0].real = 12.0; // Q generator is centibels for this standard filter.
    fluid_iir_filter_set_q(&fixture.filter, param);
    fluid_iir_filter_calc(&fixture.filter, fluid_hz2ct(0.45 * sample_rate), 0.0);
    TEST_ASSERT(fixture.filter.fres_incr_count == FLUID_BUFSIZE);
    TEST_ASSERT(fixture.filter.q_incr_count == FLUID_BUFSIZE);

    auto first = make_signal(SignalKind::Tail);
    fluid_iir_filter_apply(&fixture.filter, &fixture.custom, first.data(), FLUID_BUFSIZE / 2);
    TEST_ASSERT(fixture.filter.fres_incr_count == FLUID_BUFSIZE / 2);
    TEST_ASSERT(fixture.filter.q_incr_count == FLUID_BUFSIZE / 2);
    TEST_ASSERT(fixture.filter.last_fres > old_fres);
    TEST_ASSERT(fixture.filter.last_q > old_q);
    for(unsigned i = 0; i < FLUID_BUFSIZE / 2; ++i)
        TEST_ASSERT(std::isfinite(first[i]));

    auto second = make_signal(SignalKind::Noise);
    fluid_iir_filter_apply(&fixture.filter, &fixture.custom, second.data(), FLUID_BUFSIZE / 2);
    TEST_ASSERT(fixture.filter.fres_incr_count == 0);
    TEST_ASSERT(fixture.filter.q_incr_count == 0);
    // The live scalar path stores last_fres through IIR_COEFF_T (float) on
    // every sample, so the smoothed endpoint is intentionally not double exact.
    TEST_ASSERT(std::abs(fixture.filter.last_fres - fixture.filter.fres) <= 0.05);
    for(unsigned i = 0; i < FLUID_BUFSIZE / 2; ++i)
        TEST_ASSERT(std::isfinite(second[i]));

    // The next call is stable again; compare the W4 path with the scalar
    // recurrence from the exact same state at this transition boundary.
    FilterFixture reference = fixture;
    auto expected = make_signal(SignalKind::Impulse);
    auto actual = expected;
    apply_stable_scalar(reference.filter, expected.data(), 64);
    fluid_iir_filter_apply(&fixture.filter, &fixture.custom, actual.data(), 64);
    std::array<fluid_real_t, 64> error{};
    for(unsigned i = 0; i < 64; ++i)
    {
        TEST_ASSERT(std::isfinite(actual[i]));
        error[i] = actual[i] - expected[i];
    }
    const double ref_rms = rms(expected.data(), 64);
    TEST_ASSERT(ref_rms > 0.0);
    TEST_ASSERT(20.0 * std::log10(rms(error.data(), 64) / ref_rms) <= -60.0);
    TEST_ASSERT(std::abs(fixture.filter.hist1 - reference.filter.hist1) <= 1.0e-8);
    TEST_ASSERT(std::abs(fixture.filter.hist2 - reference.filter.hist2) <= 1.0e-8);
}
}

int main(void)
{
    test_counts_and_parameter_range();
    test_custom_filter_precedes_stable_standard_filter();
    test_real_smoothing_fallback_then_stable_block();
    return 0;
}
