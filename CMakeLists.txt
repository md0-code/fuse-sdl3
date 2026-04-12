cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0177)
  cmake_policy(SET CMP0177 NEW)
endif()

project(fuse VERSION 0.2.0 LANGUAGES C)

include(GNUInstallDirs)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

if(WIN32)
  add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
  if(DEFINED ENV{ComSpec})
    file(TO_CMAKE_PATH "$ENV{ComSpec}" FUSE_COMMAND_SHELL)
  else()
    set(FUSE_COMMAND_SHELL "C:/Windows/System32/cmd.exe")
  endif()
  set(FUSE_COMMAND_SHELL_ARG /C)
  set(FUSE_PKG_CONFIG_PATH_SEPARATOR ";")
else()
  set(FUSE_COMMAND_SHELL /bin/sh)
  set(FUSE_COMMAND_SHELL_ARG -c)
  set(FUSE_PKG_CONFIG_PATH_SEPARATOR ":")
endif()

include(CheckCSourceCompiles)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckTypeSize)
include(TestBigEndian)

find_package(PkgConfig QUIET)
find_package(Perl REQUIRED)
find_package(Threads)
find_package(OpenGL QUIET)
find_package(ZLIB)
find_package(BZip2)

# On Windows, check WinGet install locations before the REQUIRED find_program
# so that winget-installed flex/bison are discovered even if not on PATH.
if(WIN32 AND DEFINED ENV{LOCALAPPDATA})
  file(GLOB _fuse_winget_flex_candidates "$ENV{LOCALAPPDATA}/Microsoft/WinGet/Packages/*/win_flex.exe")
  list(LENGTH _fuse_winget_flex_candidates _fuse_winget_flex_count)
  if(_fuse_winget_flex_count GREATER 0)
    list(GET _fuse_winget_flex_candidates 0 FLEX_EXECUTABLE)
  endif()

  file(GLOB _fuse_winget_bison_candidates "$ENV{LOCALAPPDATA}/Microsoft/WinGet/Packages/*/win_bison.exe")
  list(LENGTH _fuse_winget_bison_candidates _fuse_winget_bison_count)
  if(_fuse_winget_bison_count GREATER 0)
    list(GET _fuse_winget_bison_candidates 0 BISON_EXECUTABLE)
  endif()
endif()

find_program(FLEX_EXECUTABLE NAMES win_flex flex REQUIRED)
find_program(BISON_EXECUTABLE NAMES win_bison bison REQUIRED)

string(TOUPPER "${CMAKE_BUILD_TYPE}" FUSE_BUILD_TYPE_UPPER)

get_filename_component(BISON_REAL_EXECUTABLE "${BISON_EXECUTABLE}" REALPATH)
get_filename_component(BISON_BIN_DIR "${BISON_REAL_EXECUTABLE}" DIRECTORY)
set(FUSE_BISON_COMMAND "${BISON_REAL_EXECUTABLE}")
if(EXISTS "${BISON_BIN_DIR}/data")
  set(FUSE_BISON_COMMAND "${CMAKE_COMMAND}" -E env "BISON_PKGDATADIR=${BISON_BIN_DIR}/data" "${BISON_REAL_EXECUTABLE}")
endif()

