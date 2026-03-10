## Plan: New Feature Milestones

Implement the refined requirements as a milestone-oriented roadmap with narrow
deliverables so each stage is reviewable, testable, and easy to track in the
direct fork. The revised sequence moves keyboard joystick defaults and related
setting changes earlier, bundles the requested startup-default changes
explicitly, then keeps Windows build support and shader work isolated later
because they carry the highest design and regression risk.

## Milestone Breakdown

1. Milestone 1: downstream versioning and branding metadata.
   Update `worktrees/fuse-downstream/configure.ac` to define a manual internal
   build number that increments per release and composes the visible version
   string as upstream-version.build-number. Update
   `worktrees/fuse-downstream/fuse.c` to print the downstream SDL3 identity,
   GitHub address, and original upstream copyright text together, while
   keeping existing upstream-facing metadata semantics stable.

2. Milestone 2: portable config precedence.
   Change `worktrees/fuse-downstream/settings.c` so the current working
   directory is checked first for `.fuserc` on Unix-like hosts and `fuse.cfg`
   on Windows, then fall back to the existing config directory. Keep XML
   parsing and legacy-config handling unchanged, and preserve command-line
   arguments as the highest-priority override.

3. Milestone 3: new joystick emulation keys and mapping groundwork.
   Audit the existing keyboard joystick and Spectrum key assignments to choose
   a safe default fire key that is not already reserved; `Left Alt` is only a
   candidate until the audit confirms it is portable and unused enough.
   Extend `worktrees/fuse-downstream/settings.dat` so cursor keys and the new
   fire key are selectable in the keyboard joystick mapping list, set them as
   the new default mapping, and investigate what the existing
   `keyboard_arrows_shifted` option actually does before deciding whether it is
   sufficient to preserve Spectrum cursor behavior by itself.

4. Milestone 4: downstream default settings refresh.
   Change the requested defaults in `worktrees/fuse-downstream/settings.dat`
   in one isolated milestone: graphics filter to `2x`, status bar off,
   fastloading off, tape traps off, and keyboard joystick defaults to cursor
   keys plus the chosen fire key. Regenerate derived outputs and verify
   whether `worktrees/fuse-downstream/ui/sdl/sdldisplay.c` needs a matching
   startup-size adjustment so the initial SDL window geometry matches the
   selected logical scale.

5. Milestone 5: command line overhaul.
   Audit the generated and built-in command-line surfaces against the actual
   downstream behavior, then add fullscreen command-line support only if it is
   still missing in practice. In the same milestone, update the built-in help
   screen and any closely coupled command-line documentation so the reported
   options match the real implementation instead of the older abbreviated
   help text.

6. Milestone 6: MinGW Windows SDL3 build support.
   Make the current downstream SDL3 tree configure and build cleanly with
   MinGW-w64 first, because that path aligns with the existing autotools
   infrastructure in `worktrees/fuse-downstream/configure.ac`. Limit source
   compatibility changes to portability and SDL UI code unless wider platform
   assumptions are proven broken.

7. Milestone 7: Visual C build path and Windows docs.
   Add the Visual C build path as either concrete source/build support or a
   validated documented workflow, depending on how much project/build metadata
   is required outside autotools. Update
   `worktrees/fuse-downstream/BUILD-SDL3.md` with distinct MinGW and Visual C
   instructions, dependency acquisition notes, and runtime DLL expectations.

8. Milestone 8: shader settings and command-line surface.
    Add settings entries for external shader selection in
    `worktrees/fuse-downstream/settings.dat`, including a command-line option
    for selecting or clearing a startup shader. Keep this milestone strictly about
    configuration surfaces and derived-output regeneration so the later
    renderer work lands on stable user-facing interfaces.

9. Milestone 9: SDL renderer shader infrastructure.
    Refactor `worktrees/fuse-downstream/ui/sdl/sdldisplay.c` so the current
    presentation path is split cleanly into frame generation, texture upload,
    and final presentation. Add the backend capability checks, shader-loading
    path, and robust non-shader fallback needed to support externally supplied
    GLSL shaders without destabilizing the baseline SDL3 renderer path.

10. Milestone 10: shader menu integration and runtime switching.
    Add menu-level control for selecting, clearing, or inspecting the active
    shader using the established patterns in `worktrees/fuse-downstream/menu.c`
    and `worktrees/fuse-downstream/ui.c`. Decide in this milestone whether runtime
    shader changes require renderer rebuilds or can rebind presentation
    resources in place, and keep that behavior localized to the SDL renderer
    layer.

11. Milestone 11: testing, docs, and workflow refresh.
    Update `worktrees/fuse-downstream/TESTING-SDL3.md` and any published
    top-level docs to cover portable mode, fullscreen usage, x2 default
    startup, joystick mode selection, Windows build paths, and shader
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

4. After milestones 6 and 7, validate at least one MinGW-w64 build and review
   the Visual C build path for completeness, recording any unresolved
   IDE/project-system gaps.

5. After milestones 8 through 10, validate shader selection from command line
   and menu, confirm invalid or unsupported shaders fail cleanly, and ensure
   non-shader rendering remains the fallback path.

6. After milestone 11, rerun the documented SDL smoke tests and confirm at least
   one non-SDL-primary build still works.

## Decisions

- Keep the milestone sequence narrow and ordered so settings and user-facing
  surfaces land before behavior that depends on them.
- Bundle the requested default-setting changes into one isolated milestone so they
   are easy to review and revert independently from later behavior changes.
- Give command-line cleanup its own milestone so fullscreen support and help-text
   alignment are resolved before Windows and shader work.
- Keep cursor-key Kempston mode optional and preserve Ctrl-plus-arrow Spectrum
  cursor semantics.
- Sequence Windows support as MinGW first, Visual C second.
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

3. If Visual C support requires substantial non-autotools project
   maintenance, keep it as a separately documented deliverable rather than
   coupling it tightly to the MinGW source changes.

4. If `keyboard_arrows_shifted` does not already provide the desired Spectrum
   cursor fallback semantics, keep its behavior unchanged and implement the
   Ctrl-plus-arrow path explicitly in the new joystick-mode logic.