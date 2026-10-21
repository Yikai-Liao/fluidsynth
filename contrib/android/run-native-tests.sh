#!/usr/bin/env bash
# Execute API 24 Android/Bionic binaries directly on the ARM host's Linux kernel.
# GitHub's hosted ARM runners do not expose nested KVM for an accelerated Android
# guest. Reuse their kernel to test both ARM ABIs without CPU emulation; this path
# does not exercise Android framework services or Android device drivers.
set -euo pipefail
abi=${1:?Usage: run-native-tests.sh <ABI> <runtime-directory> <test-bundle> <work-directory>}
runtime=$(realpath "${2:?Specify a runtime directory}")
bundle=$(realpath "${3:?Specify a test bundle}")
work=${4:?Specify a work directory}
test "$(uname -m)" = aarch64
case "$abi" in
    armeabi-v7a|arm64-v8a) ;;
    *) echo "Unsupported native Android ABI: $abi" >&2; exit 1 ;;
esac
if grep -l 'qemu-\(arm\|aarch64\)' /proc/sys/fs/binfmt_misc/* 2>/dev/null; then
    echo 'Disable QEMU binfmt handlers before checking native ARM execution.' >&2
    exit 1
fi
mkdir -p "$work"
work=$(realpath "$work")
root="$work/root"
mkdir -p "$root"/{system,proc,dev,data/local/tmp/fluidsynth-tests} "$work/logs"
cleanup() {
    local status=$?
    for directory in dev proc system; do
        if mountpoint -q "$root/$directory"; then sudo umount "$root/$directory"; fi
    done
    exit "$status"
}
trap cleanup EXIT
sudo mount --bind "$runtime/system" "$root/system"
sudo mount -o remount,bind,ro "$root/system"
sudo mount -t proc proc "$root/proc"
sudo mount --bind /dev "$root/dev"
tar -xzf "$bundle" -C "$root/data/local/tmp/fluidsynth-tests"
target="$root/data/local/tmp/fluidsynth-tests"
cp -a "$target/test/." "$target/"
sudo chown -R 2000:2000 "$root/data"
uname -a
lscpu
cat "$runtime/provenance.txt"
grep -E '^ro\.(build.version.sdk|product.cpu.abi)=' "$runtime/system/build.prop"
printf 'Execution: native %s CPU, Android API 24 Bionic, host Linux kernel\n' "$abi"
passed=0
failed=0
for executable in "$target"/test_*; do
    [[ -f "$executable" && -x "$executable" ]] || continue
    name=${executable##*/}
    file "$executable"
    if timeout --kill-after=5 60 sudo chroot --userspec=2000:2000 "$root" /system/bin/sh -c \
        'cd /data/local/tmp/fluidsynth-tests && export LD_LIBRARY_PATH=/data/local/tmp/fluidsynth-tests/lib:/system/lib64:/system/lib && exec "./$1"' sh "$name" \
        > "$work/logs/$name.log" 2>&1; then
        echo "PASS $name"
        passed=$((passed + 1))
    else
        echo "FAIL $name"
        cat "$work/logs/$name.log"
        failed=$((failed + 1))
    fi
done
printf 'ABI: %s\nPassed: %s\nFailed: %s\n' "$abi" "$passed" "$failed" | tee "$work/results.txt"
test "$passed" -gt 0
test "$failed" -eq 0
