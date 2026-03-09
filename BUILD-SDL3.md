# Building Fuse SDL3

This repository publishes the SDL3 downstream fork of Fuse and the exact patch
series used to derive it from upstream Fuse 1.6.0.

What is in this repository:

* the full downstream source tree;
* the exported patch stack in `exported-patches/`; and
* the instructions below for building either this tree directly or rebuilding
  the same result from upstream plus patches.

Companion documents:

* `TESTING-SDL3.md` for runtime and native-Linux validation.
* `RELEASE-SDL3.md` for publishing updated patch exports and release artifacts.

What is intentionally not in this repository:

* a vendored read-only upstream Fuse mirror.

## Supported upstream base

The published patch series applies to upstream tag `fuse-1.6.0`.

## Required dependencies

Minimum required to build the SDL3 UI:

* C toolchain: `gcc` or `clang`, `make`
* Autotools: `autoconf`, `automake`, `libtool`, `pkg-config`
* `libspectrum >= 1.5.0`
* `SDL3`

Recommended optional dependencies:

* `libpng`
* `libxml2`
* `libgcrypt`
* `zlib`
* `glib-2.0`

`glib-2.0` is required when your `libspectrum` build does not provide its own
internal GLib replacement. Installing it is the simplest option on most Linux
distributions.

## Example package installs

Debian or Ubuntu:

```sh
sudo apt update
sudo apt install build-essential autoconf automake libtool pkg-config \
  libspectrum-dev libsdl3-dev libglib2.0-dev libpng-dev libxml2-dev \
  libgcrypt20-dev zlib1g-dev
```

Fedora:

```sh
sudo dnf install gcc make autoconf automake libtool pkgconf-pkg-config \
  libspectrum-devel SDL3-devel glib2-devel libpng-devel libxml2-devel \
  libgcrypt-devel zlib-devel
```

Arch Linux:

```sh
sudo pacman -S --needed base-devel autoconf automake libtool pkgconf \
  libspectrum sdl3 glib2 libpng libxml2 libgcrypt zlib
```

Check that the required pkg-config modules are visible:

```sh
pkg-config --modversion sdl3 libspectrum
```

## Build directly from this repository

Clone the downstream fork and build the SDL3 UI:

```sh
git clone --branch sdl3-integration https://github.com/md0-code/fuse-sdl3.git
cd fuse-sdl3
./configure --with-sdl
make -j"$(nproc)"
```

If you are building from a fresh git checkout and autotools complains about
generated files, regenerate them first:

```sh
autoreconf -fi
./configure --with-sdl
make -j"$(nproc)"
```

Install system-wide if required:

```sh
sudo make install
```

## Rebuild from upstream plus the published patches

If you want to reproduce the downstream fork from pristine upstream source,
apply the exported patch series to upstream Fuse 1.6.0:

```sh
git clone https://git.code.sf.net/p/fuse-emulator/fuse fuse-upstream
cd fuse-upstream
git checkout fuse-1.6.0
git am /path/to/fuse-sdl3/exported-patches/*.patch
autoreconf -fi
./configure --with-sdl
make -j"$(nproc)"
```

That gives you the same SDL3 downstream code line that is published in this
repository, without requiring a vendored upstream mirror.

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
  ./configure --with-sdl
make -j"$(nproc)"
```

## Minimal smoke test

After building, confirm the executable starts:

```sh
timeout 5s ./fuse --no-sound --machine 48
```

On a headless system or in CI, use SDL dummy drivers:

```sh
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  timeout 5s ./fuse --no-sound --machine 48
```

For a fuller runtime checklist, including native-Linux fullscreen validation,
see `TESTING-SDL3.md`.

## SDL3 presentation notes

This fork uses SDL3 for the SDL UI and supports aspect-correct fullscreen
presentation. Texture filtering can be selected with `FUSE_SDL_SCALE_MODE`:

```sh
FUSE_SDL_SCALE_MODE=nearest ./fuse --machine 48
FUSE_SDL_SCALE_MODE=linear ./fuse --machine 48
FUSE_SDL_SCALE_MODE=pixelart ./fuse --machine 48
```

When preparing a public update of this fork, follow `RELEASE-SDL3.md` so the
branch head, exported patches, and builder-facing documentation stay aligned.