set(FUSE_LIBSPECTRUM_ROOT "" CACHE PATH "Optional prefix containing a prebuilt libspectrum for native builds")
set(_fuse_internal_libspectrum_default OFF)
if(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/external/libspectrum/CMakeLists.txt")
  set(_fuse_internal_libspectrum_default ON)
endif()
option(FUSE_USE_INTERNAL_LIBSPECTRUM "Build the vendored libspectrum subproject instead of using an external package" ${_fuse_internal_libspectrum_default})

if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
  set(_fuse_pkg_config_paths
    "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib/pkgconfig"
    "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/pkgconfig"
  )
  set(_fuse_pkg_config_path_env "$ENV{PKG_CONFIG_PATH}")
  foreach(_fuse_pkg_config_path IN LISTS _fuse_pkg_config_paths)
    if(EXISTS "${_fuse_pkg_config_path}")
      if(_fuse_pkg_config_path_env)
        set(_fuse_pkg_config_path_env
          "${_fuse_pkg_config_path}${FUSE_PKG_CONFIG_PATH_SEPARATOR}${_fuse_pkg_config_path_env}"
        )
      else()
        set(_fuse_pkg_config_path_env "${_fuse_pkg_config_path}")
      endif()
    endif()
  endforeach()
  if(_fuse_pkg_config_path_env)
    set(ENV{PKG_CONFIG_PATH} "${_fuse_pkg_config_path_env}")
  endif()
endif()

if(WIN32)
  find_package(SDL3 CONFIG REQUIRED)
  set(FUSE_SDL_TARGET SDL3::SDL3)
  find_package(SDL3_shadercross CONFIG QUIET)
else()
  if(NOT PkgConfig_FOUND)
    message(FATAL_ERROR "pkg-config is required on non-Windows builds")
  endif()
  pkg_check_modules(SDL REQUIRED IMPORTED_TARGET sdl3)
  set(FUSE_SDL_TARGET PkgConfig::SDL)
  pkg_check_modules(SDLSHADERCROSS QUIET IMPORTED_TARGET sdl3-shadercross)
endif()

set(HAVE_SDL_SHADERCROSS 0)
if(TARGET SDL3_shadercross::SDL3_shadercross OR TARGET PkgConfig::SDLSHADERCROSS)
  set(HAVE_SDL_SHADERCROSS 1)
endif()

set(HAVE_OPENGL 0)
if(TARGET OpenGL::GL OR OPENGL_FOUND)
  set(HAVE_OPENGL 1)
endif()

set(FUSE_LIBSPECTRUM_TARGET "")
if(FUSE_LIBSPECTRUM_ROOT)
  find_path(LIBSPECTRUM_INCLUDE_DIR
    NAMES libspectrum.h
    HINTS "${FUSE_LIBSPECTRUM_ROOT}/include"
  )
  find_library(LIBSPECTRUM_LIBRARY
    NAMES libspectrum spectrum
    HINTS "${FUSE_LIBSPECTRUM_ROOT}/lib" "${FUSE_LIBSPECTRUM_ROOT}/bin"
  )
  if(WIN32)
    if(FUSE_BUILD_TYPE_UPPER STREQUAL "DEBUG")
      set(_fuse_libspectrum_runtime_hints
        "${FUSE_LIBSPECTRUM_ROOT}/debug/bin"
        "${FUSE_LIBSPECTRUM_ROOT}/debug/lib"
      )
      file(GLOB LIBSPECTRUM_RUNTIME_DLLS
        "${FUSE_LIBSPECTRUM_ROOT}/debug/bin/*.dll"
        "${FUSE_LIBSPECTRUM_ROOT}/debug/lib/*.dll"
      )
    elseif(FUSE_BUILD_TYPE_UPPER STREQUAL "RELEASE" OR FUSE_BUILD_TYPE_UPPER STREQUAL "RELWITHDEBINFO" OR FUSE_BUILD_TYPE_UPPER STREQUAL "MINSIZEREL")
      set(_fuse_libspectrum_runtime_hints
        "${FUSE_LIBSPECTRUM_ROOT}/bin"
        "${FUSE_LIBSPECTRUM_ROOT}/lib"
      )
      file(GLOB LIBSPECTRUM_RUNTIME_DLLS
        "${FUSE_LIBSPECTRUM_ROOT}/bin/*.dll"
        "${FUSE_LIBSPECTRUM_ROOT}/lib/*.dll"
      )
    else()
      set(_fuse_libspectrum_runtime_hints
        "${FUSE_LIBSPECTRUM_ROOT}/bin"
        "${FUSE_LIBSPECTRUM_ROOT}/debug/bin"
        "${FUSE_LIBSPECTRUM_ROOT}/lib"
        "${FUSE_LIBSPECTRUM_ROOT}/debug/lib"
      )
      file(GLOB LIBSPECTRUM_RUNTIME_DLLS
        "${FUSE_LIBSPECTRUM_ROOT}/bin/*.dll"
        "${FUSE_LIBSPECTRUM_ROOT}/debug/bin/*.dll"
        "${FUSE_LIBSPECTRUM_ROOT}/lib/*.dll"
        "${FUSE_LIBSPECTRUM_ROOT}/debug/lib/*.dll"
      )
    endif()
    find_file(LIBSPECTRUM_RUNTIME
      NAMES spectrum-9.dll spectrum.dll libspectrum.dll
      HINTS ${_fuse_libspectrum_runtime_hints}
    )
  endif()
endif()

if(LIBSPECTRUM_INCLUDE_DIR AND LIBSPECTRUM_LIBRARY)
  add_library(FUSE::LIBSPECTRUM UNKNOWN IMPORTED)
  set_target_properties(FUSE::LIBSPECTRUM PROPERTIES
    IMPORTED_LOCATION "${LIBSPECTRUM_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBSPECTRUM_INCLUDE_DIR}"
  )
  set(LIBSPECTRUM_INCLUDE_DIRS "${LIBSPECTRUM_INCLUDE_DIR}")
  set(LIBSPECTRUM_CFLAGS_OTHER "")
  set(FUSE_LIBSPECTRUM_TARGET FUSE::LIBSPECTRUM)
elseif(FUSE_USE_INTERNAL_LIBSPECTRUM)
  if(EXISTS "${CMAKE_SOURCE_DIR}/external/libspectrum/CMakeLists.txt")
    set(LIBSPECTRUM_INSTALL OFF CACHE BOOL "Install libspectrum targets and development files" FORCE)
    add_subdirectory(external/libspectrum)
    set(FUSE_LIBSPECTRUM_TARGET libspectrum::libspectrum)
    set(LIBSPECTRUM_INCLUDE_DIRS "${FUSE_INTERNAL_LIBSPECTRUM_INCLUDE_DIRS}")
    set(LIBSPECTRUM_CFLAGS_OTHER "${FUSE_INTERNAL_LIBSPECTRUM_CFLAGS_OTHER}")
  else()
    message(WARNING "FUSE_USE_INTERNAL_LIBSPECTRUM is ON but external/libspectrum is missing; falling back to external package discovery")
  endif()
endif()

if(NOT FUSE_LIBSPECTRUM_TARGET AND DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
  set(_fuse_vcpkg_libspectrum_prefix "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
  find_path(LIBSPECTRUM_INCLUDE_DIR
    NAMES libspectrum.h
    HINTS "${_fuse_vcpkg_libspectrum_prefix}/include"
  )
  find_library(LIBSPECTRUM_LIBRARY
    NAMES libspectrum spectrum
    HINTS "${_fuse_vcpkg_libspectrum_prefix}/lib" "${_fuse_vcpkg_libspectrum_prefix}/debug/lib"
  )

  if(LIBSPECTRUM_INCLUDE_DIR AND LIBSPECTRUM_LIBRARY)
    add_library(FUSE::LIBSPECTRUM UNKNOWN IMPORTED)
    set_target_properties(FUSE::LIBSPECTRUM PROPERTIES
      IMPORTED_LOCATION "${LIBSPECTRUM_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${LIBSPECTRUM_INCLUDE_DIR}"
    )
    set(LIBSPECTRUM_INCLUDE_DIRS "${LIBSPECTRUM_INCLUDE_DIR}")
    set(LIBSPECTRUM_CFLAGS_OTHER "")
    set(FUSE_LIBSPECTRUM_TARGET FUSE::LIBSPECTRUM)
  endif()
elseif(PkgConfig_FOUND)
  pkg_check_modules(LIBSPECTRUM QUIET IMPORTED_TARGET libspectrum>=1.5.0)
  if(TARGET PkgConfig::LIBSPECTRUM)
    set(FUSE_LIBSPECTRUM_TARGET PkgConfig::LIBSPECTRUM)
  endif()
endif()

if(NOT FUSE_LIBSPECTRUM_TARGET)
  message(FATAL_ERROR
    "libspectrum >= 1.5.0 was not found. Run scripts/build-win.ps1 on Windows, enable FUSE_USE_INTERNAL_LIBSPECTRUM when external/libspectrum is present, or point FUSE_LIBSPECTRUM_ROOT at a native libspectrum prefix."
  )
endif()

option(FUSE_WITH_LIBXML2 "Build with XML settings support" ON)
option(FUSE_WITH_PNG "Build with PNG screenshot support when available" ON)
option(FUSE_WITH_SOCKETS "Enable sockets-dependent features" ON)
option(FUSE_WITH_PTHREAD "Enable pthread-dependent features" ON)
option(FUSE_USE_JOYSTICK "Enable SDL joystick support" ON)
option(FUSE_SMALLMEM "Disable the large-memory Z80 opcode path" OFF)
option(FUSE_PORTABLE_PACKAGE "Build package archives with a portable layout" OFF)
set(FUSE_ROMSDIR "" CACHE PATH "Optional installed ROM directory override")

set(FUSE_BUILD_NUMBER 1)
set(FUSE_NAME "Fuse SDL3")
set(FUSE_URL "https://github.com/md0-code/fuse-sdl3")
set(FUSE_VERSION "${PROJECT_VERSION}")
set(FUSE_COPYRIGHT "Published under the GNU General Public License, version 3 or later")
set(FUSE_RC_VERSION "0,2,0,0")
set(PACKAGE "fuse-sdl3")
set(PACKAGE_BUGREPORT "https://github.com/md0-code/fuse-sdl3/issues")
set(PACKAGE_NAME "Fuse SDL3")
set(PACKAGE_STRING "Fuse SDL3 ${PROJECT_VERSION}")
set(PACKAGE_TARNAME "fuse-sdl3")
set(PACKAGE_URL "https://github.com/md0-code/fuse-sdl3")
set(PACKAGE_VERSION "${PROJECT_VERSION}")
set(VERSION "${PROJECT_VERSION}")
set(LT_OBJDIR ".libs/")

set(HAVE_CONFIG_H 1)
set(UI_SDL 1)
set(USE_WIDGET 1)
set(FUSE_ICON_AVAILABLE 1)
set(YYTEXT_POINTER 1)
set(STDC_HEADERS 1)
set(AC_APPLE_UNIVERSAL_BUILD 0)
set(NO_SOUND 0)
set(DIRECTSOUND_VERSION "")
set(PTHREAD_CREATE_JOINABLE "")
set(WINVER "")
set(_WIN32_IE 0x0501)
set(WIN32_LEAN_AND_MEAN 1)

check_include_file(dlfcn.h HAVE_DLFCN_H)
check_include_file(inttypes.h HAVE_INTTYPES_H)
check_include_file(libgen.h HAVE_LIBGEN_H)
check_include_file(siginfo.h HAVE_SIGINFO_H)
check_include_file(stdint.h HAVE_STDINT_H)
check_include_file(stdio.h HAVE_STDIO_H)
check_include_file(stdlib.h HAVE_STDLIB_H)
check_include_file(strings.h HAVE_STRINGS_H)
check_include_file(string.h HAVE_STRING_H)
check_include_file(sys/audioio.h HAVE_SYS_AUDIOIO_H)
check_include_file(sys/audio.h HAVE_SYS_AUDIO_H)
check_include_file(sys/soundcard.h HAVE_SYS_SOUNDCARD_H)
check_include_file(sys/stat.h HAVE_SYS_STAT_H)
check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(unistd.h HAVE_UNISTD_H)

check_symbol_exists(dirname "libgen.h" HAVE_DIRNAME)
check_symbol_exists(strcasecmp "string.h" HAVE_STRING_STRCASECMP)
if(NOT HAVE_STRING_STRCASECMP)
  check_symbol_exists(strcasecmp "strings.h" HAVE_STRINGS_STRCASECMP)
endif()
check_function_exists(fsync HAVE_FSYNC)
check_function_exists(geteuid HAVE_GETEUID)
check_function_exists(getopt_long HAVE_GETOPT_LONG)

check_type_size("long" SIZEOF_LONG)
test_big_endian(WORDS_BIGENDIAN)

if(SIZEOF_LONG GREATER_EQUAL 8)
  set(BLIP_BUFFER_ACCURACY 32)
else()
  set(BLIP_BUFFER_ACCURACY 16)
endif()

if(FUSE_SMALLMEM)
  set(HAVE_ENOUGH_MEMORY 0)
else()
  set(HAVE_ENOUGH_MEMORY 1)
endif()

set(HAVE_LIBM 0)
find_library(MATH_LIBRARY m)
if(MATH_LIBRARY)
  set(HAVE_LIBM 1)
endif()

set(LIBSPECTRUM_REQUIRES_GLIB 0)
set(_saved_required_includes "${CMAKE_REQUIRED_INCLUDES}")
set(_saved_required_flags "${CMAKE_REQUIRED_FLAGS}")
set(CMAKE_REQUIRED_INCLUDES ${LIBSPECTRUM_INCLUDE_DIRS})
string(REPLACE ";" " " _libspectrum_required_flags "${LIBSPECTRUM_CFLAGS_OTHER}")
set(CMAKE_REQUIRED_FLAGS "${_libspectrum_required_flags}")
check_c_source_compiles(
  "#include <libspectrum.h>\n#if LIBSPECTRUM_HAS_GLIB_REPLACEMENT == 0\n#error missing\n#endif\nint main(void){return 0;}"
  LIBSPECTRUM_HAS_INTERNAL_GLIB)
set(CMAKE_REQUIRED_INCLUDES "${_saved_required_includes}")
set(CMAKE_REQUIRED_FLAGS "${_saved_required_flags}")

if(WIN32)
  set(LIBSPECTRUM_HAS_INTERNAL_GLIB 1)
endif()

set(HAVE_LIB_GLIB 0)
if(NOT LIBSPECTRUM_HAS_INTERNAL_GLIB)
  if(NOT PkgConfig_FOUND)
    message(FATAL_ERROR "pkg-config is required to locate glib-2.0 for this build")
  endif()
  pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0>=2.20.0)
  set(HAVE_LIB_GLIB 1)
endif()

set(_saved_required_includes "${CMAKE_REQUIRED_INCLUDES}")
set(_saved_required_flags "${CMAKE_REQUIRED_FLAGS}")
set(CMAKE_REQUIRED_INCLUDES ${LIBSPECTRUM_INCLUDE_DIRS})
set(CMAKE_REQUIRED_FLAGS "${_libspectrum_required_flags}")
check_c_source_compiles(
  "#include <libspectrum.h>\nint main(void){int test = LIBSPECTRUM_SUPPORTS_ZLIB_COMPRESSION; return test ? 0 : 0;}"
  LIBSPEC_HAVE_ZLIB)
check_c_source_compiles(
  "#include <libspectrum.h>\nint main(void){int test = LIBSPECTRUM_SUPPORTS_BZ2_COMPRESSION; return test ? 0 : 0;}"
  LIBSPEC_HAVE_BZ2)
check_c_source_compiles(
  "#include <libspectrum.h>\nint main(void){int test = LIBSPECTRUM_SUPPORTS_AUDIOFILE; return test ? 0 : 0;}"
  LIBSPEC_HAVE_AUDIOFILE)
set(CMAKE_REQUIRED_INCLUDES "${_saved_required_includes}")
set(CMAKE_REQUIRED_FLAGS "${_saved_required_flags}")

set(HAVE_LIB_XML2 0)
if(FUSE_WITH_LIBXML2)
  if(WIN32)
    find_package(LibXml2 REQUIRED)
  else()
    if(NOT PkgConfig_FOUND)
      message(FATAL_ERROR "pkg-config is required to locate libxml2 on this platform")
    endif()
    pkg_check_modules(XML REQUIRED IMPORTED_TARGET libxml-2.0>=2.6.0)
  endif()
  set(HAVE_LIB_XML2 1)
endif()

set(USE_LIBPNG 0)
set(HAVE_ZLIB_H 0)
if(FUSE_WITH_PNG)
  if(WIN32)
    find_package(PNG QUIET)
    if(PNG_FOUND)
      set(USE_LIBPNG 1)
    endif()
  else()
    if(PkgConfig_FOUND)
      pkg_check_modules(PNG IMPORTED_TARGET libpng)
    endif()
    if(PNG_FOUND)
      set(USE_LIBPNG 1)
    endif()
  endif()
endif()

if(ZLIB_FOUND)
  set(HAVE_ZLIB_H 1)
endif()

set(FUSE_GENERATED_MIME_XML "${CMAKE_BINARY_DIR}/data/fuse.xml")
set(FUSE_DESKTOP_FILE_NAME "io.github.md0_code.FuseSDL3.desktop")
set(FUSE_APPSTREAM_COMPONENT_ID "${FUSE_DESKTOP_FILE_NAME}")
set(FUSE_APPSTREAM_FILE_NAME "io.github.md0_code.FuseSDL3.appdata.xml")
set(FUSE_GENERATED_DESKTOP_FILE "${CMAKE_BINARY_DIR}/data/${FUSE_DESKTOP_FILE_NAME}")
set(FUSE_GENERATED_APPSTREAM_FILE "${CMAKE_BINARY_DIR}/data/${FUSE_APPSTREAM_FILE_NAME}")
set(FUSE_RUNTIME_DEPENDENCY_SCRIPT "${CMAKE_BINARY_DIR}/cmake/InstallRuntimeDependencies.cmake")

if(FUSE_PORTABLE_PACKAGE)
  set(FUSE_RUNTIME_INSTALL_DESTINATION ".")
  if(UNIX AND NOT WIN32)
    set(FUSE_RUNTIME_LIBRARY_INSTALL_DESTINATION "lib")
  else()
    set(FUSE_RUNTIME_LIBRARY_INSTALL_DESTINATION ".")
  endif()
  set(FUSE_ROMS_INSTALL_DESTINATION "roms")
  set(FUSE_DESKTOP_INTEGRATION_STATUS "portable package")
else()
  set(FUSE_RUNTIME_INSTALL_DESTINATION "${CMAKE_INSTALL_BINDIR}")
  set(FUSE_RUNTIME_LIBRARY_INSTALL_DESTINATION "${FUSE_RUNTIME_INSTALL_DESTINATION}")
  set(FUSE_ROMS_INSTALL_DESTINATION "${CMAKE_INSTALL_DATADIR}/fuse/roms")
  set(FUSE_DESKTOP_INTEGRATION_STATUS "generated")
endif()

set(FUSE_DEFAULT_CONFIG_NAME ".fuse-sdl3")

set(FUSE_INSTALLED_DEFAULT_CONFIG_NAME "${FUSE_DEFAULT_CONFIG_NAME}")
if(FUSE_PORTABLE_PACKAGE)
  set(FUSE_INSTALLED_DEFAULT_CONFIG_NAME ".fuse-sdl3.default")
endif()

if(HAVE_LIB_XML2)
  set(FUSE_DEFAULT_CONFIG_FORMAT xml)
else()
  set(FUSE_DEFAULT_CONFIG_FORMAT ini)
endif()

set(FUSE_GENERATED_DEFAULT_CONFIG "${CMAKE_BINARY_DIR}/${FUSE_DEFAULT_CONFIG_NAME}")

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/data")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/cmake")

file(READ "${CMAKE_SOURCE_DIR}/data/fuse.xml.in" FUSE_MIME_XML_CONTENT)
string(REPLACE "\r\n" "\n" FUSE_MIME_XML_CONTENT "${FUSE_MIME_XML_CONTENT}")
string(REPLACE "\r" "\n" FUSE_MIME_XML_CONTENT "${FUSE_MIME_XML_CONTENT}")
foreach(_fuse_marker IN ITEMS
  "##LIBSPECTRUM_HAVE_AUDIOFILE_BEGIN##"
  "##LIBSPECTRUM_HAVE_AUDIOFILE_END##"
  "##LIBSPECTRUM_HAVE_ZLIB_BEGIN##"
  "##LIBSPECTRUM_HAVE_ZLIB_END##"
  "##LIBSPECTRUM_HAVE_BZ2_BEGIN##"
  "##LIBSPECTRUM_HAVE_BZ2_END##")
  string(REPLACE "${_fuse_marker}" "" FUSE_MIME_XML_CONTENT "${FUSE_MIME_XML_CONTENT}")
endforeach()
file(WRITE "${FUSE_GENERATED_MIME_XML}" "${FUSE_MIME_XML_CONTENT}")

string(REGEX MATCHALL "<mime-type type=\"[^\"]+\">" FUSE_MIME_MATCHES "${FUSE_MIME_XML_CONTENT}")
set(FUSE_DESKTOP_MIME_TYPES "")
foreach(_fuse_mime_match IN LISTS FUSE_MIME_MATCHES)
  if(_fuse_mime_match MATCHES "<mime-type type=\"([^\"]+)\">")
    string(APPEND FUSE_DESKTOP_MIME_TYPES "${CMAKE_MATCH_1};")
  endif()
endforeach()

file(READ "${CMAKE_SOURCE_DIR}/data/fuse.desktop.in" FUSE_DESKTOP_TEMPLATE_CONTENT)
string(REPLACE "@MIME_TYPES@" "${FUSE_DESKTOP_MIME_TYPES}" FUSE_DESKTOP_CONTENT "${FUSE_DESKTOP_TEMPLATE_CONTENT}")
file(WRITE "${FUSE_GENERATED_DESKTOP_FILE}" "${FUSE_DESKTOP_CONTENT}")

configure_file(
  "${CMAKE_SOURCE_DIR}/data/fuse.appdata.xml.in"
  "${FUSE_GENERATED_APPSTREAM_FILE}"
  @ONLY
)

set(FUSE_RUNTIME_SEARCH_DIRS "")
if(WIN32 AND FUSE_LIBSPECTRUM_ROOT)
  if(FUSE_BUILD_TYPE_UPPER STREQUAL "DEBUG")
    list(APPEND FUSE_RUNTIME_SEARCH_DIRS
      "${FUSE_LIBSPECTRUM_ROOT}/debug/bin"
      "${FUSE_LIBSPECTRUM_ROOT}/debug/lib"
    )
  elseif(FUSE_BUILD_TYPE_UPPER STREQUAL "RELEASE" OR FUSE_BUILD_TYPE_UPPER STREQUAL "RELWITHDEBINFO" OR FUSE_BUILD_TYPE_UPPER STREQUAL "MINSIZEREL")
    list(APPEND FUSE_RUNTIME_SEARCH_DIRS
      "${FUSE_LIBSPECTRUM_ROOT}/bin"
      "${FUSE_LIBSPECTRUM_ROOT}/lib"
    )
  else()
    list(APPEND FUSE_RUNTIME_SEARCH_DIRS
      "${FUSE_LIBSPECTRUM_ROOT}/bin"
      "${FUSE_LIBSPECTRUM_ROOT}/debug/bin"
      "${FUSE_LIBSPECTRUM_ROOT}/lib"
      "${FUSE_LIBSPECTRUM_ROOT}/debug/lib"
    )
  endif()
endif()

set(FUSE_RUNTIME_SEARCH_DIRS_ESCAPED "")
foreach(_fuse_runtime_dir IN LISTS FUSE_RUNTIME_SEARCH_DIRS)
  if(EXISTS "${_fuse_runtime_dir}")
    string(REPLACE "\\" "/" _fuse_runtime_dir_normalized "${_fuse_runtime_dir}")
    list(APPEND FUSE_RUNTIME_SEARCH_DIRS_ESCAPED "${_fuse_runtime_dir_normalized}")
  endif()
endforeach()

set(FUSE_RUNTIME_POST_EXCLUDE_REGEXES "")
if(WIN32)
  list(APPEND FUSE_RUNTIME_POST_EXCLUDE_REGEXES
    ".*[/\\\\][Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\\\][Ss]ystem32[/\\\\].*\\.dll$"
    ".*[/\\\\][Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\\\][Ss][Yy][Ss][Ww][Oo][Ww]64[/\\\\].*\\.dll$"
  )
else()
  list(APPEND FUSE_RUNTIME_POST_EXCLUDE_REGEXES
    ".*/ld-linux[^/]*\\.so[^/]*$"
    ".*/libc\\.so[^/]*$"
    ".*/libdl\\.so[^/]*$"
    ".*/libgcc_s\\.so[^/]*$"
    ".*/libm\\.so[^/]*$"
    ".*/libpthread\\.so[^/]*$"
    ".*/librt\\.so[^/]*$"
    ".*/libutil\\.so[^/]*$"
  )
endif()

string(REPLACE ";" "|" FUSE_RUNTIME_SEARCH_DIRS_SERIALIZED "${FUSE_RUNTIME_SEARCH_DIRS_ESCAPED}")
string(REPLACE ";" "|" FUSE_RUNTIME_POST_EXCLUDE_SERIALIZED "${FUSE_RUNTIME_POST_EXCLUDE_REGEXES}")

configure_file(
  "${CMAKE_SOURCE_DIR}/cmake/InstallRuntimeDependencies.cmake.in"
  "${FUSE_RUNTIME_DEPENDENCY_SCRIPT}"
  @ONLY
)

function(fuse_shell_to_file output command deps)
  get_filename_component(output_dir "${output}" DIRECTORY)
  set(command_file "${output}.command")
  file(WRITE "${command_file}" "${command}\n")
  add_custom_command(
    OUTPUT "${output}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_dir}"
    COMMAND
      "${CMAKE_COMMAND}"
      -DOUTPUT_FILE=${output}
      -DSHELL_EXECUTABLE=${FUSE_COMMAND_SHELL}
      -DSHELL_ARG=${FUSE_COMMAND_SHELL_ARG}
      -DCOMMAND_FILE=${command_file}
      -P "${CMAKE_SOURCE_DIR}/cmake/RunShellToFile.cmake"
    DEPENDS ${deps} "${command_file}"
    VERBATIM
  )
endfunction()

function(fuse_command_to_file output deps)
  get_filename_component(output_dir "${output}" DIRECTORY)
  set(command_file "${output}.argv")
  file(WRITE "${command_file}" "")
  foreach(arg IN LISTS ARGN)
    file(APPEND "${command_file}" "${arg}\n")
  endforeach()
  add_custom_command(
    OUTPUT "${output}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_dir}"
    COMMAND
      "${CMAKE_COMMAND}"
      -DOUTPUT_FILE=${output}
      -DCOMMAND_FILE=${command_file}
      -P "${CMAKE_SOURCE_DIR}/cmake/RunProcessToFile.cmake"
    DEPENDS ${deps} "${command_file}"
    VERBATIM
  )
endfunction()

function(fuse_pipeline_to_file output deps)
  set(options)
  set(oneValueArgs)
  set(multiValueArgs COMMAND1 COMMAND2)
  cmake_parse_arguments(FUSE_PIPE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  get_filename_component(output_dir "${output}" DIRECTORY)
  set(command1_file "${output}.command1.argv")
  set(command2_file "${output}.command2.argv")

  file(WRITE "${command1_file}" "")
  foreach(arg IN LISTS FUSE_PIPE_COMMAND1)
    file(APPEND "${command1_file}" "${arg}\n")
  endforeach()

  file(WRITE "${command2_file}" "")
  foreach(arg IN LISTS FUSE_PIPE_COMMAND2)
    file(APPEND "${command2_file}" "${arg}\n")
  endforeach()

  add_custom_command(
    OUTPUT "${output}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_dir}"
    COMMAND
      "${CMAKE_COMMAND}"
      -DOUTPUT_FILE=${output}
      -DCOMMAND1_FILE=${command1_file}
      -DCOMMAND2_FILE=${command2_file}
      -P "${CMAKE_SOURCE_DIR}/cmake/RunProcessPipelineToFile.cmake"
    DEPENDS ${deps} "${command1_file}" "${command2_file}"
    VERBATIM
  )
endfunction()

if(FUSE_WITH_PNG)
  if(USE_LIBPNG)
    set(USE_LIBPNG 1)
  endif()
endif()

set(HAVE_PTHREAD 0)
set(HAVE_PTHREAD_PRIO_INHERIT 0)
if(Threads_FOUND AND FUSE_WITH_PTHREAD)
  set(HAVE_PTHREAD 1)
  check_symbol_exists(PTHREAD_PRIO_INHERIT "pthread.h" HAVE_PTHREAD_PRIO_INHERIT)
endif()

set(HAVE_SOCKETS 0)
if(FUSE_WITH_SOCKETS)
  check_include_file(sys/socket.h HAVE_SYS_SOCKET_H)
  if(HAVE_SYS_SOCKET_H)
    set(HAVE_SOCKETS 1)
  endif()
endif()

set(BUILD_TTX2000S 0)
if(HAVE_SOCKETS)
  set(BUILD_TTX2000S 1)
endif()

set(BUILD_SPECTRANET 0)
if(HAVE_SOCKETS AND HAVE_PTHREAD)
  set(BUILD_SPECTRANET 1)
endif()

set(BUILD_SPECCYBOOT 0)
set(_saved_required_includes "${CMAKE_REQUIRED_INCLUDES}")
check_c_source_compiles(
  "#include <linux/if_tun.h>\n#include <net/if.h>\n#include <sys/ioctl.h>\nint main(void){int test1 = IFF_TAP | IFF_NO_PI; int test2 = TUNSETIFF; return test1 == test2;}"
  HAVE_LINUX_TUNTAP)
set(CMAKE_REQUIRED_INCLUDES "${_saved_required_includes}")
if(HAVE_LINUX_TUNTAP)
  set(BUILD_SPECCYBOOT 1)
endif()

if(FUSE_USE_JOYSTICK)
  set(USE_JOYSTICK 1)
else()
  set(USE_JOYSTICK 0)
endif()

if(FUSE_ROMSDIR)
  set(ROMSDIR "${FUSE_ROMSDIR}")
elseif(NOT FUSE_PORTABLE_PACKAGE)
  set(ROMSDIR "${CMAKE_INSTALL_FULL_DATADIR}/fuse/roms")
endif()

configure_file(
  "${CMAKE_SOURCE_DIR}/config.h.cmake.in"
  "${CMAKE_BINARY_DIR}/config.h"
  @ONLY
)

set(GENERATED_SETTINGS_C "${CMAKE_BINARY_DIR}/settings.c")
set(GENERATED_SETTINGS_H "${CMAKE_BINARY_DIR}/settings.h")
set(GENERATED_OPTIONS_H "${CMAKE_BINARY_DIR}/options.h")
set(GENERATED_DEBUGGER_DIR "${CMAKE_BINARY_DIR}/debugger")
set(GENERATED_DEBUGGER_LEXER_C "${GENERATED_DEBUGGER_DIR}/commandl.c")
set(GENERATED_DEBUGGER_PARSER_C "${GENERATED_DEBUGGER_DIR}/commandy.c")
set(GENERATED_DEBUGGER_PARSER_H "${GENERATED_DEBUGGER_DIR}/commandy.h")
set(GENERATED_SDL_KEYSYMS "${CMAKE_BINARY_DIR}/ui/sdl/keysyms.c")
set(GENERATED_WIDGET_MENU_DATA "${CMAKE_BINARY_DIR}/ui/widget/menu_data.c")
set(GENERATED_WIDGET_OPTIONS_C "${CMAKE_BINARY_DIR}/ui/widget/options.c")
set(GENERATED_WIDGET_OPTIONS_H "${CMAKE_BINARY_DIR}/ui/widget/options_internals.h")
set(GENERATED_WIDGET_FONT "${CMAKE_BINARY_DIR}/ui/widget/fuse.font")
set(GENERATED_EMBEDDED_ASSETS_H "${CMAKE_BINARY_DIR}/embedded_assets.h")
set(GENERATED_EMBEDDED_ASSETS_C "${CMAKE_BINARY_DIR}/embedded_assets.c")
set(FUSE_EMBEDDED_ASSETS_SPEC "${CMAKE_BINARY_DIR}/embedded_assets.spec")
set(GENERATED_Z80_OPCODES_BASE "${CMAKE_BINARY_DIR}/z80/opcodes_base.c")
set(GENERATED_Z80_CB "${CMAKE_BINARY_DIR}/z80/z80_cb.c")
set(GENERATED_Z80_DDFD "${CMAKE_BINARY_DIR}/z80/z80_ddfd.c")
set(GENERATED_Z80_DDFDCB "${CMAKE_BINARY_DIR}/z80/z80_ddfdcb.c")
set(GENERATED_Z80_ED "${CMAKE_BINARY_DIR}/z80/z80_ed.c")

fuse_command_to_file(
  "${GENERATED_SETTINGS_C}"
  "${CMAKE_SOURCE_DIR}/settings.pl;${CMAKE_SOURCE_DIR}/settings.dat"
  "${PERL_EXECUTABLE}" "-I${CMAKE_SOURCE_DIR}/perl" "${CMAKE_SOURCE_DIR}/settings.pl" "${CMAKE_SOURCE_DIR}/settings.dat"
)

fuse_command_to_file(
  "${GENERATED_SETTINGS_H}"
  "${CMAKE_SOURCE_DIR}/settings-header.pl;${CMAKE_SOURCE_DIR}/settings.dat"
  "${PERL_EXECUTABLE}" "-I${CMAKE_SOURCE_DIR}/perl" "${CMAKE_SOURCE_DIR}/settings-header.pl" "${CMAKE_SOURCE_DIR}/settings.dat"
)

fuse_pipeline_to_file(
  "${GENERATED_OPTIONS_H}"
  "${CMAKE_SOURCE_DIR}/perl/cpp-perl.pl;${CMAKE_BINARY_DIR}/config.h;${CMAKE_SOURCE_DIR}/ui/widget/options-header.pl;${CMAKE_SOURCE_DIR}/ui/options.dat;${CMAKE_SOURCE_DIR}/perl/Fuse.pm;${CMAKE_SOURCE_DIR}/perl/Fuse/Dialog.pm"
  COMMAND1 "${PERL_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/perl/cpp-perl.pl" "${CMAKE_BINARY_DIR}/config.h" "${CMAKE_SOURCE_DIR}/ui/options.dat"
  COMMAND2 "${PERL_EXECUTABLE}" "-I${CMAKE_SOURCE_DIR}/perl" "${CMAKE_SOURCE_DIR}/ui/widget/options-header.pl" - public
)

fuse_command_to_file(
  "${GENERATED_SDL_KEYSYMS}"
  "${CMAKE_SOURCE_DIR}/keysyms.pl;${CMAKE_SOURCE_DIR}/keysyms.dat"
  "${PERL_EXECUTABLE}" "-I${CMAKE_SOURCE_DIR}/perl" "${CMAKE_SOURCE_DIR}/keysyms.pl" sdl "${CMAKE_SOURCE_DIR}/keysyms.dat"
)

fuse_pipeline_to_file(
  "${GENERATED_WIDGET_OPTIONS_C}"
  "${CMAKE_SOURCE_DIR}/perl/cpp-perl.pl;${CMAKE_BINARY_DIR}/config.h;${CMAKE_SOURCE_DIR}/ui/widget/options.pl;${CMAKE_SOURCE_DIR}/ui/options.dat;${CMAKE_SOURCE_DIR}/perl/Fuse.pm;${CMAKE_SOURCE_DIR}/perl/Fuse/Dialog.pm"
  COMMAND1 "${PERL_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/perl/cpp-perl.pl" "${CMAKE_BINARY_DIR}/config.h" "${CMAKE_SOURCE_DIR}/ui/options.dat"
  COMMAND2 "${PERL_EXECUTABLE}" "-I${CMAKE_SOURCE_DIR}/perl" "${CMAKE_SOURCE_DIR}/ui/widget/options.pl" -
)

fuse_pipeline_to_file(
  "${GENERATED_WIDGET_OPTIONS_H}"
  "${CMAKE_SOURCE_DIR}/perl/cpp-perl.pl;${CMAKE_BINARY_DIR}/config.h;${CMAKE_SOURCE_DIR}/ui/widget/options-header.pl;${CMAKE_SOURCE_DIR}/ui/options.dat;${CMAKE_SOURCE_DIR}/perl/Fuse.pm;${CMAKE_SOURCE_DIR}/perl/Fuse/Dialog.pm"
  COMMAND1 "${PERL_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/perl/cpp-perl.pl" "${CMAKE_BINARY_DIR}/config.h" "${CMAKE_SOURCE_DIR}/ui/options.dat"
  COMMAND2 "${PERL_EXECUTABLE}" "-I${CMAKE_SOURCE_DIR}/perl" "${CMAKE_SOURCE_DIR}/ui/widget/options-header.pl" -
)

fuse_pipeline_to_file(
  "${GENERATED_WIDGET_MENU_DATA}"
  "${CMAKE_SOURCE_DIR}/perl/cpp-perl.pl;${CMAKE_SOURCE_DIR}/menu_data.dat;${CMAKE_SOURCE_DIR}/menu_data.pl;${CMAKE_BINARY_DIR}/config.h"
  COMMAND1 "${PERL_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/perl/cpp-perl.pl" "${CMAKE_BINARY_DIR}/config.h" "${CMAKE_SOURCE_DIR}/menu_data.dat"
  COMMAND2 "${PERL_EXECUTABLE}" "-I${CMAKE_SOURCE_DIR}/perl" "${CMAKE_SOURCE_DIR}/menu_data.pl" widget
)

fuse_command_to_file(
  "${GENERATED_WIDGET_FONT}"
  "${CMAKE_SOURCE_DIR}/ui/widget/mkfusefont.pl;${CMAKE_SOURCE_DIR}/ui/widget/fuse.font.sbf"
  "${PERL_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/ui/widget/mkfusefont.pl" "${CMAKE_SOURCE_DIR}/ui/widget/fuse.font.sbf"
)

fuse_command_to_file(
  "${FUSE_GENERATED_DEFAULT_CONFIG}"
  "${CMAKE_SOURCE_DIR}/scripts/generate-default-config.pl;${CMAKE_SOURCE_DIR}/settings.dat"
  "${PERL_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/generate-default-config.pl" "${CMAKE_SOURCE_DIR}/settings.dat" "${FUSE_DEFAULT_CONFIG_FORMAT}"
)

file(WRITE "${FUSE_EMBEDDED_ASSETS_SPEC}"
  "LIB|cassette.bmp|${CMAKE_SOURCE_DIR}/lib/cassette.bmp\n"
  "LIB|keyboard.png|${CMAKE_SOURCE_DIR}/lib/keyboard.png\n"
  "LIB|keyboard.scr|${CMAKE_SOURCE_DIR}/lib/keyboard.scr\n"
  "LIB|logo-about.bmp|${CMAKE_SOURCE_DIR}/lib/logo-about.bmp\n"
  "LIB|logo-icon.bmp|${CMAKE_SOURCE_DIR}/lib/logo-icon.bmp\n"
  "LIB|microdrive.bmp|${CMAKE_SOURCE_DIR}/lib/microdrive.bmp\n"
  "LIB|plus3disk.bmp|${CMAKE_SOURCE_DIR}/lib/plus3disk.bmp\n"
  "WIDGET|fuse.font|${GENERATED_WIDGET_FONT}\n"
)

add_custom_command(
  OUTPUT "${GENERATED_EMBEDDED_ASSETS_H}" "${GENERATED_EMBEDDED_ASSETS_C}"
  COMMAND
    "${CMAKE_COMMAND}"
    -DINPUT_SPEC=${FUSE_EMBEDDED_ASSETS_SPEC}
    -DOUTPUT_HEADER=${GENERATED_EMBEDDED_ASSETS_H}
    -DOUTPUT_SOURCE=${GENERATED_EMBEDDED_ASSETS_C}
    -P "${CMAKE_SOURCE_DIR}/cmake/GenerateEmbeddedAssets.cmake"
  DEPENDS
    "${FUSE_EMBEDDED_ASSETS_SPEC}"
    "${CMAKE_SOURCE_DIR}/lib/cassette.bmp"
    "${CMAKE_SOURCE_DIR}/lib/keyboard.png"
    "${CMAKE_SOURCE_DIR}/lib/keyboard.scr"
    "${CMAKE_SOURCE_DIR}/lib/logo-about.bmp"
    "${CMAKE_SOURCE_DIR}/lib/logo-icon.bmp"
    "${CMAKE_SOURCE_DIR}/lib/microdrive.bmp"
    "${CMAKE_SOURCE_DIR}/lib/plus3disk.bmp"
    "${GENERATED_WIDGET_FONT}"
    "${CMAKE_SOURCE_DIR}/cmake/GenerateEmbeddedAssets.cmake"
  VERBATIM
)

add_custom_command(
  OUTPUT "${GENERATED_DEBUGGER_PARSER_C}" "${GENERATED_DEBUGGER_PARSER_H}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${GENERATED_DEBUGGER_DIR}"
  COMMAND ${FUSE_BISON_COMMAND} -d -o "${GENERATED_DEBUGGER_PARSER_C}" "${CMAKE_SOURCE_DIR}/debugger/commandy.y"
  DEPENDS "${CMAKE_SOURCE_DIR}/debugger/commandy.y"
  VERBATIM
)

add_custom_command(
  OUTPUT "${GENERATED_DEBUGGER_LEXER_C}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${GENERATED_DEBUGGER_DIR}"
  COMMAND "${FLEX_EXECUTABLE}" -o "${GENERATED_DEBUGGER_LEXER_C}" "${CMAKE_SOURCE_DIR}/debugger/commandl.l"
  DEPENDS "${CMAKE_SOURCE_DIR}/debugger/commandl.l" "${GENERATED_DEBUGGER_PARSER_C}" "${GENERATED_DEBUGGER_PARSER_H}"
  VERBATIM
)

fuse_command_to_file(
  "${GENERATED_Z80_OPCODES_BASE}"
  "${CMAKE_SOURCE_DIR}/z80/z80.pl;${CMAKE_SOURCE_DIR}/z80/opcodes_base.dat"
  "${PERL_EXECUTABLE}" "-I${CMAKE_SOURCE_DIR}/perl" "${CMAKE_SOURCE_DIR}/z80/z80.pl" "${CMAKE_SOURCE_DIR}/z80/opcodes_base.dat"
)

fuse_command_to_file(
  "${GENERATED_Z80_CB}"
  "${CMAKE_SOURCE_DIR}/z80/z80.pl;${CMAKE_SOURCE_DIR}/z80/opcodes_cb.dat"
  "${PERL_EXECUTABLE}" "-I${CMAKE_SOURCE_DIR}/perl" "${CMAKE_SOURCE_DIR}/z80/z80.pl" "${CMAKE_SOURCE_DIR}/z80/opcodes_cb.dat"
)

fuse_command_to_file(
  "${GENERATED_Z80_DDFD}"
  "${CMAKE_SOURCE_DIR}/z80/z80.pl;${CMAKE_SOURCE_DIR}/z80/opcodes_ddfd.dat"
  "${PERL_EXECUTABLE}" "-I${CMAKE_SOURCE_DIR}/perl" "${CMAKE_SOURCE_DIR}/z80/z80.pl" "${CMAKE_SOURCE_DIR}/z80/opcodes_ddfd.dat"
)

fuse_command_to_file(
  "${GENERATED_Z80_DDFDCB}"
  "${CMAKE_SOURCE_DIR}/z80/z80.pl;${CMAKE_SOURCE_DIR}/z80/opcodes_ddfdcb.dat"
  "${PERL_EXECUTABLE}" "-I${CMAKE_SOURCE_DIR}/perl" "${CMAKE_SOURCE_DIR}/z80/z80.pl" "${CMAKE_SOURCE_DIR}/z80/opcodes_ddfdcb.dat"
)

fuse_command_to_file(
  "${GENERATED_Z80_ED}"
  "${CMAKE_SOURCE_DIR}/z80/z80.pl;${CMAKE_SOURCE_DIR}/z80/opcodes_ed.dat"
  "${PERL_EXECUTABLE}" "-I${CMAKE_SOURCE_DIR}/perl" "${CMAKE_SOURCE_DIR}/z80/z80.pl" "${CMAKE_SOURCE_DIR}/z80/opcodes_ed.dat"
)

set(FUSE_GENERATED_FILES
  "${CMAKE_BINARY_DIR}/config.h"
  "${FUSE_GENERATED_DEFAULT_CONFIG}"
  "${GENERATED_SETTINGS_C}"
  "${GENERATED_SETTINGS_H}"
  "${GENERATED_OPTIONS_H}"
  "${GENERATED_EMBEDDED_ASSETS_H}"
  "${GENERATED_EMBEDDED_ASSETS_C}"
  "${GENERATED_DEBUGGER_LEXER_C}"
  "${GENERATED_DEBUGGER_PARSER_C}"
  "${GENERATED_DEBUGGER_PARSER_H}"
  "${GENERATED_SDL_KEYSYMS}"
  "${GENERATED_WIDGET_MENU_DATA}"
  "${GENERATED_WIDGET_OPTIONS_C}"
  "${GENERATED_WIDGET_OPTIONS_H}"
  "${GENERATED_WIDGET_FONT}"
  "${GENERATED_Z80_OPCODES_BASE}"
  "${GENERATED_Z80_CB}"
  "${GENERATED_Z80_DDFD}"
  "${GENERATED_Z80_DDFDCB}"
  "${GENERATED_Z80_ED}"
)

add_custom_target(fuse-generated DEPENDS ${FUSE_GENERATED_FILES})

set(FUSE_SOURCES
  debugger/breakpoint.c
  debugger/command.c
  "${GENERATED_DEBUGGER_LEXER_C}"
  "${GENERATED_DEBUGGER_PARSER_C}"
  debugger/debugger.c
  debugger/disassemble.c
  debugger/event.c
  debugger/expression.c
  debugger/system_variable.c
  debugger/variable.c
  display.c
  event.c
  fuse.c
  infrastructure/startup_manager.c
  input.c
  keyboard.c
  loader.c
  machine.c
  machines/machines_periph.c
  machines/pentagon1024.c
  machines/pentagon512.c
  machines/pentagon.c
  machines/scorpion.c
  machines/spec128.c
  machines/spec16.c
  machines/spec48.c
  machines/spec48_ntsc.c
  machines/specplus2a.c
  machines/specplus2.c
  machines/specplus3.c
  machines/specplus3e.c
  machines/spec_se.c
  machines/tc2048.c
  machines/tc2068.c
  machines/ts2068.c
  memory_pages.c
  mempool.c
  menu.c
  module.c
  movie.c
  periph.c
  peripherals/ay.c
  peripherals/covox.c
  peripherals/dandanator.c
  peripherals/dck.c
  peripherals/disk/beta.c
  peripherals/disk/crc.c
  peripherals/disk/didaktik.c
  peripherals/disk/disciple.c
  peripherals/disk/disk.c
  peripherals/disk/fdd.c
  peripherals/disk/opus.c
  peripherals/disk/plusd.c
  peripherals/disk/trdos.c
  peripherals/disk/upd_fdc.c
  peripherals/disk/wd_fdc.c
  peripherals/fuller.c
  peripherals/ide/divide.c
  peripherals/ide/divmmc.c
  peripherals/ide/divxxx.c
  peripherals/ide/ide.c
  peripherals/ide/simpleide.c
  peripherals/ide/zxatasp.c
  peripherals/ide/zxcf.c
  peripherals/ide/zxmmc.c
  peripherals/if1.c
  peripherals/if2.c
  peripherals/joystick.c
  peripherals/kempmouse.c
  peripherals/melodik.c
  peripherals/multiface.c
  peripherals/printer.c
  peripherals/scld.c
  peripherals/speccyboot.c
  peripherals/specdrum.c
  peripherals/spectranet.c
  peripherals/ttx2000s.c
  peripherals/ula.c
  peripherals/usource.c
  phantom_typist.c
  pokefinder/pokefinder.c
  pokefinder/pokemem.c
  profile.c
  psg.c
  rectangle.c
  rzx.c
  screenshot.c
  slt.c
  snapshot.c
  sound/blipbuffer.c
  sound.c
  spectrum.c
  svg.c
  tape.c
  timer/timer.c
  ui.c
  uidisplay.c
  uimedia.c
  ui/scaler/scaler.c
  ui/sdl/sdlglsl.c
  ui/sdl/sdlshader.c
  ui/sdl/sdldisplay.c
  ui/sdl/sdljoystick.c
  ui/sdl/sdlkeyboard.c
  ui/sdl/sdlui.c
  ui/widget/about.c
  ui/widget/binary.c
  ui/widget/browse.c
  ui/widget/debugger.c
  ui/widget/error.c
  ui/widget/filesel.c
  ui/widget/memory.c
  ui/widget/menu.c
  ui/widget/picture.c
  ui/widget/pokefinder.c
  ui/widget/pokemem.c
  ui/widget/query.c
  ui/widget/roms.c
  ui/widget/select.c
  ui/widget/text.c
  ui/widget/widget.c
  unittests/unittests.c
  utils.c
  z80/z80.c
  z80/z80_debugger_variables.c
  z80/z80_ops.c
  "${GENERATED_SETTINGS_C}"
  "${GENERATED_SDL_KEYSYMS}"
  "${GENERATED_WIDGET_MENU_DATA}"
  "${GENERATED_WIDGET_OPTIONS_C}"
  "${GENERATED_EMBEDDED_ASSETS_C}"
)

if(NOT HAVE_DIRNAME)
  list(APPEND FUSE_SOURCES compat/dirname.c)
endif()

if(NOT HAVE_GETOPT_LONG)
  list(APPEND FUSE_SOURCES compat/getopt.c compat/getopt1.c)
endif()

if(WIN32)
  list(PREPEND FUSE_SOURCES
    compat/unix/dir.c
    compat/unix/file.c
    compat/win32/osname.c
    compat/win32/paths.c
    compat/win32/timer.c
  )
else()
  list(PREPEND FUSE_SOURCES
    compat/linux/relative_paths.c
    compat/unix/dir.c
    compat/unix/file.c
    compat/unix/osname.c
    compat/unix/paths.c
    compat/unix/timer.c
  )
endif()

if(HAVE_SOCKETS)
  if(WIN32)
    list(APPEND FUSE_SOURCES compat/win32/socket.c)
  else()
    list(APPEND FUSE_SOURCES compat/unix/socket.c)
  endif()
endif()

if(BUILD_SPECCYBOOT)
  list(APPEND FUSE_SOURCES compat/unix/tuntap.c peripherals/nic/enc28j60.c)
endif()

if(BUILD_SPECTRANET)
  list(APPEND FUSE_SOURCES peripherals/flash/am29f010.c peripherals/nic/w5100.c peripherals/nic/w5100_socket.c)
endif()

list(APPEND FUSE_SOURCES sound/sdlsound.c timer/sdl.c)
set(SOUND_FIFO 1)

set(FUSE_LIBRETRO_EXCLUDED_SOURCES
  fuse.c
  ui/sdl/sdlglsl.c
  ui/sdl/sdlshader.c
  ui/sdl/sdldisplay.c
  ui/sdl/sdljoystick.c
  ui/sdl/sdlkeyboard.c
  ui/sdl/sdlui.c
  ui/widget/about.c
  ui/widget/binary.c
  ui/widget/browse.c
  ui/widget/debugger.c
  ui/widget/error.c
  ui/widget/filesel.c
  ui/widget/memory.c
  ui/widget/menu.c
  ui/widget/picture.c
  ui/widget/pokefinder.c
  ui/widget/pokemem.c
  ui/widget/query.c
  ui/widget/roms.c
  ui/widget/select.c
  ui/widget/text.c
  ui/widget/widget.c
  unittests/unittests.c
  sound/sdlsound.c
  timer/sdl.c
  "${GENERATED_WIDGET_OPTIONS_C}"
  "${GENERATED_WIDGET_MENU_DATA}"
)

set(FUSE_LIBRETRO_SOURCES ${FUSE_SOURCES})
list(REMOVE_ITEM FUSE_LIBRETRO_SOURCES ${FUSE_LIBRETRO_EXCLUDED_SOURCES})
list(APPEND FUSE_LIBRETRO_SOURCES
  frontend/runtime.c
  libretro/libretro_core.c
  libretro/libretro_frontend.c
  libretro/libretro_option_stubs.c
)

add_library(fuse-scalers16 OBJECT ui/scaler/scalers.c)
add_library(fuse-scalers32 OBJECT ui/scaler/scalers.c)

target_include_directories(fuse-scalers16 PRIVATE
  "${CMAKE_BINARY_DIR}"
  "${CMAKE_SOURCE_DIR}"
  ${LIBSPECTRUM_INCLUDE_DIRS}
  ${SDL_INCLUDE_DIRS}
)
target_include_directories(fuse-scalers32 PRIVATE
  "${CMAKE_BINARY_DIR}"
  "${CMAKE_SOURCE_DIR}"
  ${LIBSPECTRUM_INCLUDE_DIRS}
  ${SDL_INCLUDE_DIRS}
)

if(HAVE_LIB_GLIB)
  target_include_directories(fuse-scalers16 PRIVATE ${GLIB_INCLUDE_DIRS})
  target_include_directories(fuse-scalers32 PRIVATE ${GLIB_INCLUDE_DIRS})
endif()

if(HAVE_LIB_XML2)
  target_include_directories(fuse-scalers16 PRIVATE ${XML_INCLUDE_DIRS})
  target_include_directories(fuse-scalers32 PRIVATE ${XML_INCLUDE_DIRS})
endif()

if(USE_LIBPNG)
  target_include_directories(fuse-scalers16 PRIVATE ${PNG_INCLUDE_DIRS})
  target_include_directories(fuse-scalers32 PRIVATE ${PNG_INCLUDE_DIRS})
endif()

target_compile_definitions(fuse-scalers16 PRIVATE HAVE_CONFIG_H FUSEDATADIR=\"${CMAKE_BINARY_DIR}\" SCALER_DATA_SIZE=2)
target_compile_definitions(fuse-scalers32 PRIVATE HAVE_CONFIG_H FUSEDATADIR=\"${CMAKE_BINARY_DIR}\" SCALER_DATA_SIZE=4)
add_dependencies(fuse-scalers16 fuse-generated)
add_dependencies(fuse-scalers32 fuse-generated)

add_executable(fuse ${FUSE_SOURCES})
add_dependencies(fuse fuse-generated)

add_library(fuse-libretro SHARED ${FUSE_LIBRETRO_SOURCES})
add_dependencies(fuse-libretro fuse-generated)

if(WIN32)
  set_target_properties(fuse PROPERTIES WIN32_EXECUTABLE ON)
endif()

if(FUSE_USE_INTERNAL_LIBSPECTRUM AND TARGET ${FUSE_INTERNAL_LIBSPECTRUM_GENERATED_TARGET})
  add_dependencies(fuse-scalers16 ${FUSE_INTERNAL_LIBSPECTRUM_GENERATED_TARGET})
  add_dependencies(fuse-scalers32 ${FUSE_INTERNAL_LIBSPECTRUM_GENERATED_TARGET})
  add_dependencies(fuse ${FUSE_INTERNAL_LIBSPECTRUM_GENERATED_TARGET})
  add_dependencies(fuse-libretro ${FUSE_INTERNAL_LIBSPECTRUM_GENERATED_TARGET})
endif()

if(FUSE_PORTABLE_PACKAGE AND UNIX AND NOT WIN32)
  set_target_properties(fuse PROPERTIES
    BUILD_RPATH "\$ORIGIN/lib"
    INSTALL_RPATH "\$ORIGIN/lib"
  )
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/repro/libdecor-wayland-repro.c")
  add_executable(libdecor-wayland-repro repro/libdecor-wayland-repro.c)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/repro/fuse-linkset-repro.c")
  add_executable(fuse-linkset-repro repro/fuse-linkset-repro.c)
endif()

if(NOT WIN32)
  pkg_check_modules(LIBDECOR REQUIRED IMPORTED_TARGET libdecor-0)
  pkg_check_modules(WAYLANDCLIENT REQUIRED IMPORTED_TARGET wayland-client)
  if(EXISTS "${CMAKE_SOURCE_DIR}/repro/libdecor-direct-repro.c")
    add_executable(libdecor-direct-repro repro/libdecor-direct-repro.c)
  endif()
endif()

set_source_files_properties(
  z80/z80_ops.c
  PROPERTIES OBJECT_DEPENDS "${GENERATED_Z80_OPCODES_BASE};${GENERATED_Z80_CB};${GENERATED_Z80_DDFD};${GENERATED_Z80_DDFDCB};${GENERATED_Z80_ED}"
)

if(CMAKE_C_COMPILER_ID MATCHES "Clang")
  set_source_files_properties(
    compat/getopt.c
    compat/getopt1.c
    "${GENERATED_SETTINGS_C}"
    PROPERTIES COMPILE_OPTIONS "-Wno-deprecated-non-prototype"
  )
endif()

target_include_directories(fuse PRIVATE
  "${CMAKE_BINARY_DIR}"
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/compat"
  ${LIBSPECTRUM_INCLUDE_DIRS}
  ${SDL_INCLUDE_DIRS}
)

target_include_directories(fuse-libretro PRIVATE
  "${CMAKE_BINARY_DIR}"
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/compat"
  ${LIBSPECTRUM_INCLUDE_DIRS}
  ${SDL_INCLUDE_DIRS}
)

if(HAVE_LIB_GLIB)
  target_include_directories(fuse PRIVATE ${GLIB_INCLUDE_DIRS})
  target_compile_options(fuse PRIVATE ${GLIB_CFLAGS_OTHER})
  target_include_directories(fuse-libretro PRIVATE ${GLIB_INCLUDE_DIRS})
  target_compile_options(fuse-libretro PRIVATE ${GLIB_CFLAGS_OTHER})
endif()

target_compile_definitions(fuse PRIVATE HAVE_CONFIG_H FUSEDATADIR=\"${CMAKE_BINARY_DIR}\")
target_compile_definitions(fuse-libretro PRIVATE HAVE_CONFIG_H FUSEDATADIR="${CMAKE_BINARY_DIR}")

set_target_properties(fuse-libretro PROPERTIES
  PREFIX ""
  OUTPUT_NAME "fuse_libretro"
)

target_link_libraries(fuse PRIVATE
  ${FUSE_LIBSPECTRUM_TARGET}
  ${FUSE_SDL_TARGET}
)

target_link_libraries(fuse-libretro PRIVATE
  ${FUSE_LIBSPECTRUM_TARGET}
)

if(TARGET libdecor-wayland-repro)
  target_link_libraries(libdecor-wayland-repro PRIVATE
    ${FUSE_SDL_TARGET}
  )
endif()

if(TARGET fuse-linkset-repro)
  target_link_libraries(fuse-linkset-repro PRIVATE
    ${FUSE_SDL_TARGET}
    ${CMAKE_DL_LIBS}
  )
endif()

if(TARGET libdecor-wayland-repro)
  target_include_directories(libdecor-wayland-repro PRIVATE
    ${SDL_INCLUDE_DIRS}
  )
endif()

if(TARGET fuse-linkset-repro)
  target_include_directories(fuse-linkset-repro PRIVATE
    ${SDL_INCLUDE_DIRS}
  )
endif()

if(NOT WIN32)
  if(TARGET libdecor-direct-repro)
    target_link_libraries(libdecor-direct-repro PRIVATE
      PkgConfig::LIBDECOR
      PkgConfig::WAYLANDCLIENT
    )
  endif()
endif()

if(HAVE_LIB_GLIB)
  target_link_libraries(fuse PRIVATE PkgConfig::GLIB)
  target_link_libraries(fuse-libretro PRIVATE PkgConfig::GLIB)
endif()

if(HAVE_LIB_XML2)
  if(WIN32)
    target_link_libraries(fuse PRIVATE LibXml2::LibXml2)
    target_link_libraries(fuse-libretro PRIVATE LibXml2::LibXml2)
  else()
    target_link_libraries(fuse PRIVATE PkgConfig::XML)
    target_link_libraries(fuse-libretro PRIVATE PkgConfig::XML)
  endif()
endif()

if(USE_LIBPNG)
  if(WIN32)
    target_link_libraries(fuse PRIVATE PNG::PNG)
    target_link_libraries(fuse-libretro PRIVATE PNG::PNG)
  else()
    target_link_libraries(fuse PRIVATE PkgConfig::PNG)
    target_link_libraries(fuse-libretro PRIVATE PkgConfig::PNG)
  endif()
endif()

if(ZLIB_FOUND)
  target_link_libraries(fuse PRIVATE ZLIB::ZLIB)
  target_link_libraries(fuse-libretro PRIVATE ZLIB::ZLIB)
endif()

if(HAVE_PTHREAD)
  target_link_libraries(fuse PRIVATE Threads::Threads)
endif()

if(MATH_LIBRARY)
  target_link_libraries(fuse PRIVATE "${MATH_LIBRARY}")
  target_link_libraries(fuse-libretro PRIVATE "${MATH_LIBRARY}")
endif()

if(HAVE_OPENGL)
  target_link_libraries(fuse PRIVATE OpenGL::GL)
endif()

if(WIN32 AND HAVE_SOCKETS)
  target_link_libraries(fuse PRIVATE ws2_32)
  target_link_libraries(fuse-libretro PRIVATE ws2_32)
endif()

set(FUSE_VCPKG_RUNTIME_DLLS)
if(WIN32 AND DEFINED VCPKG_TARGET_TRIPLET)
  file(GLOB FUSE_VCPKG_RUNTIME_DLLS
    "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/bin/*.dll"
    "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/debug/bin/*.dll"
  )
endif()

target_sources(fuse PRIVATE
  $<TARGET_OBJECTS:fuse-scalers16>
  $<TARGET_OBJECTS:fuse-scalers32>
)

target_sources(fuse-libretro PRIVATE
  $<TARGET_OBJECTS:fuse-scalers16>
  $<TARGET_OBJECTS:fuse-scalers32>
)

add_custom_command(TARGET fuse POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:fuse>/lib"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:fuse>/roms"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:fuse>/shaders"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/lib" "$<TARGET_FILE_DIR:fuse>/lib"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/roms" "$<TARGET_FILE_DIR:fuse>/roms"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/shaders" "$<TARGET_FILE_DIR:fuse>/shaders"
  VERBATIM
)

add_custom_command(TARGET fuse-libretro POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:fuse-libretro>/lib"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:fuse-libretro>/roms"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/lib" "$<TARGET_FILE_DIR:fuse-libretro>/lib"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/roms" "$<TARGET_FILE_DIR:fuse-libretro>/roms"
  VERBATIM
)

if(WIN32)
  add_custom_command(TARGET fuse POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different $<TARGET_RUNTIME_DLLS:fuse> "$<TARGET_FILE_DIR:fuse>"
    COMMAND_EXPAND_LISTS
    VERBATIM
  )

  add_custom_command(TARGET fuse-libretro POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different $<TARGET_RUNTIME_DLLS:fuse-libretro> "$<TARGET_FILE_DIR:fuse-libretro>"
    COMMAND_EXPAND_LISTS
    VERBATIM
  )

  if(FUSE_VCPKG_RUNTIME_DLLS)
    add_custom_command(TARGET fuse-libretro POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${FUSE_VCPKG_RUNTIME_DLLS} "$<TARGET_FILE_DIR:fuse-libretro>"
      COMMAND_EXPAND_LISTS
      VERBATIM
    )
  endif()
endif()

install(TARGETS fuse
  RUNTIME DESTINATION "${FUSE_RUNTIME_INSTALL_DESTINATION}"
  PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
)

install(SCRIPT "${FUSE_RUNTIME_DEPENDENCY_SCRIPT}")

install(FILES "${FUSE_GENERATED_DEFAULT_CONFIG}"
  DESTINATION "${FUSE_RUNTIME_INSTALL_DESTINATION}"
  RENAME "${FUSE_INSTALLED_DEFAULT_CONFIG_NAME}"
)

install(DIRECTORY "${CMAKE_SOURCE_DIR}/roms/"
  DESTINATION "${FUSE_ROMS_INSTALL_DESTINATION}"
)

install(DIRECTORY "${CMAKE_SOURCE_DIR}/shaders/"
  DESTINATION "${FUSE_RUNTIME_INSTALL_DESTINATION}/shaders"
)

if(UNIX AND NOT APPLE AND NOT FUSE_PORTABLE_PACKAGE)
  install(FILES
    "${FUSE_GENERATED_APPSTREAM_FILE}"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo"
  )

  install(FILES
    "${FUSE_GENERATED_MIME_XML}"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/mime/packages"
  )

  install(FILES
    "${FUSE_GENERATED_DESKTOP_FILE}"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
  )

  foreach(_icon_size IN ITEMS 16 32 48 64 128 256)
    install(FILES "${CMAKE_SOURCE_DIR}/data/icons/${_icon_size}x${_icon_size}/fuse.png"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${_icon_size}x${_icon_size}/apps"
      RENAME io.github.md0_code.FuseSDL3.png
    )
    install(FILES "${CMAKE_SOURCE_DIR}/data/icons/${_icon_size}x${_icon_size}/application-x-spectrum.png"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${_icon_size}x${_icon_size}/mimetypes"
      RENAME application-x-spectrum.png
    )
  endforeach()

  if(EXISTS "${CMAKE_SOURCE_DIR}/data/shell-completion/bash/fuse")
    install(FILES "${CMAKE_SOURCE_DIR}/data/shell-completion/bash/fuse"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/bash-completion/completions"
    )
  endif()
endif()

set(CPACK_PACKAGE_NAME "fuse-sdl3")
set(CPACK_PACKAGE_VENDOR "Fuse SDL3")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "fuse-sdl3")
set(CPACK_PACKAGE_CONTACT "https://github.com/md0-code/fuse-sdl3")
set(CPACK_MONOLITHIC_INSTALL ON)
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_STRIP_FILES OFF)
set(CPACK_VERBATIM_VARIABLES YES)
set(CPACK_PACKAGE_FILE_NAME "fuse-sdl3-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_ARCHIVE_COMPONENT_INSTALL OFF)
if(NOT DEFINED CPACK_GENERATOR OR CPACK_GENERATOR STREQUAL "")
  if(WIN32)
    set(CPACK_GENERATOR "ZIP" CACHE STRING "Archive generators")
  else()
    set(CPACK_GENERATOR "TGZ" CACHE STRING "Archive generators")
  endif()
endif()

include(CPack)

message(STATUS "Fuse SDL3 CMake build configured")
message(STATUS "  SDL3: enabled")
message(STATUS "  widget UI: enabled")
message(STATUS "  sockets: ${HAVE_SOCKETS}")
message(STATUS "  spectranet: ${BUILD_SPECTRANET}")
message(STATUS "  speccyboot: ${BUILD_SPECCYBOOT}")
message(STATUS "  libxml2: ${HAVE_LIB_XML2}")
message(STATUS "  libpng: ${USE_LIBPNG}")
message(STATUS "  desktop integration: ${FUSE_DESKTOP_INTEGRATION_STATUS}")
message(STATUS "  package generators: ${CPACK_GENERATOR}")