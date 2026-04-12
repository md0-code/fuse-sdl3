#include <config.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libspectrum.h>

#include "display.h"
#include "frontend/runtime.h"
#include "fuse.h"
#include "libretro/libretro.h"
#include "libretro/libretro_frontend.h"
#include "machine.h"
#include "peripherals/joystick.h"
#include "pokefinder/pokemem.h"
#include "settings.h"
#include "snapshot.h"
#include "utils.h"

#define FUSE_LIBRETRO_STATE_MAGIC 0x46534C52u
#define FUSE_LIBRETRO_STATE_VERSION 1u

typedef struct libretro_state_header {
  uint32_t magic;
  uint32_t version;
  uint32_t payload_size;
  uint32_t snapshot_type;
  char machine_id[ 32 ];
} libretro_state_header;

typedef struct libretro_cheat_entry {
  int enabled;
  int valid;
  libspectrum_byte bank;
  libspectrum_word address;
  libspectrum_byte value;
} libretro_cheat_entry;

static retro_environment_t environment_callback;
static retro_video_refresh_t video_refresh_callback;
static retro_audio_sample_t audio_sample_callback;
static retro_audio_sample_batch_t audio_batch_callback;
static retro_input_poll_t input_poll_callback;
static retro_input_state_t input_state_callback;

static int runtime_initialised;
static int game_loaded;
static int options_dirty;

static libretro_cheat_entry *cheat_entries;
static size_t cheat_entry_count;

static void RETRO_CALLCONV libretro_keyboard_callback( bool down,
                                                       unsigned keycode,
                                                       uint32_t character,
                                                       uint16_t key_modifiers );
static void libretro_register_variables( void );
static void libretro_apply_variables( int force );
static void libretro_apply_bool_variable( const char *key, int *field );
static void libretro_apply_machine_variable( void );
static void libretro_apply_stereo_variable( void );
static void libretro_apply_joystick_variable( void );
static int libretro_get_variable( const char *key, const char **value );
static int libretro_string_enabled( const char *value );
static int libretro_build_state( unsigned char **buffer, size_t *length );
static void libretro_rebuild_cheats( void );
static int libretro_parse_cheat( const char *code, libretro_cheat_entry *entry );
static void libretro_reset_cheats( void );
static void libretro_update_geometry( struct retro_system_av_info *info );
static const char *libretro_machine_id_from_value( const char *value );
static int libretro_joystick_type_from_value( const char *value );

unsigned RETRO_API RETRO_CALLCONV
retro_api_version( void )
{
  return RETRO_API_VERSION;
}

void RETRO_API RETRO_CALLCONV
retro_set_environment( retro_environment_t cb )
{
  struct retro_keyboard_callback keyboard_callback;

  environment_callback = cb;
  libretro_frontend_set_environment( cb );
  libretro_register_variables();

  keyboard_callback.callback = libretro_keyboard_callback;
  if( environment_callback ) {
    environment_callback( RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK,
                          &keyboard_callback );
  }
}

void RETRO_API RETRO_CALLCONV
retro_set_video_refresh( retro_video_refresh_t cb )
{
  video_refresh_callback = cb;
}

void RETRO_API RETRO_CALLCONV
retro_set_audio_sample( retro_audio_sample_t cb )
{
  audio_sample_callback = cb;
}

void RETRO_API RETRO_CALLCONV
retro_set_audio_sample_batch( retro_audio_sample_batch_t cb )
{
  audio_batch_callback = cb;
}

void RETRO_API RETRO_CALLCONV
retro_set_input_poll( retro_input_poll_t cb )
{
  input_poll_callback = cb;
  libretro_frontend_set_input_callbacks( input_poll_callback,
                                         input_state_callback );
}

void RETRO_API RETRO_CALLCONV
retro_set_input_state( retro_input_state_t cb )
{
  input_state_callback = cb;
  libretro_frontend_set_input_callbacks( input_poll_callback,
                                         input_state_callback );
}

void RETRO_API RETRO_CALLCONV
retro_set_controller_port_device( unsigned port, unsigned device )
{
  (void)port;
  (void)device;
}

