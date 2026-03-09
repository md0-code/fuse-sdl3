# Testing Fuse SDL3

This note describes the validation expected for the SDL3 downstream fork,
especially the graphics path that benefits from testing on a real Linux desktop
instead of only under WSLg or CI dummy drivers.

## Baseline build check

From the repository root:

```sh
make -j"$(nproc)" fuse
```

The build must complete without local source edits beyond the intended branch
contents.

## Basic startup smoke tests

Windowed startup:

```sh
timeout -s KILL 5s ./fuse --no-sound --machine 48
```

Headless or CI startup:

```sh
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  timeout -s KILL 5s ./fuse --no-sound --machine 48
```

Both commands are expected to survive until timeout rather than exit early with
an SDL initialization failure.

## Media loading smoke tests

Tape autoload:

```sh
timeout -s KILL 5s ./fuse --no-sound --auto-load --tape /path/to/game.tap
```

ROM override:

```sh
timeout -s KILL 5s ./fuse --no-sound --rom-48 /path/to/diagnostic.rom
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

## Fullscreen scale-mode checks

Run the same startup command with each texture scale mode:

```sh
FUSE_SDL_SCALE_MODE=nearest ./fuse --machine 48
FUSE_SDL_SCALE_MODE=linear ./fuse --machine 48
FUSE_SDL_SCALE_MODE=pixelart ./fuse --machine 48
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

* `make -j"$(nproc)" fuse` succeeds from a clean tree;
* windowed startup works;
* dummy-driver startup works;
* fullscreen works on native Linux with correct aspect ratio; and
* `exported-patches/` still matches the current downstream branch history.