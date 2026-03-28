# Known Issues

## Segmentation Fault on Wayland (RESOLVED)

**Status**: Automatically fixed in current version.

**Symptom**: Fuse SDL3 crashes with a segmentation fault during startup on Wayland-based desktop environments.

**Cause**: This is due to a compatibility issue between SDL3's Wayland backend and certain versions of libdecor/Pango libraries. The crash occurs during font system initialization in the graphics subsystem.

**Automatic Fix**: Fuse SDL3 now automatically detects Wayland usage and switches to the X11 backend to avoid these crashes. You should see a message like:

```
./fuse: automatically switching SDL video backend to x11 to avoid known Wayland libdecor crashes
```

**Manual Override**: If you want to force Wayland usage despite the risk, you can disable the automatic workaround:

```bash
FUSE_SDL_DISABLE_LIBDECOR_WORKAROUND=1 ./fuse
```

**Original Manual Workaround** (no longer needed): Force SDL3 to use X11 instead of Wayland:

```bash
SDL_VIDEODRIVER=x11 ./fuse
```

**Technical Details**: 
- Stack trace signature: Crash at address like `0x0000000000000791` in `g_hash_table_add()` → `pango_cairo_font_map_get_default()` → SDL3 Wayland backend
- Affects: Modern Linux distributions using Wayland by default (GNOME, some KDE configurations)
- Systems with newer libdecor/Pango library versions  
- Does not affect X11-only desktop environments