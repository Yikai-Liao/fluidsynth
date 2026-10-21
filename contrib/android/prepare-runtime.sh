#!/usr/bin/env bash
# Extract API 24 Bionic binaries from Google's pinned ARM system images.
set -euo pipefail
abi=${1:?Usage: prepare-runtime.sh <armeabi-v7a|arm64-v8a> <runtime-directory>}
runtime=${2:?Specify a runtime directory}
case "$abi" in
    armeabi-v7a) archive=armeabi-v7a-24_r07.zip; checksum=3454546b4eed2d6c3dd06d47757d6da9f4176033 ;;
    arm64-v8a) archive=arm64-v8a-24_r09.zip; checksum=e88ebdf4533efa0370603ee4ab0e7834e0cc364f ;;
    *) echo "Unsupported native Android ABI: $abi" >&2; exit 1 ;;
esac
mkdir -p "$runtime"
runtime=$(realpath "$runtime")
work=$(mktemp -d "${RUNNER_TEMP:-/tmp}/android-runtime.XXXXXX")
cleanup() {
    local status=$?
    if mountpoint -q "$work/image"; then sudo umount "$work/image"; fi
    rm -rf "$work"
    exit "$status"
}
trap cleanup EXIT
curl --fail --location --retry 3 -o "$work/image.zip" \
    "https://dl.google.com/android/repository/sys-img/android/$archive"
printf '%s  %s\n' "$checksum" "$work/image.zip" | sha1sum --check
entry=$(unzip -Z1 "$work/image.zip" | sed -n '\|/system.img$|p')
test -n "$entry"
unzip -p "$work/image.zip" "$entry" > "$work/system.img"
mkdir "$work/image"
sudo mount -o loop,ro "$work/system.img" "$work/image"
mkdir -p "$runtime/system"
for directory in bin lib lib64 etc usr; do
    if [[ -d "$work/image/$directory" ]]; then
        sudo cp -a "$work/image/$directory" "$runtime/system/"
    fi
done
sudo cp "$work/image/build.prop" "$runtime/system/"
sudo chown -R "$(id -u):$(id -g)" "$runtime"
grep -E '^ro\.(build.version.sdk|product.cpu.abi)=' "$runtime/system/build.prop"
grep -qx 'ro.build.version.sdk=24' "$runtime/system/build.prop"
grep -qx "ro.product.cpu.abi=$abi" "$runtime/system/build.prop"
printf '%s %s\n' "$archive" "$checksum" > "$runtime/provenance.txt"
