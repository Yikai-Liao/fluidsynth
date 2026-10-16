#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
WORKTREE="${FLUIDSYNTH_G_WORKTREE:-/root/code/fluidsynth-worktrees/combined-iir-dattorro-20261016}"
BUILD_DIR="$SCRIPT_DIR/build"
TASK_CPU="${FLUIDSYNTH_G_CPU:-2}"
EXPECTED_HEAD="571bc72e72b756f25333b3e31fb3046161c27632"

if [[ "$(git -C "$WORKTREE" rev-parse HEAD)" != "$EXPECTED_HEAD" ]]; then
    echo "unexpected FluidSynth worktree revision" >&2
    exit 1
fi
if [[ -n "$(git -C "$WORKTREE" status --porcelain)" ]]; then
    echo "FluidSynth worktree must be clean before applying the test-only probe patch" >&2
    exit 1
fi

git -C "$WORKTREE" apply "$SCRIPT_DIR/g-hook.patch"
cleanup() {
    git -C "$WORKTREE" apply --reverse "$SCRIPT_DIR/g-hook.patch"
    rm -f "$WORKTREE/test/test_dattorro_multirate_probe.cpp" \
          "$WORKTREE/test/test_g_halfband_wet_probe.cpp"
}
trap cleanup EXIT

cp "$SCRIPT_DIR/g_probe.cpp" "$WORKTREE/test/test_dattorro_multirate_probe.cpp"
cp "$SCRIPT_DIR/wet_probe.cpp" "$WORKTREE/test/test_g_halfband_wet_probe.cpp"
export FLUIDSYNTH_G_SCREEN_DIR="$SCRIPT_DIR"

cmake -S "$WORKTREE" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
    -Denable-floats=OFF -Denable-readline=OFF -Denable-libsndfile=OFF \
    -Denable-pulseaudio=OFF -Denable-jack=OFF -Denable-alsa=OFF \
    -Denable-oss=OFF -Denable-waveout=OFF -Denable-dbus=OFF \
    > "$SCRIPT_DIR/build-config.log" 2>&1
cmake --build "$BUILD_DIR" \
    --target test_dattorro_multirate_probe test_g_halfband_wet_probe -j4 \
    > "$SCRIPT_DIR/build.log" 2>&1

taskset -c "$TASK_CPU" "$BUILD_DIR/test/test_dattorro_multirate_probe" \
    > "$SCRIPT_DIR/raw-timing-final.txt"
taskset -c "$TASK_CPU" "$BUILD_DIR/test/test_g_halfband_wet_probe" \
    "$SCRIPT_DIR/wet-full.f64le" "$SCRIPT_DIR/wet-halfband.f64le" \
    > "$SCRIPT_DIR/raw-wet-render.txt"

g++ -O3 -DNDEBUG -std=c++17 -ffp-contract=off -Wall -Wextra -Wpedantic \
    -I"$SCRIPT_DIR" -o "$SCRIPT_DIR/frequency_response" \
    "$SCRIPT_DIR/frequency_response.cpp"
"$SCRIPT_DIR/frequency_response" > "$SCRIPT_DIR/raw-frequency-response.txt"
g++ -O3 -DNDEBUG -std=c++17 -ffp-contract=off -Wall -Wextra -Wpedantic \
    -o "$SCRIPT_DIR/analyze_wet" "$SCRIPT_DIR/analyze_wet.cpp"
"$SCRIPT_DIR/analyze_wet" "$SCRIPT_DIR/wet-full.f64le" \
    "$SCRIPT_DIR/wet-halfband.f64le" > "$SCRIPT_DIR/raw-wet-quality.txt"
python3 "$SCRIPT_DIR/summarize.py" "$SCRIPT_DIR"
