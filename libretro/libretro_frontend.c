#include <config.h>

#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <libspectrum.h>

#include "debugger/debugger.h"
#include "display.h"
#include "input.h"
#include "keyboard.h"
#include "libretro/libretro.h"
#include "libretro/libretro_frontend.h"
#include "machine.h"
#include "pokefinder/pokemem.h"
#include "settings.h"
#include "sound.h"
#include "timer/timer.h"
#include "ui/scaler/scaler.h"
#include "ui/sdl/sdldisplay.h"
#include "ui/ui.h"
#include "ui/uidisplay.h"
#include "ui/uijoystick.h"

static retro_environment_t environment_callback;
static retro_input_poll_t input_poll_callback;
static retro_input_state_t input_state_callback;
static struct retro_log_callback log_callback;
static int log_callback_available;

static uint32_t *video_buffer;
static int video_width;
static int video_height;
static size_t video_pitch;

static int16_t *audio_buffer;
static size_t audio_capacity;
static size_t audio_count;
static int audio_channels = 2;
static int audio_sample_rate = 44100;
static int libretro_altgr_symbol_active;
static unsigned libretro_port_device[ 2 ] = {
  RETRO_DEVICE_JOYPAD,
  RETRO_DEVICE_JOYPAD,
};

static int joypad_state[ 2 ][ 16 ];
static int joypad_previous[ 2 ][ 16 ];

static const struct {
  unsigned retro_id;
  input_key fuse_button;
} libretro_joypad_mappings[] = {
  { RETRO_DEVICE_ID_JOYPAD_UP, INPUT_JOYSTICK_UP },
  { RETRO_DEVICE_ID_JOYPAD_DOWN, INPUT_JOYSTICK_DOWN },
  { RETRO_DEVICE_ID_JOYPAD_LEFT, INPUT_JOYSTICK_LEFT },
  { RETRO_DEVICE_ID_JOYPAD_RIGHT, INPUT_JOYSTICK_RIGHT },
  { RETRO_DEVICE_ID_JOYPAD_B, INPUT_JOYSTICK_FIRE_1 },
  { RETRO_DEVICE_ID_JOYPAD_A, INPUT_JOYSTICK_FIRE_2 },
  { RETRO_DEVICE_ID_JOYPAD_Y, INPUT_JOYSTICK_FIRE_3 },
  { RETRO_DEVICE_ID_JOYPAD_X, INPUT_JOYSTICK_FIRE_4 },
  { RETRO_DEVICE_ID_JOYPAD_L, INPUT_JOYSTICK_FIRE_5 },
  { RETRO_DEVICE_ID_JOYPAD_R, INPUT_JOYSTICK_FIRE_6 },
};

static input_key libretro_map_keycode( unsigned keycode );
static input_key libretro_map_character( uint32_t character );
static int libretro_is_modifier_key( input_key key );
static int libretro_altgr_symbol_required( input_key mapped_key,
                                           uint16_t key_modifiers );
static void libretro_update_altgr_symbol_state( int required );
static void libretro_emit_joystick_event( int which, input_key button,
                                          int pressed );
static void libretro_emit_keyboard_event( int down, input_key native_key,
                                          input_key spectrum_key );
static uint32_t libretro_palette_colour( int colour );
static int libretro_audio_ensure_capacity( size_t samples );

void
libretro_frontend_set_environment( retro_environment_t cb )
{
  environment_callback = cb;
  log_callback_available = 0;
  memset( &log_callback, 0, sizeof( log_callback ) );

  if( environment_callback &&
      environment_callback( RETRO_ENVIRONMENT_GET_LOG_INTERFACE,
                            &log_callback ) &&
      log_callback.log ) {
    log_callback_available = 1;
  }
}

void
libretro_frontend_set_input_callbacks( retro_input_poll_t poll_cb,
                                       retro_input_state_t state_cb )
{
  input_poll_callback = poll_cb;
  input_state_callback = state_cb;
}

