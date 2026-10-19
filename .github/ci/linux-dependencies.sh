#!/usr/bin/env bash
set -euo pipefail

cache=${1:?Expected dependency cache directory}

package_manifest() {
    dpkg-query -W -f='${binary:Package}\t${Version}\n' | LC_ALL=C sort
}

image_identity() {
    printf '%s/%s/%s\n' "$ImageOS" "$ImageVersion" "$(dpkg --print-architecture)"
}

if [[ -f "$cache/packages.txt" ]]; then
    image_identity | cmp - "$cache/image.txt"
    (cd "$cache" && sha256sum -c SHA256SUMS)
    sudo dpkg --unpack "$cache"/archives/*.deb
    sudo dpkg --configure --pending
    sudo apt-get check
    package_manifest | diff -u "$cache/packages.txt" -
    exit
fi

packages=(
    clang-15
    ladspa-sdk
    libasound2-dev
    libdbus-1-dev
    libjack-dev
    libpulse-dev
    libreadline-dev
    libsndfile-dev
    libsystemd-dev
    libomp-dev
    portaudio19-dev
)

mkdir -p "$cache/archives/partial"
sudo apt-get update -y
sudo apt-get -yq --no-install-suggests --no-install-recommends \
    -o "Dir::Cache::archives=$cache/archives" -o APT::Keep-Downloaded-Packages=true \
    install "${packages[@]}"
sudo rm -f "$cache/archives/lock"
sudo rmdir "$cache/archives/partial"

# Save a bundle only when APT downloaded packages for this runner image.
if compgen -G "$cache/archives/*.deb" > /dev/null; then
    package_manifest > "$cache/packages.txt"
    image_identity > "$cache/image.txt"
    (cd "$cache" && sha256sum archives/*.deb > SHA256SUMS)
    if (( $(du -sb "$cache" | cut -f1) <= 450 * 1024 * 1024 )); then
        echo 'cacheable=true' >> "$GITHUB_OUTPUT"
    fi
fi
