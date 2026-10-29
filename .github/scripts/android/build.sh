#!/usr/bin/env bash
# Build the Android CI dependencies, FluidSynth, test binaries and SDK artifact.
set -euo pipefail

abi=${1:?Usage: build.sh <ABI> <build-directory>}
work=${2:?Specify a build directory}
ndk=${ANDROID_NDK_HOME:?Set ANDROID_NDK_HOME}
api=${ANDROID_API:?Set ANDROID_API}
case "$abi" in
    armeabi-v7a) triple=arm-linux-androideabi ;;
    arm64-v8a) triple=aarch64-linux-android ;;
    x86) triple=i686-linux-android ;;
    x86_64) triple=x86_64-linux-android ;;
    *) echo "Unsupported Android ABI: $abi" >&2; exit 1 ;;
esac
source_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../.." && pwd)
mkdir -p "$work"
work=$(realpath "$work")
prefix="$work/install"
sources="$work/sources"
toolchain="$ndk/toolchains/llvm/prebuilt/linux-x86_64"
jobs=${CMAKE_BUILD_PARALLEL_LEVEL:-$(nproc)}
export PKG_CONFIG_PATH='' PKG_CONFIG_LIBDIR="$prefix/lib/pkgconfig"
cmake_args=(-G Ninja
    "-DCMAKE_TOOLCHAIN_FILE=$ndk/build/cmake/android.toolchain.cmake"
    "-DANDROID_ABI=$abi" "-DANDROID_PLATFORM=android-$api"
    -DANDROID_STL=c++_shared -DCMAKE_BUILD_TYPE=RelWithDebInfo
    "-DCMAKE_INSTALL_PREFIX=$prefix" -DCMAKE_INSTALL_LIBDIR=lib
    "-DCMAKE_PREFIX_PATH=$prefix" "-DCMAKE_FIND_ROOT_PATH=$prefix"
    '-DCMAKE_SHARED_LINKER_FLAGS=-Wl,-z,max-page-size=16384'
    '-DCMAKE_EXE_LINKER_FLAGS=-Wl,-z,max-page-size=16384'
    -DBUILD_SHARED_LIBS=ON)

download_source() {
    local name=$1 url=$2
    mkdir -p "$sources/$name"
    curl --fail --location --retry 3 "$url" -o "$sources/$name.tar"
    tar -xf "$sources/$name.tar" --strip-components=1 -C "$sources/$name"
}

build_dependency() {
    local name=$1
    shift
    cmake -S "$sources/$name" -B "$work/build/$name" "${cmake_args[@]}" \
        -DBUILD_TESTING=OFF "$@"
    cmake --build "$work/build/$name" --parallel "$jobs"
    cmake --install "$work/build/$name"
}

# Keep the dependency versions and build options from the Azure pipeline.
download_source ogg https://github.com/xiph/ogg/releases/download/v1.3.6/libogg-1.3.6.tar.gz
download_source vorbis https://github.com/xiph/vorbis/releases/download/v1.3.7/libvorbis-1.3.7.tar.gz
download_source flac https://github.com/xiph/flac/archive/refs/tags/1.4.3.tar.gz
download_source opus https://github.com/xiph/opus/archive/refs/tags/v1.5.2.tar.gz
download_source sndfile https://github.com/libsndfile/libsndfile/releases/download/1.2.2/libsndfile-1.2.2.tar.xz
download_source oboe https://github.com/google/oboe/archive/1.10.0.tar.gz

build_dependency ogg -DINSTALL_DOCS=OFF
build_dependency vorbis
# FLAC and Opus need an explicit C standard with these releases. Disable FLAC's
# x86 assembly: its assembler does not accept the NDK's stack protector flags.
build_dependency flac -DCMAKE_C_STANDARD=99 -DCMAKE_C_STANDARD_REQUIRED=ON \
    -DWITH_ASM=OFF -DBUILD_CXXLIBS=OFF -DBUILD_PROGRAMS=OFF \
    -DBUILD_EXAMPLES=OFF -DBUILD_DOCS=OFF -DINSTALL_MANPAGES=OFF
