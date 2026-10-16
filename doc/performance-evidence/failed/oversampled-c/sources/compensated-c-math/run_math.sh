#!/usr/bin/env bash
set -euo pipefail

analysis_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
python3 "$analysis_root/compensated_c_math.py"
python3 "$analysis_root/export_prefilter_coefficients.py"
