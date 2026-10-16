#!/usr/bin/env bash
set -euo pipefail

analysis_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
data_root="${FLUIDSYNTH_C_SCREEN_DATA_ROOT:?Set to the parent directory containing fractional-delay-ab/ and cubic-sinc25/}"
cpu="${FLUIDSYNTH_PERF_CPU:-2}"
cd "$analysis_root"

g++ -O3 -DNDEBUG -std=c++17 -ffp-contract=off -Wall -Wextra -Wpedantic \
    "$analysis_root/compensated_c_perf_screen.cpp" \
    -o "$analysis_root/compensated_c_perf_screen"

taskset -c "$cpu" "$analysis_root/compensated_c_perf_screen" \
    "$data_root" "$analysis_root"
