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
