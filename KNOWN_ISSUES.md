# Known Issues

## Segmentation Fault on Wayland

**Symptom**: Fuse SDL3 crashes with a segmentation fault during startup on Wayland-based desktop environments.

**Cause**: This is due to a compatibility issue between SDL3's Wayland backend and certain versions of libdecor/Pango libraries. The crash occurs during font system initialization in the graphics subsystem.

**Stack trace signature**: 
- Crash at address `0x0000000000000791` 
- In `g_hash_table_add()` → `pango_cairo_font_map_get_default()` → SDL3 Wayland backend

**Workaround**: Force SDL3 to use X11 instead of Wayland by setting the SDL video driver:

```bash
SDL_VIDEODRIVER=x11 ./fuse
```

**Permanent fix**: Add this to your shell profile (`.bashrc`, `.zshrc`, etc.) if you primarily want to run Fuse with X11:

```bash
export SDL_VIDEODRIVER=x11
```

**Alternative**: Run Fuse under Xwayland (which is the default fallback for many applications):

```bash
# This usually works automatically, but can be forced:
GDK_BACKEND=x11 fuse
```

This issue affects primarily:
- Modern Linux distributions using Wayland by default (GNOME, some KDE configurations)
- Systems with newer libdecor/Pango library versions
- Does not affect X11-only desktop environments

The issue will be resolved when SDL3 or the underlying graphics libraries fix the Wayland compatibility problems.