void
libretro_frontend_set_controller_port_device( unsigned port, unsigned device )
{
  size_t i;

  if( port >= ARRAY_SIZE( libretro_port_device ) ) return;
  if( libretro_port_device[ port ] == device ) return;

  if( libretro_port_device[ port ] == RETRO_DEVICE_JOYPAD ) {
    for( i = 0; i < ARRAY_SIZE( libretro_joypad_mappings ); i++ ) {
      unsigned id = libretro_joypad_mappings[ i ].retro_id;

      if( joypad_previous[ port ][ id ] ) {
        libretro_emit_joystick_event( (int)port,
                                      libretro_joypad_mappings[ i ].fuse_button,
                                      0 );
      }
    }
  }

  memset( joypad_previous[ port ], 0, sizeof( joypad_previous[ port ] ) );
  memset( joypad_state[ port ], 0, sizeof( joypad_state[ port ] ) );
  libretro_port_device[ port ] = device;
}

void
libretro_log_message( enum retro_log_level level, const char *fmt, ... )
{
  va_list ap;
  char buffer[ 2048 ];

  va_start( ap, fmt );
  vsnprintf( buffer, sizeof( buffer ), fmt, ap );
  va_end( ap );

  if( log_callback_available ) {
    log_callback.log( level, "%s", buffer );
  } else {
    FILE *stream = level >= RETRO_LOG_WARN ? stderr : stdout;
    fputs( buffer, stream );
    fflush( stream );
  }
}

void
libretro_frontend_capture_input( void )
{
  unsigned port, id;

  if( !input_state_callback ) {
    memset( joypad_state, 0, sizeof( joypad_state ) );
    return;
  }

  if( input_poll_callback ) input_poll_callback();

  for( port = 0; port < 2; port++ ) {
    if( libretro_port_device[ port ] != RETRO_DEVICE_JOYPAD ) {
      memset( joypad_state[ port ], 0, sizeof( joypad_state[ port ] ) );
      continue;
    }

    for( id = 0; id < 16; id++ ) {
      joypad_state[ port ][ id ] =
        input_state_callback( port, RETRO_DEVICE_JOYPAD, 0, id ) ? 1 : 0;
    }
  }
}

static void
libretro_emit_joystick_event( int which, input_key button, int pressed )
{
  input_event_t event;

  event.type = pressed ? INPUT_EVENT_JOYSTICK_PRESS
                       : INPUT_EVENT_JOYSTICK_RELEASE;
  event.types.joystick.which = which;
  event.types.joystick.button = button;
  input_event( &event );
}

void
ui_joystick_poll( void )
{
  size_t port, i;

  for( port = 0; port < 2; port++ ) {
    if( libretro_port_device[ port ] != RETRO_DEVICE_JOYPAD ) continue;

    for( i = 0; i < ARRAY_SIZE( libretro_joypad_mappings ); i++ ) {
      unsigned id = libretro_joypad_mappings[ i ].retro_id;
      int current = joypad_state[ port ][ id ];
      int previous = joypad_previous[ port ][ id ];

      if( current != previous ) {
        libretro_emit_joystick_event(
          (int)port,
          libretro_joypad_mappings[ i ].fuse_button,
          current
        );
      }

      joypad_previous[ port ][ id ] = current;
    }
  }
}

int
ui_joystick_init( void )
{
  memset( joypad_state, 0, sizeof( joypad_state ) );
  memset( joypad_previous, 0, sizeof( joypad_previous ) );
  libretro_port_device[ 0 ] = RETRO_DEVICE_JOYPAD;
  libretro_port_device[ 1 ] = RETRO_DEVICE_JOYPAD;
  return 2;
}

void
ui_joystick_end( void )
{
}

static input_key
libretro_map_character( uint32_t character )
{
  if( character >= 0x20 && character <= 0x7f ) {
    return (input_key)character;
  }

  return INPUT_KEY_NONE;
}