build_dependency opus -DCMAKE_C_STANDARD=99 -DCMAKE_C_STANDARD_REQUIRED=ON \
    -DBUILD_PROGRAMS=OFF -DOPUS_MAY_HAVE_NEON=ON
build_dependency sndfile -DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF

# Oboe's release does not install its library and headers by default.
cmake -S "$sources/oboe" -B "$work/build/oboe" "${cmake_args[@]}"
cmake --build "$work/build/oboe" --parallel "$jobs"
cp "$work/build/oboe/liboboe.so" "$prefix/lib/"
cp -a "$sources/oboe/include/oboe" "$prefix/include/"

cmake -S "$source_root" -B "$work/build/fluidsynth" "${cmake_args[@]}" \
    -Denable-opensles=ON -Denable-floats=ON -Denable-oboe=ON \
    -Denable-dbus=OFF -Denable-oss=OFF -Denable-openmp=OFF
cmake --build "$work/build/fluidsynth" --parallel "$jobs"
cmake --build "$work/build/fluidsynth" --target check --parallel "$jobs"
cmake --install "$work/build/fluidsynth"

# The example's CMakeLists uses a source-tree-relative library search path.
# Supply the CI install prefix through CMake without changing the example.
cmake -S "$source_root/doc/android/fluidsynth-assetloader" -B "$work/build/assetloader" \
    "${cmake_args[@]}" "-DCMAKE_SHARED_LINKER_FLAGS=-L$prefix/lib -Wl,-z,max-page-size=16384"
cmake --build "$work/build/assetloader" --parallel "$jobs"

for feature in OBOE_SUPPORT OPENSLES_SUPPORT LIBSNDFILE_SUPPORT LIBSNDFILE_HASVORBIS; do
    if ! grep -qx "#define $feature 1" "$work/build/fluidsynth/config.h"; then
        echo "Required Android feature is missing: $feature" >&2
        exit 1
    fi
done

package="$work/package"
rm -rf "$package"
mkdir -p "$package/lib/$abi" "$package/include"
cp "$work/build/assetloader/libfluidsynth-assetloader.so" "$package/lib/$abi/"
for library in FLAC fluidsynth oboe ogg opus sndfile vorbis vorbisenc vorbisfile; do
    cp -L "$prefix/lib/lib$library.so" "$package/lib/$abi/"
done
cp -a "$prefix/include/fluidsynth.h" "$prefix/include/fluidsynth" "$package/include/"

# Use the existing alignment check with the NDK's cross-platform ELF reader.
mkdir -p "$work/tools"
ln -sf "$toolchain/bin/llvm-objdump" "$work/tools/objdump"
PATH="$work/tools:$PATH" bash "$source_root/contrib/check_elf_alignment.sh" "$package/lib/$abi"

# Both test backends consume the same executables, relative test data and libs.
tests="$work/tests"
rm -rf "$tests"
mkdir -p "$tests/lib"
for executable in "$work/build/fluidsynth/test"/test_*; do
    [[ -f "$executable" && -x "$executable" ]] || continue
    cp -a "$executable" "$tests/"
done
cp -a "$work/build/fluidsynth/test/sf2" "$work/build/fluidsynth/test/test" "$tests/"
cp -a "$package/lib/$abi/." "$tests/lib/"
# The C++ runtime is needed to execute tests, but is not part of the SDK artifact.
cp "$toolchain/sysroot/usr/lib/$triple/libc++_shared.so" "$tests/lib/"
# The NDK's 32-bit C++ runtime uses 4 KB alignment; check the runtime on 64-bit.
if [[ "$abi" == arm64-v8a || "$abi" == x86_64 ]]; then
    PATH="$work/tools:$PATH" bash "$source_root/contrib/check_elf_alignment.sh" "$tests/lib/libc++_shared.so"
fi
