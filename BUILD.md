# Building Fuse SDL3

Fuse SDL3 currently supports Windows (including WSL2) and Linux native builds.
This document describes the basic build process starting from the source code available in this repository.

### Linux

Minimum required to build the SDL3 UI on Linux:

* C toolchain: `gcc` or `clang`
* `cmake`
* `pkg-config`
* `libspectrum >= 1.5.0`
* `SDL3`
* `libxml2`

Recommended optional Linux dependencies:

* `libpng`
* `libgcrypt`
* `zlib`
* `glib-2.0`
* `bzip2`

`glib-2.0` is required when your `libspectrum` build does not provide its own
internal GLib replacement. Installing it is the simplest option on most Linux
distributions.

### Linux example package installs

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

### Windows

Minimum required to build with the supported native Windows workflow:

* Visual Studio or Build Tools with the `Desktop development with C++` workload
* `winget`
* a repo-local `vcpkg` checkout managed by `scripts/build-win.ps1`

The Windows helper script installs these host-side tools when missing:

* Perl — the script first reuses the Perl bundled with Git for Windows
  (`<git-root>\usr\bin\perl.exe`) if Git is already installed, and only falls
  back to installing [Strawberry Perl](https://strawberryperl.com/) via winget
  when no Perl interpreter is found at all. All scripts use only core Perl
  modules so any standard Perl distribution works.
* `pkg-config-lite`
* `ninja`
* WinFlexBison (`win_flex` and `win_bison`)

The Windows helper also resolves the C compiler from the active Visual Studio
toolchain, preferring `clang-cl` and falling back to `cl.exe`, and installs the
native third-party libraries through the repo-local `vcpkg` manifest instead of
using system packages.

## Primary build commands

These are the supported short paths for day-to-day development:

Linux:

```sh
sh ./scripts/build-linux.sh
```

Windows PowerShell:

```powershell
./scripts/build-win.ps1
```

Both commands configure a CMake build, compile Fuse SDL3, and finish with a
compiled binary in the selected build directory.

## Building the libretro core

The repo also builds an in-tree libretro core target named `fuse-libretro`.
On Windows this produces `fuse_libretro.dll` in the selected build directory.

After configuring the build tree, build only the libretro target with:

```powershell
cmake --build build-win --target fuse-libretro
```

The libretro target stages the runtime asset directories it needs into the same
build output directory:

- `build-win/roms`
- `build-win/lib`

Those staged directories are required because the core resolves ROMs and other
auxiliary assets relative to the current working directory when hosted by
RetroArch.

## RetroArch smoke test

For Windows, the repo includes a repeatable RetroArch smoke test script for the
libretro target:

```powershell
./scripts/smoke-test-libretro.ps1 -RetroArchRoot "D:\oldgames\frontends\retroarch"
```

By default the script:

- uses `build-win/fuse_libretro.dll`
- creates a tiny deterministic Interface 2 cartridge at
  `build-win/libretro-smoke/if2-loop.rom`
- runs RetroArch for a fixed number of frames using `--max-frames`
- writes a log to `build-win/libretro-smoke/retroarch-smoke.log`
- captures a screenshot to `build-win/libretro-smoke/retroarch-smoke.png`

You can override the content path if you want to test a real snapshot, tape,
disk, or cartridge file:

```powershell
./scripts/smoke-test-libretro.ps1 `
  -RetroArchRoot "D:\oldgames\frontends\retroarch" `
  -ContentPath .\some-test.z80
```

To also produce binary distribution archives from the install tree:

Linux:

```sh
sh ./scripts/build-linux.sh --package
```

Windows PowerShell:

```powershell
./scripts/build-win.ps1 -Package
```

To build a Linux AppImage from the same portable layout:

```sh
sh ./scripts/build-linux.sh --appimage
```

Each packaging flow emits one platform-specific archive into the selected build
directory: Linux writes `.tar.gz`, while Windows writes `.zip`. The archive is
laid out as a portable bundle with the executable and default config file at
the archive root.

The `--appimage` flow uses that same portable Linux layout, adds an `AppRun`
launcher, and builds a single `.AppImage`. If `appimagetool` is not installed,
the Linux script downloads a matching binary automatically.

## Installing and uninstalling on Linux

After building, install to the default prefix (`/usr/local`):

```sh
sudo sh ./scripts/build-linux.sh --install
```

Or combine the build and install in one command with a custom prefix:

```sh
sh ./scripts/build-linux.sh --install --install-prefix=/usr
```

To uninstall, pass `--uninstall` with the same build directory used to install:

```sh
sudo sh ./scripts/build-linux.sh --uninstall
```

This removes every file that was recorded in `build-linux/install_manifest.txt`
during the install step.

## Script help

Both build scripts document all supported options:

Linux:

```sh
sh ./scripts/build-linux.sh --help
```

Windows PowerShell:

```powershell
Get-Help ./scripts/build-win.ps1 -Detailed
```
