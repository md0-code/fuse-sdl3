# Fuse SDL3

This repository is a directly maintained SDL3 fork of Fuse 1.6.0.

It is intended to stand on its own as the main fork repository:

* the full source tree lives here;
* build, validation, and release instructions live alongside the code; and
* there is no outer patch-export workflow or vendored pristine upstream mirror.

## Repository layout

The main builder-facing documents are:

* `BUILD-SDL3.md` for dependency setup and build steps;
* `CHANGELOG-SDL3.md` for a high-level summary of downstream features since
  Fuse 1.6.0;
* `TESTING-SDL3.md` for runtime validation and smoke tests;
* `RELEASE-SDL3.md` for release preparation; and
* `INSTALL` for the short CMake install summary.

The primary short build commands are:

* Linux: `sh ./scripts/build-linux.sh`
* Windows PowerShell: `./scripts/build-windows.ps1`

To build distributable archives with bundled runtime dependencies:

* Linux: `sh ./scripts/build-linux.sh --package`
* Windows PowerShell: `./scripts/build-windows.ps1 -Package`

To build a Linux AppImage:

* Linux: `sh ./scripts/build-linux.sh --appimage`

Those commands emit one platform-specific archive from the CMake install tree:
Linux writes `.tar.gz`, while Windows writes `.zip`. Each archive uses a
portable root layout with the executable and default config file at the
archive root. Linux places bundled runtime libraries under `lib/`, while
Windows keeps them at the archive root. `roms/` sits beside them, and the
default config file remains in the root
(`.fuserc.default` on Linux, `fuse.cfg.default` on Windows). The non-ROM UI support assets are
embedded, so there is no `share/` payload in the portable package.

The AppImage path stages the same portable Linux layout into an AppDir, then
builds a single `.AppImage`. If `appimagetool` is not installed, the Linux
script downloads a suitable copy automatically.

The roadmap for planned fork work is tracked in `planNewFeatures.md`.

## Fork behavior

This fork keeps the upstream Fuse codebase as its base and carries the SDL3
integration directly in the main branch history.

Important current fork expectations:

* CMake is the only supported build system for this fork;
* the maintained product is the SDL3 UI build;
* `libxml2` is enabled by default so XML settings support is always present; and
* the repository should remain free of obsolete patch-stack artifacts.

## What Fuse provides

Fuse is an emulator of the ZX Spectrum family and several related machines.
This fork preserves the upstream emulation scope while updating the SDL UI to
SDL3.

Highlights include:

* Spectrum 16K/48K/128K/+2/+2A/+3 emulation;
* Spectrum +3e and SE, Timex TC2048/TC2068/TS2068, Pentagon 128/512/1024, and
  Scorpion ZS 256 support;
* tape loading, snapshots, and RZX input recording support;
* multiple joystick, printer, storage, audio, and network peripherals; and
* SDL-based audio and video presentation with the downstream SDL3 port.

## Dependencies

At minimum, building the SDL3 fork requires:

* a C toolchain;
* `cmake`;
* `pkg-config` on non-Windows platforms;
* `libspectrum >= 1.5.0`;
* `SDL3`; and
* `libxml2`.

Optional libraries such as `libpng`, `libgcrypt`, `zlib`, `glib-2.0`, and
`libao` enable additional functionality when available.

See `BUILD-SDL3.md` for exact package names and build commands.

## Help and upstream context

Upstream Fuse project resources are still relevant for emulator behavior,
machine support, and general community help:

* mailing list: <fuse-emulator-devel@lists.sf.net>
* forums: <http://sourceforge.net/p/fuse-emulator/discussion/>
* upstream project page: <http://fuse-emulator.sourceforge.net/>

This fork keeps the original upstream project context intact while maintaining
the SDL3-specific integration and release flow directly in this repository.