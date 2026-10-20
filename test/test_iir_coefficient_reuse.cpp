#include "test.h"
#include "fluid_iir_filter.h"
#include "synth/fluid_synth.h"
#include <cstring>
#include <algorithm>
#include <cfloat>
#include <cmath>

/* Compare smoothed coefficients with a fresh calculation at the final state. */
static void check_smoothing(fluid_iir_sincos_t *table, fluid_real_t cutoff_step,
                            fluid_real_t q_step)
{
    fluid_iir_filter_t filter = {};
    filter.type = FLUID_IIR_LOWPASS;
    filter.sincos_table = table;
    filter.last_fres = 6000.25;
    filter.last_q = 0.707;
    filter.fres_incr = cutoff_step;
    filter.fres_incr_count = cutoff_step != 0 ? FLUID_BUFSIZE : 0;
    filter.q_incr = q_step;
    filter.q_incr_count = q_step != 0 ? FLUID_BUFSIZE : 0;
    fluid_iir_filter_t bypass = {};
    fluid_real_t samples[FLUID_BUFSIZE] = {};
    fluid_iir_filter_apply(&filter, &bypass, samples, FLUID_BUFSIZE);

    fluid_iir_filter_t expected = filter;
    expected.fres = filter.last_fres;
    expected.filter_startup = 1;
    fluid_iir_filter_calc(&expected, FRES_MAX, 0);
    const IIR_COEFF_T actual[] = {filter.a1, filter.a2, filter.b02, filter.b1};
    const IIR_COEFF_T reference[] = {expected.a1, expected.a2, expected.b02, expected.b1};
    for(int i = 0; i < 4; ++i)
    {
        /* The IIR Release build permits floating-point reassociation. */
        TEST_ASSERT(std::abs(actual[i] - reference[i])
                    <= 8 * FLT_EPSILON * std::max(1.0f, std::abs(reference[i])));
    }
    TEST_ASSERT(filter.fres_incr_count == 0 && filter.q_incr_count == 0);
}

enum { Frames = 8192 };

static void check_rate_change(int initial_rate, int target_rate, int resonance, int custom_filter)
{
    fluid_settings_t *settings[2];
    fluid_synth_t *synth[2];
    float output[2][Frames * 2];
    int i, nonzero = 0;

    for(i = 0; i < 2; i++)
    {
        settings[i] = new_fluid_settings();
        TEST_ASSERT(settings[i] != NULL);
        TEST_SUCCESS(fluid_settings_setnum(settings[i], "synth.sample-rate", i ? initial_rate : target_rate));
        TEST_SUCCESS(fluid_settings_setint(settings[i], "synth.reverb.active", 0));
        TEST_SUCCESS(fluid_settings_setint(settings[i], "synth.chorus.active", 0));
        synth[i] = new_fluid_synth(settings[i]);
        TEST_ASSERT(synth[i] != NULL);
        if(i != 0)
        {
            /* Apply startup rate changes before rendering the first voice. */
            fluid_synth_set_sample_rate_immediately(synth[i], 22050);
            fluid_synth_set_sample_rate_immediately(synth[i], target_rate);
        }
        TEST_SUCCESS(fluid_synth_sfload(synth[i], TEST_SOUNDFONT, 1));
        TEST_SUCCESS(fluid_synth_program_change(synth[i], 0, 18));
        TEST_SUCCESS(fluid_synth_set_gen(synth[i], 0, GEN_FILTERFC, -4500));
        TEST_SUCCESS(fluid_synth_set_gen(synth[i], 0, GEN_FILTERQ, resonance));
        if(custom_filter)
        {
            TEST_SUCCESS(fluid_synth_set_custom_filter(synth[i], FLUID_IIR_HIGHPASS, 0));
            TEST_SUCCESS(fluid_synth_set_gen(synth[i], 0, GEN_CUSTOM_FILTERFC, 6000));
        }
        TEST_SUCCESS(fluid_synth_noteon(synth[i], 0, 60, 100));
        TEST_SUCCESS(fluid_synth_write_float(synth[i], Frames, output[i], 0, 2, output[i], 1, 2));
    }

    for(i = 0; i < Frames * 2; i++)
    {
        nonzero |= output[0][i] != 0;
    }
    TEST_ASSERT(nonzero);
    /* Compare all cutoff entries, including those this preset does not use. */
    TEST_ASSERT(memcmp(synth[0]->iir_sincos_table, synth[1]->iir_sincos_table,
                       sizeof(synth[0]->iir_sincos_table)) == 0);
    TEST_ASSERT(memcmp(output[0], output[1], sizeof(output[0])) == 0);

    for(i = 0; i < 2; i++)
    {
        delete_fluid_synth(synth[i]);
        delete_fluid_settings(settings[i]);
    }
}

int main()
{
    fluid_iir_sincos_t table[SINCOS_TAB_SIZE];
    fluid_iir_filter_init_table(table, 44100);
    check_smoothing(table, 0.005, 0); // First step must initialize coefficients.
    check_smoothing(table, 0.04, 0); // Cutoff crosses table entries.
    check_smoothing(table, 2, 0); // Large steps bypass coefficient reuse.
    check_smoothing(table, 0, 0.01); // Q changes without a cutoff change.
    check_smoothing(table, 0.04, 0.01);
    for(int resonance = 0; resonance <= 700; resonance += 700)
    {
        check_rate_change(44100, 48000, resonance, 0);
        check_rate_change(48000, 44100, resonance, 0);
        check_rate_change(44100, 96000, resonance, 0);
        check_rate_change(48000, 44100, resonance, 1);
    }
    return 0;
}
