#include <config.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#include <unistd.h>

#ifdef HAVE_LIB_XML2
#include <libxml/encoding.h>
#endif

#include <libspectrum.h>

#include "debugger/debugger.h"
#include "display.h"
#include "event.h"
#include "frontend/runtime.h"
#include "fuse.h"
#include "infrastructure/startup_manager.h"
#include "keyboard.h"
#include "machine.h"
#include "machines/machines_periph.h"
#include "memory_pages.h"
#include "module.h"
#include "movie.h"
#include "mempool.h"
#include "peripherals/ay.h"
#include "peripherals/dandanator.h"
#include "peripherals/dck.h"
#include "peripherals/disk/beta.h"
#include "peripherals/disk/didaktik.h"
#include "peripherals/disk/fdd.h"
#include "peripherals/fuller.h"
#include "peripherals/ide/divide.h"
#include "peripherals/ide/divmmc.h"
#include "peripherals/ide/simpleide.h"
#include "peripherals/ide/zxatasp.h"
#include "peripherals/ide/zxcf.h"
#include "peripherals/ide/zxmmc.h"
#include "peripherals/if1.h"
#include "peripherals/if2.h"
#include "peripherals/joystick.h"
#include "peripherals/kempmouse.h"
#include "peripherals/melodik.h"
#include "peripherals/multiface.h"
#include "peripherals/printer.h"
#include "peripherals/scld.h"
#include "peripherals/speccyboot.h"
#include "peripherals/spectranet.h"
#include "peripherals/ttx2000s.h"
#include "peripherals/ula.h"
#include "peripherals/usource.h"
#include "phantom_typist.h"
#include "pokefinder/pokemem.h"
#include "profile.h"
#include "psg.h"
#include "rzx.h"
#include "screenshot.h"
#include "settings.h"
#include "slt.h"
#include "snapshot.h"
#include "sound.h"
#include "spectrum.h"
#include "tape.h"
#include "timer/timer.h"
#include "ui/scaler/scaler.h"
#include "ui/ui.h"
#include "ui/uimedia.h"
#include "utils.h"
#include "z80/z80.h"

const char *fuse_progname = "fuse";
int fuse_exiting = 0;
int fuse_emulation_paused = 0;
libspectrum_creator *fuse_creator = NULL;

static const char * const LIBSPECTRUM_MIN_VERSION = "0.5.0";
static display_startup_context display_context;

static int creator_init( void *context );
static void creator_end( void );
static void creator_register_startup( void );
static int fuse_libspectrum_init( void *context );
static void libspectrum_register_startup( void );
static int libxml2_init( void *context );
static void libxml2_register_startup( void );
static int setuid_init( void *context );
static void setuid_register_startup( void );
static int run_startup_manager( int *argc, char ***argv );

static int
fuse_libspectrum_init( void *context )
{
  (void)context;

  if( libspectrum_check_version( LIBSPECTRUM_MIN_VERSION ) ) {
    if( libspectrum_init() ) return 1;
  } else {
    ui_error( UI_ERROR_ERROR,
              "libspectrum version %s found, but %s required",
              libspectrum_version(), LIBSPECTRUM_MIN_VERSION );
    return 1;
  }

  return 0;
}

static void
libspectrum_register_startup( void )
{
  startup_manager_module dependencies[] = {
    STARTUP_MANAGER_MODULE_DISPLAY
  };
  startup_manager_register( STARTUP_MANAGER_MODULE_LIBSPECTRUM, dependencies,
                            ARRAY_SIZE( dependencies ), fuse_libspectrum_init,
                            NULL, NULL );
}

static int
libxml2_init( void *context )
{
  (void)context;
#ifdef HAVE_LIB_XML2
  LIBXML_TEST_VERSION
#endif
  return 0;
}

static void
libxml2_register_startup( void )
{
  startup_manager_module dependencies[] = { STARTUP_MANAGER_MODULE_SETUID };
  startup_manager_register( STARTUP_MANAGER_MODULE_LIBXML2, dependencies,
                            ARRAY_SIZE( dependencies ), libxml2_init, NULL,
                            NULL );
}

static int
setuid_init( void *context )
{
  (void)context;
#ifdef HAVE_GETEUID
  if( !geteuid() ) {
    if( setuid( getuid() ) ) {
      ui_error( UI_ERROR_ERROR, "Could not drop root privileges" );
      return 1;
    }
  }
#endif
  return 0;
}

