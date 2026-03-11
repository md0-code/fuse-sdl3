cmake_minimum_required(VERSION 3.20)

project(fuse VERSION 1.6.0.1 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

include(CheckCSourceCompiles)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckTypeSize)
include(TestBigEndian)

find_package(PkgConfig REQUIRED)
find_package(Perl REQUIRED)
find_package(Threads)
find_package(ZLIB)

pkg_check_modules(LIBSPECTRUM REQUIRED IMPORTED_TARGET libspectrum>=1.5.0)
pkg_check_modules(SDL REQUIRED IMPORTED_TARGET sdl3)

option(FUSE_WITH_LIBXML2 "Build with XML settings support" ON)
option(FUSE_WITH_PNG "Build with PNG screenshot support when available" ON)
option(FUSE_WITH_SOCKETS "Enable sockets-dependent features" ON)
option(FUSE_WITH_PTHREAD "Enable pthread-dependent features" ON)
option(FUSE_USE_JOYSTICK "Enable SDL joystick support" ON)
option(FUSE_SMALLMEM "Disable the large-memory Z80 opcode path" OFF)
set(FUSE_ROMSDIR "" CACHE PATH "Optional installed ROM directory override")

set(FUSE_BUILD_NUMBER 1)
set(FUSE_UPSTREAM_VERSION "1.6.0")
set(FUSE_DOWNSTREAM_NAME "Fuse SDL3")
set(FUSE_DOWNSTREAM_URL "https://github.com/md0-code/fuse-sdl3.git")
set(FUSE_COPYRIGHT "(c) 1999-2021 Philip Kendall and others")
set(FUSE_RC_VERSION "1,6,0,1")
set(PACKAGE "fuse")
set(PACKAGE_BUGREPORT "http://sourceforge.net/p/fuse-emulator/bugs/")
set(PACKAGE_NAME "fuse")
set(PACKAGE_STRING "fuse ${PROJECT_VERSION}")
set(PACKAGE_TARNAME "fuse")
set(PACKAGE_URL "http://fuse-emulator.sourceforge.net/")
set(PACKAGE_VERSION "${PROJECT_VERSION}")
set(VERSION "${PROJECT_VERSION}")
set(LT_OBJDIR ".libs/")

set(HAVE_CONFIG_H 1)
set(UI_SDL 1)
set(USE_WIDGET 1)
set(FUSE_ICON_AVAILABLE 0)
set(YYTEXT_POINTER 1)
set(STDC_HEADERS 1)
set(AC_APPLE_UNIVERSAL_BUILD 0)
set(NO_SOUND 0)
set(DIRECTSOUND_VERSION "")
set(PTHREAD_CREATE_JOINABLE "")
set(WINVER "")
set(_WIN32_IE "")
set(WIN32_LEAN_AND_MEAN "")

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

set(HAVE_LIB_GLIB 0)
if(NOT LIBSPECTRUM_HAS_INTERNAL_GLIB)
  pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0>=2.20.0)
  set(HAVE_LIB_GLIB 1)
endif()

set(HAVE_LIB_XML2 0)
if(FUSE_WITH_LIBXML2)
  pkg_check_modules(XML REQUIRED IMPORTED_TARGET libxml-2.0>=2.6.0)
  set(HAVE_LIB_XML2 1)
endif()

set(USE_LIBPNG 0)
set(HAVE_ZLIB_H 0)
if(FUSE_WITH_PNG)
  pkg_check_modules(PNG IMPORTED_TARGET libpng)
  if(PNG_FOUND)
    set(USE_LIBPNG 1)
  endif()
endif()

if(ZLIB_FOUND)
  set(HAVE_ZLIB_H 1)
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
endif()

configure_file(
  "${CMAKE_SOURCE_DIR}/config.h.cmake.in"
  "${CMAKE_BINARY_DIR}/config.h"
  @ONLY
)