void RETRO_API RETRO_CALLCONV
retro_get_system_info( struct retro_system_info *info )
{
  if( !info ) return;

  memset( info, 0, sizeof( *info ) );
  info->library_name = "Fuse SDL3";
  info->library_version = FUSE_VERSION;
  info->need_fullpath = true;
  info->block_extract = false;
  info->valid_extensions =
    "szx|z80|sna|tap|tzx|pzx|csw|dsk|trd|fdi|udi|mgt|img|opd|opu|hdf|mdr|dck|rom|rzx|pok";
}

static void
libretro_update_geometry( struct retro_system_av_info *info )
{
  int width = libretro_frontend_get_video_width();
  int height = libretro_frontend_get_video_height();
  double fps = 50.0;

  if( !info ) return;

  if( width <= 0 ) width = DISPLAY_ASPECT_WIDTH;
  if( height <= 0 ) height = DISPLAY_SCREEN_HEIGHT;

  if( machine_current ) {
    fps = (double)machine_current->timings.processor_speed /
          (double)machine_current->timings.tstates_per_frame;
  }

  info->geometry.base_width = (unsigned)width;
  info->geometry.base_height = (unsigned)height;
  info->geometry.max_width = DISPLAY_SCREEN_WIDTH;
  info->geometry.max_height = DISPLAY_SCREEN_HEIGHT * 2;
  info->geometry.aspect_ratio = height > 0 ? (float)width / (float)height
                                           : 4.0f / 3.0f;
  info->timing.fps = fps;
  info->timing.sample_rate = libretro_frontend_get_audio_sample_rate();
}

void RETRO_API RETRO_CALLCONV
retro_get_system_av_info( struct retro_system_av_info *info )
{
  if( !info ) return;
  memset( info, 0, sizeof( *info ) );
  libretro_update_geometry( info );
}

void RETRO_API RETRO_CALLCONV
retro_init( void )
{
  runtime_initialised = 0;
  game_loaded = 0;
  options_dirty = 1;
}

void RETRO_API RETRO_CALLCONV
retro_deinit( void )
{
  if( runtime_initialised ) {
    fuse_runtime_shutdown();
    runtime_initialised = 0;
    game_loaded = 0;
  }

  libretro_reset_cheats();
}

void RETRO_API RETRO_CALLCONV
retro_reset( void )
{
  if( !runtime_initialised ) return;

  fuse_runtime_reset( 1 );
  libretro_frontend_reset_runtime_state();
  libretro_rebuild_cheats();
}

bool RETRO_API RETRO_CALLCONV
retro_load_game( const struct retro_game_info *game )
{
  enum retro_pixel_format pixel_format = RETRO_PIXEL_FORMAT_XRGB8888;

  if( !game || !game->path || !game->path[0] ) return false;

  if( environment_callback ) {
    environment_callback( RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &pixel_format );
  }

  if( runtime_initialised ) {
    fuse_runtime_shutdown();
    runtime_initialised = 0;
    game_loaded = 0;
  }

  if( fuse_runtime_init( "fuse-libretro" ) ) {
    libretro_log_message( RETRO_LOG_ERROR,
                          "libretro: failed to initialise emulator runtime\n" );
    return false;
  }

  runtime_initialised = 1;
  libretro_apply_variables( 1 );

  if( fuse_runtime_load_file( game->path ) ) {
    libretro_log_message( RETRO_LOG_ERROR,
                          "libretro: failed to load content '%s'\n",
                          game->path );
    fuse_runtime_shutdown();
    runtime_initialised = 0;
    return false;
  }

  fuse_runtime_refresh_display();
  libretro_frontend_reset_runtime_state();
  game_loaded = 1;
  return true;
}

void RETRO_API RETRO_CALLCONV
retro_unload_game( void )
{
  if( runtime_initialised ) {
    fuse_runtime_shutdown();
  }

  runtime_initialised = 0;
  game_loaded = 0;
  libretro_reset_cheats();
}

unsigned RETRO_API RETRO_CALLCONV
retro_get_region( void )
{
  if( machine_current && machine_current->id &&
      !strcmp( machine_current->id, "48_ntsc" ) ) {
    return RETRO_REGION_NTSC;
  }

  return RETRO_REGION_PAL;
}

