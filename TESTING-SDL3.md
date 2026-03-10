# Testing Fuse SDL3

This note describes the validation expected for the SDL3 downstream fork,
especially the graphics path that benefits from testing on a real Linux desktop
instead of only under WSLg or CI dummy drivers.

## Baseline build check

From the repository root:

```sh
./configure --with-sdl
make -j"$(nproc)" fuse
```

The build must complete without local source edits beyond the intended branch
contents. Unless you intentionally configured with `--without-libxml2`, the
`./configure` summary should report `libxml2 support: yes` so XML settings
handling is built in.

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

## Non-SDL-primary build verification

To confirm the shared SDL3-related changes do not break other UI selections,
also verify at least one non-SDL-primary build. A verified path for this fork
is an Xlib build in a clean detached worktree:

```sh
git worktree add --detach /tmp/fuse-xlib HEAD
cd /tmp/fuse-xlib
autoreconf -fi
./configure --without-gtk
make -j"$(nproc)"
timeout -s KILL 5s ./fuse --no-sound --machine 48
```

This confirms that the non-SDL-primary Xlib configuration still configures,
builds, and starts while sharing the same downstream source line.

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

## Debian sid Wayland workaround

One Debian sid environment reproduced a crash inside `libdecor-gtk.so` during
`SDL_Init( SDL_INIT_VIDEO )`. In that case:

* `SDL_VIDEO_DRIVER=x11 ./fuse` worked;
* `SDL_VIDEO_DRIVER=wayland ./fuse` still crashed; and
* `LIBDECOR_PLUGIN_DIR=/nonexistent SDL_VIDEO_DRIVER=wayland ./fuse` worked.

That combination indicates a system `libdecor` plugin failure rather than a
Fuse-side rendering or emulation failure. When validating Wayland on affected
systems, record whether Fuse succeeds with the plugin path disabled before
treating the issue as an emulator regression.

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