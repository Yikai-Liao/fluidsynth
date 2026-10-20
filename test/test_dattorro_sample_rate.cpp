#include "test.h"
#include "fluid_rev_dattorro.h"
#include <cstring>

int main()
{
    for(int rate : {8000, 44100, 48000, 96000})
    {
        fluid_revmodel_dattorro reverb(rate);
        reverb.set(FLUID_REVMODEL_SET_ALL, 0.6, 0.3, 0.8, 0.7);
        fluid_real_t in[FLUID_BUFSIZE] = {1}, left[2][FLUID_BUFSIZE], right[2][FLUID_BUFSIZE];
        reverb.processreplace(in, left[0], right[0]);
        TEST_SUCCESS(reverb.samplerate_change(rate == 48000 ? 44100 : 48000));
        TEST_SUCCESS(reverb.samplerate_change(rate));
        fluid_revmodel_dattorro native(rate);
        native.set(FLUID_REVMODEL_SET_ALL, 0.6, 0.3, 0.8, 0.7);
        double energy = 0;
        for(int block = 0; block < 200; block++)
        {
            if(block) in[0] = 0;
            if(block == 100) TEST_SUCCESS(reverb.samplerate_change(rate));
            reverb.processreplace(in, left[0], right[0]);
            native.processreplace(in, left[1], right[1]);
            TEST_ASSERT(std::memcmp(left[0], left[1], sizeof(left[0])) == 0);
            TEST_ASSERT(std::memcmp(right[0], right[1], sizeof(right[0])) == 0);
            for(auto value : left[0]) energy += value * value;
        }
        TEST_ASSERT(energy > 0);
        TEST_ASSERT(reverb.samplerate_change(0) == FLUID_FAILED);
        TEST_ASSERT(reverb.cached_sample_rate == rate);
    }
    return EXIT_SUCCESS;
}
