# Libretro implementation plan

## Goal

Add a libretro core as a new in-repo build target, while keeping the existing
SDL3 desktop application intact and minimizing duplicate logic.

## Status

The first libretro milestone described in this document is now implemented in
tree for version 0.2.0.

Delivered now:

- reusable runtime bootstrap in `frontend/`
- `fuse-libretro` CMake target
- software-rendered XRGB8888 video path
- audio batch output
- keyboard callback and basic joypad mapping
- snapshot-backed save states
- basic cheat translation through internal poke trainers
- initial core options for machine, fastload, traps, sound, monochrome TV,
  AY stereo, and joystick mode

Still pending from the roadmap below:

- no-game boot mode
- disk control interface
- richer memory map exposure
- broader libretro-native options coverage
- deeper frontend integration and UX polish

The cleanest implementation is not a direct `fuse -> libretro` port and not a
completely separate read-only wrapper project. The cleanest implementation is a
small architectural split inside this repository:

- Extract a reusable emulator library from the current application.
- Keep SDL3 as one frontend that links that library.
- Add a libretro frontend as a second target that links the same library.

This keeps one source of truth for emulation, media handling, snapshots,
peripherals, and settings, while avoiding a parallel build system and a second
copy of startup logic.

## Design principles

- Keep the emulation core frontend-agnostic.
- Do not leak SDL-specific concepts into the libretro target.
- Prefer existing Fuse abstractions where they already exist.
- Ship a software-rendered libretro core first.
- Use the existing snapshot and poke infrastructure before inventing new
  formats.
- Keep the first libretro milestone small enough to boot content, run one
  frame, serialize, and receive input.

## Current codebase facts that matter

The current tree already contains several seams that make this feasible:

- Frame progression is already organized around per-frame work in `spectrum.c`.
- Video presentation already passes through `uidisplay.*` and `display.*`.
- Input already passes through `input_event()` instead of going directly from
  SDL to machine state.
- Save-state logic already exists via `snapshot_copy_to()`,
  `snapshot_copy_from()`, and `snapshot_read_buffer()`.
- Cheat support already exists in the `pokefinder` and `pokemem` code.
- The main thing that is not reusable yet is startup and application lifetime,
  which are still owned by `fuse.c`.

## Recommended architecture

## Target layout

Introduce three build products:

- `fuse-core`
  - Static library or object library containing emulation, media handling,
    settings, snapshot support, poke handling, debugger support, and reusable
    frontend abstractions.
- `fuse`
  - Existing SDL3 application, updated to link `fuse-core` plus SDL frontend
    files.
- `fuse-libretro`
  - Libretro shared library implementing the libretro API and linking
    `fuse-core` plus a libretro-specific video, audio, timing, and environment
    layer.

## Proposed source split

Keep the current directory layout mostly intact. Add a new `libretro/`
directory and a small reusable runtime API.

Suggested additions:

```text
libretro/
  libretro_core.c
  libretro_audio.c
  libretro_video.c
  libretro_input.c
  libretro_options.c
  libretro_options.h
  libretro_mapper.c
  libretro_mapper.h
  libretro_serialize.c
  libretro_cheats.c
  libretro_paths.c
  libretro_log.c

frontend/
  frontend.h
  frontend.c
```

The `frontend/` layer is intentionally small. Its job is to expose the minimum
runtime control surface that both `fuse` and `fuse-libretro` need.

## Minimal reusable runtime API

Create a small public API that wraps what `fuse.c` currently does implicitly.

Example responsibilities:

- Initialize the emulator runtime with frontend-selected services.
- Select machine and startup settings programmatically.
- Load content from a path or memory buffer.
- Run exactly one emulated frame.
- Reset, pause, and shut down cleanly.
- Serialize and unserialize state.
- Apply and remove cheats.
- Expose geometry, timing, and frame buffer information.

A minimal C-facing API could look like this conceptually:

```c
int fuse_runtime_init(const fuse_frontend_ops *ops,
                      const fuse_runtime_config *config);
int fuse_runtime_load_content(const fuse_content *content);
int fuse_runtime_run_frame(void);
int fuse_runtime_reset(int hard_reset);
int fuse_runtime_get_video_frame(fuse_video_frame *frame);
int fuse_runtime_serialize_size(size_t *size);
int fuse_runtime_serialize(void *data, size_t size);
int fuse_runtime_unserialize(const void *data, size_t size);
int fuse_runtime_apply_cheat(const fuse_cheat_desc *cheat);
void fuse_runtime_unload_content(void);
void fuse_runtime_shutdown(void);
```

