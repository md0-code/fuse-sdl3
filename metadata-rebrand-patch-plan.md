# Metadata And Config Rebrand Patch Plan

## Goal

Centralize downstream metadata so the project can switch from `fuse` to
`fuse-sdl3` for config-file naming and downstream package/config identity while
leaving the exact product name, version, and copyright string user-selectable
before code changes are applied.

This plan assumes these two policy goals:

1. Config filenames should change from the current `fuse` forms to
   `fuse-sdl3` forms.
2. The exact downstream display name, downstream version, and copyright text
   remain open inputs.

## User-Selectable Inputs

Fill these in before implementing the patch set.

| Key | Purpose | Current Value | New Value To Choose |
| --- | --- | --- | --- |
| `FUSE_DOWNSTREAM_NAME` | Main display name shown in UI and about/version output | `Fuse SDL3` | `<choose>` |
| `PROJECT_VERSION` | Downstream release version | `1.6.0.1` | `<choose>` |
| `FUSE_UPSTREAM_VERSION` | Upstream reference version | `1.6.0` | `<choose or keep>` |
| `FUSE_COPYRIGHT` | Compiled copyright string | `(c) 1999-2021 Philip Kendall and others` | `<choose>` |
| `PACKAGE` | Core package/program identifier | `fuse` | `fuse-sdl3` |
| `PACKAGE_NAME` | Package display/token name in generated config metadata | `fuse` | `fuse-sdl3` |
| `PACKAGE_TARNAME` | Tar/package token | `fuse` | `fuse-sdl3` |
| `PACKAGE_STRING` | Package plus version string | `fuse ${PROJECT_VERSION}` | `fuse-sdl3 ${PROJECT_VERSION}` |
| `UNIX_CONFIG_FILE_NAME` | Linux and Unix config filename | `.fuserc` | `.fuse-sdl3rc` |
| `WIN32_CONFIG_FILE_NAME` | Windows config filename | `fuse.cfg` | `fuse-sdl3.cfg` |

## Recommended Decisions

1. Keep `FUSE_UPSTREAM_VERSION` distinct from the downstream version.
2. Keep legacy config-file fallback for at least one release cycle.
3. Let new downstream config filenames be authoritative when both old and new
   files exist.
4. Do not change the executable name unless that is an intentional separate
   migration.

## Canonical Metadata Refactor

### 1. Centralize metadata in `CMakeLists.txt`

