#!/usr/bin/env sh

set -eu

build_dir_name="build-linux"
build_package=0

for arg in "$@"; do
  case "$arg" in
    --package)
      build_package=1
      ;;
    *)
      build_dir_name="$arg"
      ;;
  esac
done

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir="$root_dir/$build_dir_name"

cpu_count() {
  if command -v nproc >/dev/null 2>&1; then
    nproc
    return
  fi

  if command -v getconf >/dev/null 2>&1; then
    getconf _NPROCESSORS_ONLN 2>/dev/null && return
  fi

  echo 1
}

jobs="${JOBS:-$(cpu_count)}"

if [ "$build_package" -eq 1 ]; then
  cmake -S "$root_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release
else
  cmake -S "$root_dir" -B "$build_dir"
fi
cmake --build "$build_dir" --parallel "$jobs"

exe_path="$build_dir/fuse"
if [ ! -x "$exe_path" ]; then
  echo "Expected build output not found: $exe_path" >&2
  exit 1
fi

smoke_stdout="$build_dir/smoke-output.txt"
smoke_stderr="$build_dir/smoke-error.txt"
rm -f "$smoke_stdout" "$smoke_stderr"

if ! "$exe_path" -V >"$smoke_stdout" 2>"$smoke_stderr"; then
  [ -f "$smoke_stdout" ] && cat "$smoke_stdout"
  [ -f "$smoke_stderr" ] && cat "$smoke_stderr" >&2
  echo "Runtime smoke test failed" >&2
  exit 1
fi

echo "Runtime smoke test passed: fuse -V"

if [ "$build_package" -eq 1 ]; then
  cmake --build "$build_dir" --target package --parallel "$jobs"
  echo "Package archives generated in $build_dir"
fi