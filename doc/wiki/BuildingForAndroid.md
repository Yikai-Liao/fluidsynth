# 🤖 Building for Android

This page demonstrates how to cross-compile the upstream version of fluidsynth for Android, along with **all the dependencies** that fluidsynth requires. Other users have taken shortcuts by custom modifications to fluidsynth's code base that we do not officially support.

You may find more information about FluidSynth on Android [in the /doc folder](https://github.com/FluidSynth/fluidsynth/tree/master/doc/android).

## Preface

So, you want to compile fluidsynth for Android? Are you aware of the fact that fluidsynth is written in C? That means you cannot benefit from nicely portable bytecode, i.e. compile once and deploy for all smartphone architectures and Android versions out there. The code being compiled will be **native** to the device and Android NDK you are targeting. 
Thus, you will need to deploy fat binaries to your users. You'll have to do this for all architectures and Android versions you want to support!

fluidsynth is licensed under **LGPL-2.1**; [make sure you understand what that means](LicensingFAQ) when using it in your app!

Note that many of fluidsynth's dependencies are licensed under the LGPL, too. If you **statically link** any of them into your proprietary, closed-source app, you violate the LGPL and end up with a serious license infringement!
Further note that fluidsynth can be optionally compiled with dependencies that are GPL licensed. In this case **fluidsynth and your app become GPL licensed**! If you don't respect those terms and understand its consequences, you are a potential subject to legal prosecution.

When using the methods and scripts explained here, there is no guarantee of any kind that they work as expected, esp. not in a commercial product. **NO warranty is provided; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE**.

If, despite all that, you still want to use fluidsynth on Android, ...well thank you, here's a guide that hopefully turns out to be useful and gives you an idea of the cross-compilation process as a whole.

If you just want to use fluidsynth on Android without going through all the compilation trouble yourself, you may use the build artifacts from the CI build (cf. README.md).

If you have questions or need support, contact our mailing list:
https://lists.nongnu.org/mailman/listinfo/fluid-dev

## Build with the Android NDK and vcpkg

The project uses the NDK's `android.toolchain.cmake`. The optional helper below
builds a shared-library SDK with Oboe, OpenSL ES, libsndfile and its external
codecs. CI calls the same helper. It does not change the default build of other
platforms or require vcpkg for their development.

Install CMake, Ninja, pkg-config, Git, curl and zip/unzip on your build host, and
install an NDK through the Android SDK manager. CI currently uses NDK
`27.3.13750724` and vcpkg `2026.10.16`. From a source checkout:

```sh
git -c submodule.test/manual.update=none submodule update --init --depth=1 --recursive
export ANDROID_NDK_HOME="$ANDROID_HOME/ndk/27.3.13750724"
export VCPKG_ROOT="$PWD/build/vcpkg"
git clone --depth=1 --branch 2026.10.16 https://github.com/microsoft/vcpkg.git "$VCPKG_ROOT"
export ABI=arm64-v8a ANDROID_API=24 BUILD_TYPE=RelWithDebInfo
bash doc/android/build.sh build-dependencies
bash doc/android/build.sh build-and-install
bash doc/android/build.sh package
```

Use `armeabi-v7a`, `arm64-v8a`, `x86` or `x86_64`. `ANDROID_API` defaults to 24
and must be at least 24; it is the minimum API of the native libraries and must
not exceed the application's `minSdk`. `BUILD_TYPE` accepts the normal CMake
build types, including `Debug`. The default build directory separates ABI, API
and build type under `build/android`; `ANDROID_BUILD_DIR` can select another
build directory. Use a separate directory when changing NDK versions.

The vcpkg manifest in `doc/android/vcpkg` requests Oboe and libsndfile with
external codecs. vcpkg selects and verifies the source releases, builds their
transitive dependencies and applies its maintained fixes. The small Android
triplets select shared libraries, the requested API and 16 KB ELF alignment.
To update dependencies, select a newer vcpkg release; no download hashes are
maintained in this project. CI selects its release with `VCPKG_VERSION`.

The helper installs headers, libraries and the existing `FluidSynthConfig.cmake`
package under `<build>/dependencies/<triplet>`. The asset-loader example uses
`find_package(FluidSynth CONFIG REQUIRED COMPONENTS shared)` and
`FluidSynth::libfluidsynth`, which is also the interface available to an Android
application's CMake project. For a Gradle `externalNativeBuild` project, let the
Android Gradle Plugin supply the NDK toolchain, ABI and minimum API; point CMake
at the matching installed package rather than copying the helper's toolchain
arguments into Gradle. Use a consistent NDK and `c++_shared` across shared
libraries; package one copy of `libc++_shared.so` with the application.

The `package` operation checks required features, shared-library presence and
every ELF LOAD segment. It writes headers and shared libraries to
`<build>/package/include` and `<build>/package/lib/<ABI>`. The NDK's 32-bit C++
runtime retains its 4 KB alignment exemption; 64-bit runtimes are checked for
16 KB alignment. Application/APK packaging and device compatibility tests remain
the consuming application's responsibility.

### Run Android unit tests

`build-and-install` builds the existing unit test executables. The `check` target
still does not execute cross-compiled tests automatically. `package` stages the
executables, test data, libraries and CMake-generated CTest definitions together
under `<build>/tests`. The test launcher maps CTest's executable paths into the
Android test directory, so this directory can be moved to another host.

For a connected device or an already running emulator:

```sh
export PATH="$ANDROID_HOME/platform-tools:$PATH"
# Set ANDROID_SERIAL if more than one device is attached.
bash test/run-android-tests.sh "build/android/$ABI-api$ANDROID_API-$BUILD_TYPE/tests"
```

CTest owns test selection, timeout, failure output and the JUnit report. ADB only
stages and launches the suite. The launcher is also usable with ordinary NDK
CMake builds by setting
`-DCMAKE_CROSSCOMPILING_EMULATOR=android-test-launcher.sh` and putting `test/`
on `PATH`. Host-side soundfont dump comparisons are not registered for Android;
the Android unit-test suite retains the existing 44 tests in CI's configuration.

CI runs x86 and x86_64 on API 24 KVM emulators. For the two ARM ABIs, SDK Manager
verifies a system image and `debugfs` (e2fsprogs) extracts its Bionic userspace.
ARM runners consume the built tests and userspace together. These tests use the
host Linux kernel and do not exercise Android framework services, audio devices
or an Android guest kernel. No device connection is required to build the SDK.

CI caches installed dependencies, emulator/system images and extracted ARM
userspace. Dependency keys include the host, ABI, API, NDK and vcpkg release;
update their revision when changing manifest features, triplets or installation
policy. Weekly scheduled builds bypass caches. Tests and the SDK are each built
once per ABI. Log artifacts include CTest output and JUnit reports.

## Prerequisites for the standalone toolchain guide

#### This guide...

* assumes that you are on a Linux machine.
* requires an active internet connection (for downloading dependency libraries).
* assumes that you have a solid understanding of how to compile C/C++ programs from source under Linux.
* assumes that you know about Autotools, CMake, Make, Clang, GCC, Shell.
* recommends that you are a C/C++ programmer (so that you are able to understand and fix compilation errors).
* uses the [Standalone Toolchain approach of the Android NDK](https://developer.android.com/ndk/guides/standalone_toolchain).
* uses clang for compilation as shipped by the Android NDK.
* has been successfully tested for Android NDK r17b and r17c.
* exemplary compiles for `arm-eabi`.

#### Android NDK

The NDK has a few prerequisites itself. Unfortunately, they don't seem to be documented. E.g. the `clang60` compiler shipped with the NDK may require `libtinfo.so.5` to be installed on your host system. So, make sure `libncurses5` is installed.
Keep an eye open for errors like

```
android-ndk-toolchain/bin/clang60: error while loading shared libraries: libtinfo.so.5: cannot open shared object file: No such file or directory
```

to capture even more missing dependencies that the NDK requires and make sure they are installed on your host system.

## Getting started

* Read the preface above!
* [Seriously, read the preface](#preface)!!
* Download the Android NDK
* Download [fluid-android-prepare.sh](https://gist.github.com/derselbst/885641b86b8bd0e5fd1b1d846d4b286b) and [fluid-android-compile.sh](https://gist.github.com/derselbst/88917bf177d25cfc1067a37b442dd5ff)
* Open `fluid-android-prepare.sh` and study it
* Adjust the `$DEV` and `$NDK` variables to your needs
* Adjust the target architecture and `$ANDROID_API` to your needs
* Open the `bash` shell, source the `fluid-android-prepare.sh`, make a folder
```shell
source fluid-android-prepare.sh
mkdir $DEV
```
* Provide the extracted NDK folder (e.g. android-ndk-r17b) at `$NDK` (i.e. move it there)
* Move `android-prepare.sh` and `android-compile.sh` into `$DEV`
* `cd $DEV`
* In the same shell, where you've just sourced `fluid-android-prepare.sh`, start the build process by
```shell
bash fluid-android-compile.sh
```

That's it. All dependencies are downloaded and compiled one after another. No errors should occur. 
Once done you should end up with a directory `$PREFIX` containing all binaries needed to run fluidsynth on Android (about 100 MiB on arm-eabi) that you can integrate into your favorite environment or IDE 
(cf. [Using prebuilt libraries on Android](Using-prebuilt-libraries-on-Android.md)).

## If compilation of dependencies fails

Study the output of autotools. Keep an eye open for any warnings that might come up. Also, study autotools `config.log`. You might try to ask for help at our mailing list. However, preferably you should contact the respective project directly.

## If compilation of fluidsynth fails

Ask for help at our mailing list. Provide as much information as possible! (Commands, log files, etc.) If you have already identified a bug in fluidsynth's build system or elsewhere, feel free to open a ticket directly here at GitHub.