function(fuse_perl_rule output command deps)
  get_filename_component(output_dir "${output}" DIRECTORY)
  add_custom_command(
    OUTPUT "${output}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_dir}"
    COMMAND /bin/sh -c "${command}"
    DEPENDS ${deps}
    VERBATIM
  )
endfunction()

set(GENERATED_SETTINGS_C "${CMAKE_BINARY_DIR}/settings.c")
set(GENERATED_SETTINGS_H "${CMAKE_BINARY_DIR}/settings.h")
set(GENERATED_OPTIONS_H "${CMAKE_BINARY_DIR}/options.h")
set(GENERATED_SDL_KEYSYMS "${CMAKE_BINARY_DIR}/ui/sdl/keysyms.c")
set(GENERATED_WIDGET_MENU_DATA "${CMAKE_BINARY_DIR}/ui/widget/menu_data.c")
set(GENERATED_WIDGET_OPTIONS_C "${CMAKE_BINARY_DIR}/ui/widget/options.c")
set(GENERATED_WIDGET_OPTIONS_H "${CMAKE_BINARY_DIR}/ui/widget/options_internals.h")
set(GENERATED_WIDGET_FONT "${CMAKE_BINARY_DIR}/ui/widget/fuse.font")
set(GENERATED_Z80_OPCODES_BASE "${CMAKE_BINARY_DIR}/z80/opcodes_base.c")
set(GENERATED_Z80_CB "${CMAKE_BINARY_DIR}/z80/z80_cb.c")
set(GENERATED_Z80_DDFD "${CMAKE_BINARY_DIR}/z80/z80_ddfd.c")
set(GENERATED_Z80_DDFDCB "${CMAKE_BINARY_DIR}/z80/z80_ddfdcb.c")
set(GENERATED_Z80_ED "${CMAKE_BINARY_DIR}/z80/z80_ed.c")

fuse_perl_rule(
  "${GENERATED_SETTINGS_C}"
  "${PERL_EXECUTABLE} -I${CMAKE_SOURCE_DIR}/perl ${CMAKE_SOURCE_DIR}/settings.pl ${CMAKE_SOURCE_DIR}/settings.dat > ${GENERATED_SETTINGS_C}.tmp && mv ${GENERATED_SETTINGS_C}.tmp ${GENERATED_SETTINGS_C}"
  "${CMAKE_SOURCE_DIR}/settings.pl;${CMAKE_SOURCE_DIR}/settings.dat"
)

fuse_perl_rule(
  "${GENERATED_SETTINGS_H}"
  "${PERL_EXECUTABLE} -I${CMAKE_SOURCE_DIR}/perl ${CMAKE_SOURCE_DIR}/settings-header.pl ${CMAKE_SOURCE_DIR}/settings.dat > ${GENERATED_SETTINGS_H}.tmp && mv ${GENERATED_SETTINGS_H}.tmp ${GENERATED_SETTINGS_H}"
  "${CMAKE_SOURCE_DIR}/settings-header.pl;${CMAKE_SOURCE_DIR}/settings.dat"
)

fuse_perl_rule(
  "${GENERATED_OPTIONS_H}"
  "${PERL_EXECUTABLE} ${CMAKE_SOURCE_DIR}/perl/cpp-perl.pl ${CMAKE_BINARY_DIR}/config.h ${CMAKE_SOURCE_DIR}/ui/options.dat | ${PERL_EXECUTABLE} -I${CMAKE_SOURCE_DIR}/perl ${CMAKE_SOURCE_DIR}/ui/widget/options-header.pl - public > ${GENERATED_OPTIONS_H}.tmp && mv ${GENERATED_OPTIONS_H}.tmp ${GENERATED_OPTIONS_H}"
  "${CMAKE_SOURCE_DIR}/perl/cpp-perl.pl;${CMAKE_BINARY_DIR}/config.h;${CMAKE_SOURCE_DIR}/ui/widget/options-header.pl;${CMAKE_SOURCE_DIR}/ui/options.dat;${CMAKE_SOURCE_DIR}/perl/Fuse.pm;${CMAKE_SOURCE_DIR}/perl/Fuse/Dialog.pm"
)

