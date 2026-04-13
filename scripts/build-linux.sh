#!/usr/bin/env sh

set -eu

build_dir_name="build-linux"
build_package=0
build_appimage=0
build_libretro=0
build_install=0
build_uninstall=0
install_prefix=""
build_dir_overridden=0

for arg in "$@"; do
  case "$arg" in
    --help|-h)
      cat <<'EOF'
Usage: build-linux.sh [OPTIONS] [BUILD_DIR]

Options:
  --libretro             Build the libretro core instead of the desktop app
  --package              Build portable release archive (.tar.gz)
  --appimage             Build AppImage
  --install              Install after building (may require sudo)
  --install-prefix=DIR   Set installation prefix used with --install
  --uninstall            Uninstall using BUILD_DIR/install_manifest.txt
  -h, --help             Show this help and exit

Arguments:
  BUILD_DIR              Build output directory (default: build-linux)

Environment:
  JOBS                   Number of parallel build jobs (default: nproc)
  APPIMAGETOOL           Path to appimagetool binary
  FUSE_APPIMAGE_VALIDATE_APPSTREAM
                         Set to 1 to enable AppStream validation
EOF
      exit 0
      ;;
    --install)
      build_install=1
      ;;
    --install-prefix=*)
      install_prefix="${arg#--install-prefix=}"
      ;;
    --uninstall)
      build_uninstall=1
      ;;
    --package)
      build_package=1
      ;;
    --libretro)
      build_libretro=1
      ;;
    --appimage)
      build_appimage=1
      ;;
    *)
      build_dir_name="$arg"
      build_dir_overridden=1
      ;;
  esac
done

if [ "$build_libretro" -eq 1 ] && [ "$build_dir_overridden" -eq 0 ]; then
  build_dir_name="build-linux-libretro"
fi

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir="$root_dir/$build_dir_name"

if [ "$build_libretro" -eq 1 ]; then
  if [ "$build_appimage" -eq 1 ]; then
    echo "--libretro cannot be combined with --appimage" >&2
    exit 1
  fi

  if [ "$build_install" -eq 1 ]; then
    echo "--libretro cannot be combined with --install" >&2
    exit 1
  fi

  if [ "$build_uninstall" -eq 1 ]; then
    echo "--libretro cannot be combined with --uninstall" >&2
    exit 1
  fi

  if [ -n "$install_prefix" ]; then
    echo "--libretro cannot be combined with --install-prefix" >&2
    exit 1
  fi
fi

if [ "$build_uninstall" -eq 1 ]; then
  manifest="$build_dir/install_manifest.txt"
  if [ ! -f "$manifest" ]; then
    echo "No install manifest found: $manifest" >&2
    echo "Run with --install first." >&2
    exit 1
  fi
  xargs rm -vf < "$manifest"
  exit 0
fi

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

get_project_version() {
  tr -d '\r' < "$root_dir/CMakeLists.txt" |
    sed -n 's/^project(fuse VERSION \([^ ]*\) LANGUAGES C)$/\1/p'
}

jobs="${JOBS:-$(cpu_count)}"

set -- cmake -S "$root_dir" -B "$build_dir"

if [ "$build_libretro" -eq 1 ]; then
  set -- "$@" -DFUSE_PORTABLE_PACKAGE=OFF -DCMAKE_BUILD_TYPE=Release
elif [ "$build_package" -eq 1 ] || [ "$build_appimage" -eq 1 ]; then
  set -- "$@" -DFUSE_PORTABLE_PACKAGE=ON
else
  set -- "$@" -DFUSE_PORTABLE_PACKAGE=OFF
fi

if [ "$build_libretro" -eq 0 ] && { [ "$build_package" -eq 1 ] || [ "$build_appimage" -eq 1 ]; }; then
  set -- "$@" -DCMAKE_BUILD_TYPE=Release
fi

if [ "$build_libretro" -eq 0 ] && [ "$build_package" -eq 1 ]; then
  set -- "$@" -DCPACK_GENERATOR=TGZ
fi

"$@"

if [ "$build_libretro" -eq 1 ]; then
  cmake --build "$build_dir" --target fuse-libretro --parallel "$jobs"
else
  cmake --build "$build_dir" --parallel "$jobs"
fi

if [ "$build_libretro" -eq 1 ]; then
  core_path="$build_dir/fuse-sdl3_libretro.so"
  info_path="$build_dir/fuse-sdl3_libretro.info"
  libretro_rom_dir="$build_dir/roms"
  libretro_lib_dir="$build_dir/lib"

  if [ ! -f "$core_path" ]; then
    echo "Expected libretro core not found: $core_path" >&2
    exit 1
  fi

  if [ ! -f "$info_path" ]; then
    echo "Expected libretro info file not found: $info_path" >&2
    exit 1
  fi

  if [ ! -d "$libretro_rom_dir" ] || [ ! -d "$libretro_lib_dir" ]; then
    echo "Expected staged libretro asset directories not found in $build_dir" >&2
    exit 1
  fi

  chmod 755 "$core_path"

  if [ "$build_package" -eq 1 ]; then
    version=$(get_project_version)
    if [ -z "$version" ]; then
      echo "Failed to resolve project version from $root_dir/CMakeLists.txt" >&2
      exit 1
    fi

    arch=$(uname -m)
    archive_base="fuse-sdl3_libretro-${version}-Linux-${arch}"
    staging_dir="$build_dir/$archive_base"
    archive_path="$build_dir/${archive_base}.tar.gz"

    rm -rf "$staging_dir"
    rm -f "$archive_path"
    mkdir -p "$staging_dir"

    cp "$core_path" "$staging_dir/"
    cp "$info_path" "$staging_dir/"
    cp -a "$libretro_lib_dir" "$staging_dir/lib"
    cp -a "$libretro_rom_dir" "$staging_dir/roms"

    tar -C "$build_dir" -czf "$archive_path" "$archive_base"
    rm -rf "$staging_dir"

    echo "Libretro package generated in $archive_path"
  fi

  exit 0
fi

exe_path="$build_dir/fuse"
if [ ! -f "$exe_path" ]; then
  echo "Expected build output not found: $exe_path" >&2
  exit 1
fi

chmod 755 "$exe_path"

if [ "$build_package" -eq 1 ]; then
  cmake --build "$build_dir" --target package --parallel "$jobs"
  echo "Package archives generated in $build_dir"
fi

if [ "$build_install" -eq 1 ]; then
  if [ -n "$install_prefix" ]; then
    cmake --install "$build_dir" --prefix "$install_prefix"
  else
    cmake --install "$build_dir"
  fi
fi

if [ "$build_appimage" -eq 1 ]; then
  appdir="$build_dir/fuse-sdl3.AppDir"
  appimage_tool="${APPIMAGETOOL:-}"
  appimage_tool_appstream_flag="--no-appstream"
  arch=$(uname -m)
  version=$(get_project_version)
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
  cp "$icon_source" "$appdir/io.github.md0_code.FuseSDL3.png"

  mkdir -p "$appdir/usr/share/applications" "$appdir/usr/share/icons/hicolor/256x256/apps" "$appdir/usr/share/metainfo"
  cp "$desktop_source" "$appdir/usr/share/applications/$desktop_file_name"
  cp "$icon_source" "$appdir/usr/share/icons/hicolor/256x256/apps/io.github.md0_code.FuseSDL3.png"

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