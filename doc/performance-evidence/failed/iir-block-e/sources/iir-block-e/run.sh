#!/usr/bin/env bash
set -euo pipefail

task_dir="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$task_dir/final"
g++ -O3 -DNDEBUG -std=c++17 -ffp-contract=off -Wall -Wextra -Wpedantic \
  "$task_dir/iir_block_screen.cpp" -o "$task_dir/iir_block_screen"

if command -v taskset >/dev/null 2>&1; then
  taskset -c "${IIR_SCREEN_CPU:-2}" "$task_dir/iir_block_screen" "$task_dir/final"
else
  "$task_dir/iir_block_screen" "$task_dir/final"
fi
python3 "$task_dir/summarize.py"