This API should be thin. It should call existing subsystems instead of
re-implementing them.

## Frontend responsibility split

`fuse-core` should own:

- Machine state
- Memory
- Peripherals
- Media insertion and ejection
- Snapshot import and export
- Poke and trainer logic
- Core settings representation
- Per-frame emulation logic

The frontend should own:

- Host input polling and translation
- Host video buffer ownership
- Host audio callback or batch submission
- Host timing policy
- User-facing configuration exposure
- Frontend logging
- Frontend save path and auxiliary path policy

## What not to carry into libretro

Do not port these SDL desktop concepts into the libretro core:

- SDL window management
- SDL fullscreen modes
- SDL renderer and shader pipeline
- Widget-based modal UI
- SDL-specific status bar visuals
- SDL file pickers
- SDL joystick enumeration model

For libretro, the frontend already provides presentation, overlays, shaders,
menuing, input remapping, and save directories.

## Implementation phases

## Phase 1: Extract a reusable runtime

Purpose: move application-owned startup and shutdown out of `main()` so another
frontend can drive the emulator.

Work items:

1. Move `fuse_init()`, `fuse_end()`, and the non-CLI parts of `fuse_run()`
   behind a reusable runtime API.
2. Split command-line parsing from runtime initialization.
3. Replace direct reliance on `argc/argv` during startup with a runtime config
   structure.
4. Keep `fuse.c` as a thin SDL desktop entry point that fills in the config and
   calls the reusable API.
5. Ensure `startup_manager_run()` and `startup_manager_run_end()` remain owned
   by the shared runtime, not by the SDL executable.

Definition of done:

- The existing SDL app still boots and behaves the same.
- `main()` becomes a thin wrapper.
- Another target can initialize the emulator without pretending to be a CLI
  executable.

## Phase 2: Add a libretro frontend layer

Purpose: create a minimal libretro target that can boot content and run a frame.

Work items:

1. Add `libretro/libretro_core.c` with the standard libretro entry points.
2. Register callbacks from `retro_set_environment()`, `retro_set_video_refresh()`,
   `retro_set_audio_sample_batch()`, `retro_set_input_poll()`, and
   `retro_set_input_state()`.
3. Implement `retro_init()`, `retro_deinit()`, `retro_get_system_info()`,
   `retro_get_system_av_info()`, `retro_load_game()`, `retro_unload_game()`,
   `retro_reset()`, and `retro_run()`.
4. Configure the runtime with a libretro-specific frontend ops table.
5. Report a fixed software video format and audio rate initially.

Recommended first video format:

- `RETRO_PIXEL_FORMAT_XRGB8888`

Reasoning:

- It is widely supported.
- It avoids frontend-side conversion surprises.
- It is the simplest long-term format for overlays, menus, and palette work.

Recommended first audio model:

- Collect one emulated frame of PCM in core-owned memory.
- Submit audio once per `retro_run()` via `audio_batch_cb`.

Definition of done:

- Core loads in RetroArch.
- A supported content file boots.
- Video and audio are produced each frame.
- Input changes machine state.

## Phase 3: Replace SDL-owned services with libretro services

Purpose: make the libretro target independent of SDL subsystems.

Work items:

1. Add a libretro `uidisplay` backend that writes directly into a core-owned
   XRGB8888 frame buffer.
2. Add a libretro audio backend that collects PCM instead of opening an SDL
   audio device.
3. Add a libretro timing backend that does not sleep.
4. Add a libretro input adapter that feeds `input_event()` and joystick state
   from frontend callbacks.
5. Disable or stub widget-only UI paths that are not meaningful in libretro.

Important rule:

- `retro_run()` must never block on timers or on audio buffer fullness.

The SDL timing path currently regulates speed. In libretro, the frontend owns
frame pacing. The core should always emulate one frame and return.

Definition of done:

- No libretro code depends on SDL windowing, audio, or sleeping.
- `retro_run()` is deterministic and non-blocking.

## Phase 4: Content loading and media policy

Purpose: make libretro content behavior predictable and frontend-friendly.