static input_key
libretro_map_keycode( unsigned keycode )
{
  if( keycode >= 0x20 && keycode <= 0x7f ) {
    return (input_key)keycode;
  }

  switch( keycode ) {
  case RETROK_BACKSPACE: return INPUT_KEY_BackSpace;
  case RETROK_TAB: return INPUT_KEY_Tab;
  case RETROK_RETURN: return INPUT_KEY_Return;
  case RETROK_ESCAPE: return INPUT_KEY_Escape;
  case RETROK_KP_ENTER: return INPUT_KEY_KP_Enter;
  case RETROK_UP: return INPUT_KEY_Up;
  case RETROK_DOWN: return INPUT_KEY_Down;
  case RETROK_LEFT: return INPUT_KEY_Left;
  case RETROK_RIGHT: return INPUT_KEY_Right;
  case RETROK_INSERT: return INPUT_KEY_Insert;
  case RETROK_DELETE: return INPUT_KEY_Delete;
  case RETROK_HOME: return INPUT_KEY_Home;
  case RETROK_END: return INPUT_KEY_End;
  case RETROK_PAGEUP: return INPUT_KEY_Page_Up;
  case RETROK_PAGEDOWN: return INPUT_KEY_Page_Down;
  case RETROK_CAPSLOCK: return INPUT_KEY_Caps_Lock;
  case RETROK_F1: return INPUT_KEY_F1;
  case RETROK_F2: return INPUT_KEY_F2;
  case RETROK_F3: return INPUT_KEY_F3;
  case RETROK_F4: return INPUT_KEY_F4;
  case RETROK_F5: return INPUT_KEY_F5;
  case RETROK_F6: return INPUT_KEY_F6;
  case RETROK_F7: return INPUT_KEY_F7;
  case RETROK_F8: return INPUT_KEY_F8;
  case RETROK_F9: return INPUT_KEY_F9;
  case RETROK_F10: return INPUT_KEY_F10;
  case RETROK_F11: return INPUT_KEY_F11;
  case RETROK_F12: return INPUT_KEY_F12;
  case RETROK_LSHIFT: return INPUT_KEY_Shift_L;
  case RETROK_RSHIFT: return INPUT_KEY_Shift_R;
  case RETROK_LCTRL: return INPUT_KEY_Control_L;
  case RETROK_RCTRL: return INPUT_KEY_Control_R;
  case RETROK_LALT: return INPUT_KEY_Alt_L;
  case RETROK_RALT: return INPUT_KEY_Alt_R;
  case RETROK_LMETA: return INPUT_KEY_Meta_L;
  case RETROK_RMETA: return INPUT_KEY_Meta_R;
  case RETROK_LSUPER: return INPUT_KEY_Super_L;
  case RETROK_RSUPER: return INPUT_KEY_Super_R;
  case RETROK_MODE: return INPUT_KEY_Mode_switch;
  default: return INPUT_KEY_NONE;
  }
}

static int
libretro_is_modifier_key( input_key key )
{
  switch( key ) {
  case INPUT_KEY_Shift_L:
  case INPUT_KEY_Shift_R:
  case INPUT_KEY_Control_L:
  case INPUT_KEY_Control_R:
  case INPUT_KEY_Alt_L:
  case INPUT_KEY_Alt_R:
  case INPUT_KEY_Meta_L:
  case INPUT_KEY_Meta_R:
  case INPUT_KEY_Super_L:
  case INPUT_KEY_Super_R:
  case INPUT_KEY_Mode_switch:
    return 1;
  default:
    return 0;
  }
}

static int
libretro_altgr_symbol_required( input_key mapped_key, uint16_t key_modifiers )
{
  if( libretro_is_modifier_key( mapped_key ) ) return 0;

  return ( key_modifiers & ( RETROKMOD_CTRL | RETROKMOD_ALT ) ) ==
         ( RETROKMOD_CTRL | RETROKMOD_ALT );
}

static void
libretro_update_altgr_symbol_state( int required )
{
  if( required == libretro_altgr_symbol_active ) return;

  libretro_emit_keyboard_event( required, INPUT_KEY_Control_R,
                                INPUT_KEY_Control_R );
  libretro_altgr_symbol_active = required;
}

static void
libretro_emit_keyboard_event( int down, input_key native_key,
                              input_key spectrum_key )
{
  input_event_t event;

  if( native_key == INPUT_KEY_NONE && spectrum_key == INPUT_KEY_NONE ) return;

  event.type = down ? INPUT_EVENT_KEYPRESS : INPUT_EVENT_KEYRELEASE;
  event.types.key.native_key = native_key;
  event.types.key.spectrum_key = spectrum_key;
  input_event( &event );
}

void
libretro_frontend_keyboard_event( bool down, unsigned keycode,
                                  uint32_t character,
                                  uint16_t key_modifiers )
{
  input_key mapped_key;
  input_key mapped_char;

  mapped_key = libretro_map_keycode( keycode );
  mapped_char = libretro_map_character( character );
  libretro_update_altgr_symbol_state(
    libretro_altgr_symbol_required( mapped_key, key_modifiers )
  );

  if( down ) {
    libretro_emit_keyboard_event( 1,
                                  mapped_char != INPUT_KEY_NONE ? mapped_char
                                                                : mapped_key,
                                  mapped_key != INPUT_KEY_NONE ? mapped_key
                                                               : mapped_char );
  } else {
    libretro_emit_keyboard_event( 0,
                                  mapped_key,
                                  mapped_key );
  }
}

