#!/usr/bin/env bash
# Fingerprint the hosted Homebrew environment before restoring CMake discoveries.
set -euo pipefail
eligible=true
while IFS= read -r name; do
    [[ -n "${!name-}" ]] || continue
    case "$name" in
        # build.sh replaces this path with libffi's directory, recorded below.
        PKG_CONFIG_PATH) continue ;;
        CMAKE_*|PKG_CONFIG|PKG_CONFIG_*)
            echo "Bypass Xcode configuration cache for custom $name"
            eligible=false ;;
    esac
done < <(compgen -e)
echo "eligible=$eligible" >> "$GITHUB_OUTPUT"
if [[ "$eligible" != true ]]; then exit 0; fi
{
    printf '%s\n' "$GITHUB_WORKSPACE" "$RUNNER_TEMP"
    command -v cmake
    command -v pkg-config
    cmake --version
    pkg-config --version
    xcode-select -p
    xcodebuild -version
    xcrun --find clang
    xcrun --show-sdk-path
    xcrun --show-sdk-build-version
    brew list --versions
    # Include package roots/directories without mistaking unrelated image tool
    # roots for overrides. Hash the values instead of exposing them in artifacts.
    roots_hash=$(
        while IFS= read -r name; do
            if [[ "$name" == *_ROOT || "$name" == *_DIR ]]; then
                printf '%s=%s\n' "$name" "${!name-}"
            fi
        done < <(compgen -e | LC_ALL=C sort) | shasum -a 256 | awk '{print $1}'
    )
    printf 'package-roots=%s\n' "$roots_hash"
    # build.sh replaces PKG_CONFIG_PATH with this path for the framework profile.
    printf 'PKG_CONFIG_PATH=%s/lib/pkgconfig\n' "$(brew --prefix libffi)"
    for name in PATH CC CXX CFLAGS CXXFLAGS CPPFLAGS LDFLAGS ARCHFLAGS SDKROOT \
        DEVELOPER_DIR TOOLCHAINS MACOSX_DEPLOYMENT_TARGET CPATH C_INCLUDE_PATH \
        CPLUS_INCLUDE_PATH OBJC_INCLUDE_PATH LIBRARY_PATH; do
        printf '%s=%s\n' "$name" "${!name-}"
    done
} > "$RUNNER_TEMP/xcode-environment.txt"
echo "hash=$(shasum -a 256 "$RUNNER_TEMP/xcode-environment.txt" | awk '{print $1}')" >> "$GITHUB_OUTPUT"
