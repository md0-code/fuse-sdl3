# Fuse SDL3 Changelog

High-level downstream changes since the public Fuse 1.6.0 release.

Port start in this repository's first-parent history: `ab0ee54b` (`0001: switch build system to SDL3 detection`).

## Summary

- Native SDL3 port for the maintained frontend: SDL video, audio, and input were moved onto the SDL3 path, with follow-up fixes for startup and audio regressions.
- Modernized build and packaging: Fuse-SDL3 moved to a CMake-based build, added native Linux and Windows workflows, portable archives, AppImage output, embedded UI assets, and improved runtime dependency bundling.
- Better SDL presentation: fullscreen now stays aspect-correct instead of stretching, and the SDL path supports startup filter selection.
- New keyboard joystick feature: the default keyboard joystick mapping now uses the cursor keys for movement and left Alt for fire, with `F12` toggling those cursor keys between joystick control and normal cursor-key behavior.
- Updated defaults: the fork now starts with `2x` scaling, enables settings autosave by default, disables tape traps and fastload by default, and hides the status bar by default.
- New shortcuts for the SDL path: the current tree also wires `F11` to toggle fullscreen directly.
- Shader support: the SDL frontend can load a startup shader preset, keep persisted parameter overrides, and use optional `SDL3_shadercross` plus OpenGL-backed presentation when available.
- Expanded command-line surface: the downstream UI now exposes startup/display switches such as `--full-screen`, `--sdl-fullscreen-mode`, `--graphics-filter`, `--startup-shader`, and `--clear-startup-shader`.
- Fork-specific config and workflow changes: the repository is maintained directly as the SDL3 fork, prefers portable XML config files, and still tolerates legacy non-XML `.fuserc` files.
