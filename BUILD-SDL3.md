# Building Fuse SDL3

This repository publishes the SDL3 fork of Fuse 1.6.0.

What is in this repository:

* the full downstream source tree;
* the instructions below for building and validating this tree directly.

Companion documents:

* `TESTING-SDL3.md` for runtime and native-Linux validation.
* `RELEASE-SDL3.md` for release preparation and validation.

What is intentionally not in this repository:

* a vendored read-only upstream Fuse mirror.

## Upstream base

This fork started from upstream tag `fuse-1.6.0` and is now maintained
directly in this repository.

## Required dependencies

Minimum required to build the SDL3 UI:

* C toolchain: `gcc` or `clang`
* `cmake`
* `pkg-config` on non-Windows platforms
* `libspectrum >= 1.5.0`
* `SDL3`
* `libxml2`

Recommended optional dependencies:

* `libpng`
* `libgcrypt`
* `zlib`
* `glib-2.0`
* `bzip2`

`glib-2.0` is required when your `libspectrum` build does not provide its own
internal GLib replacement. Installing it is the simplest option on most Linux
distributions.

## Example package installs

Debian or Ubuntu:

```sh
sudo apt update
sudo apt install build-essential cmake pkg-config \
  libspectrum-dev libsdl3-dev libglib2.0-dev libpng-dev libxml2-dev \
  libgcrypt20-dev zlib1g-dev
```

Fedora:

```sh
sudo dnf install gcc make cmake pkgconf-pkg-config \
  libspectrum-devel SDL3-devel glib2-devel libpng-devel libxml2-devel \
  libgcrypt-devel zlib-devel
```

Arch Linux:

```sh
sudo pacman -S --needed base-devel cmake pkgconf \
  libspectrum sdl3 glib2 libpng libxml2 libgcrypt zlib
```

Check that the required pkg-config modules are visible:

```sh
pkg-config --modversion sdl3 libspectrum libxml-2.0
```

## Primary build commands

These are the supported short paths for day-to-day development:

Linux:

```sh
sh ./scripts/build-linux.sh
```

Windows PowerShell:

```powershell
./scripts/build-windows.ps1
```

Both commands configure a CMake build, compile Fuse SDL3, and finish with a
runtime smoke test using `fuse -V` or `fuse.exe -V`.

To also produce binary distribution archives from the install tree:

Linux:

```sh
sh ./scripts/build-linux.sh --package
```

Windows PowerShell:

```powershell
./scripts/build-windows.ps1 -Package
```

To build a Linux AppImage from the same portable layout:

```sh
sh ./scripts/build-linux.sh --appimage
```

Each packaging flow emits one platform-specific archive into the selected build
directory: Linux writes `.tar.gz`, while Windows writes `.zip`. The archive is
laid out as a portable bundle with the executable and default config file at
the archive root. Linux places bundled runtime libraries under `lib/`, while
Windows keeps them at the archive root. `roms/` sits beside them, and the
default config file remains in the root
(`.fuserc.default` on Linux, `fuse.cfg.default` on Windows). The non-ROM UI assets are embedded
in the executable, so the package payload does not include a `share/`
directory. Packaging mode configures a `Release` build so the archive payload
is suitable for redistribution.

The `--appimage` flow uses that same portable Linux layout, adds an `AppRun`
launcher, and builds a single `.AppImage`. If `appimagetool` is not installed,
the Linux script downloads a matching binary automatically.

If you need package installation details, custom prefixes, or lower-level
manual invocations, use the platform sections below.

## Native Windows dependency path

The supported Windows dependency workflow uses:

* `winget` for host tools needed by this repository (`perl`, `pkg-config`,
  `ninja`);
* a repo-local `vcpkg` checkout for native third-party libraries; and
* a native `clang-cl` CMake configure, optionally pointed at a prebuilt
  `libspectrum` prefix with `FUSE_LIBSPECTRUM_ROOT`.

Bootstrap the Windows host and native libraries from PowerShell:

```powershell
./scripts/bootstrap-windows-deps.ps1
```

For the standard native Windows path from a regular PowerShell prompt, use the
single entry point:

```powershell
./scripts/build-windows.ps1
```

That script installs host tools if needed, bootstraps `external/vcpkg`,
installs the pinned native libraries from `vcpkg.json`, builds the local
overlay `libspectrum` port natively under Windows into the matching `vcpkg`
triplet prefix, builds Fuse, stages the runtime DLLs, and verifies the result
with `fuse.exe -V`.

The native `libspectrum` build path for this repository is:

* source checkout in `external/libspectrum`;
* local overlay port in `vcpkg-ports/libspectrum`; and
* `vcpkg_configure_make(... USE_WRAPPERS ...)`, which runs autotools through
  the MSVC/clang-cl wrapper path instead of producing a MinGW-only build.

If you need to build `libspectrum` by itself before configuring Fuse, run:

```powershell
./external/vcpkg/vcpkg.exe install --classic --triplet x64-windows `
  --overlay-ports "$PWD/vcpkg-ports" libspectrum
```

That produces a native Windows prefix under:

```text
external/vcpkg/installed/x64-windows
```

The Fuse CMake build also accepts:

```text
-DFUSE_LIBSPECTRUM_ROOT=C:/path/to/libspectrum-prefix
```

where that prefix contains `include/libspectrum.h` plus a native Windows
`libspectrum` library built with the same ABI.

The Windows bootstrap installs these host-side tools when missing:

* Strawberry Perl;
* `pkg-config-lite`;
* `ninja`; and
* WinFlexBison (`win_flex` and `win_bison`) for debugger source generation.

## Native Windows manual CMake build with clang-cl

From a regular PowerShell prompt in the repository root:

```powershell
cmake -S . -B build-windows -G Ninja `
  -DCMAKE_C_COMPILER=clang-cl `
  -DCMAKE_TOOLCHAIN_FILE="$PWD/external/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DFUSE_LIBSPECTRUM_ROOT="$PWD/external/vcpkg/installed/x64-windows"