fuse_perl_rule(
  "${GENERATED_SDL_KEYSYMS}"
  "${PERL_EXECUTABLE} -I${CMAKE_SOURCE_DIR}/perl ${CMAKE_SOURCE_DIR}/keysyms.pl sdl ${CMAKE_SOURCE_DIR}/keysyms.dat > ${GENERATED_SDL_KEYSYMS}.tmp && mv ${GENERATED_SDL_KEYSYMS}.tmp ${GENERATED_SDL_KEYSYMS}"
  "${CMAKE_SOURCE_DIR}/keysyms.pl;${CMAKE_SOURCE_DIR}/keysyms.dat"
)

add_custom_command(
  OUTPUT "${GENERATED_WIDGET_OPTIONS_C}" "${GENERATED_WIDGET_OPTIONS_H}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/ui/widget"
  COMMAND /bin/sh -c "${PERL_EXECUTABLE} ${CMAKE_SOURCE_DIR}/perl/cpp-perl.pl ${CMAKE_BINARY_DIR}/config.h ${CMAKE_SOURCE_DIR}/ui/options.dat | ${PERL_EXECUTABLE} -I${CMAKE_SOURCE_DIR}/perl ${CMAKE_SOURCE_DIR}/ui/widget/options.pl - > ${GENERATED_WIDGET_OPTIONS_C}.tmp && mv ${GENERATED_WIDGET_OPTIONS_C}.tmp ${GENERATED_WIDGET_OPTIONS_C}"
  COMMAND /bin/sh -c "${PERL_EXECUTABLE} ${CMAKE_SOURCE_DIR}/perl/cpp-perl.pl ${CMAKE_BINARY_DIR}/config.h ${CMAKE_SOURCE_DIR}/ui/options.dat | ${PERL_EXECUTABLE} -I${CMAKE_SOURCE_DIR}/perl ${CMAKE_SOURCE_DIR}/ui/widget/options-header.pl - > ${GENERATED_WIDGET_OPTIONS_H}.tmp && mv ${GENERATED_WIDGET_OPTIONS_H}.tmp ${GENERATED_WIDGET_OPTIONS_H}"
  DEPENDS
    "${CMAKE_SOURCE_DIR}/perl/cpp-perl.pl"
    "${CMAKE_BINARY_DIR}/config.h"
    "${CMAKE_SOURCE_DIR}/ui/widget/options.pl"
    "${CMAKE_SOURCE_DIR}/ui/widget/options-header.pl"
    "${CMAKE_SOURCE_DIR}/ui/options.dat"
    "${CMAKE_SOURCE_DIR}/perl/Fuse.pm"
    "${CMAKE_SOURCE_DIR}/perl/Fuse/Dialog.pm"
  VERBATIM
)

fuse_perl_rule(
  "${GENERATED_WIDGET_MENU_DATA}"
  "${PERL_EXECUTABLE} ${CMAKE_SOURCE_DIR}/perl/cpp-perl.pl ${CMAKE_BINARY_DIR}/config.h ${CMAKE_SOURCE_DIR}/menu_data.dat | ${PERL_EXECUTABLE} -I${CMAKE_SOURCE_DIR}/perl ${CMAKE_SOURCE_DIR}/menu_data.pl widget > ${GENERATED_WIDGET_MENU_DATA}.tmp && mv ${GENERATED_WIDGET_MENU_DATA}.tmp ${GENERATED_WIDGET_MENU_DATA}"
  "${CMAKE_SOURCE_DIR}/perl/cpp-perl.pl;${CMAKE_SOURCE_DIR}/menu_data.dat;${CMAKE_SOURCE_DIR}/menu_data.pl;${CMAKE_BINARY_DIR}/config.h"
)