int
ui_init( int *argc, char ***argv )
{
  (void)argc;
  (void)argv;

  ui_mouse_present = 0;
  ui_mouse_grabbed = 0;
  return 0;
}

int
ui_event( void )
{
  return 0;
}

int
ui_end( void )
{
  ui_joystick_end();
  return uidisplay_end();
}

int
ui_error_specific( ui_error_level severity, const char *message )
{
  enum retro_log_level level = RETRO_LOG_INFO;

  switch( severity ) {
  case UI_ERROR_WARNING: level = RETRO_LOG_WARN; break;
  case UI_ERROR_ERROR: level = RETRO_LOG_ERROR; break;
  case UI_ERROR_INFO:
  default:
    level = RETRO_LOG_INFO;
    break;
  }

  libretro_log_message( level, "%s\n", message );
  return 0;
}

int
ui_debugger_activate( void )
{
  libretro_log_message( RETRO_LOG_WARN,
                        "libretro: debugger activation requested but no debugger UI is available\n" );
  return 0;
}

int
ui_debugger_deactivate( int interruptable )
{
  (void)interruptable;
  return 0;
}

int
ui_debugger_update( void )
{
  return 0;
}

int
ui_debugger_disassemble( libspectrum_word address )
{
  (void)address;
  return 0;
}

void
ui_breakpoints_updated( void )
{
}

int
ui_widgets_reset( void )
{
  return 0;
}

ui_confirm_save_t
ui_confirm_save_specific( const char *message )
{
  (void)message;
  return UI_CONFIRM_SAVE_DONTSAVE;
}

int
ui_statusbar_update( ui_statusbar_item item, ui_statusbar_state state )
{
  (void)item;
  (void)state;
  return 0;
}

int
ui_statusbar_update_speed( float speed )
{
  (void)speed;
  return 0;
}

int
ui_tape_browser_update( ui_tape_browser_update_type change,
                        libspectrum_tape_block *block )
{
  (void)change;
  (void)block;
  return 0;
}

char *
ui_get_open_filename( const char *title )
{
  (void)title;
  return NULL;
}

char *
ui_get_save_filename( const char *title )
{
  (void)title;
  return NULL;
}

int
ui_query( const char *message )
{
  (void)message;
  return 0;
}

void
ui_popup_menu( int native_key )
{
  (void)native_key;
}

void
ui_widget_keyhandler( int native_key )
{
  (void)native_key;
}

void
ui_pokemem_selector( const char *filename )
{
  if( filename ) pokemem_read_from_file( filename );
}

void
widget_show_transient_message( const char *message )
{
  (void)message;
}

void
widget_frame( void )
{
}

int
ui_menu_item_set_active( const char *path, int active )
{
  (void)path;
  (void)active;
  return 0;
}

int
widget_init( void )
{
  return 0;
}

int
widget_end( void )
{
  return 0;
}

void
widget_finish( void )
{
}

ui_confirm_joystick_t
ui_confirm_joystick( libspectrum_joystick libspectrum_type, int inputs )
{
  (void)libspectrum_type;
  (void)inputs;
  return UI_CONFIRM_JOYSTICK_NONE;
}

int
ui_get_rollback_point( GSList *points )
{
  (void)points;
  return -1;
}

int
menu_select_roms_with_title( const char *title, size_t start, size_t count,
                             int is_peripheral )
{
  (void)title;
  (void)start;
  (void)count;
  (void)is_peripheral;
  return 1;
}

scaler_type
menu_get_scaler( scaler_available_fn selector )
{
  scaler_type scaler;

  if( selector && selector( current_scaler ) ) return current_scaler;

  for( scaler = 0; scaler < SCALER_NUM; scaler++ ) {
    if( !selector || selector( scaler ) ) return scaler;
  }

  return SCALER_NORMAL;
}

size_t
sdldisplay_shader_parameter_count( void )
{
  return 0;
}

int
sdldisplay_shader_parameter_get_info( size_t index,
                                      sdldisplay_shader_parameter_info *info )
{
  (void)index;
  (void)info;
  return 1;
}

int
sdldisplay_shader_parameter_set( size_t index, float value )
{
  (void)index;
  (void)value;
  return 1;
}

