#!/usr/bin/env bash
set -euo pipefail

analysis_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
worktree="${FLUIDSYNTH_CANDIDATE_WORKTREE:-/root/code/fluidsynth-worktrees/compensated-c-perf-20261016}"
soundfont="${FLUIDSYNTH_MUSESCORE_SF2:-/root/code/fluidsynth-analysis-20261015/assets/MuseScore_General.sf2}"
output_dir="${1:-$analysis_root/full-render-rerun}"
cpu="${FLUIDSYNTH_PERF_CPU:-2}"

if [[ ! -f "$soundfont" ]]; then
    echo "SoundFont not found: $soundfont" >&2
    exit 2
fi
if [[ ! -f "$worktree/build-compensated/src/libfluidsynth.so" ]]; then
    echo "Experimental build not found under $worktree/build-compensated/src" >&2
    exit 2
fi
mkdir -p "$output_dir"

g++ -O3 -DNDEBUG -std=c++17 -ffp-contract=off -w \
    -I"$worktree/build-compensated" -I"$worktree/build-compensated/include" \
    -I"$worktree/include" -I"$worktree/src" -I"$worktree/src/synth" \
    -I"$worktree/src/rvoice" -I"$worktree/src/sfloader" -I"$worktree/src/utils" \
    -I"$worktree/src/gentables" -I"$worktree/src/bindings" \
    -I"$worktree/src/midi" -I"$worktree/src/drivers" \
    "$analysis_root/full_renderer_probe.cpp" \
    -L"$worktree/build-compensated/src" -lfluidsynth \
    -Wl,-rpath,"$worktree/build-compensated/src" \
    -o "$output_dir/full_renderer_probe"

for method in cubic compensated6; do
    for repeat in 1 2; do
        name="$method-strings-fx-$repeat"
        taskset -c "$cpu" "$output_dir/full_renderer_probe" "$soundfont" "$method" 30000 \
            "$analysis_root" "$output_dir/$name.csv" 48 1 > "$output_dir/$name.json"
    done
done
name="sinc25-strings-fx-1"
taskset -c "$cpu" "$output_dir/full_renderer_probe" "$soundfont" sinc25 30000 \
    "$analysis_root" "$output_dir/$name.csv" 48 1 > "$output_dir/$name.json"

echo "renderer callback measurements written to $output_dir"
