#!/usr/bin/env bash
# Validate native builds, installs and consumers through one local entry point.
set -eo pipefail
profile=${1:?Usage: build.sh <profile> <build-directory>}
build_dir=${2:?Specify a build directory}
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd "$script_dir/../.." && pwd)
mkdir -p "$build_dir"
build_dir=$(cd "$build_dir" && pwd)
stage="$build_dir/package"
jobs=${BUILD_JOBS:-$(sysctl -n hw.logicalcpu)}
platform_args=()

phase() {
    local label=$1
    shift
    local started=$SECONDS
    "$@"
    printf 'TIMING %s %s seconds\n' "$label" "$((SECONDS - started))"
}

if [[ "$profile" == ports-universal ]]; then
    export PATH="$PATH:/opt/local/bin"
    export PKG_CONFIG_LIBDIR=/opt/local/lib/pkgconfig:/opt/local/share/pkgconfig
    export PKG_CONFIG_PATH=$PKG_CONFIG_LIBDIR
    dependency_prefix=/opt/local
    dependency_args=(-Werror=dev "-DCMAKE_PREFIX_PATH=$dependency_prefix" "-DCMAKE_IGNORE_PREFIX_PATH=$(brew --prefix)")
    platform_args=("-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64")
    for library in libsndfile libreadline; do
        lipo "/opt/local/lib/$library.dylib" -verify_arch arm64 x86_64
    done
else
    dependency_prefix=$(brew --prefix)
    export PKG_CONFIG_PATH="$(brew --prefix libffi)/lib/pkgconfig"
    if [[ "$profile" == unix-static-readline ]]; then
        export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$(brew --prefix readline)/lib/pkgconfig"
    fi
    dependency_args=("-DCMAKE_PREFIX_PATH=$(brew --prefix libomp)")
    if [[ "$profile" == framework-10.14 ]]; then
        platform_args=(-DCMAKE_OSX_DEPLOYMENT_TARGET=10.14)
    fi
fi
configure_args=(-DFLUIDSYNTH_MACOS_PROFILE="$profile" -C "$script_dir/profiles.cmake" "${dependency_args[@]}")
phase configure cmake -G Ninja -S "$repo_root" -B "$build_dir/build" "${configure_args[@]}"
phase compile cmake --build "$build_dir/build" --parallel "$jobs"
phase unit-tests env CTEST_PARALLEL_LEVEL="${CTEST_PARALLEL_LEVEL:-$jobs}" \
    cmake --build "$build_dir/build" --target check --parallel "$jobs"
phase demos cmake --build "$build_dir/build" --target demo --parallel "$jobs"

# Exercise the default install layout without writing into the host prefix.
phase default-install env DESTDIR="$build_dir/default-install" cmake --install "$build_dir/build"
phase configure-artifact-prefix cmake -G Ninja -S "$repo_root" -B "$build_dir/build" "${configure_args[@]}" -DCMAKE_INSTALL_PREFIX="$stage"
phase rebuild-artifact-prefix cmake --build "$build_dir/build" --parallel "$jobs"
phase artifact-install cmake --install "$build_dir/build"

if [[ "$profile" == framework* ]]; then
    test -f "$stage/Library/Frameworks/FluidSynth.framework/FluidSynth"
    # Check Xcode once per OS; the 10.14 profile checks deployment with Ninja.
    if [[ "$profile" == framework ]]; then
        fetchcontent_sources=()
        # Reuse Ninja's pinned sources while Xcode creates its own build metadata.
        for dependency in signalsmith-linear signalsmith-dsp signalsmith-hilbert; do
            source_dir="$build_dir/build/_deps/$dependency-src"
            test -f "$source_dir/CMakeLists.txt"
            dependency_upper=$(printf '%s' "$dependency" | tr '[:lower:]' '[:upper:]')
            fetchcontent_sources+=("-DFETCHCONTENT_SOURCE_DIR_$dependency_upper=$source_dir")
        done
        phase configure-xcode cmake -S "$repo_root" -B "$build_dir/xcode" -G Xcode \
            -Denable-sdl3=OFF -Denable-readline=OFF -Denable-dbus=OFF "${platform_args[@]}" "${fetchcontent_sources[@]}"
        phase build-xcode cmake --build "$build_dir/xcode" --config Debug --parallel "$jobs"
    fi
else
    mkdir -p "$build_dir/consumer"
    cat > "$build_dir/consumer/main.c" <<'C'
#include <fluidsynth.h>
int main(void) {
    fluid_settings_t *settings = new_fluid_settings();
    if (!settings) return 1;
    delete_fluid_settings(settings);
    return 0;
}
C
    export PKG_CONFIG_PATH="$stage/lib/pkgconfig:$PKG_CONFIG_PATH"
    if [[ "$profile" != ports-universal ]]; then
        export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$(brew --prefix readline)/lib/pkgconfig"
    fi
    # pkg-config emits separate compiler and linker arguments.
    c++ "$build_dir/consumer/main.c" -o "$build_dir/pkgconfig-consumer" \
        "-Wl,-rpath,$stage/lib" $(pkg-config --cflags --static fluidsynth) $(pkg-config --libs --static fluidsynth)
    "$build_dir/pkgconfig-consumer"
    cat > "$build_dir/consumer/CMakeLists.txt" <<'CMAKE'
cmake_minimum_required(VERSION 3.13)
project(FluidSynthConsumer C CXX)
find_package(FluidSynth CONFIG REQUIRED)
add_executable(consumer main.c)
target_link_libraries(consumer PRIVATE FluidSynth::libfluidsynth)
CMAKE
    consumer_prefix="$stage;$dependency_prefix"
    if [[ "$profile" != ports-universal ]]; then
        consumer_prefix="$consumer_prefix;$(brew --prefix libomp)"
    fi
    cmake -G Ninja -S "$build_dir/consumer" -B "$build_dir/consumer-build" \
        "${dependency_args[@]}" "-DCMAKE_PREFIX_PATH=$consumer_prefix" "${platform_args[@]}"
    cmake --build "$build_dir/consumer-build" --parallel "$jobs"
    "$build_dir/consumer-build/consumer"
fi

if [[ "$profile" == ports-universal ]]; then
    cp -pL /opt/local/lib/*.dylib "$stage/lib/"
    for binary in "$stage/bin/fluidsynth" "$stage"/lib/libfluidsynth*.dylib; do
        lipo "$binary" -verify_arch arm64 x86_64
        lipo -info "$binary"
    done
fi
