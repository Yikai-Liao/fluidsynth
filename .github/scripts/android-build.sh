#!/usr/bin/env bash
# Run the Android CI dependency build or package the existing project builds.
set -Eeuo pipefail
current_phase=setup
trap 'status=$?; printf "::error::Android %s failed (exit %s)\n" "$current_phase" "$status" >&2; exit "$status"' ERR
phase=${1:?Specify dependencies or package}
api=${ANDROID_API:?Set ANDROID_API to the supported API 24}
if [[ "$api" != 24 ]]; then
    echo 'Android CI requires API 24 to match the pinned native ARM runtimes.' >&2
    exit 2
fi
root=$(git rev-parse --show-toplevel)
work="$root/build/android"
prefix="$work/prefix"
ndk=${ANDROID_NDK_HOME:?Set ANDROID_NDK_HOME}
abi=${ABI:?Set ABI}
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

# Configure each downloaded dependency with its own existing CMakeLists.txt.
dependency() {
    local name=$1 url=$2 checksum=$3
    shift 3
    current_phase="dependency-$name"
    printf '::group::%s\n' "$current_phase"
    mkdir -p "$work/source/$name"
    curl --fail --location --retry 3 -o "$work/$name.tar" "$url"
    printf '%s  %s\n' "$checksum" "$work/$name.tar" | sha256sum --check
    tar -xf "$work/$name.tar" -C "$work/source/$name" --strip-components=1
    cmake -S "$work/source/$name" -B "$work/build/$name" "${args[@]}" \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DBUILD_TESTING=OFF "$@"
    cmake --build "$work/build/$name" --parallel "$jobs"
    cmake --install "$work/build/$name"
    printf '::endgroup::\n'
}

if [[ "$phase" == dependencies ]]; then
    dependency ogg https://github.com/xiph/ogg/releases/download/v1.3.6/libogg-1.3.6.tar.gz \
        83e6704730683d004d20e21b8f7f55dcb3383cdf84c0daedf30bde175f774638 -DINSTALL_DOCS=OFF
    dependency vorbis https://github.com/xiph/vorbis/releases/download/v1.3.7/libvorbis-1.3.7.tar.gz \
        0e982409a9c3fc82ee06e08205b1355e5c6aa4c36bca58146ef399621b0ce5ab
    # FLAC/Opus need an explicit C standard. FLAC's x86 assembler rejects NDK flags.
    dependency flac https://codeload.github.com/xiph/flac/tar.gz/refs/tags/1.4.3 \
        0a4bb82a30609b606650d538a804a7b40205366ce8fc98871b0ecf3fbb0611ee \
        -DCMAKE_C_STANDARD=99 -DCMAKE_C_STANDARD_REQUIRED=ON -DWITH_ASM=OFF \
        -DBUILD_CXXLIBS=OFF -DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_DOCS=OFF -DINSTALL_MANPAGES=OFF
    dependency opus https://codeload.github.com/xiph/opus/tar.gz/refs/tags/v1.5.2 \
        9480e329e989f70d69886ded470c7f8cfe6c0667cc4196d4837ac9e668fb7404 \
        -DCMAKE_C_STANDARD=99 -DCMAKE_C_STANDARD_REQUIRED=ON -DOPUS_MAY_HAVE_NEON=ON
    dependency sndfile https://github.com/libsndfile/libsndfile/releases/download/1.2.2/libsndfile-1.2.2.tar.xz \
        3799ca9924d3125038880367bf1468e53a1b7e3686a934f098b7e1d286cdb80e \
        -DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DENABLE_MPEG=OFF
    oboe_version=1.10.0
    dependency oboe "https://codeload.github.com/google/oboe/tar.gz/refs/tags/$oboe_version" \
        0e4245f8860c4287040a5d76501c588490bcc9cb57614c486c0c201a5dde3e9f
    # Oboe installs under lib/<ABI>; FluidSynth searches lib through pkg-config.
    cp "$work/build/oboe/liboboe.so" "$prefix/lib/"
    cat > "$prefix/lib/pkgconfig/oboe-1.0.pc" <<EOF
prefix=$prefix
libdir=\${prefix}/lib
includedir=\${prefix}/include
Name: Oboe
Description: Oboe library
Version: $oboe_version
Libs: -L\${libdir} -loboe -landroid -llog
Cflags: -I\${includedir}
EOF
    exit 0
fi
if [[ "$phase" != package ]]; then echo "Unknown CI phase: $phase" >&2; exit 1; fi

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
current_phase=package-libraries
cp "$work/build/assetloader/libfluidsynth-assetloader.so" "$prefix/lib/"
for feature in OBOE_SUPPORT OPENSLES_SUPPORT LIBSNDFILE_SUPPORT LIBSNDFILE_HASVORBIS; do
    grep -qx "#define $feature 1" "$work/build/fluidsynth/config.h"
done
case "$abi" in
    armeabi-v7a) triple=arm-linux-androideabi ;;
    arm64-v8a) triple=aarch64-linux-android ;;
    x86) triple=i686-linux-android ;;
    x86_64) triple=x86_64-linux-android ;;
    *) echo "Unknown Android ABI: $abi" >&2; exit 1 ;;
esac
toolchain="$ndk/toolchains/llvm/prebuilt/linux-x86_64"
cp "$toolchain/sysroot/usr/lib/$triple/libc++_shared.so" "$prefix/lib/"
package="$work/package"
mkdir -p "$package/lib/$abi" "$package/include"

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

for library in FLAC fluidsynth-assetloader fluidsynth oboe ogg opus sndfile vorbis vorbisenc vorbisfile c++_shared; do
    current_phase="package-lib$library.so"
    target="$package/lib/$abi/lib$library.so"
    cp -L "$prefix/lib/lib$library.so" "$target"
    check_load_alignment "$target"
done
current_phase=package-headers
cp -a "$prefix/include/fluidsynth.h" "$prefix/include/fluidsynth" "$package/include/"
