#!/usr/bin/env sh

set -eu

build_dir_name="build-linux"
build_package=0
build_appimage=0

for arg in "$@"; do
  case "$arg" in
    --package)
      build_package=1
      ;;
    --appimage)
      build_appimage=1
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

set -- cmake -S "$root_dir" -B "$build_dir"

if [ "$build_package" -eq 1 ] || [ "$build_appimage" -eq 1 ]; then
  set -- "$@" -DFUSE_PORTABLE_PACKAGE=ON
else
  set -- "$@" -DFUSE_PORTABLE_PACKAGE=OFF
fi

if [ "$build_package" -eq 1 ] || [ "$build_appimage" -eq 1 ]; then
  set -- "$@" -DCMAKE_BUILD_TYPE=Release
fi

if [ "$build_package" -eq 1 ]; then
  set -- "$@" -DCPACK_GENERATOR=TGZ
fi

"$@"

cmake --build "$build_dir" --parallel "$jobs"

exe_path="$build_dir/fuse"
if [ ! -f "$exe_path" ]; then
  echo "Expected build output not found: $exe_path" >&2
  exit 1
fi

chmod 755 "$exe_path"

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

if [ "$build_appimage" -eq 1 ]; then
  appdir="$build_dir/fuse-sdl3.AppDir"
  appimage_tool="${APPIMAGETOOL:-}"
  appimage_tool_appstream_flag="--no-appstream"
  arch=$(uname -m)
  version=$(sed -n 's/^project(fuse VERSION \([^ ]*\) LANGUAGES C)$/\1/p' "$root_dir/CMakeLists.txt")
  appimage_path="$build_dir/fuse-sdl3-${version}-${arch}.AppImage"
  desktop_file_name="io.github.md0_code.FuseSDL3.desktop"
  appstream_file_name="io.github.md0_code.FuseSDL3.appdata.xml"
  desktop_source="$build_dir/data/$desktop_file_name"
  appstream_source="$build_dir/data/$appstream_file_name"
  icon_source="$root_dir/data/icons/256x256/fuse.png"
  appimagetool_url=""

  case "$arch" in
    x86_64)
      appimagetool_url="https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"
      ;;
    aarch64)
      appimagetool_url="https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-aarch64.AppImage"
      ;;
    *)
      echo "Unsupported architecture for automatic appimagetool download: $arch" >&2
      exit 1
      ;;
  esac

  if [ -z "$appimage_tool" ]; then
    if command -v appimagetool >/dev/null 2>&1; then
      appimage_tool=$(command -v appimagetool)
    else
      appimage_tool="$build_dir/$(basename "$appimagetool_url")"
      if [ ! -x "$appimage_tool" ]; then
        curl -L "$appimagetool_url" -o "$appimage_tool"
        chmod 755 "$appimage_tool"
      fi
    fi
  fi

  if [ "${FUSE_APPIMAGE_VALIDATE_APPSTREAM:-0}" = "1" ]; then
    appimage_tool_appstream_flag=""
  fi

  rm -rf "$appdir"
  cmake --install "$build_dir" --prefix "$appdir"

  if [ ! -f "$desktop_source" ]; then
    echo "Expected desktop file not found: $desktop_source" >&2
    exit 1
  fi

  cp "$desktop_source" "$appdir/$desktop_file_name"
  cp "$icon_source" "$appdir/fuse.png"

  mkdir -p "$appdir/usr/share/applications" "$appdir/usr/share/icons/hicolor/256x256/apps" "$appdir/usr/share/metainfo"
  cp "$desktop_source" "$appdir/usr/share/applications/$desktop_file_name"
  cp "$icon_source" "$appdir/usr/share/icons/hicolor/256x256/apps/fuse.png"

  if [ -f "$appstream_source" ]; then
    cp "$appstream_source" "$appdir/usr/share/metainfo/$appstream_file_name"
  fi

  printf '%s\n' \
    '#!/usr/bin/env sh' \
    'set -eu' \
    'HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)' \
    'cd "$HERE"' \
    'export LD_LIBRARY_PATH="$HERE/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"' \
    'exec "$HERE/fuse" "$@"' > "$appdir/AppRun"
  chmod 755 "$appdir/AppRun"

  rm -f "$appimage_path"
  ARCH="$arch" VERSION="$version" APPIMAGE_EXTRACT_AND_RUN=1 "$appimage_tool" ${appimage_tool_appstream_flag:+"$appimage_tool_appstream_flag"} "$appdir" "$appimage_path"

  if [ ! -f "$appimage_path" ]; then
    echo "Expected AppImage output not found: $appimage_path" >&2
    exit 1
  fi

  chmod 755 "$appimage_path"

  appimage_smoke_stdout="$build_dir/appimage-smoke-output.txt"
  appimage_smoke_stderr="$build_dir/appimage-smoke-error.txt"
  rm -f "$appimage_smoke_stdout" "$appimage_smoke_stderr"

  if ! APPIMAGE_EXTRACT_AND_RUN=1 "$appimage_path" -V >"$appimage_smoke_stdout" 2>"$appimage_smoke_stderr"; then
    [ -f "$appimage_smoke_stdout" ] && cat "$appimage_smoke_stdout"
    [ -f "$appimage_smoke_stderr" ] && cat "$appimage_smoke_stderr" >&2
    echo "AppImage smoke test failed" >&2
    exit 1
  fi

  echo "AppImage generated in $appimage_path"
  echo "AppImage smoke test passed: $(basename "$appimage_path") -V"
fi