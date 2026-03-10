# Releasing Fuse SDL3

This checklist keeps the published repository consistent as a distribution point
for the SDL3 fork.

## Release goals

Each public update should leave the repository in this state:

* branch history contains the full downstream implementation;
* builder-facing docs describe the current dependency and build flow; and
* no obsolete workflow-only artifacts are tracked in the repository.

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

## Keep docs aligned with the code

Review these files whenever dependencies, build switches, or runtime behavior
change:

* `README.md`
* `BUILD-SDL3.md`
* `TESTING-SDL3.md`
* `INSTALL`

At minimum, confirm that:

* `libspectrum >= 1.5.0` is still accurate;
* `--with-sdl` still means the SDL3 UI;
* any new runtime knobs are documented; and
* the build and validation steps still match the repository as cloned.

If a platform-specific runtime workaround is known, such as the Debian sid
Wayland `libdecor` workaround documented in `BUILD-SDL3.md` and
`TESTING-SDL3.md`, keep that note current as part of release prep.

## Keep the repository surface clean

Do not add obsolete workflow-only outer-repository artifacts to this published branch.
In particular, do not track:

* a read-only upstream mirror clone;
* local test captures and scratch logs.

## Publish the update

Commit the documentation updates together with the code changes they describe,
then push the integration branch:

```sh
git add README.md BUILD-SDL3.md TESTING-SDL3.md RELEASE-SDL3.md INSTALL
git commit -m "<next patch number>: describe release-facing update"
git push origin sdl3-integration
```

If you create a public milestone or release tag, create it only after the code
and docs are already up to date.