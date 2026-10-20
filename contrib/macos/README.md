# Native macOS CI entry points

The GitHub Actions experiment uses Homebrew and MacPorts directly. The workflow
selects runner images, restores dependency caches, calls these entry points and
uploads artifacts. Azure remains enabled during the experiment.

On a macOS host with Xcode command-line tools, CMake, Ninja and pkg-config installed:

```sh
git -c submodule.test/manual.update=none submodule update --init --depth=1 --recursive
bash contrib/macos/dependencies.sh brew
bash contrib/macos/build.sh unix-static build/macos-static
```

`profiles.cmake` owns CMake configuration choices. Available profiles are
`unix-shared`, `unix-static`, `unix-static-readline`, `framework`,
`framework-10.14` and `ports-universal`. Use a separate build directory for each
profile. `BUILD_JOBS` sets build and CTest parallelism (default: all logical CPUs).
`CTEST_PARALLEL_LEVEL` can override the test limit separately. Native builds use
Ninja; framework generator checks use Xcode.

Each build compiles FluidSynth, runs `check`, builds demos, stages the default
installation using `DESTDIR`, then reconfigures and rebuilds for its artifact
prefix. Unix library builds compile and execute both a pkg-config `--static`
consumer and an installed CMake consumer. Framework builds also compile with the
Xcode generator in Debug once per OS, in the `framework` profile. The 10.14 row
retains its deployment-target build/tests; Azure's repeated default Xcode check
in that row is covered by the regular framework row. Xcode reuses the same
FetchContent source checkouts as the Ninja build, with separate generator
metadata. Artifacts are under `<build>/package`; these are SDK
staging artifacts and retain package-manager runtime paths, rather than a
relocatable application bundle.

The matrix preserves eight Homebrew configurations across macOS 15 and 26,
including the separate readline pkg-config path and macOS 10.14 deployment-target
check, plus two MacPorts universal builds. Explicit Intel runner labels preserve
the x86_64 host environment and the 10.14 check. MacPorts compiles both arm64 and
x86_64 slices; unit tests execute the host slice. SDL3 replaces the obsolete SDL2
package/options because the current FluidSynth CMake configuration uses SDL3.

For the universal profile, provide universal libsndfile and readline installations
under `/opt/local`, then run:

```sh
bash contrib/macos/build.sh ports-universal build/macos-universal
```

`dependencies.sh ports` prepares a **fresh macOS host**: it installs MacPorts into
`/opt/local` and sets universal variants. It requires sudo and refuses to replace
an existing MacPorts installation. Installer archives are checked against
`macports.lock`. The installer synchronizes the official signed HTTPS ports tree
and its prebuilt platform index. MacPorts selects official package archives or
builds missing universal variants; no local portgroup patches are applied.
Resolved versions are printed and saved under `/opt/local/share/fluidsynth-ci`.
Universal artifacts include installed dependency dylibs, and `lipo` checks both
FluidSynth slices.

To reduce repeated work, CI skips the large manual-test submodule, builds independent
matrix jobs in parallel, caches Homebrew downloads and caches installed MacPorts
headers, libraries and support files before FluidSynth validation. Ports sources
and build directories are excluded from this SDK cache. FetchContent downloads
and population metadata have a separate cache scoped to the image, profile and
dependency recipe. MacPorts keys include the runner image version, OS/architecture and the setup recipe, with no
partial-key restore. A manual run with `clean_dependencies` bypasses the dependency caches.
Homebrew formula versions follow the runner's Homebrew metadata; only its download
cache is reused, not an installed prefix. This workflow is limited to experiment
branches and manual runs and does not change release publishing.
