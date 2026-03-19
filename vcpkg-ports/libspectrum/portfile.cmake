vcpkg_from_git(
  OUT_SOURCE_PATH SOURCE_PATH
  URL "https://git.code.sf.net/p/fuse-emulator/libspectrum"
  REF f982a143c239497cc7ad7da4a805aa2ab7855970
  FETCH_REF refs/tags/libspectrum-1.5.0
)

# The vcpkg MSVC windres wrapper drives rc.exe with GNU windres-style
# positional arguments. Upstream only uses the resource object for DLL
# version metadata, so skip it for native Windows builds and keep the
# library build itself intact.
file(READ "${SOURCE_PATH}/configure.ac" _configure_ac)
string(REPLACE
  "    if test \"$WINDRES\" != no; then\n      WINDRES_OBJ=\"windres.o\"\n      WINDRES_LDFLAGS=\"-Xlinker windres.o\"\n    fi"
  "    if test \"$WINDRES\" != no && test \"$ac_cv_c_compiler_gnu\" = yes; then\n      WINDRES_OBJ=\"windres.o\"\n      WINDRES_LDFLAGS=\"-Xlinker windres.o\"\n    fi"
  _configure_ac
  "${_configure_ac}"
)
string(REPLACE
  "    [AC_SEARCH_LIBS(compress2, z zdll)\n     have_zlib=\"yes\"]"
  "    [AC_SEARCH_LIBS(compress2, z zlib zdll)\n     have_zlib=\"yes\"]"
  _configure_ac
  "${_configure_ac}"
)
file(WRITE "${SOURCE_PATH}/configure.ac" "${_configure_ac}")

file(READ "${SOURCE_PATH}/make-perl.c" _make_perl_c)
string(REPLACE
  "  } else if( sizeof( void* ) == sizeof( long ) ) {\n    printf( \"#define GINT_TO_POINTER(i)      ((gpointer)  (glong)(i))\\n\" );\n    printf( \"#define GPOINTER_TO_INT(p)      ((gint)   (glong)(p))\\n\" );\n    printf( \"#define GPOINTER_TO_UINT(p)     ((guint)  (gulong)(p))\\n\" );\n  } else {\n    fprintf( stderr, \"No plausible int to pointer cast found\\n\" );\n    return 1;\n  }"
  "  } else if( sizeof( void* ) == sizeof( long ) ) {\n    printf( \"#define GINT_TO_POINTER(i)      ((gpointer)  (glong)(i))\\n\" );\n    printf( \"#define GPOINTER_TO_INT(p)      ((gint)   (glong)(p))\\n\" );\n    printf( \"#define GPOINTER_TO_UINT(p)     ((guint)  (gulong)(p))\\n\" );\n  } else if( sizeof( void* ) == sizeof( long long ) ) {\n    printf( \"#define GINT_TO_POINTER(i)      ((gpointer)  (long long)(i))\\n\" );\n    printf( \"#define GPOINTER_TO_INT(p)      ((gint)   (long long)(p))\\n\" );\n    printf( \"#define GPOINTER_TO_UINT(p)     ((guint)  (unsigned long long)(p))\\n\" );\n  } else {\n    fprintf( stderr, \"No plausible int to pointer cast found\\n\" );\n    return 1;\n  }"
  _make_perl_c
  "${_make_perl_c}"
)
file(WRITE "${SOURCE_PATH}/make-perl.c" "${_make_perl_c}")

vcpkg_configure_make(
  SOURCE_PATH "${SOURCE_PATH}"
  AUTOCONFIG
  DETERMINE_BUILD_TRIPLET
  USE_WRAPPERS
  OPTIONS
    --with-fake-glib
    --without-libgcrypt
    --without-libaudiofile
    "--with-local-prefix=${CURRENT_INSTALLED_DIR}"
)

vcpkg_install_make()
vcpkg_copy_pdbs()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

foreach(_pc_file IN ITEMS
  "${CURRENT_PACKAGES_DIR}/lib/pkgconfig/libspectrum.pc"
  "${CURRENT_PACKAGES_DIR}/debug/lib/pkgconfig/libspectrum.pc"
)
  if(EXISTS "${_pc_file}")
    file(READ "${_pc_file}" _pc_contents)
    if(EXISTS "${CURRENT_PACKAGES_DIR}/lib/libspectrum.lib" OR EXISTS "${CURRENT_PACKAGES_DIR}/debug/lib/libspectrum.lib")
      string(REPLACE "-lspectrum" "-llibspectrum" _pc_contents "${_pc_contents}")
      file(WRITE "${_pc_file}" "${_pc_contents}")
    endif()
  endif()
endforeach()

vcpkg_fixup_pkgconfig()

file(WRITE "${CURRENT_PACKAGES_DIR}/share/${PORT}/usage"
  "The upstream autotools build has been wrapped to produce a native Windows libspectrum build.\n"
  "Headers: include/libspectrum.h\n"
  "Import libraries are installed under lib/ and debug/lib/.\n"
)

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYING")