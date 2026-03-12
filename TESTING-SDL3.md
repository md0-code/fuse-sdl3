# Testing Fuse SDL3

This note describes the validation expected for the SDL3 downstream fork,
especially the graphics path that benefits from testing on a real Linux desktop
instead of only under WSLg or CI dummy drivers.

## Baseline build check

From the repository root:

CMake build:

```sh
cmake -S . -B build-linux
cmake --build build-linux -j"$(nproc)"
```

The build must complete without local source edits beyond the intended branch
contents.

## Basic startup smoke tests

CMake executable, windowed startup:

```sh
timeout -s KILL 5s ./build-linux/fuse --no-sound --machine 48
```

CMake executable, headless or CI startup:

```sh
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  timeout -s KILL 5s ./build-linux/fuse --no-sound --machine 48
```

Both commands are expected to survive until timeout rather than exit early with
an SDL initialization failure.

## Media loading smoke tests

Tape autoload:

```sh
timeout -s KILL 5s ./build-linux/fuse --no-sound --auto-load --tape /path/to/game.tap
```

ROM override:

```sh
timeout -s KILL 5s ./build-linux/fuse --no-sound --rom-48 /path/to/diagnostic.rom
```

These are useful for catching regressions in startup, file loading, and early
display initialization.

## Native Linux graphics validation

Use a native Linux desktop session for acceptance of the SDL3 presentation path.
WSLg is useful for smoke tests, but it is not the right place to make final
judgments about fullscreen behavior, filtering, or compositor interaction.

Validate all of the following:

* the SDL window opens normally in windowed mode;
* entering fullscreen from the UI works;
* leaving fullscreen returns to a sane windowed size;
* the image stays aspect-correct instead of stretching to fill the display;
* the image scales to the desktop resolution instead of staying at a small
  integer multiple when fullscreen is active; and
* no extra stale window contents remain visible after fullscreen transitions.

## Debian sid Wayland workaround

One Debian sid environment reproduced a crash inside `libdecor-gtk.so` during
`SDL_Init( SDL_INIT_VIDEO )`. In that case:

* `SDL_VIDEODRIVER=x11 ./build-linux/fuse` worked;
* `SDL_VIDEODRIVER=wayland ./build-linux/fuse` still crashed; and
* `LIBDECOR_PLUGIN_DIR=/nonexistent SDL_VIDEODRIVER=wayland ./build-linux/fuse` worked.

That combination indicates a system `libdecor` plugin failure rather than a
Fuse-side rendering or emulation failure. When validating Wayland on affected
systems, record whether Fuse succeeds with the plugin path disabled before
treating the issue as an emulator regression. Current Linux Wayland builds now
probe the Wayland `libdecor` path in a child process and automatically switch to
`SDL_VIDEODRIVER=x11` if that probe crashes. If no X11 display is available,
Fuse falls back to `LIBDECOR_PLUGIN_DIR=/nonexistent` instead. Set
`FUSE_SDL_DISABLE_LIBDECOR_WORKAROUND=1` to bypass that automatic probe and
fallback logic.

## Fullscreen scale-mode checks

Run the same startup command with each texture scale mode:

```sh
FUSE_SDL_SCALE_MODE=nearest ./build-linux/fuse --machine 48
FUSE_SDL_SCALE_MODE=linear ./build-linux/fuse --machine 48
FUSE_SDL_SCALE_MODE=pixelart ./build-linux/fuse --machine 48
```

Check for the following behavior:

* `nearest`: sharp pixels, no filtering.
* `linear`: visibly smoother interpolation during non-integer scaling.
* `pixelart`: accepted by SDL if supported by the platform backend; if the
  backend falls back internally, startup should still succeed.

## Manual fullscreen checklist

On native Linux, verify both menu-driven and shortcut-driven fullscreen entry if
the shortcut is wired by the active UI:

* start in windowed mode;
* enter fullscreen through the options/menu path;
* inspect letterboxing and aspect ratio;
* exit fullscreen;
* re-enter fullscreen after changing scale mode;
* confirm no crash, black frame, or stale backing-store artifacts occur.

## Expected acceptance bar

Before calling a branch ready for distribution, keep these true:

* `cmake --build build-linux -j"$(nproc)"` succeeds from a clean tree;
* windowed startup works;
* dummy-driver startup works;
* fullscreen works on native Linux with correct aspect ratio; and
* the checked-in build docs still match the repository as cloned.