int
sdldisplay_shader_parameter_reset_to_default( size_t index )
{
  (void)index;
  return 1;
}

int
sdldisplay_shader_parameter_reset_to_preset( size_t index )
{
  (void)index;
  return 1;
}

int
uidisplay_init( int width, int height )
{
  size_t pixels;

  if( width <= 0 || height <= 0 ) return 1;

  free( video_buffer );
  video_buffer = NULL;

  pixels = (size_t)width * (size_t)height;
  video_buffer = calloc( pixels, sizeof( *video_buffer ) );
  if( !video_buffer ) return 1;

  video_width = width;
  video_height = height;
  video_pitch = (size_t)width * sizeof( *video_buffer );
  display_ui_initialised = 1;

  memset( video_buffer, 0, pixels * sizeof( *video_buffer ) );
  display_refresh_all();

  return 0;
}

void
uidisplay_area( int x, int y, int w, int h )
{
  (void)x;
  (void)y;
  (void)w;
  (void)h;
}

void
uidisplay_frame_end( void )
{
}

int
uidisplay_hotswap_gfx_mode( void )
{
  return 0;
}

void
uidisplay_frame_save( void )
{
}

void
uidisplay_frame_restore( void )
{
}

int
uidisplay_end( void )
{
  free( video_buffer );
  video_buffer = NULL;
  video_width = 0;
  video_height = 0;
  video_pitch = 0;
  display_ui_initialised = 0;
  return 0;
}

static uint32_t
libretro_palette_colour( int colour )
{
  static const uint32_t colour_palette[16] = {
    0xff000000, 0xff0000c0, 0xffc00000, 0xffc000c0,
    0xff00c000, 0xff00c0c0, 0xffc0c000, 0xffc0c0c0,
    0xff000000, 0xff0000ff, 0xffff0000, 0xffff00ff,
    0xff00ff00, 0xff00ffff, 0xffffff00, 0xffffffff,
  };
  static const uint32_t bw_palette[16] = {
    0xff000000, 0xff222222, 0xff444444, 0xff666666,
    0xff555555, 0xff777777, 0xff999999, 0xffbbbbbb,
    0xff000000, 0xff333333, 0xff666666, 0xff888888,
    0xff777777, 0xffaaaaaa, 0xffdddddd, 0xffffffff,
  };

  if( colour < 0 ) colour = 0;
  if( colour > 15 ) colour = 15;

  return settings_current.bw_tv ? bw_palette[ colour ]
                                : colour_palette[ colour ];
}

void
uidisplay_putpixel( int x, int y, int colour )
{
  uint32_t pixel = libretro_palette_colour( colour );

  if( !video_buffer || x < 0 || y < 0 ) return;

  if( machine_current->timex ) {
    int px = x << 1;
    int py = y << 1;
    if( px + 1 >= video_width || py + 1 >= video_height ) return;
    video_buffer[ py * video_width + px ] = pixel;
    video_buffer[ py * video_width + px + 1 ] = pixel;
    video_buffer[ ( py + 1 ) * video_width + px ] = pixel;
    video_buffer[ ( py + 1 ) * video_width + px + 1 ] = pixel;
  } else {
    if( x >= video_width || y >= video_height ) return;
    video_buffer[ y * video_width + x ] = pixel;
  }
}

void
uidisplay_plot8( int x, int y, libspectrum_byte data, libspectrum_byte ink,
                 libspectrum_byte paper )
{
  uint32_t palette_ink = libretro_palette_colour( ink );
  uint32_t palette_paper = libretro_palette_colour( paper );
  int bit;

  if( !video_buffer ) return;

  if( machine_current->timex ) {
    int px = x << 4;
    int py = y << 1;
    if( py + 1 >= video_height ) return;

    for( bit = 0; bit < 8; bit++ ) {
      uint32_t pixel = ( data & ( 0x80 >> bit ) ) ? palette_ink : palette_paper;
      int column = px + bit * 2;
      if( column + 1 >= video_width ) break;
      video_buffer[ py * video_width + column ] = pixel;
      video_buffer[ py * video_width + column + 1 ] = pixel;
      video_buffer[ ( py + 1 ) * video_width + column ] = pixel;
      video_buffer[ ( py + 1 ) * video_width + column + 1 ] = pixel;
    }
  } else {
    int px = x << 3;
    if( y >= video_height ) return;

    for( bit = 0; bit < 8; bit++ ) {
      if( px + bit >= video_width ) break;
      video_buffer[ y * video_width + px + bit ] =
        ( data & ( 0x80 >> bit ) ) ? palette_ink : palette_paper;
    }
  }
}

