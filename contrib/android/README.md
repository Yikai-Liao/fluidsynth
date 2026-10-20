# Android native superbuild

This experimental entry point builds Ogg, Vorbis, FLAC, Opus, libsndfile and
Oboe, then FluidSynth, its native tests and the Android asset loader. Dependency
versions, SHA-256 hashes, options and ordering live in `dependencies.cmake`.
Azure remains enabled while the GitHub Actions migration is evaluated.

On Linux, install CMake 3.24 or newer, Ninja, pkg-config and Google's Android SDK
command-line tools. Install the same NDK as CI:

```sh
sdkmanager 'ndk;27.3.13750724'
git -c submodule.test/manual.update=none submodule update --init --depth=1 --recursive
cmake -S contrib/android -B build/android-arm64 -G Ninja \
  -DANDROID_NDK="$ANDROID_HOME/ndk/27.3.13750724" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=24
cmake --build build/android-arm64 --target android-package --parallel 2
```

Use a separate build directory for each ABI (`armeabi-v7a`, `arm64-v8a`, `x86`,
`x86_64`). The NDK's official `android.toolchain.cmake` configures every child
project. The default configuration is RelWithDebInfo, shared libraries and float
DSP samples, matching Azure. `ANDROID_BUILD_JOBS` sets each child's parallelism; CI
uses all runner CPUs. Preinstalled emulator/ADB binaries are reused.
The superbuild stages everything in `<build>/prefix`; `<build>/package` contains
FluidSynth headers and unversioned libraries under `lib/<ABI>`, including
`libc++_shared.so`. Packaging runs the existing ELF alignment checker on every
built library, plus the NDK runtime for 64-bit ABIs. The NDK's 32-bit runtime is
exempt because 16 KB Android devices use 64-bit ABIs. Applications must also comply with these dependencies' licenses.

For emulator tests, build x86 or x86_64, install the matching API 24 image and
ensure the current user can access `/dev/kvm`:

```sh
sdkmanager 'emulator' 'platform-tools' 'system-images;android-24;default;x86_64'
bash contrib/android/run-emulator.sh build/android-x86_64 x86_64
```

The script creates its own AVD, waits for Android to finish booting, calls the
existing `test/run-android-tests.sh`, saves results and emulator/logcat output to
`<build>/logs`, and removes its emulator and AVD on exit. Port 5554 must be free.
ARM builds are compiled and checked for ELF alignment; they are not run under
software emulation on the x86 GitHub runner. ELF alignment checks do not establish
runtime compatibility with a 16 KB page-size device.

The workflow caches the emulator binary, API 24 system images and the dependency
installation prefix. CMake FetchContent downloads and population metadata have
a separate cache scoped to the ABI, NDK, runner image and dependency recipe.
The native dependency prefix is saved before
FluidSynth is installed. Keys include ABI, API, NDK version and dependency/build
recipes, with no partial-key fallback. On a hit, CI configures with
`ANDROID_BUILD_DEPENDENCIES=OFF`; this requires an already populated prefix. A
manual run with `clean_dependencies` checks an entirely uncached build. The
workflow currently runs on this experiment's branches or manual dispatch; it does
not publish releases or replace Azure triggers yet.
