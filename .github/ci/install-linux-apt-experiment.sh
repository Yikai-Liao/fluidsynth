#!/usr/bin/env bash
set -euo pipefail

mode=${1:?Expected original, no-ppa, single-update, or runner-tools}
case "$mode" in
  original|no-ppa|single-update|runner-tools) ;;
  *) echo "Unknown APT mode: $mode" >&2; exit 2 ;;
esac

mapfile -t requested_packages < "$(dirname "$0")/linux-dependencies.txt"
packages=()
for package in "${requested_packages[@]}"; do
  # Exclude SDL2 from every arm; FluidSynth discovers SDL3.
  [[ $package == libsdl2-dev ]] && continue
  if [[ $mode == runner-tools ]]; then
    case "$package" in
      clang-tidy|cmake|cmake-data|ninja-build) continue ;;
    esac
  fi
  packages+=("$package")
done

run_stage() {
  local phase=$1 started=$SECONDS
  shift
  printf '\n::group::%s\n' "$phase"
  "$@"
  printf '%s\t%s\n' "$phase" "$((SECONDS - started))" >> "$DEPENDENCY_TIMINGS"
  printf '::endgroup::\n'
}

if [[ $mode == runner-tools ]]; then
  command -v cmake ninja
fi
case "$mode" in
  original)
    run_stage repository sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
    ;;
  single-update)
    run_stage repository sudo add-apt-repository --no-update -y ppa:ubuntu-toolchain-r/test
    ;;
esac
run_stage apt-update sudo apt-get update -y
apt_options=()
if [[ -n ${APT_BUNDLE_DIR:-} ]]; then
  mkdir -p "$APT_BUNDLE_DIR/archives/partial"
  apt_options=(-o "Dir::Cache::archives=$APT_BUNDLE_DIR/archives" -o APT::Keep-Downloaded-Packages=true)
fi
run_stage apt-install sudo apt-get "${apt_options[@]}" -yq --no-install-suggests --no-install-recommends install "${packages[@]}"
