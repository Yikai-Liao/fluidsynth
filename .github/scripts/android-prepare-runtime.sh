#!/usr/bin/env bash
# Install an SDK system image and extract its Bionic userspace for ARM test jobs.
set -euo pipefail
api=${ANDROID_API:?Set ANDROID_API}
abi=${1:?Usage: android-prepare-runtime.sh <armeabi-v7a|arm64-v8a> <runtime-directory>}
runtime=${2:?Specify a runtime directory}
sdk=${ANDROID_HOME:?Set ANDROID_HOME to the Android SDK directory}
case "$abi" in
    armeabi-v7a|arm64-v8a) ;;
    *) echo "Unsupported native Android ABI: $abi" >&2; exit 1 ;;
esac
image="$sdk/system-images/android-$api/default/$abi"
"$sdk/cmdline-tools/latest/bin/sdkmanager" "system-images;android-$api;default;$abi"
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
mkdir "$work/image"
sudo mount -o loop,ro "$image/system.img" "$work/image"
mkdir -p "$runtime/system"
for directory in bin lib lib64 etc usr; do
    if [[ -d "$work/image/$directory" ]]; then
        sudo cp -a "$work/image/$directory" "$runtime/system/"
    fi
done
sudo cp "$work/image/build.prop" "$runtime/system/"
sudo chown -R "$(id -u):$(id -g)" "$runtime"
grep -E '^ro\.(build.version.sdk|product.cpu.abi)=' "$runtime/system/build.prop"
grep -qx "ro.build.version.sdk=$api" "$runtime/system/build.prop"
grep -qx "ro.product.cpu.abi=$abi" "$runtime/system/build.prop"
cp "$image/source.properties" "$runtime/provenance.txt"
