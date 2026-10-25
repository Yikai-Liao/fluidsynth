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
# Copy the Bionic userspace directly from the SDK's ext4 image.
mkdir -p "$runtime/system"
directories=(bin lib etc usr)
if [[ "$abi" == arm64-v8a ]]; then directories+=(lib64); fi
for directory in "${directories[@]}"; do
    debugfs -R "rdump /$directory $runtime/system" "$image/system.img"
done
debugfs -R "dump /build.prop $runtime/system/build.prop" "$image/system.img"
grep -E '^ro\.(build.version.sdk|product.cpu.abi)=' "$runtime/system/build.prop"
grep -qx "ro.build.version.sdk=$api" "$runtime/system/build.prop"
grep -qx "ro.product.cpu.abi=$abi" "$runtime/system/build.prop"
cp "$image/source.properties" "$runtime/provenance.txt"
