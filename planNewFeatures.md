## Plan: New Feature Milestones

Implement the refined requirements as a milestone-oriented roadmap with narrow
deliverables so each stage is reviewable, testable, and easy to track in the
direct fork. The revised sequence moves keyboard joystick defaults and related
setting changes earlier, bundles the requested startup-default changes
explicitly, then keeps the SDL-only frontend reduction, native Windows build
work, and shader work isolated later because they carry the highest design and
regression risk.

## Milestone Breakdown

1. Milestone 1: downstream versioning and branding metadata.
    [done]

2. Milestone 2: portable config precedence.
    [done]

3. Milestone 3: new joystick emulation keys and mapping groundwork.
    [done]
    
4. Milestone 4: default settings refresh.
    [done]

5. Milestone 5: command line overhaul.
   [done]
   Audit the generated and built-in command-line surfaces against the actual
   behavior, then add fullscreen command-line support only if it is
   still missing in practice. In the same milestone, update the built-in help
   screen and any closely coupled command-line documentation so the reported
   options match the real implementation instead of the older abbreviated
   help text.

6. Milestone 6: SDL-only frontend reduction.
   [done]
   Freeze the downstream frontend scope as `ui/sdl` plus `ui/widget`, then
   disable the GTK, Xlib, fb, svga, Wii, and legacy Win32 UI backends from the
   active downstream build. Limit this milestone to build-surface reduction,
   generated-file dependency cleanup, and shared-interface changes required to
   stop legacy UI types from leaking into retained code. Keep Windows platform
   support code that is still useful for the retained SDL build, especially
   path, timer, socket, launcher, and runtime-integration helpers.

7. Milestone 7: CMake build foundation for the retained SDL plus widget tree.
   [done]
   Add a top-level CMake build that models the actual downstream target:
   retained core code, SDL frontend code, widget UI code, generated sources,
   and the platform support code that is still needed. Prove the CMake source
   model on Linux first so source lists, generation rules, include paths, and
   link dependencies are correct before Windows-specific compiler issues are
   introduced. Model only the retained SDL and widget generators so the new
   build does not silently depend on outputs from deleted frontend paths.

8. Milestone 8: native Windows `clang-cl` bring-up and docs.
   [done]
   Define a reproducible dependency-acquisition path for Windows, then make the
   CMake build compile and run natively on Windows with `clang-cl`. Treat
   compiler and runtime fixes as shared portability cleanup where possible, and
   update `BUILD-SDL3.md` with the new CMake-based Windows workflow,
   dependency notes, and runtime DLL expectations. Validate Visual Studio usage
   through the CMake generator after the command-line `clang-cl` path works.
   Finish by validating resource lookup, configuration directories, ROM
   discovery, temporary files, Unicode-path handling, timers, sockets, and DLL
   assumptions under native Windows.

9. Milestone 9: shader settings and command-line surface.
    Add settings entries for external shader selection in
    `settings.dat`, including a command-line option
    for selecting or clearing a startup shader. Keep this milestone strictly about
    configuration surfaces and derived-output regeneration so the later
    renderer work lands on stable user-facing interfaces.

10. Milestone 10: SDL renderer shader infrastructure.
    The app must be able to load .slangp shaders designed from retroarch
    (examples in ~shaders). Investigate using SDL_shadercross for implementing 
    support. Refactor `ui/sdl/sdldisplay.c` so the current
    presentation path is split cleanly into frame generation, texture upload,
    and final presentation. Add the backend capability checks, shader-loading
    path, and robust non-shader fallback needed to support externally supplied
    GLSL shaders without destabilizing the baseline SDL3 renderer path.

11. Milestone 11: shader menu integration and runtime switching.
    Add menu-level control for selecting, clearing, or inspecting the active
    shader using the established patterns in `menu.c`
    and `ui.c`. Decide in this milestone whether runtime
    shader changes require renderer rebuilds or can rebind presentation
    resources in place, and keep that behavior localized to the SDL renderer
    layer.

12. Milestone 12: testing, docs, and workflow refresh.
    Update `TESTING-SDL3.md` and any published
    top-level docs to cover portable mode, fullscreen usage, x2 default
    startup, joystick mode selection, the CMake-based Windows build path, and shader
    selection and fallback behavior. Finish by rerunning the documented build
    and smoke-test workflow against the direct fork.

## Verification Gates

1. After milestones 1 through 4, rebuild from a clean tree and verify branding
   output, portable config precedence, the requested default-setting changes,
   and default x2 startup behavior.

2. After milestones 3 and 4, verify that the refreshed joystick defaults and the
   requested startup defaults are coherent, including the chosen fire key,
   `keyboard_arrows_shifted` interaction, and default x2 startup behavior.

3. After milestone 5, verify fullscreen command-line behavior if newly added, and
   confirm the built-in help text matches the actual downstream command-line
   implementation.

4. After milestones 6 through 8, validate the reduced SDL-only frontend set,
   confirm the Linux CMake build reaches parity with the retained autotools SDL
   path, verify at least one native Windows `clang-cl` build and launch, and
   confirm Visual Studio use remains a thin CMake-generator wrapper rather than
   a separately maintained project path.

5. After milestones 9 through 11, validate shader selection from command line
   and menu, confirm invalid or unsupported shaders fail cleanly, and ensure
   non-shader rendering remains the fallback path.

6. After milestone 12, rerun the documented SDL smoke tests and confirm the
   documented CMake and retained downstream build workflows still work.

## Decisions

- Keep the milestone sequence narrow and ordered so settings and user-facing
  surfaces land before behavior that depends on them.
- Bundle the requested default-setting changes into one isolated milestone so they
   are easy to review and revert independently from later behavior changes.
- Give command-line cleanup its own milestone so fullscreen support and help-text
   alignment are resolved before Windows and shader work.
- Keep cursor-key Kempston mode optional and preserve Ctrl-plus-arrow Spectrum
  cursor semantics.
- Sequence Windows support as SDL-only frontend reduction first, CMake second,
  and native Windows `clang-cl` bring-up third.
- Keep the retained frontend target explicit: `ui/sdl` plus `ui/widget`, not a
   widget rewrite.
- Land shader configuration surfaces before shader renderer internals so the
  user-facing contract stabilizes early.

## Risks And Split Points

1. If SDL3 cannot support external GLSL injection portably through the current
   renderer abstraction, split milestone 9 into a renderer-backend milestone and a
   shader-application milestone.

2. If the final fire-key choice is ambiguous across platforms, make it
   explicitly configurable instead of hard-coding a single default; treat
   `Left Alt` as provisional until verified against existing bindings and OS
   quirks.

3. If the CMake Windows path requires significant dependency or generator
   policy beyond the initial `clang-cl` bring-up, keep Visual Studio-specific
   polish separate from the core source and compiler enablement work.

4. Because generated-source rules are currently spread across frontend-specific
   make fragments, keep milestone 7 focused on modeling only the retained SDL
   and widget generators so the new build does not depend on deleted frontend
   outputs.

5. If Windows dependency acquisition is not standardized early, the port can
   appear to build while actually depending on an undocumented mixture of
   MSYS2, manual DLL copies, and host-specific toolchain state.

6. If `keyboard_arrows_shifted` does not already provide the desired Spectrum
   cursor fallback semantics, keep its behavior unchanged and implement the
   Ctrl-plus-arrow path explicitly in the new joystick-mode logic.
