#!/usr/bin/env bash
set -euo pipefail

analysis_root=/root/code/fluidsynth-analysis-20261015/mipmap-d-screen
source_file="$analysis_root/mipmap_d_screen.cpp"
binary="$analysis_root/mipmap_d_screen"
samples_csv=/root/code/fluidsynth-analysis-20261015/fractional-delay-ab/samples.csv
pcm_dir=/root/code/fluidsynth-analysis-20261015/cubic-sinc25/sf2-samples

test -f "$samples_csv"
test -d "$pcm_dir"
"${CXX:-g++}" -O3 -DNDEBUG -std=c++17 -ffp-contract=off -Wall -Wextra -Wpedantic \
    "$source_file" -o "$binary"
"$binary" "$analysis_root"
python3 "$analysis_root/summarize.py" "$analysis_root"
