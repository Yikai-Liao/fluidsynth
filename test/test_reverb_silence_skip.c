#include "test.h"
#include "fluid_rev.h"
#include <math.h>
#include <string.h>

/* Engine types that fluid_rvoice_mixer may skip on silent blocks. */
static const int skippable_types[] = {FLUID_REVERB_TYPE_DATTORRO};

/* Engines that must keep running while their input is silent. */
static const int running_types[] =
{
    FLUID_REVERB_TYPE_FDN, FLUID_REVERB_TYPE_FREEVERB,
    FLUID_REVERB_TYPE_LEXVERB, FLUID_REVERB_TYPE_SIGNALSMITH
};

static fluid_revmodel_t *new_test_reverb(int type)
{
    fluid_revmodel_t *rev = new_fluid_revmodel(48000, 48000, type);
    if(rev != NULL)
    {
        fluid_revmodel_set(rev, FLUID_REVMODEL_SET_ALL, 0.6, 0.4, 1.0, 0.9);
    }
    return rev;
}

/* A reverb that was reset and then only fed zeros has to stay in its reset
 * state: processing silence and skipping it must give the same output. */
static void check_skipped_silence(int type)
{
    /* Dattorro's readout taps are tens of milliseconds behind its input, so
     * the signal has to run long enough for any output to appear. */
    enum { kSilentBands = 8, kSignalBands = 200 };
    fluid_real_t silent[FLUID_BUFSIZE] = {0};
    fluid_real_t in[FLUID_BUFSIZE];
    fluid_real_t skipped_l[FLUID_BUFSIZE], skipped_r[FLUID_BUFSIZE];
    fluid_real_t reference_l[FLUID_BUFSIZE], reference_r[FLUID_BUFSIZE];
    fluid_revmodel_t *skipped = new_test_reverb(type);
    fluid_revmodel_t *reference = new_test_reverb(type);
    int band, i;
    double energy = 0;

    TEST_ASSERT(skipped != NULL && reference != NULL);

    /* Only the left reverb runs through the silence the mixer would skip. */
    for(band = 0; band < kSilentBands; band++)
    {
        memset(skipped_l, 0, sizeof(skipped_l));
        memset(skipped_r, 0, sizeof(skipped_r));
        fluid_revmodel_processmix(skipped, silent, skipped_l, skipped_r);
        TEST_ASSERT(skipped_l[0] == 0 && skipped_l[FLUID_BUFSIZE - 1] == 0);
        TEST_ASSERT(skipped_r[0] == 0 && skipped_r[FLUID_BUFSIZE - 1] == 0);
    }

    for(band = 0; band < kSignalBands; band++)
    {
        for(i = 0; i < FLUID_BUFSIZE; i++)
        {
            in[i] = sin(0.05 * (band * FLUID_BUFSIZE + i)) * exp(-0.001 * i);
            skipped_l[i] = skipped_r[i] = 0;
            reference_l[i] = reference_r[i] = 0;
        }

        fluid_revmodel_processmix(skipped, in, skipped_l, skipped_r);
        fluid_revmodel_processmix(reference, in, reference_l, reference_r);

        for(i = 0; i < FLUID_BUFSIZE; i++)
        {
            /* Byte-wise identity: the skipped blocks must not have moved any
             * state that the following input depends on. */
            TEST_ASSERT(memcmp(&skipped_l[i], &reference_l[i], sizeof(fluid_real_t)) == 0);
            TEST_ASSERT(memcmp(&skipped_r[i], &reference_r[i], sizeof(fluid_real_t)) == 0);
            energy += reference_l[i] * reference_l[i];
        }
    }

    /* Guard against a match on silence only. */
    TEST_ASSERT(energy > 1e-6);

    delete_fluid_revmodel(skipped);
    delete_fluid_revmodel(reference);
}

int main(void)
{
    size_t i;

    for(i = 0; i < sizeof(skippable_types) / sizeof(skippable_types[0]); i++)
    {
        fluid_revmodel_t *rev = new_test_reverb(skippable_types[i]);
        TEST_ASSERT(rev != NULL);
        TEST_ASSERT(fluid_revmodel_can_skip_silence(rev) == 1);
        delete_fluid_revmodel(rev);
        check_skipped_silence(skippable_types[i]);
    }

    for(i = 0; i < sizeof(running_types) / sizeof(running_types[0]); i++)
    {
        fluid_revmodel_t *rev = new_test_reverb(running_types[i]);

        /* The Signalsmith engine is optional. */
        if(rev == NULL)
        {
            TEST_ASSERT(running_types[i] == FLUID_REVERB_TYPE_SIGNALSMITH);
            continue;
        }

        TEST_ASSERT(fluid_revmodel_can_skip_silence(rev) == 0);
        delete_fluid_revmodel(rev);
    }

    return 0;
}