void
uidisplay_plot16( int x, int y, libspectrum_word data, libspectrum_byte ink,
                  libspectrum_byte paper )
{
  uint32_t palette_ink = libretro_palette_colour( ink );
  uint32_t palette_paper = libretro_palette_colour( paper );
  int bit;
  int px = x << 4;
  int py = y << 1;

  if( !video_buffer || px >= video_width || py + 1 >= video_height ) return;

  for( bit = 0; bit < 16; bit++ ) {
    uint32_t pixel = ( data & ( 0x8000 >> bit ) ) ? palette_ink : palette_paper;
    if( px + bit >= video_width ) break;
    video_buffer[ py * video_width + px + bit ] = pixel;
    video_buffer[ ( py + 1 ) * video_width + px + bit ] = pixel;
  }
}

void
uidisplay_blit_logo( int display_x, int display_y, int w, int h )
{
  (void)display_x;
  (void)display_y;
  (void)w;
  (void)h;
}

static int
libretro_audio_ensure_capacity( size_t samples )
{
  int16_t *new_buffer;
  size_t new_capacity;

  if( audio_count + samples <= audio_capacity ) return 0;

  new_capacity = audio_capacity ? audio_capacity : 4096;
  while( new_capacity < audio_count + samples ) new_capacity <<= 1;

  new_buffer = realloc( audio_buffer, new_capacity * sizeof( *audio_buffer ) );
  if( !new_buffer ) return 1;

  audio_buffer = new_buffer;
  audio_capacity = new_capacity;
  return 0;
}

int
sound_lowlevel_init( const char *device, int *freqptr, int *stereoptr )
{
  (void)device;

  audio_channels = ( stereoptr && *stereoptr ) ? 2 : 1;
  audio_sample_rate = freqptr ? *freqptr : 44100;
  audio_count = 0;

  if( libretro_audio_ensure_capacity( (size_t)audio_channels * 4096 ) ) return 1;

  return 0;
}

void
sound_lowlevel_end( void )
{
  audio_count = 0;
}

void
sound_lowlevel_frame( libspectrum_signed_word *data, int len )
{
  if( !data || len <= 0 ) return;
  if( libretro_audio_ensure_capacity( (size_t)len ) ) return;

  memcpy( audio_buffer + audio_count, data, (size_t)len * sizeof( *audio_buffer ) );
  audio_count += (size_t)len;
}

int
sound_lowlevel_buffer_space( void )
{
  return INT_MAX / 2;
}

const int16_t *
libretro_frontend_get_audio_data( size_t *frames )
{
  if( frames ) {
    *frames = audio_channels > 0 ? audio_count / (size_t)audio_channels : 0;
  }
  return audio_buffer;
}

void
libretro_frontend_clear_audio( void )
{
  audio_count = 0;
}

int
libretro_frontend_get_audio_sample_rate( void )
{
  return audio_sample_rate;
}

int
libretro_frontend_get_audio_channels( void )
{
  return audio_channels;
}

int
libretro_frontend_get_video_width( void )
{
  return video_width;
}

int
libretro_frontend_get_video_height( void )
{
  return video_height;
}

const void *
libretro_frontend_get_video_data( size_t *pitch )
{
  if( pitch ) *pitch = video_pitch;
  return video_buffer;
}

void
libretro_frontend_reset_runtime_state( void )
{
  if( libretro_altgr_symbol_active ) {
    libretro_emit_keyboard_event( 0, INPUT_KEY_Control_R,
                                  INPUT_KEY_Control_R );
    libretro_altgr_symbol_active = 0;
  }

  memset( joypad_previous, 0, sizeof( joypad_previous ) );
  memset( joypad_state, 0, sizeof( joypad_state ) );
  audio_count = 0;
}

double
timer_get_time( void )
{
  return (double)clock() / (double)CLOCKS_PER_SEC;
}

void
timer_sleep( int ms )
{
  (void)ms;
}

int
ui_mouse_grab( int startup )
{
  (void)startup;
  return 0;
}

int
ui_mouse_release( int suspend )
{
  (void)suspend;
  return 0;
}