void RETRO_API RETRO_CALLCONV
retro_run( void )
{
  const void *video_data;
  const int16_t *audio_data;
  size_t pitch;
  size_t frames;
  bool updated = false;

  if( !runtime_initialised || !game_loaded ) return;

  if( environment_callback ) {
    environment_callback( RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated );
    if( updated || options_dirty ) libretro_apply_variables( updated || options_dirty );
  }

  libretro_frontend_clear_audio();
  libretro_frontend_capture_input();
  fuse_runtime_run_frame();

  video_data = libretro_frontend_get_video_data( &pitch );
  if( video_refresh_callback ) {
    video_refresh_callback( video_data,
                            (unsigned)libretro_frontend_get_video_width(),
                            (unsigned)libretro_frontend_get_video_height(),
                            pitch );
  }

  audio_data = libretro_frontend_get_audio_data( &frames );
  if( audio_batch_callback && audio_data && frames ) {
    audio_batch_callback( audio_data, frames );
  } else if( audio_sample_callback && audio_data ) {
    size_t i;
    int channels = libretro_frontend_get_audio_channels();
    for( i = 0; i < frames; i++ ) {
      int16_t left = audio_data[ i * channels ];
      int16_t right = channels > 1 ? audio_data[ i * channels + 1 ] : left;
      audio_sample_callback( left, right );
    }
  }
}

size_t RETRO_API RETRO_CALLCONV
retro_serialize_size( void )
{
  unsigned char *buffer = NULL;
  size_t length = 0;

  if( !runtime_initialised ) return 0;

  if( libretro_build_state( &buffer, &length ) ) return 0;
  libspectrum_free( buffer );
  return length;
}

bool RETRO_API RETRO_CALLCONV
retro_serialize( void *data, size_t size )
{
  unsigned char *buffer = NULL;
  size_t length = 0;

  if( !runtime_initialised ) return false;
  if( libretro_build_state( &buffer, &length ) ) return false;

  if( size < length ) {
    libspectrum_free( buffer );
    return false;
  }

  memcpy( data, buffer, length );
  libspectrum_free( buffer );
  return true;
}

bool RETRO_API RETRO_CALLCONV
retro_unserialize( const void *data, size_t size )
{
  const libretro_state_header *header = data;
  const unsigned char *payload;

  if( !runtime_initialised || !data || size < sizeof( *header ) ) return false;

  if( header->magic != FUSE_LIBRETRO_STATE_MAGIC ||
      header->version != FUSE_LIBRETRO_STATE_VERSION ) {
    return false;
  }

  if( sizeof( *header ) + header->payload_size > size ) return false;

  payload = (const unsigned char *)data + sizeof( *header );

  if( snapshot_read_buffer( payload, header->payload_size,
                            (libspectrum_id_t)header->snapshot_type ) ) {
    return false;
  }

  fuse_runtime_refresh_display();
  libretro_frontend_reset_runtime_state();
  libretro_rebuild_cheats();
  return true;
}

void RETRO_API RETRO_CALLCONV
retro_cheat_reset( void )
{
  libretro_reset_cheats();
}

void RETRO_API RETRO_CALLCONV
retro_cheat_set( unsigned index, bool enabled, const char *code )
{
  libretro_cheat_entry entry;
  libretro_cheat_entry *new_entries;
  size_t new_count;
  size_t i;

  memset( &entry, 0, sizeof( entry ) );
  entry.enabled = enabled ? 1 : 0;

  if( enabled && code ) {
    entry.valid = !libretro_parse_cheat( code, &entry );
    if( !entry.valid ) {
      libretro_log_message( RETRO_LOG_WARN,
                            "libretro: ignoring unsupported cheat '%s'\n",
                            code );
    }
  }

  if( index >= cheat_entry_count ) {
    new_count = index + 1;
    new_entries = realloc( cheat_entries, new_count * sizeof( *new_entries ) );
    if( !new_entries ) return;

    for( i = cheat_entry_count; i < new_count; i++ ) {
      memset( &new_entries[ i ], 0, sizeof( new_entries[ i ] ) );
    }

    cheat_entries = new_entries;
    cheat_entry_count = new_count;
  }

  cheat_entries[ index ] = entry;
  libretro_rebuild_cheats();
}

bool RETRO_API RETRO_CALLCONV
retro_load_game_special( unsigned special_type,
                         const struct retro_game_info *info,
                         size_t num_info )
{
  (void)special_type;
  (void)info;
  (void)num_info;
  return false;
}

RETRO_API void *RETRO_CALLCONV
retro_get_memory_data( unsigned id )
{
  (void)id;
  return NULL;
}

size_t RETRO_API RETRO_CALLCONV
retro_get_memory_size( unsigned id )
{
  (void)id;
  return 0;
}

