#include "test.h"
#include "fluid_chorus.h"
#include <math.h>
#include <string.h>

/* fluid_rvoice_mixer skips a chorus that was reset and has only been fed
 * silence, calling fluid_chorus_processsilence() instead of processing the
 * block. That replacement must leave the chorus in the state the skipped
 * blocks would have produced. */
int main(void)
{
    enum { kSilentBlocks = 8, kSignalBlocks = 8 };
    const fluid_real_t silent[FLUID_BUFSIZE] = {0};
    fluid_real_t in[FLUID_BUFSIZE];
    fluid_real_t skipped_l[FLUID_BUFSIZE], skipped_r[FLUID_BUFSIZE];
    fluid_real_t dropped_l[FLUID_BUFSIZE], dropped_r[FLUID_BUFSIZE];
    fluid_real_t reference_l[FLUID_BUFSIZE], reference_r[FLUID_BUFSIZE];
    fluid_chorus_t *dropped = new_fluid_chorus(44100);
    fluid_chorus_t *reference = new_fluid_chorus(44100);
    int block, i;
    double energy = 0;

    TEST_ASSERT(dropped != NULL && reference != NULL);

    fluid_chorus_set(dropped, FLUID_CHORUS_SET_ALL, 3, 0.9f, 0.3f, 0.4f, FLUID_CHORUS_MOD_SINE);
    fluid_chorus_set(reference, FLUID_CHORUS_SET_ALL, 3, 0.9f, 0.3f, 0.4f, FLUID_CHORUS_MOD_SINE);

    /* Only `dropped` runs the skip path; `reference` processes the silence. */
    for(block = 0; block < kSilentBlocks; block++)
    {
        memset(dropped_l, 0, sizeof(dropped_l));
        memset(dropped_r, 0, sizeof(dropped_r));
        memset(reference_l, 0, sizeof(reference_l));
        memset(reference_r, 0, sizeof(reference_r));

        fluid_chorus_processsilence(dropped);
        fluid_chorus_processmix(reference, silent, reference_l, reference_r);

        /* Silence in, silence out: the mixer may leave the output untouched. */
        for(i = 0; i < FLUID_BUFSIZE; i++)
        {
            TEST_ASSERT(reference_l[i] == 0 && reference_r[i] == 0);
        }
    }

    for(block = 0; block < kSignalBlocks; block++)
    {
        for(i = 0; i < FLUID_BUFSIZE; i++)
        {
            in[i] = (fluid_real_t)sin(0.07 * (block * FLUID_BUFSIZE + i));
            dropped_l[i] = dropped_r[i] = 0;
            reference_l[i] = reference_r[i] = 0;
        }

        fluid_chorus_processmix(dropped, in, dropped_l, dropped_r);
        fluid_chorus_processmix(reference, in, reference_l, reference_r);

        for(i = 0; i < FLUID_BUFSIZE; i++)
        {
            TEST_ASSERT(memcmp(&dropped_l[i], &reference_l[i], sizeof(fluid_real_t)) == 0);
            TEST_ASSERT(memcmp(&dropped_r[i], &reference_r[i], sizeof(fluid_real_t)) == 0);
            energy += reference_l[i] * reference_l[i];
        }
    }

    /* Guard against a match on silence only. */
    TEST_ASSERT(energy > 1e-6);

    delete_fluid_chorus(dropped);
    delete_fluid_chorus(reference);
    return 0;
}