Recommended initial content policy:

- Accept one primary content file in `retro_load_game()`.
- Use existing content identification logic to load snapshots, tapes, disks,
  cartridges, recordings, and auxiliary `.pok` files where practical.
- Support `no game` mode later for machine boot without media.

Recommended order of support:

1. Snapshots
2. Tape files
3. Disk files
4. Interface 2 and Timex cartridges
5. Auxiliary `.pok` files
6. Recording playback if desired

Recommended path policy:

- Keep libretro save files and generated files in the frontend save directory.
- Resolve ROMs and optional assets through libretro-provided system and save
  directories, not SDL desktop defaults.

Later enhancement:

- Add disk control support for multi-disk workflows.

## Phase 5: Save and load state

Purpose: provide a clean and reliable serialization strategy from day one.

## Recommended implementation

Use the existing snapshot infrastructure as the first serialization backend.

Recommended baseline approach:

1. Build a `libspectrum_snap` with `snapshot_copy_to()`.
2. Serialize it to an in-memory buffer with `libspectrum_snap_write()`.
3. Prefix the buffer with a tiny libretro-specific header:
   - magic
   - version
   - machine id
   - format id
   - payload size
4. Load by validating the header and then calling `libspectrum_snap_read()` or
   `snapshot_read_buffer()`.

Why this is the best first implementation:

- It reuses a code path that already exists.
- It captures machine and peripheral state through established module hooks.
- It keeps the libretro port small.
- It reduces the chance of missing hidden state during the first milestone.

## Save-state options

### Option A: In-memory SZX snapshot wrapper

Use SZX-compatible snapshot serialization in memory.

Pros:

- Fastest route to correctness
- Reuses existing code
- Most maintainable first version

Cons:

- State size may be larger than necessary
- Extra allocation and format overhead
- Not ideal if ultra-low-latency runahead becomes a priority

Recommendation:

- Use this first.

### Option B: Custom raw binary state format

Create a versioned internal serializer for only the exact runtime state the core
needs.

Pros:

- Smaller states
- Faster serialize and unserialize
- Better for heavy runahead workloads

Cons:

- Much more engineering work
- Higher risk of correctness bugs
- Easy to miss module-owned state

Recommendation:

- Only do this later if profiling proves the snapshot path is too slow.

### Option C: Hybrid wrapper

Use snapshot-based serialization first, then optionally add an optimized raw
state path behind the same external API.

Pros:

- Safe initial delivery
- Clear path to future optimization

Cons:

- Slightly more complexity in long-term maintenance

Recommendation:

- This is the best long-term plan.

## Save-state requirements

- `retro_serialize_size()` must be stable for the active machine and content.
- `retro_serialize()` must not touch host paths.
- `retro_unserialize()` must fully rebuild machine and peripheral state.
- State versioning must be explicit.
- If a future format change occurs, reject incompatible state loads cleanly.

## Optional save-state enhancements

- Add a compact uncompressed mode for runahead-heavy frontends.
- Add a post-load refresh path so the next frame always redraws the full screen.
- Capture cheat trainer state so active cheats survive save-state round trips.

## Phase 6: Cheats

Purpose: expose existing Fuse cheat functionality through libretro cleanly.

## Recommended implementation

Use a hybrid cheat model:

- Treat libretro frontend cheats as runtime-defined trainers.
- Reuse the existing poke and trainer concepts where possible.
- Support both immediate memory writes and reusable trainer groups.

## Cheat options

### Option A: Direct libretro cheat writes only

Implement `retro_cheat_set()` by translating codes into direct memory writes.

Pros:

- Simple frontend integration
- Minimal UI expectations

Cons:

- Loses Fuse trainer structure
- Harder to manage grouped cheats or restore original values

Recommendation:

- Acceptable for a minimal first pass, but not the best end state.

### Option B: Reuse `pokemem` trainers internally

Translate libretro cheats into internal trainers and use existing activation and
deactivation logic.

Pros:

- Matches existing codebase behavior
- Supports grouped cheats
- Supports restore values and toggling more cleanly

Cons:

- Requires a small adapter layer

Recommendation:

- This is the preferred implementation.

### Option C: File-based `.pok` support only

Load `.pok` files as content or auxiliary content.

Pros:

- Leverages existing Fuse behavior

Cons:

