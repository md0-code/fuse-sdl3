# Fuse SDL3 Changelog

## Version 0.2.1

- Fixed the Windows release failing to start with a `VCRUNTIME140D.dll was not found` error: the portable package is now built as a fully static binary (static libraries and static C runtime), so `fuse.exe` no longer depends on any external DLLs or the Visual C++ runtime. The previous dynamic build could bundle debug-configuration dependency DLLs (notably a debug `spectrum-9.dll`), which require a runtime only present on machines with Visual Studio installed.
- Fixed a crash when opening the file dialog a second time on Windows. The remembered per-category directory was allocated by libspectrum but released with the C runtime's `free()`; the mismatched allocators corrupted the heap. All such file-selector allocations now pair with their correct allocator.
- Fixed an out-of-bounds crash in the Poke Memory dialog that occurred when toggling a cheat before moving the selection (the selection index could stay at its initial `-1`).
- Poke Memory: enabling a cheat now applies it immediately and keeps its checkbox state, and reopening the same `.pok` file no longer reverts pokes that were already applied.
- Poke Memory: active pokes are now re-asserted every frame, so cheats keep working on multiload games that reload (and overwrite) the poked code mid-game.

## Version 0.2.0

- Added an in-tree libretro core target alongside the SDL3 desktop frontend, backed by a reusable runtime shared between both entry points.
- Expanded the libretro feature set with save states, cheat translation, no-content boot, disk playlists and disk control, categorized options, controller descriptors, RetroPad media actions, and published memory maps.
- Added Linux and Windows libretro distribution packaging flows, including Windows standalone packaging with bundled `.info` metadata and embedded asset support.
- Tightened the Windows build helper around generated-settings validation and libretro packaging behavior.
- Added repository release automation for publishing Linux and Windows libretro packages with changelog-based release notes.

## Version 0.1.1

- Fixed automatic Wayland crash detection that wasn't working properly - now reliably fails back to X11 when Wayland causes segfaults
- Implemented cross-platform configuration system with separate Linux and Windows path settings in settings.dat
- Updated configuration file naming from `.fuserc.default` to `.fuse-sdl3.default` for better project identification
- Fixed missing string.h include in menu.c that caused compilation errors on some systems
- Improved build system reliability and package naming consistency

## Version 0.1.0 (first public release)

- Native SDL3 port for the maintained frontend: SDL video, audio, and input were moved onto the SDL3 path, with follow-up fixes for startup and audio regressions.
- Modernized build and packaging: Fuse-SDL3 moved to a CMake-based build, added native Linux and Windows workflows, portable archives, AppImage output, embedded UI assets, and improved runtime dependency bundling.
- Better SDL presentation: fullscreen now stays aspect-correct instead of stretching, and the SDL path supports startup filter selection.
- New cartridge support: full Dandanator 512 KB ROM set emulation, including insert/eject, startup loading and EEPROM programming simulation.
- New keyboard joystick feature: the default keyboard joystick mapping now uses the cursor keys for movement and left Alt for fire, with `F12` toggling those cursor keys between joystick control and normal cursor-key behavior.
- Updated defaults: the fork now starts with `2x` scaling, enables settings autosave by default, disables tape traps and fastload by default, and hides the status bar by default.
- File selector enhancements: last used paths are persisted separately across restarts for every file type (allows loading each type of content from a different folder). Missing media files referenced in saved settings (dandanator, Interface 2, snapshot, tape) are now skipped non-fatally on startup — the bad path is cleared and the emulator continues running.
- New shortcuts for the SDL path: the current tree also wires `F11` to toggle fullscreen directly.
- Shader support: the SDL frontend can load a startup shader preset, keep persisted parameter overrides, and use optional `SDL3_shadercross` plus OpenGL-backed presentation when available.
- Expanded command-line surface: the SDL3 UI now exposes startup/display switches such as `--full-screen`, `--sdl-fullscreen-mode`, `--graphics-filter`, `--startup-shader`, and `--clear-startup-shader`.
- Fork-specific config and workflow changes: the repository is maintained directly as the SDL3 fork, prefers portable XML config files, and still tolerates legacy non-XML `.fuserc` files.
