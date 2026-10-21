#!/usr/bin/env bash
# Run the macOS CI build, installation and consumer checks.
set -Eeo pipefail
current_phase=setup
trap 'status=$?; printf "::error::macOS %s failed (exit %s)\n" "$current_phase" "$status" >&2; exit "$status"' ERR
profile=${1:?Usage: build.sh <profile> <build-directory>}
build_dir=${2:?Specify a build directory}
platform_args=()
readline_pkgconfig=
configure_args=(-DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -Denable-framework=OFF)
case "$profile" in
    unix-shared) ;;
    unix-static) configure_args+=(-DBUILD_SHARED_LIBS=OFF) ;;
    unix-static-readline)
        configure_args+=(-DBUILD_SHARED_LIBS=OFF)
        readline_pkgconfig=":$(brew --prefix readline)/lib/pkgconfig"
        ;;
    framework) configure_args+=(-Denable-framework=ON) ;;
    framework-10.14)
        configure_args+=(-Denable-framework=ON)
        platform_args=(-DCMAKE_OSX_DEPLOYMENT_TARGET=10.14)
        ;;
    ports-universal)
        configure_args+=(-DCMAKE_BUILD_TYPE=RelWithDebInfo -Denable-sdl3=OFF)
        platform_args=("-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64")
        ;;
    *) echo "Unknown macOS CI configuration: $profile" >&2; exit 1 ;;
esac
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd "$script_dir/../.." && pwd)
mkdir -p "$build_dir"
build_dir=$(cd "$build_dir" && pwd)
stage="$build_dir/package"
jobs=${BUILD_JOBS:-$(sysctl -n hw.logicalcpu)}

phase() {
    local label=$1
    shift
    local started=$SECONDS
    current_phase=$label
    printf '::group::%s\n' "$label"
    "$@"
    printf 'TIMING %s %s seconds\n' "$label" "$((SECONDS - started))"
    printf '::endgroup::\n'
}

if [[ "$profile" == ports-universal ]]; then
    export PATH="$PATH:/opt/local/bin"
    export PKG_CONFIG_LIBDIR=/opt/local/lib/pkgconfig:/opt/local/share/pkgconfig
    export PKG_CONFIG_PATH=$PKG_CONFIG_LIBDIR
    dependency_args=(-Werror=dev -DCMAKE_PREFIX_PATH=/opt/local "-DCMAKE_IGNORE_PREFIX_PATH=$(brew --prefix)")
    for library in libsndfile libreadline; do
        lipo "/opt/local/lib/$library.dylib" -verify_arch arm64 x86_64
    done
else
    export PKG_CONFIG_PATH="$(brew --prefix libffi)/lib/pkgconfig$readline_pkgconfig"
    dependency_args=("-DCMAKE_PREFIX_PATH=$(brew --prefix libomp)")
fi
# Disable arm64 function deduplication: Apple's linker can fold main() into
# another function and emit a zero LC_MAIN entry, causing SIGILL at startup.
if [[ $(uname -m) == arm64 || $profile == ports-universal ]]; then
    platform_args+=(-DCMAKE_EXE_LINKER_FLAGS=-Wl,-no_deduplicate)
fi
configure_args+=("${dependency_args[@]}" "${platform_args[@]}")
phase configure cmake -G Ninja -S "$repo_root" -B "$build_dir/build" "${configure_args[@]}"
phase compile cmake --build "$build_dir/build" --parallel "$jobs"
phase unit-tests env CTEST_PARALLEL_LEVEL="${CTEST_PARALLEL_LEVEL:-$jobs}" \
    cmake --build "$build_dir/build" --target check --parallel "$jobs"
phase demos cmake --build "$build_dir/build" --target demo --parallel "$jobs"

# Exercise the default install layout without writing into the host prefix.
phase default-install env DESTDIR="$build_dir/default-install" cmake --install "$build_dir/build"
# DefaultDirs.cmake computes framework destinations from the configured prefix.
# Reconfigure and rebuild before staging the SDK at its artifact location.
phase configure-artifact-prefix cmake -G Ninja -S "$repo_root" -B "$build_dir/build" "${configure_args[@]}" -DCMAKE_INSTALL_PREFIX="$stage"
phase rebuild-artifact-prefix cmake --build "$build_dir/build" --parallel "$jobs"
phase artifact-install cmake --install "$build_dir/build"

current_phase=validate-installed-framework
if [[ "$profile" == framework || "$profile" == framework-10.14 ]]; then
    test -f "$stage/Library/Frameworks/FluidSynth.framework/FluidSynth"
    # Check Xcode once per OS; the 10.14 profile checks deployment with Ninja.
    if [[ "$profile" == framework ]]; then
        fetchcontent_sources=()
        # Reuse the three sources declared in signalsmith-audio-basics/CMakeLists.txt
        # while Xcode creates its own build metadata.
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
    current_phase=validate-pkgconfig-consumer
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

fi

if [[ "$profile" == ports-universal ]]; then
    current_phase=package-universal
    cp -pL /opt/local/lib/*.dylib "$stage/lib/"
    for binary in "$stage/bin/fluidsynth" "$stage"/lib/libfluidsynth*.dylib; do
        lipo "$binary" -verify_arch arm64 x86_64
        lipo -info "$binary"
    done
fi

# Archive the SDK before upload to retain executable modes and symlinks.
phase archive-sdk tar -cf "$build_dir/sdk.tar" -C "$stage" .
