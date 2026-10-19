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

if [[ $mode == original ]]; then
  "${privilege[@]}" add-apt-repository -y ppa:ubuntu-toolchain-r/test
fi
"${privilege[@]}" apt-get update -y
"${privilege[@]}" apt-get -yq --no-install-suggests --no-install-recommends install "${packages[@]}"