- Weak libretro UX on its own
- Does not cover frontend cheat UI

Recommendation:

- Support as a secondary feature, not as the main cheat path.

## Cheat plan

Recommended staged implementation:

1. Implement `retro_cheat_reset()`.
2. Implement `retro_cheat_set(index, enabled, code)` using an internal adapter.
3. Support a simple code grammar first:
   - bank/address/value
   - address/value for common machine memory
4. Internally materialize those entries as trainer-backed pokes.
5. Later add optional `.pok` file import and auto-detection.

Recommended cheat behavior:

- Cheats should be reapplied after reset and after state load if still enabled.
- Cheats should be visible to save-state code if the frontend expects a state to
  restore the same gameplay condition.
- Invalid cheat lines should log warnings but not crash the core.

## Phase 7: Native libretro options menu

Purpose: expose runtime-configurable options through the frontend menu without
reusing the SDL widget menu system.

## Recommended implementation

Create one declarative option table and generate libretro core options from it.

Each entry should contain:

- key
- category
- user-facing label
- description
- value list
- default value
- whether a restart is required
- whether a content reload is required
- apply callback
- optional visibility predicate

This single table should feed:

- `RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2`
- Internal option parsing and application
- Future documentation generation if needed

## Option categories

Recommended categories:

- `system`
- `media`
- `video`
- `audio`
- `input`
- `peripherals`
- `advanced`
- `debug`

## Recommended initial options

Only expose options that make sense in libretro. Do not expose desktop-only
entries.

### System

- Machine model
  - 48K
  - 128K
  - +2
  - +2A
  - +3
  - Pentagon 128
  - Scorpion
  - Timex variants if desired
- Issue 2 emulation
- Late timings
- Auto-boot behavior for content classes

### Media

- Fastload
- Tape traps
- Loading sound
- Auto-load tape on insert
- Default disk interface selection when ambiguous

### Video

- Border mode
  - full border
  - cropped border
  - no border
- Timex display presentation mode if needed
- Alternate flash handling only if already supported internally

Do not expose:

- SDL fullscreen mode
- SDL graphics filter selection
- SDL startup shader
- Status bar visibility

Those belong to the frontend, not the core.

### Audio

- Sound enabled
- AY stereo separation
- Sound loading noise
- Optional audio latency hint only if the core actually uses it

### Input

- Joystick type
  - Kempston
  - Sinclair 1
  - Sinclair 2
  - Cursor
  - None
- Keyboard joystick output type
- Keyboard joystick fire mapping policy
- Kempston mouse enable

### Peripherals

- Interface 2 enable
- Dandanator enable if it makes sense without desktop UI
- ZXCF or IDE default enable policy
- DivIDE or DivMMC default enable policy

### Advanced

- Confirmed content-type auto-selection policy
- Snapshot compatibility mode if needed later
- Deterministic audio behavior toggle only if profiling demands it

### Debug

- Built-in debugger enable should usually stay hidden or disabled for release
  builds
- Tracing features should be development-only options

## Configurable entry behavior

Recommended rules:

- Apply immediately when safe.
- Mark machine-changing options as restart-required.
- Mark media-interface-changing options as content-reload-required if changing
  them live is unsafe.
- Use dynamic visibility so irrelevant entries disappear for incompatible
  machines.

Examples:

- Timex Dock options only appear for Timex-capable machine selections.
- Disk-interface defaults only appear for disk-capable content policies.
- Mouse options only appear when a mouse-capable input mode is active.

## Phase 8: Polish and frontend integration features

Purpose: make the core feel native in libretro frontends.

Recommended features after the first working release:

- Keyboard descriptors for Spectrum-relevant inputs.
- Controller info so frontends can expose gamepad vs keyboard-centric layouts.
- Input descriptors for menu toggles, tape controls, and disk actions.
- Full-screen redraw after load-state, reset, and machine switch.
- Save directory integration for persistent writable media.
- Better content naming and logging.

## Other useful libretro core features

These are worth considering after the baseline port is working.

## High-value features

### Disk control interface

Implement the libretro disk control interface for multi-disk software and drive
switching.

Why it matters:

- Strong UX improvement for disk-based Spectrum content.
- Avoids frontend users needing desktop-style menus.

### No-game boot mode

Support `RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME` so the user can boot the core
without content and then attach media later.

Why it matters:

