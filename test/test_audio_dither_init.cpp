#include "test.h"
#include "fluidsynth.h"
#include "fluid_audio_convert.h"
#include <array>
#include <atomic>
#include <cstring>
#include <thread>

int main()
{
    std::atomic<int> ready{0};
    std::array<std::thread, 8> threads;
    for(auto &thread : threads)
    {
        thread = std::thread([&]()
        {
            ready++;
            while(ready != 8) std::this_thread::yield();
            float in[16] = {};
            int16_t out[32];
            const float *channels[] = {in, in};
            TEST_SUCCESS(fluid_audio_planar_float_to_s16(out, 2, channels, 2, 16));
            int index = 0;
            fluid_synth_dither_s16(&index, 16, in, in, out, 0, 2, out, 1, 2);
        });
    }
    for(auto &thread : threads) thread.join();
    float snapshot[DITHER_CHANNELS][DITHER_SIZE];
    std::memcpy(snapshot, rand_table, sizeof(snapshot));
    init_dither();
    TEST_ASSERT(std::memcmp(snapshot, rand_table, sizeof(snapshot)) == 0);
    return EXIT_SUCCESS;
}
