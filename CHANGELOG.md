# Fuse SDL3 Changelog

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
