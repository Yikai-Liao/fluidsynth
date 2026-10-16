#!/usr/bin/env bash
set -euo pipefail
task_dir="$(cd "$(dirname "$0")" && pwd)"
python3 "$task_dir/make_metadata.py"
c++ -O3 -DNDEBUG -std=c++17 -ffp-contract=off -Wall -Wextra -Wpedantic \
  "$task_dir/fractional_delay_screen.cpp" -o "$task_dir/fractional_delay_screen"
"$task_dir/fractional_delay_screen" "$task_dir"
python3 "$task_dir/summarize.py"
