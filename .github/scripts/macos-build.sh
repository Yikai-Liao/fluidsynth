#!/usr/bin/env bash
set -eo pipefail

profile=${1:?Usage: macos-build.sh <profile> <work-directory>}
work_dir=${2:?Specify a work directory}
build_type=Release
shared_libraries=ON
build_framework=OFF
platform_args=()
case "$profile" in
    unix-shared) ;;
    unix-static|unix-static-readline) shared_libraries=OFF ;;
    framework) build_framework=ON ;;
    framework-10.14)
        build_framework=ON
        platform_args=(-DCMAKE_OSX_DEPLOYMENT_TARGET=10.14)
        ;;
    ports-universal)
        build_type=RelWithDebInfo
        platform_args=("-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64")
        ;;
    *) echo "Unknown macOS CI configuration: $profile" >&2; exit 1 ;;
esac

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd "$script_dir/../.." && pwd)
mkdir -p "$work_dir"
work_dir=$(cd "$work_dir" && pwd)
build_dir="$work_dir/build"
package_dir="$work_dir/package"
jobs=${BUILD_JOBS:-$(sysctl -n hw.logicalcpu)}

cmake_args=("-DCMAKE_BUILD_TYPE=$build_type" "-DBUILD_SHARED_LIBS=$shared_libraries" "-Denable-framework=$build_framework")
if [[ "$profile" == ports-universal ]]; then
    export PATH="$PATH:/opt/local/bin"
    export PKG_CONFIG_LIBDIR=/opt/local/lib/pkgconfig:/opt/local/share/pkgconfig
    PKG_CONFIG_PATH=$PKG_CONFIG_LIBDIR
    cmake_args+=(-Denable-sdl3=OFF -Werror=dev -DCMAKE_PREFIX_PATH=/opt/local "-DCMAKE_IGNORE_PREFIX_PATH=$(brew --prefix)")
    for library in libsndfile libreadline; do
        lipo "/opt/local/lib/$library.dylib" -verify_arch arm64 x86_64
    done
else
    PKG_CONFIG_PATH="$(brew --prefix libffi)/lib/pkgconfig"
    if [[ "$profile" == unix-static-readline ]]; then
        PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$(brew --prefix readline)/lib/pkgconfig"
    fi
    cmake_args+=("-DCMAKE_PREFIX_PATH=$(brew --prefix libomp)")
fi
export PKG_CONFIG_PATH
cmake_args+=("${platform_args[@]}")

cmake -G Ninja -S "$repo_root" -B "$build_dir" "${cmake_args[@]}"
cmake --build "$build_dir" --parallel "$jobs"
CTEST_PARALLEL_LEVEL="${CTEST_PARALLEL_LEVEL:-$jobs}" \
    cmake --build "$build_dir" --target check --parallel "$jobs"
cmake --build "$build_dir" --target demo --parallel "$jobs"

DESTDIR="$work_dir/default-install" cmake --install "$build_dir"
# DefaultDirs.cmake computes framework destinations from the configured prefix.
# Reconfigure and rebuild before staging the SDK at its artifact location.
cmake -G Ninja -S "$repo_root" -B "$build_dir" "${cmake_args[@]}" "-DCMAKE_INSTALL_PREFIX=$package_dir"
cmake --build "$build_dir" --parallel "$jobs"
cmake --install "$build_dir"

if [[ "$build_framework" == ON ]]; then
    framework_binary="$package_dir/Library/Frameworks/FluidSynth.framework/FluidSynth"
    if [[ ! -f "$framework_binary" ]]; then
        printf 'Installed framework binary is missing: %s\n' "$framework_binary" >&2
        exit 1
    fi
else
    mkdir -p "$work_dir/consumer"
    cat > "$work_dir/consumer/main.c" <<'C'
#include <fluidsynth.h>
int main(void) {
    fluid_settings_t *settings = new_fluid_settings();
    if (!settings) return 1;
    delete_fluid_settings(settings);
    return 0;
}
C
    PKG_CONFIG_PATH="$package_dir/lib/pkgconfig:$PKG_CONFIG_PATH"
    if [[ "$profile" != ports-universal ]]; then
        PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$(brew --prefix readline)/lib/pkgconfig"
    fi
    pkg_config_flags=$(pkg-config --cflags --libs --static fluidsynth)
    read -r -a consumer_flags <<< "$pkg_config_flags"
    c++ "$work_dir/consumer/main.c" -o "$work_dir/pkgconfig-consumer" \
        "-Wl,-rpath,$package_dir/lib" "${consumer_flags[@]}"
    "$work_dir/pkgconfig-consumer"
fi

if [[ "$profile" == framework ]]; then
    fetchcontent_sources=()
    # Reuse Ninja's downloaded FetchContent sources for the Xcode build.
    for dependency in signalsmith-linear signalsmith-dsp signalsmith-hilbert; do
        source_dir="$build_dir/_deps/$dependency-src"
        if [[ ! -f "$source_dir/CMakeLists.txt" ]]; then
            printf 'FetchContent source is missing: %s/CMakeLists.txt\n' "$source_dir" >&2
            exit 1
        fi
        dependency_upper=$(printf '%s' "$dependency" | tr '[:lower:]' '[:upper:]')
        fetchcontent_sources+=("-DFETCHCONTENT_SOURCE_DIR_$dependency_upper=$source_dir")
    done
    cmake -S "$repo_root" -B "$work_dir/xcode" -G Xcode \
        -Denable-sdl3=OFF -Denable-readline=OFF -Denable-dbus=OFF "${platform_args[@]}" "${fetchcontent_sources[@]}"
    cmake --build "$work_dir/xcode" --config Debug --parallel "$jobs"
fi

if [[ "$profile" == ports-universal ]]; then
    cp -pL /opt/local/lib/*.dylib "$package_dir/lib/"
    for binary in "$package_dir/bin/fluidsynth" "$package_dir"/lib/libfluidsynth*.dylib; do
        lipo "$binary" -verify_arch arm64 x86_64
        lipo -info "$binary"
    done
fi

# Archive the SDK before upload to retain executable modes and symlinks.
tar -cf "$work_dir/sdk.tar" -C "$package_dir" .
