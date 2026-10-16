#!/usr/bin/env bash
set -euo pipefail

: "${FLUIDSYNTH_E2_SOURCE:?set to the matching FluidSynth source tree}"
: "${FLUIDSYNTH_E2_BASELINE_BUILD:?set to a built baseline tree containing src/libfluidsynth}"

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
generated_include="$FLUIDSYNTH_E2_BASELINE_BUILD/include"
source_include="$FLUIDSYNTH_E2_SOURCE/include"
lib_dir="$FLUIDSYNTH_E2_BASELINE_BUILD/src"

cc -O3 -std=c11 -I"$generated_include" -I"$source_include" "$root/render_probe_e2.c" \
  -L"$lib_dir" -lfluidsynth -lm -o "$root/render_probe_e2"
cc -O3 -std=c11 -I"$generated_include" -I"$source_include" "$root/render_probe_extended.c" \
  -L"$lib_dir" -lfluidsynth -lm -o "$root/render_probe_extended"
