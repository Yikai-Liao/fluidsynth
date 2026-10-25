#include "test.h"
#include <fluidsynth.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <new>
#include <string>
#include <vector>

static bool track_allocations;
static size_t largest_allocation;

void *operator new(std::size_t size)
{
    if(track_allocations)
    {
        largest_allocation = std::max(largest_allocation, size);
    }

    if(void *memory = std::malloc(size ? size : 1))
    {
        return memory;
    }

    throw std::bad_alloc{};
}

void *operator new[](std::size_t size)
{
    return ::operator new(size);
}

void *operator new(std::size_t size, const std::nothrow_t &) noexcept
{
    if(track_allocations)
    {
        largest_allocation = std::max(largest_allocation, size);
    }

    return std::malloc(size ? size : 1);
}

void *operator new[](std::size_t size, const std::nothrow_t &) noexcept
{
    return ::operator new(size, std::nothrow);
}

void operator delete(void *memory) noexcept
{
    std::free(memory);
}

void operator delete[](void *memory) noexcept
{
    std::free(memory);
}

void operator delete(void *memory, std::size_t) noexcept
{
    std::free(memory);
}

void operator delete[](void *memory, std::size_t) noexcept
{
    std::free(memory);
}

void operator delete(void *memory, const std::nothrow_t &) noexcept
{
    std::free(memory);
}

void operator delete[](void *memory, const std::nothrow_t &) noexcept
{
    std::free(memory);
}

static uint32_t read_u32(const std::vector<uint8_t> &data, size_t offset)
{
    return static_cast<uint32_t>(data[offset])
           | (static_cast<uint32_t>(data[offset + 1]) << 8)
           | (static_cast<uint32_t>(data[offset + 2]) << 16)
           | (static_cast<uint32_t>(data[offset + 3]) << 24);
}

static void write_u32(std::vector<uint8_t> &data, size_t offset, uint32_t value)
{
    for(unsigned int i = 0; i < 4; ++i)
    {
        data[offset + i] = static_cast<uint8_t>(value >> (i * 8));
    }
}

int main(void)
{
    std::ifstream input(TEST_DLS, std::ios::binary);
    TEST_ASSERT(input.good());
    std::vector<uint8_t> file((std::istreambuf_iterator<char>(input)), {});
    size_t ptbl = 12;

    while(ptbl + 8 <= file.size() && std::string(file.begin() + ptbl, file.begin() + ptbl + 4) != "ptbl")
    {
        const uint32_t size = read_u32(file, ptbl + 4);
        ptbl += 8 + size + (size & 1u);
    }

    TEST_ASSERT(ptbl + 8 <= file.size());
    TEST_ASSERT(std::string(file.begin() + ptbl, file.begin() + ptbl + 4) == "ptbl");
    const size_t old_end = ptbl + 8 + read_u32(file, ptbl + 4)
                           + (read_u32(file, ptbl + 4) & 1u);
    TEST_ASSERT(old_end <= file.size());

    constexpr uint32_t cue_count = 500000;
    constexpr uint32_t ptbl_size = 8 + cue_count * 4;
    std::vector<uint8_t> replacement(8 + ptbl_size, 0);
    std::copy_n("ptbl", 4, replacement.begin());
    write_u32(replacement, 4, ptbl_size);
    write_u32(replacement, 8, 8); /* cbSize */
    write_u32(replacement, 12, cue_count);
    for(uint32_t i = 0; i < cue_count; ++i)
    {
        write_u32(replacement, 16 + i * 4, 0xffffffffu); /* Invalid wave offset */
    }

    std::vector<uint8_t> malformed;
    malformed.reserve(file.size() - (old_end - ptbl) + replacement.size());
    malformed.insert(malformed.end(), file.begin(), file.begin() + ptbl);
    malformed.insert(malformed.end(), replacement.begin(), replacement.end());
    malformed.insert(malformed.end(), file.begin() + old_end, file.end());
    write_u32(malformed, 4, static_cast<uint32_t>(malformed.size() - 8));

    const char *path = "test_dls_ptbl_allocation_tmp.dls";
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    TEST_ASSERT(output.good());
    output.write(reinterpret_cast<const char *>(malformed.data()),
                 static_cast<std::streamsize>(malformed.size()));
    output.close();
    TEST_ASSERT(output.good());

    fluid_settings_t *settings = new_fluid_settings();
    TEST_ASSERT(settings != nullptr);
    fluid_synth_t *synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != nullptr);

    largest_allocation = 0;
    track_allocations = true;
    const int sfont_id = fluid_synth_sfload(synth, path, 1);
    track_allocations = false;

    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    TEST_ASSERT(std::remove(path) == 0);
    TEST_ASSERT(sfont_id == FLUID_FAILED);
    /* The 2 MB cue table is proportional to the input; it must not reserve
       a 36 MB sample vector before the first invalid cue is parsed. */
    TEST_ASSERT(largest_allocation < 8 * 1024 * 1024);
    return EXIT_SUCCESS;
}
