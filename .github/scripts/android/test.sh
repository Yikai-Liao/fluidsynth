#!/usr/bin/env bash
# Run the staged Android tests using an emulator or a Bionic chroot.
set -euo pipefail
backend=${1:?Usage: test.sh <emulator|chroot> <ABI> <work-directory>}
abi=${2:?Specify the test ABI}
work=$(realpath "${3:?Specify a work directory containing tests/}")
tests="$work/tests"
logs="$work/logs"
device_tests=/data/local/tmp/fluidsynth-tests
emulator_pid=
avd=
root=
mkdir -p "$logs"

cleanup() {
    local status=$? directory
    trap - EXIT
    if [[ -n "$emulator_pid" ]]; then
        if (( status != 0 )); then
            timeout 15 adb logcat -d 2>&1 | tail -100 || true
        fi
        timeout 10 adb emu kill >/dev/null 2>&1 || true
        kill "$emulator_pid" 2>/dev/null || true
        wait "$emulator_pid" 2>/dev/null || true
    fi
    if [[ -n "$avd" ]]; then
        avdmanager delete avd -n "$avd" >/dev/null 2>&1 || true
    fi
    if [[ -n "$root" ]]; then
        for directory in dev proc system; do
            if mountpoint -q "$root/$directory"; then sudo umount "$root/$directory" || true; fi
        done
    fi
    if (( status != 0 )) && [[ -f "$logs/emulator.log" ]]; then
        tail -100 "$logs/emulator.log" || true
    fi
    exit "$status"
}
trap cleanup EXIT

prepare_emulator_tests() {
    local sdk=${ANDROID_HOME:?Set ANDROID_HOME} api=${ANDROID_API:?Set ANDROID_API}
    export PATH="$sdk/platform-tools:$sdk/emulator:$sdk/cmdline-tools/latest/bin:$PATH"
    export ANDROID_SERIAL=emulator-5554
    export ANDROID_AVD_HOME="$work/avd"
    mkdir -p "$ANDROID_AVD_HOME"
    adb start-server
    if adb devices | grep -q "^${ANDROID_SERIAL}[[:space:]]"; then
        echo "$ANDROID_SERIAL is already in use" >&2
        exit 1
    fi
    avd="fluidsynth-${abi}-$$"
    avdmanager create avd --name "$avd" --package "system-images;android-$api;default;$abi" \
        --path "$ANDROID_AVD_HOME/$avd.avd" --force <<< no
    emulator -avd "$avd" -port 5554 -no-window -no-snapshot -no-audio -no-boot-anim \
        -accel on -gpu swiftshader_indirect -cores 2 -memory 2048 -no-metrics > "$logs/emulator.log" 2>&1 &
    emulator_pid=$!
    local deadline=$((SECONDS + 300))
    until [[ $(timeout 10 adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r') == 1 ]]; do
        if ! kill -0 "$emulator_pid" 2>/dev/null || (( SECONDS >= deadline )); then
            echo 'Emulator did not finish booting within 300 seconds' >&2
            exit 1
        fi
        sleep 2
    done
    adb shell rm -rf "$device_tests"
    adb shell mkdir -p "$device_tests"
    adb push "$tests/." "$device_tests/"
    runner=(adb shell)
}

prepare_chroot_tests() {
    if [[ $(uname -m) != aarch64 ]]; then
        echo 'Native Bionic tests currently require an aarch64 host.' >&2
        exit 1
    fi
    if grep -l 'qemu-\(arm\|aarch64\)' /proc/sys/fs/binfmt_misc/* 2>/dev/null; then
        echo 'Disable QEMU binfmt handlers before checking native ARM execution.' >&2
        exit 1
    fi
    if ! grep -qx "ro.product.cpu.abi=$abi" "$work/runtime/system/build.prop"; then
        echo "Bionic runtime does not match the test ABI: $abi" >&2
        exit 1
    fi
    root="$work/root"
    mkdir -p "$root"/{system,proc,dev,data/local/tmp/fluidsynth-tests}
    # Hosted ARM runners have no nested KVM. Use the host kernel with Bionic;
    # Android framework services and device drivers are outside this coverage.
    sudo mount --bind "$work/runtime/system" "$root/system"
    sudo mount -o remount,bind,ro "$root/system"
    sudo mount -t proc proc "$root/proc"
    sudo mount --bind /dev "$root/dev"
    cp -a "$tests/." "$root$device_tests/"
    # Android's shell UID/GID is 2000. Keep test execution unprivileged.
    sudo chown -R 2000:2000 "$root/data"
    uname -a
    lscpu
    cat "$work/runtime/provenance.txt"
    grep -E '^ro\.(build.version.sdk|product.cpu.abi)=' "$work/runtime/system/build.prop"
    runner=(sudo chroot --userspec=2000:2000 "$root" /system/bin/sh -c)
}

case "$backend" in
    emulator) prepare_emulator_tests ;;
    chroot) prepare_chroot_tests ;;
    *) echo "Unsupported Android test backend: $backend" >&2; exit 1 ;;
esac

passed=0
failed=0
# Tests share output filenames, so execute them serially.
for executable in "$tests"/test_*; do
    [[ -f "$executable" && -x "$executable" ]] || continue
    name=${executable##*/}
    echo "Running $name"
    command="cd $device_tests && export LD_LIBRARY_PATH=$device_tests/lib && exec ./$name"
    if timeout 60 "${runner[@]}" "$command"; then
        echo "PASS: $name"
        passed=$((passed + 1))
    else
        status=$?
        echo "FAIL: $name (exit $status)"
        failed=$((failed + 1))
    fi
done
echo "Android tests: $passed passed, $failed failed"
(( passed + failed > 0 && failed == 0 ))
