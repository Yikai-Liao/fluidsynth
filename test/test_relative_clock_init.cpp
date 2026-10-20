#include "test.h"
#include "fluid_sys.h"
#include <array>
#include <climits>
#include <atomic>
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
            unsigned int previous = fluid_curtime();
            for(int i = 0; i < 100; i++)
            {
                unsigned int now = fluid_curtime();
                TEST_ASSERT(now - previous <= UINT_MAX / 2);
                previous = now;
            }
        });
    }
    for(auto &thread : threads) thread.join();
    return EXIT_SUCCESS;
}
