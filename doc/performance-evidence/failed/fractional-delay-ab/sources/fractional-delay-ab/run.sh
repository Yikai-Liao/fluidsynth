#!/usr/bin/env bash
set -euo pipefail
task_dir="$(cd "$(dirname "$0")" && pwd)"
data_root="${FLUIDSYNTH_FD_DATA_ROOT:-"$task_dir/../cubic-sinc25"}"
source_root="${FLUIDSYNTH_FD_SOURCE_ROOT:-"$task_dir/../../fluidsynth-worktrees/algorithm-route-profile-20261016"}"
if [[ ! -d "$data_root/sf2-samples" || ! -d "$data_root/sf2-raw" ]]; then
  echo "set FLUIDSYNTH_FD_DATA_ROOT to the external cubic-sinc25 inputs directory" >&2
  exit 2
fi
if [[ ! -d "$source_root/.git" && ! -f "$source_root/.git" ]]; then
  echo "set FLUIDSYNTH_FD_SOURCE_ROOT to the clean FluidSynth worktree at the recorded base commit" >&2
  exit 2
fi
FLUIDSYNTH_FD_DATA_ROOT="$data_root" python3 "$task_dir/make_metadata.py"
c++ -O3 -DNDEBUG -std=c++17 -ffp-contract=off -Wall -Wextra -Wpedantic \
  "$task_dir/fractional_delay_screen.cpp" -o "$task_dir/fractional_delay_screen"
"$task_dir/fractional_delay_screen" "$task_dir" "$data_root"
FLUIDSYNTH_FD_DATA_ROOT="$data_root" FLUIDSYNTH_FD_SOURCE_ROOT="$source_root" \
  python3 "$task_dir/summarize.py"