static void
setuid_register_startup( void )
{
  startup_manager_module dependencies[] = {
    STARTUP_MANAGER_MODULE_DISPLAY,
    STARTUP_MANAGER_MODULE_LIBSPECTRUM,
  };
  startup_manager_register( STARTUP_MANAGER_MODULE_SETUID, dependencies,
                            ARRAY_SIZE( dependencies ), setuid_init, NULL,
                            NULL );
}

static int
creator_init( void *context )
{
  size_t i;
  unsigned int version[4] = { 0, 0, 0, 0 };
  char *custom;
  char osname[ 192 ];
  int custom_length;
  int sys_error;
  const char *gcrypt_version;
  libspectrum_error error;

  (void)context;

  sscanf( FUSE_VERSION, "%u.%u.%u.%u",
          &version[0], &version[1], &version[2], &version[3] );

  for( i = 0; i < 4; i++ ) if( version[i] > 0xff ) version[i] = 0xff;

  sys_error = compat_osname( osname, sizeof( osname ) );
  if( sys_error ) return 1;

  fuse_creator = libspectrum_creator_alloc();

  error = libspectrum_creator_set_program( fuse_creator, FUSE_NAME );
  if( error ) { libspectrum_creator_free( fuse_creator ); return error; }

  error = libspectrum_creator_set_major( fuse_creator,
                                         version[0] * 0x100 + version[1] );
  if( error ) { libspectrum_creator_free( fuse_creator ); return error; }

  error = libspectrum_creator_set_minor( fuse_creator,
                                         version[2] * 0x100 + version[3] );
  if( error ) { libspectrum_creator_free( fuse_creator ); return error; }

  gcrypt_version = libspectrum_gcrypt_version();
  if( !gcrypt_version ) gcrypt_version = "not available";

  custom_length = snprintf( NULL, 0,
      "program: %s\n"
      "version: %s\n"
      "home: %s\n"
      "gcrypt: %s\n"
      "libspectrum: %s\n"
      "uname: %s",
      FUSE_NAME, FUSE_VERSION, FUSE_URL,
      gcrypt_version, libspectrum_version(), osname );
  if( custom_length < 0 ) {
    libspectrum_creator_free( fuse_creator );
    return LIBSPECTRUM_ERROR_UNKNOWN;
  }

  custom = libspectrum_new( char, custom_length + 1 );
  snprintf( custom, custom_length + 1,
      "program: %s\n"
      "version: %s\n"
      "home: %s\n"
      "gcrypt: %s\n"
      "libspectrum: %s\n"
      "uname: %s",
      FUSE_NAME, FUSE_VERSION, FUSE_URL,
      gcrypt_version, libspectrum_version(), osname );

  error = libspectrum_creator_set_custom(
    fuse_creator, (libspectrum_byte*)custom, strlen( custom )
  );
  if( error ) {
    libspectrum_free( custom );
    libspectrum_creator_free( fuse_creator );
    return error;
  }

  return 0;
}

static void
creator_end( void )
{
  libspectrum_creator_free( fuse_creator );
  fuse_creator = NULL;
}

static void
creator_register_startup( void )
{
  startup_manager_module dependencies[] = { STARTUP_MANAGER_MODULE_SETUID };
  startup_manager_register( STARTUP_MANAGER_MODULE_CREATOR, dependencies,
                            ARRAY_SIZE( dependencies ), creator_init, NULL,
                            creator_end );
}

