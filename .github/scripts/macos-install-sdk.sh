#!/usr/bin/env bash
# Install the universal MacPorts SDK used by macOS CI.
set -euo pipefail
if [[ -e /opt/local/bin/port ]]; then
    echo 'MacPorts preparation requires a fresh /opt/local; the CI dependency cache should bypass installation.' >&2
    exit 1
fi

MACPORTS_VERSION=2.12.7
MACPORTS_SHA256_15=87b2e87ed7adc4aca947b42eb7ff991614cc0edfccb486237f2bbc75f28aae63
MACPORTS_SHA256_26=588f14ccd3c6199559a070a80e6cddd6871721a08cc434775bf54378fd667910
MACPORTS_SOURCE_URL=https://distfiles.macports.org/ports.tar.gz
os_version=$(sw_vers -productVersion)
case "${os_version%%.*}" in
    15) package="MacPorts-$MACPORTS_VERSION-15-Sequoia.pkg"; package_sha=$MACPORTS_SHA256_15 ;;
    26) package="MacPorts-$MACPORTS_VERSION-26-Tahoe.pkg"; package_sha=$MACPORTS_SHA256_26 ;;
    *) echo "No pinned MacPorts installer for macOS $os_version" >&2; exit 1 ;;
esac
work_dir=$(mktemp -d "${TMPDIR:-/tmp}/fluidsynth-macports.XXXXXX")
trap 'rm -rf "$work_dir"' EXIT
curl --fail --location --retry 3 -o "$work_dir/base.pkg" \
    "https://github.com/macports/macports-base/releases/download/v$MACPORTS_VERSION/$package"
printf '%s  %s\n' "$package_sha" "$work_dir/base.pkg" | shasum -a 256 -c -
# Let the installer synchronize the official signed tree and prebuilt index.
sudo mkdir -p /opt/local/etc/macports
printf '%s [default]\n' "$MACPORTS_SOURCE_URL" | sudo tee /opt/local/etc/macports/sources.conf >/dev/null
sudo installer -pkg "$work_dir/base.pkg" -target /
/opt/local/bin/port version

printf '+universal\n' | sudo tee /opt/local/etc/macports/variants.conf >/dev/null
sudo tee -a /opt/local/etc/macports/macports.conf >/dev/null <<'CONFIG'
buildfromsource ifneeded
universal_archs arm64 x86_64
ui_interactive no
CONFIG
printf 'buildmakejobs %s\n' "$(sysctl -n hw.logicalcpu)" | sudo tee -a /opt/local/etc/macports/macports.conf >/dev/null
sudo /opt/local/bin/port -N install libsndfile readline
sudo mkdir -p /opt/local/share/fluidsynth-ci
/opt/local/bin/port version | sudo tee /opt/local/share/fluidsynth-ci/macports-version.txt
/opt/local/bin/port installed | sudo tee /opt/local/share/fluidsynth-ci/macports-packages.txt