static void RETRO_CALLCONV
libretro_keyboard_callback( bool down, unsigned keycode, uint32_t character,
                            uint16_t key_modifiers )
{
  if( !runtime_initialised ) return;
  libretro_frontend_keyboard_event( down, keycode, character, key_modifiers );
}

static void
libretro_register_variables( void )
{
  static const struct retro_variable variables[] = {
    { "fuse_machine", "Machine model; 48|128|plus2|plus2a|plus3|pentagon|scorpion|2048|2068|ts2068" },
    { "fuse_fastload", "Fastload; disabled|enabled" },
    { "fuse_tape_traps", "Tape traps; disabled|enabled" },
    { "fuse_loading_sound", "Loading sound; enabled|disabled" },
    { "fuse_issue2", "Issue 2 keyboard; disabled|enabled" },
    { "fuse_bw_tv", "Black and white TV; disabled|enabled" },
    { "fuse_ay_stereo", "AY stereo separation; none|acb|abc" },
    { "fuse_joystick_port_1", "Joystick port 1; kempston|cursor|sinclair1|sinclair2|fuller|none" },
    { NULL, NULL }
  };

  if( environment_callback ) {
    environment_callback( RETRO_ENVIRONMENT_SET_VARIABLES, (void*)variables );
  }
}

static int
libretro_get_variable( const char *key, const char **value )
{
  struct retro_variable variable;

  if( value ) *value = NULL;
  if( !environment_callback ) return 1;

  variable.key = key;
  variable.value = NULL;
  if( !environment_callback( RETRO_ENVIRONMENT_GET_VARIABLE, &variable ) ||
      !variable.value ) {
    return 1;
  }

  if( value ) *value = variable.value;
  return 0;
}

static int
libretro_string_enabled( const char *value )
{
  return value && !strcmp( value, "enabled" );
}

static void
libretro_apply_bool_variable( const char *key, int *field )
{
  const char *value;

  if( !field ) return;
  if( libretro_get_variable( key, &value ) ) return;

  *field = libretro_string_enabled( value );
}

static const char *
libretro_machine_id_from_value( const char *value )
{
  if( !value || !value[0] ) return "48";
  return value;
}

static int
libretro_joystick_type_from_value( const char *value )
{
  if( !value || !strcmp( value, "kempston" ) ) return JOYSTICK_TYPE_KEMPSTON;
  if( !strcmp( value, "cursor" ) ) return JOYSTICK_TYPE_CURSOR;
  if( !strcmp( value, "sinclair1" ) ) return JOYSTICK_TYPE_SINCLAIR_1;
  if( !strcmp( value, "sinclair2" ) ) return JOYSTICK_TYPE_SINCLAIR_2;
  if( !strcmp( value, "fuller" ) ) return JOYSTICK_TYPE_FULLER;
  if( !strcmp( value, "none" ) ) return JOYSTICK_TYPE_NONE;
  return JOYSTICK_TYPE_KEMPSTON;
}

static void
libretro_apply_machine_variable( void )
{
  const char *value;
  const char *id;

  if( libretro_get_variable( "fuse_machine", &value ) ) return;

  id = libretro_machine_id_from_value( value );
  if( settings_current.start_machine && !strcmp( settings_current.start_machine, id ) ) {
    return;
  }

  settings_set_string( &settings_current.start_machine, id );
  if( runtime_initialised ) {
    fuse_runtime_select_machine( id );
    libretro_frontend_reset_runtime_state();
  }
}

static void
libretro_apply_stereo_variable( void )
{
  const char *value;
  const char *setting_value = "None";

  if( libretro_get_variable( "fuse_ay_stereo", &value ) ) return;

  if( value && !strcmp( value, "acb" ) ) {
    setting_value = "ACB";
  } else if( value && !strcmp( value, "abc" ) ) {
    setting_value = "ABC";
  }

  settings_set_string( &settings_current.stereo_ay, setting_value );
  if( runtime_initialised ) sound_init( settings_current.sound_device );
}

static void
libretro_apply_joystick_variable( void )
{
  const char *value;

  if( libretro_get_variable( "fuse_joystick_port_1", &value ) ) return;
  settings_current.joystick_1_output = libretro_joystick_type_from_value( value );
}

