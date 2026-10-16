#!/usr/bin/env bash
set -euo pipefail

analysis_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
data_root="${FLUIDSYNTH_C_SCREEN_DATA_ROOT:?Set to the parent directory containing fractional-delay-ab/ and cubic-sinc25/}"
samples_csv="$data_root/fractional-delay-ab/samples.csv"
pcm_dir="$data_root/cubic-sinc25/sf2-samples"
raw_dir="$data_root/cubic-sinc25/sf2-raw"

test -f "$samples_csv"
test -d "$pcm_dir"
test -d "$raw_dir"
g++ -O3 -DNDEBUG -std=c++17 -ffp-contract=off -Wall -Wextra -Wpedantic \
    "$analysis_root/compensated_c_sf2_screen.cpp" \
    -o "$analysis_root/compensated_c_sf2_screen"
"$analysis_root/compensated_c_sf2_screen" \
    "$analysis_root" "$samples_csv" "$pcm_dir" "$raw_dir" \
    "$analysis_root/prefilter_coefficients.csv"
