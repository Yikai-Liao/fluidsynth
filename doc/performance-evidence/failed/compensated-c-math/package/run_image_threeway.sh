#!/usr/bin/env bash
set -euo pipefail

analysis_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$analysis_root"

g++ -O3 -DNDEBUG -std=c++17 -ffp-contract=off -Wall -Wextra -Wpedantic \
    "$analysis_root/image_threeway.cpp" -o "$analysis_root/image_threeway"
"$analysis_root/image_threeway" \
    "$analysis_root/image-threeway-double.csv" \
    "$analysis_root/prefilter_coefficients.csv"

g++ -O3 -DNDEBUG -std=c++17 -ffp-contract=off -Wall -Wextra -Wpedantic \
    -DIMAGE_FLOAT "$analysis_root/image_threeway.cpp" \
    -o "$analysis_root/image_threeway_float"
"$analysis_root/image_threeway_float" \
    "$analysis_root/image-threeway-float.csv" \
    "$analysis_root/prefilter_coefficients.csv"

python3 "$analysis_root/summarize_threeway_evidence.py"
