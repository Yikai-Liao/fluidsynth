#!/bin/sh
set -eu
assetloader_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
host_os=$(uname | tr '[:upper:]' '[:lower:]')
ndk=${ANDROID_NDK_HOME:-"$HOME/android-sdk-$host_os/ndk-bundle"}
for abi in x86 x86_64 armeabi-v7a arm64-v8a; do
    cmake -S "$assetloader_dir" -B "$assetloader_dir/build/$abi" \
        "-DCMAKE_TOOLCHAIN_FILE=$ndk/build/cmake/android.toolchain.cmake" \
        -DANDROID_PLATFORM=android-27 "-DANDROID_ABI=$abi" \
        "-DFluidSynth_DIR=$assetloader_dir/../build/$abi"
    cmake --build "$assetloader_dir/build/$abi"
done
