# Fuse SDL3

A fork of [Fuse](http://fuse-emulator.sourceforge.net/) — the Free Unix Spectrum Emulator — focused on a modernized SDL3 frontend, a streamlined CMake-based build, and an opinionated set of usability improvements and new features.

Upstream baseline: **fuse 1.6.0**.  
Current version: **0.1.1** (first public release).

---

## About

Fuse is a long-standing, highly accurate ZX Spectrum emulator originally written by Philip Kendall and maintained by a community of contributors. This fork starts from the 1.6.0 release and makes the following deliberate changes:

- The SDL3 frontend is the **only** supported frontend. The GTK, X, framebuffer, and other legacy UI backends have been removed, keeping the tree small and focused.
- The build system has been replaced with **CMake**. The autotools chain is gone.
- New hardware and features have been added that are absent or incomplete in the original emulator.
- Defaults have been revised toward what feels right for day-to-day desktop use in 2025 and beyond.

This is a personal project. It is not an official Fuse release and is not affiliated with the upstream Fuse project or its maintainers.

---

## Features

### Inherited from upstream Fuse

- Accurate emulation of the Z80 CPU.
- Emulation of a wide range of ZX Spectrum and compatible hardware (see [Supported machines](#supported-machines) below).
- Tape loading and saving (`.tap`, `.tzx`), snapshot support (`.z80`, `.szx`, `.sna`, and others).
- Built-in debugger with breakpoints, disassembler, and expression evaluation.
- RZX recording and playback.
- Interface 2 ROM cartridge support.
- Poke finder, profile mode, and other developer tools.
- Kempston, Sinclair, cursor, and other joystick types.

### New and changed in this fork

- **SDL3 frontend** — video, audio, and input fully ported to SDL3, with follow-up fixes for startup and audio regressions introduced during the migration.
- **CMake build** — replaces autotools; supports Linux and native Windows (via Visual Studio / clang-cl and a repo-local vcpkg manifest). See [BUILD.md](BUILD.md) for full dependency lists and build instructions.
- **Correct fullscreen scaling** — fullscreen keeps the correct aspect ratio instead of stretching to fill the display.
- **OpenGL shader backend** — loads RetroArch-compatible `.glslp` shader presets (examples provided in `shaders/`). Shader parameters can be edited at runtime under *Options → Shader parameters…*. The backend falls back gracefully to the plain SDL renderer when OpenGL or `SDL3_shadercross` is unavailable.
- **Dandanator cartridge emulation** — full 512 KB ROM-set support with insert/eject, startup autoload, and EEPROM programming simulation.
- **Keyboard joystick** — cursor keys map to joystick directions, left Alt is fire. `F12` toggles cursor keys between joystick control and normal cursor-key behaviour.
- **`F11` for fullscreen** — direct toggle without going through the menu.
- **Per-file-type directory memory** — the file selector remembers the last-used directory separately for each content type (tape, snapshot, ROM, shader, etc.) and restores them across restarts.
- **Robust startup** — missing media files referenced in saved settings (Dandanator cartridge, Interface 2, snapshot, tape) are skipped non-fatally; the bad path is cleared and emulation starts normally.
- **Revised out-of-the-box defaults:**
  - 2× scaling at startup.
  - Settings autosave enabled.
  - Tape traps and fast-load disabled (for better compatibility with copy-protected software).
  - Status bar hidden.
- **Expanded command-line surface** — `--full-screen`, `--sdl-fullscreen-mode`, `--graphics-filter`, `--startup-shader`, and `--clear-startup-shader`.
- **Portable XML config** — settings are stored in a portable XML `.fuserc` file by default; legacy non-XML files are still read on first migration.
- **AppImage and archive packaging** — `scripts/build-linux.sh --appimage` / `--package` produce self-contained distributable bundles from the install tree.

---

## Supported machines

| Family | Models |
|---|---|
| ZX Spectrum | 16K, 48K, 48K NTSC, SE |
| ZX Spectrum 128 | 128K, +2, +2A, +3, +3e |
| Timex | TC2048, TC2068, TS2068 |
| Pentagon | 128K, 512K, 1024K |
| Scorpion | ZS256 |

---

## Platform support

| Platform | Status |
|---|---|
| Linux | Fully supported |
| Windows (native, Visual Studio / clang-cl) | Fully supported |

macOS is not currently supported or tested.

---

## Quick start

Pre-built binaries are available on the [Releases](../../releases) page.

**Linux:** download the `.tar.gz` archive, extract it, and run the `fuse` binary inside:

```sh
tar -xf fuse-sdl3-<version>-linux.tar.gz
cd fuse-sdl3-<version>-linux
./fuse
```

An AppImage is also provided if you prefer a single self-contained executable:

```sh
chmod +x fuse-sdl3-<version>.AppImage
./fuse-sdl3-<version>.AppImage
```

**Windows:** download the `.zip` archive, extract it, and run `fuse.exe`.

If you want to build from source instead, see [BUILD.md](BUILD.md).

---

## Troubleshooting

### Segmentation fault on startup (Linux/Wayland)

If Fuse crashes immediately on startup with a segmentation fault, this is likely due to a compatibility issue between SDL3's Wayland backend and system graphics libraries. 

**Quick fix:** Force Fuse to use X11 instead of Wayland:

```bash
SDL_VIDEODRIVER=x11 ./fuse
```

For more details and alternative solutions, see [KNOWN_ISSUES.md](KNOWN_ISSUES.md).

---

## Documentation

| Document | Contents |
|---|---|
| [BUILD.md](BUILD.md) | Build dependencies, platform-specific notes, and packaging instructions |
| [CHANGELOG.md](CHANGELOG.md) | What changed in each release relative to upstream |
| [KNOWN_ISSUES.md](KNOWN_ISSUES.md) | Troubleshooting guide for common runtime problems |
| [LICENSE.md](LICENSE.md) | GNU General Public License, version 3 |

---

## License

Fuse SDL3 is free software distributed under the **GNU General Public License, version 3 or later**. See [LICENSE.md](LICENSE.md) for the full text.

The original Fuse source code is copyright © 1999–2018 Philip Kendall and other contributors. This fork retains those copyright notices in all source files where they appear.
