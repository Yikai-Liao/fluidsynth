# Fork-only CI environment experiment

Base: FluidSynth/fluidsynth 414fb866b544d124103ff60d70c06355915eb34a.

Compare the original Linux dependency preparation with the same package list without the toolchain PPA, and with Docker Official Images buildpack-deps:jammy and buildpack-deps:bookworm plus that package list. The container pull and initialization count toward total job duration. No image is published and no dependency cache is used in this first comparison.

The screen includes the profiling GCC configuration and static Clang configuration. The full scope preserves all 13 native configurations and the existing PowerPC cross-build. All original native build, test, demo, install, pkg-config and CMake consumer checks run. Other workflows are manual-only on this experimental branch to prevent unrelated builds. These trigger changes are not an upstream proposal.

Compare compiler/package versions, enabled features and test inventory before attributing timing differences to setup. Record per-step durations, total runner seconds and workflow elapsed time; a Docker job is not equivalent merely because it passes.

## Cache budget if caching is retained later

Keep the repository default at 10 GB. Plan aggregate ceilings, including retained generations: Linux 1 GB, Windows 1.5 GB, macOS 2 GB, Android 2 GB, Alpine 0.5 GB. Reserve 3 GB. These are proposed budgets, not quotas enforced by GitHub. Measure cache size before adopting any cache design.

Share dependencies across feature variants. Key only on relevant OS/architecture/toolchain and dependency versions; do not key on every source commit. Bound retained generations, provide periodic dependency refresh, and skip saves above an entry budget. Do not cache preinstalled SDK/NDK or publish build artifacts as dependency caches.

The Jammy container receives the official CMake 3.31.6 binary to match the hosted runner (the distro CMake 3.22 is below the Signalsmith submodule requirement). Bookworm uses its distro CMake 3.25. It retains Clang 15 but changes the default GCC/libc/dependency versions, so compare coverage separately from package equivalence.

Debian libsndfile 1.2 exposes MP3 dependencies in its static pkg-config link flags. Install libmp3lame-dev and libmpg123-dev for Debian containers so the existing static-consumer check remains intact. Push runs now use the full matrix; manual dispatch can still select the two-configuration screen.

## APT cache experiment

linux-cache.yml compares the original setup against a dependency-only .deb bundle. The cold run downloads the same packages through the original installer and saves one shared cache. Warm runs restore that bundle, verify its runner identity and checksums, unpack/configure the packages offline, and compare the full installed-package manifest with the producer. All build/test/consumer checks remain unchanged.

The key includes runner image version, architecture, dependency recipe hash, and ISO week for periodic refresh. There is no prefix fallback. Only the profiling cell writes the shared entry, and entries above 450 MiB are not saved. This fork experiment uses one namespace and measures its actual size before any further rollout. No system directories or build outputs are cached.
