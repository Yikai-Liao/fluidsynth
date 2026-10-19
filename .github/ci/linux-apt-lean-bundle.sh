#!/usr/bin/env bash
set -euo pipefail

mode=${1:?Expected original or cached}
bundle=${APT_BUNDLE_DIR:?APT_BUNDLE_DIR is required}
script_dir=$(cd "$(dirname "$0")" && pwd)

package_manifest() {
  dpkg-query -W -f='${binary:Package}\t${Version}\n' | LC_ALL=C sort
}
image_identity() {
  printf '%s/%s/%s\n' "$ImageOS" "$ImageVersion" "$(dpkg --print-architecture)"
}
run_stage() {
  local phase=$1 started=$SECONDS
  shift
  "$@"
  printf '%s\t%s\n' "$phase" "$((SECONDS - started))" >> "$DEPENDENCY_TIMINGS"
}

if [[ $mode == original ]]; then
  env -u APT_BUNDLE_DIR bash "$script_dir/install-linux-apt-experiment.sh" runner-tools
  exit
fi
if [[ $mode != cached ]]; then
  echo "Unknown dependency mode: $mode" >&2
  exit 2
fi

if [[ ${CACHE_HIT:-false} == true ]]; then
  image_identity | cmp - "$bundle/image.txt"
  (cd "$bundle" && sha256sum -c SHA256SUMS)
  run_stage dpkg-unpack sudo dpkg --unpack "$bundle"/archives/*.deb
  run_stage dpkg-configure sudo dpkg --configure --pending
  run_stage apt-check sudo apt-get check
  package_manifest > "$RUNNER_TEMP/restored-packages.txt"
  diff -u "$bundle/packages.txt" "$RUNNER_TEMP/restored-packages.txt"
else
  mkdir -p "$bundle"
  bash "$script_dir/install-linux-apt-experiment.sh" runner-tools
  package_manifest > "$bundle/packages.txt"
  image_identity > "$bundle/image.txt"
  (cd "$bundle" && sha256sum archives/*.deb > SHA256SUMS)
  sudo rm -f "$bundle/archives/lock"
  sudo rmdir "$bundle/archives/partial"
fi

bundle_bytes=$(du -sb "$bundle" | cut -f1)
package_count=$(find "$bundle/archives" -maxdepth 1 -name '*.deb' | wc -l)
printf 'cache_hit=%s\nbundle_bytes=%s\npackage_count=%s\n' "${CACHE_HIT:-false}" "$bundle_bytes" "$package_count" | tee "$RUNNER_TEMP/apt-bundle-info.txt"
if (( bundle_bytes <= 450 * 1024 * 1024 )); then
  echo 'cacheable=true' >> "$GITHUB_OUTPUT"
else
  echo 'cacheable=false' >> "$GITHUB_OUTPUT"
  echo 'Skip cache save: dependency bundle exceeds the 450 MiB entry budget.'
fi
