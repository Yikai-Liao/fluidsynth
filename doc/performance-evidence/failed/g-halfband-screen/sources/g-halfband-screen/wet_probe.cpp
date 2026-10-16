#include <array>
#include <cstdio>
#include <fstream>
#include <type_traits>

#include "rvoice/fluid_rev_dattorro.h"
#include "hiir/Downsampler2xF64Fpu.h"
#include "hiir/PolyphaseIir2Designer.h"
#include "hiir/Upsampler2xF64Fpu.h"

namespace
{
constexpr int high_frames = FLUID_BUFSIZE;
constexpr int low_frames = FLUID_BUFSIZE / 2;
constexpr int coef_count = 6;
constexpr int total_blocks = 16537;
using sample_t = fluid_real_t;
static_assert(std::is_same<sample_t, double>::value, "Build this screen in double precision");
using frame_t = std::array<double, 2>;

bool write_frames(std::ofstream &out, const std::array<double, high_frames> &left,
                  const std::array<double, high_frames> &right)
{
    std::array<frame_t, high_frames> interleaved{};
    for(int i = 0; i < high_frames; ++i)
        interleaved[i] = {left[i], right[i]};
    out.write(reinterpret_cast<const char *>(interleaved.data()),
              static_cast<std::streamsize>(sizeof(interleaved)));
    return out.good();
}
}

int main(int argc, char **argv)
{
    if(argc != 3) return 2;
    std::ofstream baseline(argv[1], std::ios::binary | std::ios::trunc);
    std::ofstream candidate(argv[2], std::ios::binary | std::ios::trunc);
    if(!baseline || !candidate) return 3;

    const double transition = (22050.0 - 16500.0) / 88200.0;
    double coefs[coef_count]{};
    if(hiir::PolyphaseIir2Designer::compute_coefs(coefs, 80.0, transition) != coef_count) return 4;

    hiir::Downsampler2xF64Fpu<coef_count> down;
    hiir::Upsampler2xF64Fpu<coef_count> up_left, up_right;
    down.set_coefs(coefs);
    up_left.set_coefs(coefs);
    up_right.set_coefs(coefs);
    fluid_revmodel_dattorro full(88200.0), half(44100.0);
    full.set(FLUID_REVMODEL_SET_ALL, 0.7, 0.35, 1.0, 0.8);
    half.set(FLUID_REVMODEL_SET_ALL, 0.7, 0.35, 1.0, 0.8);

    std::array<double, high_frames> input{}, full_l{}, full_r{}, candidate_l{}, candidate_r{};
    std::array<double, low_frames> low_input{}, wet_l{}, wet_r{};
    input[0] = 1.0;

    for(int block = 0; block < total_blocks; ++block)
    {
        full.processreplace(input.data(), full_l.data(), full_r.data());

        down.process_block(low_input.data(), input.data(), low_frames);
        wet_l.fill(0.0);
        wet_r.fill(0.0);
        half.process_n(low_input.data(), wet_l.data(), wet_r.data(), low_frames);
        up_left.process_block(candidate_l.data(), wet_l.data(), low_frames);
        up_right.process_block(candidate_r.data(), wet_r.data(), low_frames);

        if(!write_frames(baseline, full_l, full_r) || !write_frames(candidate, candidate_l, candidate_r))
            return 5;
        input.fill(0.0);
    }
    baseline.close();
    candidate.close();
    if(!baseline || !candidate) return 6;
    std::printf("frames %d sample_rate 88200 duration_s %.9f blocks %d\n",
                total_blocks * high_frames,
                total_blocks * static_cast<double>(high_frames) / 88200.0, total_blocks);
}
