#!/usr/bin/env bash
# Build Android dependencies and project targets, or verify and package their libraries.
set -Eeuo pipefail
current_phase=setup
trap 'status=$?; printf "::error::Android %s failed (exit %s)\n" "$current_phase" "$status" >&2; exit "$status"' ERR
phase=${1:?Usage: android-build-and-package.sh <build-dependencies|build-and-install|package>}
case "$phase" in
    build-dependencies|build-and-install|package) ;;
    *) echo "Unknown Android build operation: $phase" >&2; exit 2 ;;
esac
api=${ANDROID_API:?Set ANDROID_API to 24}
if [[ "$api" != 24 ]]; then
    echo 'Android CI builds and tests the minimum supported API 24.' >&2
    exit 2
fi
root=$(git rev-parse --show-toplevel)
work="$root/build/android"
ndk=${ANDROID_NDK_HOME:?Set ANDROID_NDK_HOME}
toolchain="$ndk/toolchains/llvm/prebuilt/linux-x86_64"
abi=${ABI:?Set ABI}
case "$abi" in
    armeabi-v7a) triple=arm-linux-androideabi; triplet=arm-android-shared ;;
    arm64-v8a) triple=aarch64-linux-android; triplet=arm64-android-shared ;;
    x86) triple=i686-linux-android; triplet=x86-android-shared ;;
    x86_64) triple=x86_64-linux-android; triplet=x64-android-shared ;;
    *) echo "Unknown Android ABI: $abi" >&2; exit 1 ;;
esac
prefix="$work/dependencies/$triplet"
jobs=$(nproc)
export PKG_CONFIG_PATH= PKG_CONFIG_LIBDIR="$prefix/lib/pkgconfig"
args=(-G Ninja "-DCMAKE_TOOLCHAIN_FILE=$ndk/build/cmake/android.toolchain.cmake"
    "-DANDROID_ABI=$abi" "-DANDROID_PLATFORM=android-$api"
    -DANDROID_STL=c++_shared -DCMAKE_BUILD_TYPE=RelWithDebInfo
    "-DCMAKE_INSTALL_PREFIX=$prefix" -DCMAKE_INSTALL_LIBDIR=lib
    "-DCMAKE_PREFIX_PATH=$prefix" "-DCMAKE_FIND_ROOT_PATH=$prefix"
    -DCMAKE_SHARED_LINKER_FLAGS=-Wl,-z,max-page-size=16384
    -DCMAKE_EXE_LINKER_FLAGS=-Wl,-z,max-page-size=16384 -DBUILD_SHARED_LIBS=ON)

run_phase() {
    local label=$1
    shift
    current_phase=$label
    printf '::group::%s\n' "$label"
    "$@"
    printf '::endgroup::\n'
}

build_dependencies() {
    local vcpkg=${VCPKG_ROOT:?Set VCPKG_ROOT to the vcpkg release checkout}
    run_phase bootstrap-vcpkg "$vcpkg/bootstrap-vcpkg.sh" -disableMetrics
    export VCPKG_MAX_CONCURRENCY="$jobs"
    run_phase install-dependencies "$vcpkg/vcpkg" install \
        --x-manifest-root="$root/doc/android/vcpkg" \
        --overlay-triplets="$root/doc/android/vcpkg/triplets" \
        --triplet="$triplet" --x-install-root="$work/dependencies" \
        --binarysource=clear --clean-after-build
}

build_and_install() {
    run_phase configure-fluidsynth cmake -S "$root" -B "$work/build/fluidsynth" "${args[@]}" \
        -Denable-opensles=ON -Denable-floats=ON -Denable-oboe=ON \
        -Denable-dbus=OFF -Denable-oss=OFF -Denable-openmp=OFF
    run_phase build-fluidsynth cmake --build "$work/build/fluidsynth" --parallel "$jobs"
    run_phase build-tests cmake --build "$work/build/fluidsynth" --target check --parallel "$jobs"
    run_phase install-fluidsynth cmake --install "$work/build/fluidsynth"
    # Supply the installed library search path without changing the asset loader's
    # existing CMake configuration, which also appends its legacy dist/ path.
    run_phase configure-assetloader cmake -S "$root/doc/android/fluidsynth-assetloader" -B "$work/build/assetloader" "${args[@]}" \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        "-DCMAKE_SHARED_LINKER_FLAGS=-L$prefix/lib -Wl,-z,max-page-size=16384"
    run_phase build-assetloader cmake --build "$work/build/assetloader" --parallel "$jobs"
}

check_load_alignment() {
    local target=$1
    # NDK 32-bit libc++ uses 4 KB pages; Android 16 KB devices use 64-bit ABIs.
    if [[ "${target##*/}" == libc++_shared.so && "$abi" != arm64-v8a && "$abi" != x86_64 ]]; then
        return
    fi
    "$toolchain/bin/llvm-objdump" -p "$target" | awk -v library="$target" '
        /LOAD.*align 2\*\*/ {
            count++; split($0, a, "align 2\\*\\*")
            if (a[2]+0 < 14) {
                printf "::error::%s: LOAD alignment below 2**14: %s\n", library, $0 > "/dev/stderr"
                bad=1
            }
        }
        END {
            if (!count) printf "::error::%s: no LOAD segment alignment found\n", library > "/dev/stderr"
            exit (!count || bad)
        }'
}

package_libraries() {
    current_phase=package-libraries
    cp "$work/build/assetloader/libfluidsynth-assetloader.so" "$prefix/lib/"
    for feature in OBOE_SUPPORT OPENSLES_SUPPORT LIBSNDFILE_SUPPORT LIBSNDFILE_HASVORBIS; do
        grep -qx "#define $feature 1" "$work/build/fluidsynth/config.h"
    done
    cp "$toolchain/sysroot/usr/lib/$triple/libc++_shared.so" "$prefix/lib/"
    local package="$work/package" library target
    mkdir -p "$package/lib/$abi" "$package/include"

    for library in FLAC fluidsynth-assetloader fluidsynth oboe ogg opus sndfile vorbis vorbisenc vorbisfile c++_shared; do
        current_phase="package-lib$library.so"
        target="$package/lib/$abi/lib$library.so"
        cp -L "$prefix/lib/lib$library.so" "$target"
        check_load_alignment "$target"
    done
    current_phase=package-headers
    cp -a "$prefix/include/fluidsynth.h" "$prefix/include/fluidsynth" "$package/include/"
}

case "$phase" in
    build-dependencies) build_dependencies ;;
    build-and-install) build_and_install ;;
    package) package_libraries ;;
esac