fuse_perl_rule(
  "${GENERATED_WIDGET_FONT}"
  "${PERL_EXECUTABLE} ${CMAKE_SOURCE_DIR}/ui/widget/mkfusefont.pl ${CMAKE_SOURCE_DIR}/ui/widget/fuse.font.sbf > ${GENERATED_WIDGET_FONT}.tmp && mv ${GENERATED_WIDGET_FONT}.tmp ${GENERATED_WIDGET_FONT}"
  "${CMAKE_SOURCE_DIR}/ui/widget/mkfusefont.pl;${CMAKE_SOURCE_DIR}/ui/widget/fuse.font.sbf"
)

fuse_perl_rule(
  "${GENERATED_Z80_OPCODES_BASE}"
  "${PERL_EXECUTABLE} -I${CMAKE_SOURCE_DIR}/perl ${CMAKE_SOURCE_DIR}/z80/z80.pl ${CMAKE_SOURCE_DIR}/z80/opcodes_base.dat > ${GENERATED_Z80_OPCODES_BASE}.tmp && mv ${GENERATED_Z80_OPCODES_BASE}.tmp ${GENERATED_Z80_OPCODES_BASE}"
  "${CMAKE_SOURCE_DIR}/z80/z80.pl;${CMAKE_SOURCE_DIR}/z80/opcodes_base.dat"
)

fuse_perl_rule(
  "${GENERATED_Z80_CB}"
  "${PERL_EXECUTABLE} -I${CMAKE_SOURCE_DIR}/perl ${CMAKE_SOURCE_DIR}/z80/z80.pl ${CMAKE_SOURCE_DIR}/z80/opcodes_cb.dat > ${GENERATED_Z80_CB}.tmp && mv ${GENERATED_Z80_CB}.tmp ${GENERATED_Z80_CB}"
  "${CMAKE_SOURCE_DIR}/z80/z80.pl;${CMAKE_SOURCE_DIR}/z80/opcodes_cb.dat"
)

fuse_perl_rule(
  "${GENERATED_Z80_DDFD}"
  "${PERL_EXECUTABLE} -I${CMAKE_SOURCE_DIR}/perl ${CMAKE_SOURCE_DIR}/z80/z80.pl ${CMAKE_SOURCE_DIR}/z80/opcodes_ddfd.dat > ${GENERATED_Z80_DDFD}.tmp && mv ${GENERATED_Z80_DDFD}.tmp ${GENERATED_Z80_DDFD}"
  "${CMAKE_SOURCE_DIR}/z80/z80.pl;${CMAKE_SOURCE_DIR}/z80/opcodes_ddfd.dat"
)

fuse_perl_rule(
  "${GENERATED_Z80_DDFDCB}"
  "${PERL_EXECUTABLE} -I${CMAKE_SOURCE_DIR}/perl ${CMAKE_SOURCE_DIR}/z80/z80.pl ${CMAKE_SOURCE_DIR}/z80/opcodes_ddfdcb.dat > ${GENERATED_Z80_DDFDCB}.tmp && mv ${GENERATED_Z80_DDFDCB}.tmp ${GENERATED_Z80_DDFDCB}"
  "${CMAKE_SOURCE_DIR}/z80/z80.pl;${CMAKE_SOURCE_DIR}/z80/opcodes_ddfdcb.dat"
)

fuse_perl_rule(
  "${GENERATED_Z80_ED}"
  "${PERL_EXECUTABLE} -I${CMAKE_SOURCE_DIR}/perl ${CMAKE_SOURCE_DIR}/z80/z80.pl ${CMAKE_SOURCE_DIR}/z80/opcodes_ed.dat > ${GENERATED_Z80_ED}.tmp && mv ${GENERATED_Z80_ED}.tmp ${GENERATED_Z80_ED}"
  "${CMAKE_SOURCE_DIR}/z80/z80.pl;${CMAKE_SOURCE_DIR}/z80/opcodes_ed.dat"
)