Patch [CMakeLists.txt](CMakeLists.txt#L177) through
[CMakeLists.txt](CMakeLists.txt#L189) so every downstream-facing identifier is
driven from one block.

Required edits:

1. Replace hardcoded `fuse` package literals with variables or selected values
   for `PACKAGE`, `PACKAGE_NAME`, `PACKAGE_TARNAME`, and `PACKAGE_STRING`.
2. Keep `FUSE_DOWNSTREAM_NAME`, `FUSE_UPSTREAM_VERSION`, `FUSE_COPYRIGHT`,
   `PROJECT_VERSION`, and `FUSE_RC_VERSION` together in the same block.
3. Add explicit variables for config filenames, for example:
   `FUSE_UNIX_CONFIG_NAME` and `FUSE_WIN32_CONFIG_NAME`.

Suggested shape:

```cmake
set(FUSE_UPSTREAM_VERSION "<choose>")
set(FUSE_DOWNSTREAM_NAME "<choose>")
set(FUSE_COPYRIGHT "<choose>")
set(PACKAGE "fuse-sdl3")
set(PACKAGE_NAME "fuse-sdl3")
set(PACKAGE_TARNAME "fuse-sdl3")
set(PACKAGE_STRING "fuse-sdl3 ${PROJECT_VERSION}")
set(FUSE_UNIX_CONFIG_NAME ".fuse-sdl3rc")
set(FUSE_WIN32_CONFIG_NAME "fuse-sdl3.cfg")
```

### 2. Export metadata through generated config headers

Patch [config.h.cmake.in](config.h.cmake.in#L20) through
[config.h.cmake.in](config.h.cmake.in#L37) and
[config.h.cmake.in](config.h.cmake.in#L148) through
[config.h.cmake.in](config.h.cmake.in#L160).

Required edits:

1. Preserve the existing downstream metadata macros.
2. Add config filename macros generated from CMake values, for example:
   `FUSE_UNIX_CONFIG_NAME` and `FUSE_WIN32_CONFIG_NAME`.
3. Ensure package-name macros now resolve to `fuse-sdl3` instead of `fuse`.

This gives runtime code one configured source for both product metadata and
config filenames.

## Config Filename Migration

### 3. Switch runtime config lookup to `fuse-sdl3` names

Patch [settings.pl](settings.pl#L100) through [settings.pl](settings.pl#L104)
and the path lookup code at [settings.pl](settings.pl#L188) through
[settings.pl](settings.pl#L280).

Current behavior:

1. Windows hardcodes `fuse.cfg`.
2. Non-Windows hardcodes `.fuserc`.
3. The lookup checks the working directory first, then the platform config
   directory.

Required edits:

1. Replace the hardcoded names with generated macros from `config.h`.
2. Add legacy fallback names:
   `fuse.cfg` on Windows and `.fuserc` on Unix.
3. Implement resolution order:
   new cwd file, old cwd file, new config-dir file, old config-dir file.
4. When a legacy file is used, emit a warning stating that the legacy file was
   loaded and that the new `fuse-sdl3` filename is preferred.
5. Keep XML-versus-INI behavior unchanged.

Recommended runtime behavior:

1. If both new and old files exist in the same scope, use the new file.
2. If only the old file exists, load it without failing.
3. If saving configuration, write only the new filename.

### 4. Update generated default config filename

Patch [CMakeLists.txt](CMakeLists.txt#L345) through
[CMakeLists.txt](CMakeLists.txt#L356).

Required edits:

1. Replace direct literals `fuse.cfg` and `.fuserc` with the new metadata
   variables.
2. Ensure the generated default portable config file is emitted as
   `fuse-sdl3.cfg` on Windows and `.fuse-sdl3rc` on Unix.

## Package And Installer Identity

### 5. Switch package/config identity from `fuse` to `fuse-sdl3`

Patch [CMakeLists.txt](CMakeLists.txt#L183) through
[CMakeLists.txt](CMakeLists.txt#L189).

Required edits:

1. Set `PACKAGE`, `PACKAGE_NAME`, and `PACKAGE_TARNAME` to `fuse-sdl3`.
2. Update `PACKAGE_STRING` to use `fuse-sdl3`.
3. Review whether `PACKAGE_URL` and `PACKAGE_BUGREPORT` should stay on the
   upstream URLs or move to downstream URLs. This is a policy choice and can be
   left unchanged if you still want upstream bug routing.

### 6. Update vcpkg package identity

Patch [vcpkg.json](vcpkg.json#L1) through [vcpkg.json](vcpkg.json#L3) only if
the package manager identity should remain aligned with the new downstream
package/config naming.

Current state already uses `fuse-sdl3`, so no change may be needed here.

### 7. Update Windows installer config cleanup and version metadata

Patch [data/win32/installer.nsi.in](data/win32/installer.nsi.in#L23) through
[data/win32/installer.nsi.in](data/win32/installer.nsi.in#L27),
[data/win32/installer.nsi.in](data/win32/installer.nsi.in#L84) through
[data/win32/installer.nsi.in](data/win32/installer.nsi.in#L95), and
[data/win32/installer.nsi.in](data/win32/installer.nsi.in#L376) through
[data/win32/installer.nsi.in](data/win32/installer.nsi.in#L388).

Required edits:

1. Switch installer package tokens from `fuse` to `fuse-sdl3` where they are
   intended to reflect downstream package identity.
2. Update display strings so they derive from the chosen downstream product
   name rather than older hardcoded Fuse-only text if that is your branding
   policy.
3. Change config-file deletion from `fuse.cfg` to `fuse-sdl3.cfg`.
4. Decide whether the uninstaller should also delete the legacy `fuse.cfg`.
   Recommended default: do not delete the legacy filename automatically.

### 8. Update Windows resource metadata

Patch [windres.rc](windres.rc#L32) through [windres.rc](windres.rc#L52).

Required edits:

1. Replace hardcoded product strings so `ProductName`, `FileDescription`, and
   other Windows resource fields match the chosen downstream naming policy.
2. Keep `FUSE_RC_VERSION` and `FUSE_COPYRIGHT` driven from generated config
   macros.

## Runtime Branding Cleanup

### 9. Keep the runtime name, version, and copyright displays macro-driven

Primary files:

1. [fuse.c](fuse.c#L499) through [fuse.c](fuse.c#L551)
2. [ui/widget/about.c](ui/widget/about.c#L33) through
   [ui/widget/about.c](ui/widget/about.c#L64)
3. [ui/sdl/sdldisplay.c](ui/sdl/sdldisplay.c#L1117) through
   [ui/sdl/sdldisplay.c](ui/sdl/sdldisplay.c#L1126)
4. [ui/sdl/sdlui.c](ui/sdl/sdlui.c#L162) through
   [ui/sdl/sdlui.c](ui/sdl/sdlui.c#L169)
5. [svg.c](svg.c#L122) through [svg.c](svg.c#L129)

Required edits:

1. Remove remaining hardcoded `Fuse` product-name literals where the string is
   meant to reflect the downstream display name.
2. Decide whether exported metadata like SVG creator name should be
   `fuse-sdl3`, the display name, or remain `fuse` for compatibility.
3. Keep the about dialog, version output, and window title aligned with the
   same selected downstream display name.

## File Dialog Title Migration

### 10. Decouple file-selector persistence from display titles before renaming UI strings

Patch [ui/widget/filesel.c](ui/widget/filesel.c#L180) through
[ui/widget/filesel.c](ui/widget/filesel.c#L231) first.

Current risk:

1. Directory persistence uses exact human-readable title matches.
2. Renaming `Fuse - ...` titles directly will break category matching.

Required edits:

1. Introduce explicit file-selection category identifiers independent of title
   text.
2. Update callers in:
   [menu.c](menu.c#L88) through [menu.c](menu.c#L1026),
   [ui.c](ui.c#L741) through [ui.c](ui.c#L761),
   [uimedia.c](uimedia.c#L241),
   [ui/widget/binary.c](ui/widget/binary.c#L146) through
   [ui/widget/binary.c](ui/widget/binary.c#L149), and
   [ui/widget/query.c](ui/widget/query.c#L37).
3. After that refactor, rename visible dialog titles from `Fuse - ...` to the
   chosen downstream display text.

Recommended visible-title policy:

1. Use the selected display name prefix consistently.
2. Keep the action text unchanged unless there is a UX reason to adjust it.

## Desktop And Manifest Metadata

### 11. Review desktop-entry and AppStream naming policy

Files:

1. [data/fuse.desktop.in](data/fuse.desktop.in#L1)
2. [data/fuse.appdata.xml.in](data/fuse.appdata.xml.in#L1)
3. [data/win32/fuse.manifest.in](data/win32/fuse.manifest.in#L1)

Required decision:

1. If desktop-facing branding should also move from `Fuse` to the chosen
   downstream display name, patch these files.
2. If desktop-facing branding should stay `Fuse` for compatibility and user
   familiarity, leave them unchanged and document that as an intentional split.

Because your request specifically targets config and package identity, this can
remain a separate branding decision.

## Documentation Updates

### 12. Update docs that mention old config filenames

Primary files:

1. [README.md](README.md#L37) through [README.md](README.md#L38)
2. [BUILD-SDL3.md](BUILD-SDL3.md#L121) through
   [BUILD-SDL3.md](BUILD-SDL3.md#L122)
3. [BUILD-SDL3.md](BUILD-SDL3.md#L281)

Required edits:

1. Replace `.fuserc` with `.fuse-sdl3rc`.
2. Replace `fuse.cfg` with `fuse-sdl3.cfg`.
3. Document the legacy fallback behavior if you implement compatibility reads.

## Suggested Implementation Order

1. Patch canonical metadata in `CMakeLists.txt`.
2. Export new metadata and config names in `config.h.cmake.in`.
3. Patch runtime config lookup and migration behavior in `settings.pl`.
4. Patch generated default config output naming in `CMakeLists.txt`.
5. Patch Windows installer and resource metadata.
6. Decouple file-selector persistence from visible titles.
7. Rename remaining runtime UI strings.
8. Update desktop metadata if chosen.
9. Update docs.
10. Rebuild and verify generated outputs.

## Verification Checklist

### Config behavior

1. With only `.fuse-sdl3rc` present, the app loads it.
2. With only `.fuserc` present, the app still loads it and warns.
3. With both present, `.fuse-sdl3rc` wins.
4. With only `fuse-sdl3.cfg` present on Windows, the app loads it.
5. With only `fuse.cfg` present on Windows, the app still loads it and warns.

### Generated outputs

1. `build-linux/config.h` reflects the chosen display name, version, copyright,
   package name, and config filenames.
2. Generated package metadata no longer says `fuse` where downstream package
   identity is expected to be `fuse-sdl3`.

### Runtime displays

1. `--version` output uses the selected downstream display name and version.
2. The about dialog uses the selected display name and copyright string.
3. The SDL window title uses the selected display name.
4. Any renamed file dialogs still preserve per-category directory persistence.

### Packaging and docs

1. Windows installer metadata and resource info match the chosen values.
2. Docs no longer instruct users to look for `.fuserc` or `fuse.cfg` as the
   primary filenames.

## Out Of Scope Unless Explicitly Chosen

1. Renaming the executable from `fuse` to `fuse-sdl3`.
2. Changing MIME IDs, desktop `Exec=` names, or manifest filenames for
   compatibility-sensitive integrations.
3. Rewriting legal history in source-file copyright headers.
