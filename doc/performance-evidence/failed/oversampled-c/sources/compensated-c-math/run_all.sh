#!/usr/bin/env bash
set -euo pipefail

analysis_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
: "${FLUIDSYNTH_C_SCREEN_DATA_ROOT:?Set to the parent directory containing fractional-delay-ab/ and cubic-sinc25/}"
bash "$analysis_root/run_math.sh"
bash "$analysis_root/run_sf2_screen.sh"
python3 "$analysis_root/summarize_sf2.py"
python3 "$analysis_root/make_manifest.py"