set(FUSE_GENERATED_FILES
  "${CMAKE_BINARY_DIR}/config.h"
  "${GENERATED_SETTINGS_C}"
  "${GENERATED_SETTINGS_H}"
  "${GENERATED_OPTIONS_H}"
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
  compat/linux/relative_paths.c
  compat/unix/dir.c
  compat/unix/file.c
  compat/unix/osname.c
  compat/unix/paths.c
  compat/unix/timer.c
  debugger/breakpoint.c
  debugger/command.c
  debugger/commandl.c
  debugger/commandy.c
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
)

if(HAVE_SOCKETS)
  list(APPEND FUSE_SOURCES compat/unix/socket.c)
endif()

if(BUILD_SPECCYBOOT)
  list(APPEND FUSE_SOURCES compat/unix/tuntap.c peripherals/nic/enc28j60.c)
endif()

if(BUILD_SPECTRANET)
  list(APPEND FUSE_SOURCES peripherals/flash/am29f010.c peripherals/nic/w5100.c peripherals/nic/w5100_socket.c)
endif()

list(APPEND FUSE_SOURCES sound/sdlsound.c timer/sdl.c)
set(SOUND_FIFO 1)

add_library(fuse-scalers16 OBJECT ui/scaler/scalers.c)
add_library(fuse-scalers32 OBJECT ui/scaler/scalers.c)

target_include_directories(fuse-scalers16 PRIVATE
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_BINARY_DIR}"
  ${LIBSPECTRUM_INCLUDE_DIRS}
  ${SDL_INCLUDE_DIRS}
)
target_include_directories(fuse-scalers32 PRIVATE
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_BINARY_DIR}"
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

add_executable(fuse ${FUSE_SOURCES})
add_dependencies(fuse fuse-generated)

set_source_files_properties(
  z80/z80_ops.c
  PROPERTIES OBJECT_DEPENDS "${GENERATED_Z80_OPCODES_BASE};${GENERATED_Z80_CB};${GENERATED_Z80_DDFD};${GENERATED_Z80_DDFDCB};${GENERATED_Z80_ED}"
)

target_include_directories(fuse PRIVATE
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_BINARY_DIR}"
  ${LIBSPECTRUM_INCLUDE_DIRS}
  ${SDL_INCLUDE_DIRS}
)

target_compile_definitions(fuse PRIVATE HAVE_CONFIG_H FUSEDATADIR=\"${CMAKE_BINARY_DIR}\")

target_link_libraries(fuse PRIVATE
  PkgConfig::LIBSPECTRUM
  PkgConfig::SDL
)

if(HAVE_LIB_GLIB)
  target_link_libraries(fuse PRIVATE PkgConfig::GLIB)
endif()

if(HAVE_LIB_XML2)
  target_link_libraries(fuse PRIVATE PkgConfig::XML)
endif()

if(USE_LIBPNG)
  target_link_libraries(fuse PRIVATE PkgConfig::PNG)
endif()

if(ZLIB_FOUND)
  target_link_libraries(fuse PRIVATE ZLIB::ZLIB)
endif()

if(HAVE_PTHREAD)
  target_link_libraries(fuse PRIVATE Threads::Threads)
endif()

if(MATH_LIBRARY)
  target_link_libraries(fuse PRIVATE "${MATH_LIBRARY}")
endif()

target_sources(fuse PRIVATE
  $<TARGET_OBJECTS:fuse-scalers16>
  $<TARGET_OBJECTS:fuse-scalers32>
)

add_custom_command(TARGET fuse POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:fuse>/lib"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:fuse>/roms"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/lib" "$<TARGET_FILE_DIR:fuse>/lib"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/roms" "$<TARGET_FILE_DIR:fuse>/roms"
  VERBATIM
)

message(STATUS "Fuse SDL3 CMake build configured")
message(STATUS "  SDL3: enabled")
message(STATUS "  widget UI: enabled")
message(STATUS "  sockets: ${HAVE_SOCKETS}")
message(STATUS "  spectranet: ${BUILD_SPECTRANET}")
message(STATUS "  speccyboot: ${BUILD_SPECCYBOOT}")
message(STATUS "  libxml2: ${HAVE_LIB_XML2}")
message(STATUS "  libpng: ${USE_LIBPNG}")