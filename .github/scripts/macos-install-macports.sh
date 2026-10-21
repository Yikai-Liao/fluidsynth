#!/usr/bin/env bash
set -euo pipefail
if [[ -e /opt/local/bin/port ]]; then
    echo 'MacPorts is already installed at /opt/local/bin/port.' >&2
    exit 1
fi

version=${MACPORTS_VERSION:?Set MACPORTS_VERSION to the pinned release}
os_version=$(sw_vers -productVersion)
case "${os_version%%.*}" in
    15)
        package="MacPorts-$version-15-Sequoia.pkg"
        checksum=87b2e87ed7adc4aca947b42eb7ff991614cc0edfccb486237f2bbc75f28aae63
        ;;
    26)
        package="MacPorts-$version-26-Tahoe.pkg"
        checksum=588f14ccd3c6199559a070a80e6cddd6871721a08cc434775bf54378fd667910
        ;;
    *) echo "No pinned MacPorts installer for macOS $os_version" >&2; exit 1 ;;
esac
work_dir=$(mktemp -d "${TMPDIR:-/tmp}/fluidsynth-macports.XXXXXX")
trap 'rm -rf "$work_dir"' EXIT
curl --fail --location --retry 3 -o "$work_dir/base.pkg" \
    "https://github.com/macports/macports-base/releases/download/v$version/$package"
printf '%s  %s\n' "$checksum" "$work_dir/base.pkg" | shasum -a 256 -c -
# Point the installer's source sync at the official signed ports tree.
sudo mkdir -p /opt/local/etc/macports
printf '%s [default]\n' https://distfiles.macports.org/ports.tar.gz | sudo tee /opt/local/etc/macports/sources.conf >/dev/null
sudo installer -pkg "$work_dir/base.pkg" -target /

printf '+universal\n' | sudo tee /opt/local/etc/macports/variants.conf >/dev/null
jobs=$(sysctl -n hw.logicalcpu)
sudo tee -a /opt/local/etc/macports/macports.conf >/dev/null <<CONFIG
buildfromsource ifneeded
universal_archs arm64 x86_64
ui_interactive no
buildmakejobs $jobs
CONFIG
sudo /opt/local/bin/port -N install libsndfile readline
