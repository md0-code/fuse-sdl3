# Releasing Fuse SDL3

This checklist keeps the published repository consistent as a distribution point
for the SDL3 fork.

## Release goals

Each public update should leave the repository in this state:

* branch history contains the full downstream implementation;
* `exported-patches/` matches that history exactly;
* builder-facing docs describe the current dependency and build flow; and
* no outer-workflow artifacts such as a vendored upstream mirror are tracked in
  the repository.

## Pre-release checks

From the repository root:

```sh
git status --short
make -j"$(nproc)" fuse
timeout -s KILL 5s ./fuse --no-sound --machine 48
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  timeout -s KILL 5s ./fuse --no-sound --machine 48
```

If native Linux graphics behavior changed, also run the checklist from
`TESTING-SDL3.md`.

Before calling Phase 7 verification complete, also rerun at least one
non-SDL-primary build from `TESTING-SDL3.md` so the downstream changes are not
validated only through the SDL UI configuration.

## Refresh the exported patch series

The exported patches are part of the published repository, so regenerate them
whenever the downstream branch changes:

```sh
rm -rf exported-patches
mkdir exported-patches
git format-patch -o exported-patches tracking/fuse-1.6.0..sdl3-integration
```

Review the result:

```sh
ls -1 exported-patches
git diff --stat -- exported-patches
```

The patch subjects and ordering should match the downstream commit stack.

## Keep docs aligned with the code

Review these files whenever dependencies, build switches, or runtime behavior
change:

* `README`
* `BUILD-SDL3.md`
* `TESTING-SDL3.md`
* `INSTALL`

At minimum, confirm that:

* `libspectrum >= 1.5.0` is still accurate;
* `--with-sdl` still means the SDL3 UI;
* any new runtime knobs are documented; and
* the patch replay instructions still apply cleanly to upstream `fuse-1.6.0`.

If a platform-specific runtime workaround is known, such as the Debian sid
Wayland `libdecor` workaround documented in `BUILD-SDL3.md` and
`TESTING-SDL3.md`, keep that note current as part of release prep.

## Keep the repository surface clean

Do not add workflow-only outer-repository artifacts to this published branch.
In particular, do not track:

* a read-only upstream mirror clone;
* outer `repos/` or `worktrees/` directories; or
* local test captures and scratch logs.

Useful check:

```sh
git ls-files | rg '^(repos|worktrees)/'
```

This command should return no matches in the published repository.

## Publish the update

Commit the refreshed patches and documentation together with the code changes
they describe, then push the integration branch:

```sh
git add exported-patches README BUILD-SDL3.md TESTING-SDL3.md INSTALL
git commit -m "<next patch number>: describe release-facing update"
git push origin sdl3-integration
```

If you create a public milestone or release tag, create it only after the patch
export and docs are already up to date.