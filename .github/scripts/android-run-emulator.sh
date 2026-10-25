#!/usr/bin/env bash
# Boot an installed x86 image and run the native tests without Gradle.
set -euo pipefail

build_dir=$(realpath "${1:?Usage: run-emulator.sh <build-directory> <x86|x86_64>}")
abi=${2:?Specify x86 or x86_64}
case "$abi" in x86|x86_64) ;; *) echo "KVM tests require x86 or x86_64" >&2; exit 1 ;; esac
sdk=${ANDROID_HOME:?Set ANDROID_HOME to the Android SDK directory}
api=${ANDROID_API:?Set ANDROID_API}
export PATH="$sdk/platform-tools:$sdk/emulator:$sdk/cmdline-tools/latest/bin:$PATH"
export ANDROID_SERIAL=emulator-5554
export ANDROID_AVD_HOME="$build_dir/avd"
avd="fluidsynth-${abi}-$$"
logs="$build_dir/logs"
mkdir -p "$logs" "$ANDROID_AVD_HOME"
emulator_pid=

cleanup() {
    status=$?
    trap - EXIT
    if [[ -n "$emulator_pid" ]] && kill -0 "$emulator_pid" 2>/dev/null; then
        timeout 15 adb logcat -d > "$logs/logcat.txt" 2>&1 || true
        timeout 10 adb emu kill >/dev/null 2>&1 || true
        kill "$emulator_pid" 2>/dev/null || true
        wait "$emulator_pid" 2>/dev/null || true
    fi
    avdmanager delete avd -n "$avd" >/dev/null 2>&1 || true
    if (( status != 0 )); then tail -100 "$logs/emulator.log" || true; fi
    exit "$status"
}
trap cleanup EXIT

adb start-server
if adb devices | grep -q "^${ANDROID_SERIAL}[[:space:]]"; then
    echo "${ANDROID_SERIAL} is already in use" >&2
    exit 1
fi
avdmanager create avd --name "$avd" --package "system-images;android-$api;default;$abi" --path "$ANDROID_AVD_HOME/$avd.avd" --force <<< no
avdmanager list avd > "$logs/avd.txt"
emulator -list-avds | tee "$logs/emulator-avds.txt" | grep -Fx "$avd"
emulator -avd "$avd" -port 5554 -no-window -no-snapshot -no-audio -no-boot-anim \
    -accel on -gpu swiftshader_indirect -cores 2 -memory 2048 -no-metrics > "$logs/emulator.log" 2>&1 &
emulator_pid=$!
boot_deadline=$((SECONDS + 300))
until [[ $(timeout 10 adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r') == 1 ]]; do
    if ! kill -0 "$emulator_pid" 2>/dev/null || (( SECONDS >= boot_deadline )); then
        echo 'Emulator did not finish booting within 300 seconds' >&2
        exit 1
    fi
    sleep 2
done

# Preload the complete packaged library set; the existing test runner pushes
# only its historical dependency list and does not include Opus or Vorbisfile.
adb shell mkdir -p /data/local/tmp/fluidsynth_tests/lib
adb push "$build_dir/package/lib/$abi/." /data/local/tmp/fluidsynth_tests/lib/
export ANDROID_ABI_CMAKE="$abi" BUILD_DIR="$build_dir/build/fluidsynth" PREFIX=$(sed -n 's/^CMAKE_INSTALL_PREFIX:PATH=//p' "$build_dir/build/fluidsynth/CMakeCache.txt")
export NDK="$ANDROID_NDK_HOME" NDK_TOOLCHAIN="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64"
export ARCH=$([[ "$abi" == x86 ]] && echo i686 || echo x86_64) ANDROID_TARGET_ABI=
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
cd "$logs"
timeout 600 bash "$script_dir/../../test/run-android-tests.sh" 2>&1 | tee native-tests.log