static int
run_startup_manager( int *argc, char ***argv )
{
  startup_manager_init();

  display_context.argc = argc;
  display_context.argv = argv;

  ay_register_startup();
  beta_register_startup();
  creator_register_startup();
  covox_register_startup();
  dandanator_register_startup();
  debugger_register_startup();
  didaktik80_register_startup();
  disciple_register_startup();
  display_register_startup( &display_context );
  divide_register_startup();
  divmmc_register_startup();
  event_register_startup();
  fdd_register_startup();
  fuller_register_startup();
  if1_register_startup();
  if2_register_startup();
  joystick_register_startup();
  kempmouse_register_startup();
  keyboard_register_startup();
  libspectrum_register_startup();
  libxml2_register_startup();
  machine_register_startup();
  machines_periph_register_startup();
  melodik_register_startup();
  memory_register_startup();
  mempool_register_startup();
  multiface_register_startup();
  opus_register_startup();
  phantom_typist_register_startup();
  plusd_register_startup();
  printer_register_startup();
  profile_register_startup();
  psg_register_startup();
  rzx_register_startup();
  scld_register_startup();
  screenshot_register_startup();
  settings_register_startup();
  setuid_register_startup();
  simpleide_register_startup();
  slt_register_startup();
  sound_register_startup();
  speccyboot_register_startup();
  specdrum_register_startup();
  spectranet_register_startup();
  spectrum_register_startup();
  tape_register_startup();
  ttx2000s_register_startup();
  timer_register_startup();
  ula_register_startup();
  usource_register_startup();
  z80_register_startup();
  zxatasp_register_startup();
  zxcf_register_startup();
  zxmmc_register_startup();

  return startup_manager_run();
}

int
fuse_runtime_init( const char *program_name )
{
  int argc = 1;
  int first_arg = 1;
  int error;
  char **argv;
  char *start_scaler;

  argv = libspectrum_new( char *, 1 );
  argv[0] = (char *)( program_name && program_name[0] ? program_name : "fuse-libretro" );

  srand( (unsigned)time( NULL ) );
  fuse_progname = argv[0];
  fuse_exiting = 0;
  fuse_emulation_paused = 0;

  libspectrum_error_function = ui_libspectrum_error;

  if( settings_init( &first_arg, argc, argv ) ) {
    libspectrum_free( argv );
    return 1;
  }

  start_scaler = utils_safe_strdup( settings_current.start_scaler_mode );

  error = run_startup_manager( &argc, &argv );
  if( error ) {
    libspectrum_free( start_scaler );
    libspectrum_free( argv );
    return error;
  }

  error = machine_select_id( settings_current.start_machine );
  if( error ) {
    libspectrum_free( start_scaler );
    libspectrum_free( argv );
    return error;
  }

  error = scaler_select_id( start_scaler );
  libspectrum_free( start_scaler );
  if( error ) {
    libspectrum_free( argv );
    return error;
  }

  debugger_command_evaluate( settings_current.debugger_command );

  if( ui_mouse_present ) ui_mouse_grabbed = ui_mouse_grab( 1 );

  movie_init();

  libspectrum_free( argv );
  return 0;
}

int
fuse_runtime_run_frame( void )
{
  libspectrum_dword frame_count = spectrum_frame_count();

  while( !fuse_exiting && spectrum_frame_count() == frame_count ) {
    z80_do_opcodes();
    event_do_events();
  }

  return 0;
}

int
fuse_runtime_load_file( const char *filename )
{
  int autoload;

  if( !filename || !filename[0] ) return 1;

  autoload = tape_can_autoload();
  return utils_open_file( filename, autoload, NULL );
}

int
fuse_runtime_reset( int hard_reset )
{
  int error = machine_reset( hard_reset );
  if( !error ) display_refresh_all();
  return error;
}

int
fuse_runtime_select_machine( const char *id )
{
  int error;

  error = machine_select_id( id );
  if( !error ) display_refresh_all();
  return error;
}

void
fuse_runtime_refresh_display( void )
{
  display_refresh_all();
}

int
fuse_runtime_shutdown( void )
{
  movie_stop();

  startup_manager_run_end();

  periph_end();
  ui_end();
  ui_media_drive_end();
  module_end();
  pokemem_end();
  svg_capture_end();
  libspectrum_end();

  return 0;
}

int
fuse_emulation_pause( void )
{
  int error;

  if( fuse_emulation_paused++ ) return 0;

  if( rzx_recording && rzx_competition_mode ) {
    ui_error( UI_ERROR_INFO, "Stopping competition mode RZX recording" );
    error = rzx_stop_recording();
    if( error ) return error;
  }

  sound_pause();

  return 0;
}

int
fuse_emulation_unpause( void )
{
  int error;

  if( --fuse_emulation_paused ) return 0;

  sound_unpause();

  error = timer_estimate_reset();
  if( error ) return error;

  return 0;
}

void
fuse_abort( void )
{
  fuse_runtime_shutdown();
  abort();
}