cmake --build build-windows
```

If you prefer the Visual Studio CMake generator after the command-line path is
working, keep the same toolchain and cache variables and switch only the
generator.

Runtime expectations on Windows:

* `fuse.exe` is produced under the chosen build directory;
* `roms/` is staged next to the executable; and
* the primary helper script also stages the dependency DLLs beside `fuse.exe`
  before running the smoke test.

## Native Linux package setup

Install the required development packages, then run the primary helper:

```sh
sh ./scripts/build-linux.sh
```

That produces `build-linux/fuse` by default and verifies the executable with
`fuse -V`.

## Native Linux manual CMake build

The CMake build models the retained downstream Linux target
directly: core emulator code, retained compatibility code, SDL UI, widget UI,
retained generated-source rules, SDL sound, and the SDL timer path.

From the repository root:

```sh
cmake -S . -B build-linux
cmake --build build-linux -j"$(nproc)"
```

The generated sources stay under `build-linux/`, so the CMake path does not
depend on pre-generated outputs from removed frontend trees.

## Install with CMake

To install a built tree:

```sh
cmake --install build-linux --prefix /usr/local
```

On Windows:

```powershell
cmake --install build-win-native --prefix "$PWD/dist"
```

The install step keeps `roms/` adjacent to the executable. The remaining UI
support assets are compiled into the binary. Portable package archives flatten
the executable and runtime libraries into the archive root and include a
default config file there as well.

The `package` target uses that same install layout and writes binary
distribution archives under the build directory:

* Linux: `fuse-sdl3-<version>-<system>-<arch>.tar.gz`
* Windows: `fuse-sdl3-<version>-<system>-<arch>.zip`

Inside those archives you should expect:

* the executable at the archive root
* bundled runtime libraries under `lib/` on Linux and at the archive root on Windows
* `roms/` at the archive root
* `.fuserc.default` on Linux or `fuse.cfg.default` on Windows at the archive root

You can also invoke packaging manually after a configure step:

```sh
cmake --build build-linux --target package
```

```powershell
cmake --build build-windows --target package
```

On Linux, the install step also stages:

* `share/applications/io.github.md0_code.FuseSDL3.desktop`
* `share/mime/packages/fuse.xml`
* hicolor app and MIME icons under `share/icons/hicolor/`
* the bash completion file under `share/bash-completion/completions/` when present

## Building libspectrum from source if your distro package is too old

This fork requires `libspectrum >= 1.5.0`. If your package manager does not
provide a new enough version, build and install it into a local prefix:

```sh
git clone https://github.com/speccytools/libspectrum.git
cd libspectrum
autoreconf -fi
./configure --prefix="$HOME/opt/libspectrum"
make -j"$(nproc)"
make install
```

Then point Fuse at that pkg-config directory:

```sh
cd /path/to/fuse-sdl3
PKG_CONFIG_PATH="$HOME/opt/libspectrum/lib/pkgconfig" \
  sh ./scripts/build-linux.sh
```

## Minimal smoke tests

For the CMake build:

```sh
timeout -s KILL 5s ./build-linux/fuse --no-sound --machine 48
```

On a headless system or in CI:

```sh
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  timeout -s KILL 5s ./build-linux/fuse --no-sound --machine 48
```

After building, confirm the executable starts:

```sh
timeout 5s ./build-linux/fuse --no-sound --machine 48
```

On a headless system or in CI, use SDL dummy drivers:

```sh
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  timeout 5s ./fuse --no-sound --machine 48
```

For a fuller runtime checklist, including native-Linux fullscreen validation,
see `TESTING-SDL3.md`.

## Debian sid Wayland note

On at least one Debian sid system, native Wayland startup reached SDL video
initialization but crashed inside the system `libdecor` GTK plugin stack rather
than inside Fuse. Linux builds now probe that path in a child process before
the real SDL video initialization. If the probe crashes and Xwayland is
available, Fuse switches SDL to X11 automatically:

```sh
SDL_VIDEODRIVER=x11 ./fuse
```

If X11 is not available, the older no-plugin workaround is still used:

```sh
LIBDECOR_PLUGIN_DIR=/nonexistent SDL_VIDEODRIVER=wayland ./fuse
```

If you want to force that fallback yourself, the equivalent manual command is:

```sh
SDL_VIDEODRIVER=x11 ./fuse
```

This issue appears to be specific to the runtime `libdecor` environment on that
system, not to the downstream SDL3 rendering path itself. Set
`FUSE_SDL_DISABLE_LIBDECOR_WORKAROUND=1` to bypass the automatic crash probe and
fallback logic when testing the system `libdecor` stack directly.

## SDL3 presentation notes

This fork uses SDL3 for the SDL UI and supports aspect-correct fullscreen
presentation. Texture filtering can be selected with `FUSE_SDL_SCALE_MODE`:

```sh
FUSE_SDL_SCALE_MODE=nearest ./fuse --machine 48
FUSE_SDL_SCALE_MODE=linear ./fuse --machine 48
FUSE_SDL_SCALE_MODE=pixelart ./fuse --machine 48
```

When preparing a public update of this fork, follow `RELEASE-SDL3.md` so the
branch head, build instructions, and validation notes stay aligned.