- Matches emulator usage better.
- Useful for machine testing, BASIC, and homebrew workflows.

### Memory maps

Expose stable memory maps where practical.

Why it matters:

- Better frontend cheat tooling
- Better debugging tools
- Better RetroAchievements integration potential

### Frontend-facing media actions

Add a small set of libretro actions for common media tasks:

- Tape play or stop
- Tape rewind
- Next disk side or drive swap
- Reset
- Pause

These can be mapped through core options, input descriptors, or a libretro
virtual keyboard strategy.

### Virtual keyboard strategy

Spectrum software is keyboard-heavy. Plan for this early.

Options:

- Rely on frontend keyboard support only
- Add a simple controller-to-keyboard mapper
- Add a custom virtual keyboard later if really needed

Recommendation:

- First release should rely on frontend keyboard support and a small controller
  mapper for common games.

### Controller mapper presets

Ship a few controller presets for popular control styles:

- Kempston action layout
- Sinclair 1 action layout
- Sinclair 2 action layout
- Cursor layout

### Content auto-classification policy

Refine how ambiguous files are handled when they could map to multiple
interfaces.

This should be deterministic and documented.

### Save media flushing

If writable media is supported, add explicit flush behavior for frontend-driven
save semantics.

### Runahead readiness

Do not optimize for runahead first, but keep the serializer deterministic and
side-effect free so runahead can work later.

### Achievements readiness

If memory maps and serialization are stable, the core becomes much more useful
for frontend ecosystems that rely on deterministic memory observation.

## Explicit non-goals for version 1

To keep the port clean and shippable, version 1 should not try to solve
everything.

Non-goals:

- Porting the SDL shader system
- Recreating the SDL widget UI in libretro
- Shipping hardware-rendered libretro video on day one
- Exposing every desktop setting as a core option
- Supporting every media workflow before snapshot, tape, and common disk cases
  are stable
- Rewriting snapshot support into a new serializer before measuring the existing
  path

## Proposed milestone order

## Milestone 1: Reusable runtime

Deliverables:

- `fuse-core`
- SDL app still working
- Programmatic init, load, run-frame, reset, shutdown API

## Milestone 2: First booting core

Deliverables:

- `fuse-libretro` target
- Snapshot loading
- One-frame software rendering
- Audio batch output
- Keyboard and gamepad input

## Milestone 3: Stable state and options

Deliverables:

- Serialize and unserialize
- Core options menu
- Reset-safe runtime configuration updates

## Milestone 4: Cheats and media polish

Deliverables:

- Frontend cheats via internal trainer adapter
- `.pok` support where practical
- Better input presets
- Better machine and media defaults

## Milestone 5: Ecosystem features

Deliverables:

- Disk control interface
- No-game boot mode
- Memory maps
- Save-media flush behavior

## Suggested first release feature set

The first public libretro release should include exactly this set:

- Software-rendered video
- Audio output
- Keyboard input
- Basic controller mapper
- Snapshot loading
- Tape loading
- Common disk loading
- Save states
- Load states
- Core options for machine, fastload, traps, joystick type, border mode, and
  AY separation
- Frontend cheat support through internal trainer translation

This is enough to feel complete without dragging desktop UI assumptions into the
libretro port.

## Recommended first implementation decisions

If the project wants the shortest clean path, these are the best decisions:

- Build libretro in-tree as a new target.
- Extract `fuse-core` first.
- Use a small frontend ops layer instead of large conditional compilation.
- Use software rendering first.
- Use snapshot-based serialization first.
- Use `pokemem`-style trainer adaptation for cheats.
- Use declarative core option metadata with dynamic visibility.
- Leave SDL shaders, windowing, and widget UI out of the libretro build.

## Final recommendation

The cleanest implementation is:

1. Extract a reusable runtime from the current executable.
2. Create `fuse-core` as the shared emulator library.
3. Keep `fuse` as the SDL desktop frontend.
4. Add `fuse-libretro` as a second frontend target.
5. Ship snapshot-based save states, trainer-backed cheats, and a declarative
   libretro options menu in the first serious milestone.
6. Add disk control, no-game boot, memory maps, and other frontend-native
   features after the baseline core is stable.

That path keeps the port maintainable, minimizes duplicate code, and matches the
actual structure of this repository much better than a completely separate
wrapper project.
