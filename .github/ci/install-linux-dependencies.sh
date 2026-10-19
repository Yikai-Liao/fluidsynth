#!/usr/bin/env bash
set -euo pipefail

mode=${1:?Expected original, no-ppa, or container}
case "$mode" in
  original|no-ppa|container) ;;
  *) echo "Unknown dependency mode: $mode" >&2; exit 2 ;;
esac

privilege=()
if [[ $(id -u) != 0 ]]; then
  privilege=(sudo)
fi
mapfile -t packages < "$(dirname "$0")/linux-dependencies.txt"

if [[ $mode == container ]]; then
  source /etc/os-release
  if [[ $ID == debian ]]; then
    # Install the libraries named by Debian libsndfile's static link flags.
    packages+=(libmp3lame-dev libmpg123-dev)
  fi
fi

run_stage() {
  local phase=$1 started=$SECONDS
  shift
  "$@"
  printf '%s\t%s\n' "$phase" "$((SECONDS - started))" >> "${DEPENDENCY_TIMINGS:-/dev/null}"
}

apt_options=()
if [[ -n ${APT_BUNDLE_DIR:-} ]]; then
  mkdir -p "$APT_BUNDLE_DIR/archives/partial"
  apt_options=(-o "Dir::Cache::archives=$APT_BUNDLE_DIR/archives" -o APT::Keep-Downloaded-Packages=true)
fi

if [[ $mode == original ]]; then
  run_stage repository "${privilege[@]}" add-apt-repository -y ppa:ubuntu-toolchain-r/test
fi
run_stage apt-update "${privilege[@]}" apt-get update -y
run_stage apt-install "${privilege[@]}" apt-get "${apt_options[@]}" -yq --no-install-suggests --no-install-recommends install "${packages[@]}"