static void
libretro_apply_variables( int force )
{
  (void)force;

  libretro_apply_machine_variable();
  libretro_apply_bool_variable( "fuse_fastload", &settings_current.fastload );
  libretro_apply_bool_variable( "fuse_tape_traps", &settings_current.tape_traps );
  libretro_apply_bool_variable( "fuse_loading_sound", &settings_current.sound_load );
  libretro_apply_bool_variable( "fuse_issue2", &settings_current.issue2 );
  libretro_apply_bool_variable( "fuse_bw_tv", &settings_current.bw_tv );
  libretro_apply_stereo_variable();
  libretro_apply_joystick_variable();

  options_dirty = 0;
}

static int
libretro_build_state( unsigned char **buffer, size_t *length )
{
  unsigned char *payload = NULL;
  size_t payload_length = 0;
  int flags = 0;
  size_t total_length;
  libretro_state_header *header;
  libspectrum_snap *snap;
  int error;

  if( buffer ) *buffer = NULL;
  if( length ) *length = 0;

  snap = libspectrum_snap_alloc();
  if( !snap ) return 1;

  error = snapshot_copy_to( snap );
  if( error ) {
    libspectrum_snap_free( snap );
    return error;
  }

  error = libspectrum_snap_write( &payload, &payload_length, &flags, snap,
                                  LIBSPECTRUM_ID_SNAPSHOT_SZX,
                                  fuse_creator, 0 );
  libspectrum_snap_free( snap );
  if( error ) return error;

  total_length = sizeof( *header ) + payload_length;
  *buffer = libspectrum_new( unsigned char, total_length );
  header = (libretro_state_header *)*buffer;

  header->magic = FUSE_LIBRETRO_STATE_MAGIC;
  header->version = FUSE_LIBRETRO_STATE_VERSION;
  header->payload_size = (uint32_t)payload_length;
  header->snapshot_type = LIBSPECTRUM_ID_SNAPSHOT_SZX;
  memset( header->machine_id, 0, sizeof( header->machine_id ) );
  if( machine_current && machine_current->id ) {
    strncpy( header->machine_id, machine_current->id,
             sizeof( header->machine_id ) - 1 );
  }

  memcpy( *buffer + sizeof( *header ), payload, payload_length );
  libspectrum_free( payload );

  if( length ) *length = total_length;
  return 0;
}

static int
libretro_parse_cheat( const char *code, libretro_cheat_entry *entry )
{
  char *working;
  char *value_part;
  char *bank_part = NULL;
  char *address_part;
  char *end;
  unsigned long parsed;

  if( !code || !entry ) return 1;

  working = utils_safe_strdup( code );
  if( !working ) return 1;

  value_part = strchr( working, '=' );
  if( !value_part ) value_part = strchr( working, ' ' );
  if( !value_part ) {
    libspectrum_free( working );
    return 1;
  }

  *value_part++ = '\0';
  while( *value_part == ' ' ) value_part++;

  address_part = working;
  bank_part = strchr( working, ':' );
  if( bank_part ) {
    *bank_part++ = '\0';
    address_part = bank_part;

    parsed = strtoul( working, &end, 0 );
    if( *end || parsed > 255 ) {
      libspectrum_free( working );
      return 1;
    }
    entry->bank = (libspectrum_byte)parsed;
  } else {
    entry->bank = 8;
  }

  parsed = strtoul( address_part, &end, 0 );
  if( *end || parsed > 65535 ) {
    libspectrum_free( working );
    return 1;
  }
  entry->address = (libspectrum_word)parsed;

  parsed = strtoul( value_part, &end, 0 );
  if( *end || parsed > 255 ) {
    libspectrum_free( working );
    return 1;
  }
  entry->value = (libspectrum_byte)parsed;

  libspectrum_free( working );
  return 0;
}

static void
libretro_rebuild_cheats( void )
{
  size_t i;

  pokemem_clear();
  if( !runtime_initialised ) return;

  for( i = 0; i < cheat_entry_count; i++ ) {
    trainer_t *trainer;
    if( !cheat_entries[ i ].enabled || !cheat_entries[ i ].valid ) continue;

    trainer = pokemem_trainer_list_add( cheat_entries[ i ].bank,
                                        cheat_entries[ i ].address,
                                        cheat_entries[ i ].value );
    if( trainer ) pokemem_trainer_activate( trainer );
  }
}

static void
libretro_reset_cheats( void )
{
  free( cheat_entries );
  cheat_entries = NULL;
  cheat_entry_count = 0;
  pokemem_clear();
}
