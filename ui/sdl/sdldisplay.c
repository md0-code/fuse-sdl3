/* sdldisplay.c: Routines for dealing with the SDL display
   Copyright (c) 2000-2006 Philip Kendall, Matan Ziv-Av, Fredrick Meunier
   Copyright (c) 2015 Adrien Destugues

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

   Author contact information:

   E-mail: philip-fuse@shadowmagic.org.uk

*/

#include <config.h>

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "sdlcompat.h"

#include <libspectrum.h>

#include "display.h"
#include "fuse.h"
#include "machine.h"
#include "peripherals/scld.h"
#include "screenshot.h"
#include "settings.h"
#include "sdldisplay.h"
#include "ui/ui.h"
#include "ui/scaler/scaler.h"
#include "ui/uidisplay.h"
#include "utils.h"

#include "sdlglsl.h"
#include "sdlshader.h"

SDL_Surface *sdldisplay_gc = NULL;   /* Scaled 16-bit backbuffer */
static SDL_Window *sdldisplay_window = NULL;
static SDL_Surface *sdldisplay_window_surface = NULL;
static SDL_Renderer *sdldisplay_renderer = NULL;
static SDL_Texture *sdldisplay_texture = NULL;
static SDL_Surface *tmp_screen=NULL; /* Temporary screen for scalers */

static SDL_Surface *red_cassette[2], *green_cassette[2];
static SDL_Surface *red_mdr[2], *green_mdr[2];
static SDL_Surface *red_disk[2], *green_disk[2];
static int sdldisplay_window_visible = 0;

static ui_statusbar_state sdl_disk_state, sdl_mdr_state, sdl_tape_state;
static int sdl_status_updated;

static int tmp_screen_width;
static int sdldisplay_window_width;
static int sdldisplay_window_height;

static Uint32 colour_values[16];

static SDL_Color colour_palette[] = {
  {   0,   0,   0,   0 }, 
  {   0,   0, 192,   0 }, 
  { 192,   0,   0,   0 }, 
  { 192,   0, 192,   0 }, 
  {   0, 192,   0,   0 }, 
  {   0, 192, 192,   0 }, 
  { 192, 192,   0,   0 }, 
  { 192, 192, 192,   0 }, 
  {   0,   0,   0,   0 }, 
  {   0,   0, 255,   0 }, 
  { 255,   0,   0,   0 }, 
  { 255,   0, 255,   0 }, 
  {   0, 255,   0,   0 }, 
  {   0, 255, 255,   0 }, 
  { 255, 255,   0,   0 }, 
  { 255, 255, 255,   0 }
};

static Uint32 bw_values[16];

/* This is a rule of thumb for the maximum number of rects that can be updated
   each frame. If more are generated we just update the whole screen */
#define MAX_UPDATE_RECT 300
static SDL_Rect updated_rects[MAX_UPDATE_RECT];
static int num_rects = 0;
static libspectrum_byte sdldisplay_force_full_refresh = 1;
static int sdldisplay_has_rendered_content = 0;

static int max_fullscreen_height;
static int min_fullscreen_height;
static int fullscreen_width = 0;
static int fullscreen_x_off = 0;
static int fullscreen_y_off = 0;

/* The current size of the display (in units of DISPLAY_SCREEN_*) */
static float sdldisplay_current_size = 1;

static libspectrum_byte sdldisplay_is_full_screen = 0;

static int image_width;
static int image_height;

static int timex;
static int sdldisplay_glsl_backend_disabled;
static int sdldisplay_use_glsl_backend;
static sdlglsl_backend sdldisplay_glsl_backend;
static sdlshader_preset sdldisplay_shader_preset;
static sdlshader_parameter *sdldisplay_runtime_shader_parameters;
static size_t sdldisplay_runtime_shader_parameter_count;
static char *sdldisplay_runtime_shader_preset;
static char *sdldisplay_shader_notice_preset;

static void init_scalers( void );
static void sdldisplay_free_window( void );
static SDL_ScaleMode sdldisplay_get_scale_mode( void );
static void sdldisplay_get_render_rect( SDL_FRect *rect );
static int sdldisplay_allocate_colours( int numColours, Uint32 *colour_values,
                                        Uint32 *bw_values );

static int sdldisplay_load_gfx_mode( void );
static int sdldisplay_prepare_shader_pipeline( void );
static void sdldisplay_scale_frame( void );
static void sdldisplay_copy_frame_region( const SDL_Rect *rect,
                                          Uint32 tmp_screen_pitch,
                                          Uint32 dst_pitch );
static int sdldisplay_upload_frame( void );
static int sdldisplay_present_frame( void );
static int sdldisplay_show_window( void );
static void sdl_icon_overlay( Uint32 tmp_screen_pitch, Uint32 dstPitch );

static void
sdldisplay_set_shader_error( char **error_text, const char *message )
{
  if( !error_text ) return;

  if( *error_text ) libspectrum_free( *error_text );
  *error_text = utils_safe_strdup( message );
}

static void
sdldisplay_update_shader_menu_items( void )
{
  int active = settings_current.startup_shader && *settings_current.startup_shader;

  ui_menu_item_set_active( "/Options/Shader.../Clear", active );
  ui_menu_item_set_active( "/Options/Shader.../Parameters...", active );
}

static int
sdldisplay_shader_parameter_values_equal( float lhs, float rhs )
{
  return fabsf( lhs - rhs ) <= 1.0e-6f;
}

static float
sdldisplay_shader_parameter_preset_value( const sdlshader_parameter *parameter )
{
  return parameter->has_preset_value ? parameter->preset_value
                                     : parameter->default_value;
}

static int
sdldisplay_shader_settings_hex_value( char c )
{
  if( c >= '0' && c <= '9' ) return c - '0';
  if( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
  if( c >= 'A' && c <= 'F' ) return c - 'A' + 10;

  return -1;
}

static int
sdldisplay_shader_settings_needs_escape( unsigned char c )
{
  return c == '%' || c == '|' || c == ';' || c == '=' || c < 0x20;
}

static char*
sdldisplay_shader_settings_escape( const char *text )
{
  const unsigned char *cursor;
  char *result, *output;
  size_t length = 1;
  static const char hex[] = "0123456789ABCDEF";

  for( cursor = (const unsigned char *)text; *cursor; cursor++ ) {
    length += sdldisplay_shader_settings_needs_escape( *cursor ) ? 3 : 1;
  }

  result = libspectrum_new( char, length );
  output = result;

  for( cursor = (const unsigned char *)text; *cursor; cursor++ ) {
    if( sdldisplay_shader_settings_needs_escape( *cursor ) ) {
      *output++ = '%';
      *output++ = hex[ ( *cursor >> 4 ) & 0x0f ];
      *output++ = hex[ *cursor & 0x0f ];
    } else {
      *output++ = (char)*cursor;
    }
  }

  *output = '\0';
  return result;
}

static char*
sdldisplay_shader_settings_unescape( const char *text, size_t length )
{
  char *result, *output;
  size_t i;

  result = libspectrum_new( char, length + 1 );
  output = result;

  for( i = 0; i < length; i++ ) {
    if( text[i] == '%' && i + 2 < length ) {
      int hi = sdldisplay_shader_settings_hex_value( text[i + 1] );
      int lo = sdldisplay_shader_settings_hex_value( text[i + 2] );

      if( hi >= 0 && lo >= 0 ) {
        *output++ = (char)( ( hi << 4 ) | lo );
        i += 2;
        continue;
      }
    }

    *output++ = text[i];
  }

  *output = '\0';
  return result;
}

static void
sdldisplay_clear_runtime_shader_parameters( void )
{
  size_t i;

  for( i = 0; i < sdldisplay_runtime_shader_parameter_count; i++ ) {
    sdlshader_parameter_free( &sdldisplay_runtime_shader_parameters[i] );
  }

  free( sdldisplay_runtime_shader_parameters );
  sdldisplay_runtime_shader_parameters = NULL;
  sdldisplay_runtime_shader_parameter_count = 0;

  if( sdldisplay_runtime_shader_preset ) {
    libspectrum_free( sdldisplay_runtime_shader_preset );
    sdldisplay_runtime_shader_preset = NULL;
  }
}

static int
sdldisplay_find_shader_parameter_index( const sdlshader_parameter *parameters,
                                        size_t parameter_count,
                                        const char *name )
{
  size_t i;

  for( i = 0; i < parameter_count; i++ ) {
    if( !strcmp( parameters[i].name, name ) ) return (int)i;
  }

  return -1;
}

static int
sdldisplay_find_backend_shader_parameter_index( const char *name )
{
  return sdldisplay_find_shader_parameter_index( sdldisplay_glsl_backend.parameters,
                                                 sdldisplay_glsl_backend.parameter_count,
                                                 name );
}

static int
sdldisplay_store_runtime_shader_parameter_value( const char *preset_path,
                                                 const char *name, float value,
                                                 char **error_text )
{
  int parameter_index;

  if( !preset_path || !*preset_path ) {
    sdldisplay_set_shader_error( error_text,
                                 "no startup shader preset is active" );
    return 1;
  }

  if( sdldisplay_runtime_shader_preset &&
      strcmp( sdldisplay_runtime_shader_preset, preset_path ) ) {
    sdldisplay_clear_runtime_shader_parameters();
  }

  if( !sdldisplay_runtime_shader_preset ) {
    sdldisplay_runtime_shader_preset = utils_safe_strdup( preset_path );
  }

  parameter_index = sdldisplay_find_shader_parameter_index(
                      sdldisplay_runtime_shader_parameters,
                      sdldisplay_runtime_shader_parameter_count, name );

  if( parameter_index < 0 ) {
    sdlshader_parameter *new_parameters;
    size_t new_count = sdldisplay_runtime_shader_parameter_count + 1;

    new_parameters = realloc( sdldisplay_runtime_shader_parameters,
                              new_count * sizeof( *new_parameters ) );
    if( !new_parameters ) {
      sdldisplay_set_shader_error( error_text,
                                   "could not store runtime shader parameter" );
      return 1;
    }

    sdldisplay_runtime_shader_parameters = new_parameters;
    parameter_index = (int)sdldisplay_runtime_shader_parameter_count;
    sdlshader_parameter_init(
      &sdldisplay_runtime_shader_parameters[ parameter_index ] );
    sdldisplay_runtime_shader_parameters[ parameter_index ].name =
      utils_safe_strdup( name );
    sdldisplay_runtime_shader_parameter_count = new_count;
  }

  sdldisplay_runtime_shader_parameters[ parameter_index ].initial_value = value;
  sdldisplay_runtime_shader_parameters[ parameter_index ].has_value = 1;

  return 0;
}

static int
sdldisplay_remove_runtime_shader_parameter_value( const char *name )
{
  int parameter_index;

  parameter_index = sdldisplay_find_shader_parameter_index(
                      sdldisplay_runtime_shader_parameters,
                      sdldisplay_runtime_shader_parameter_count, name );
  if( parameter_index < 0 ) return 0;

  sdlshader_parameter_free( &sdldisplay_runtime_shader_parameters[ parameter_index ] );

  memmove( &sdldisplay_runtime_shader_parameters[ parameter_index ],
           &sdldisplay_runtime_shader_parameters[ parameter_index + 1 ],
           ( sdldisplay_runtime_shader_parameter_count - parameter_index - 1 ) *
             sizeof( *sdldisplay_runtime_shader_parameters ) );
  sdldisplay_runtime_shader_parameter_count--;

  return 0;
}

static void
sdldisplay_sync_shader_parameter_settings( void )
{
  char *escaped_preset = NULL;
  char *serialized = NULL;
  char *cursor;
  size_t i;
  size_t count = 0;
  size_t length = 0;

  if( !settings_current.startup_shader || !*settings_current.startup_shader ||
      !sdldisplay_runtime_shader_preset ||
      strcmp( settings_current.startup_shader, sdldisplay_runtime_shader_preset ) ) {
    settings_set_string( &settings_current.startup_shader_parameters, NULL );
    return;
  }

  for( i = 0; i < sdldisplay_runtime_shader_parameter_count; i++ ) {
    char *escaped_name;

    if( !sdldisplay_runtime_shader_parameters[i].has_value ) continue;
    escaped_name = sdldisplay_shader_settings_escape(
                     sdldisplay_runtime_shader_parameters[i].name );
    if( !escaped_name ) return;
    length += strlen( escaped_name ) + 32;
    libspectrum_free( escaped_name );
    count++;
  }

  if( !count ) {
    settings_set_string( &settings_current.startup_shader_parameters, NULL );
    return;
  }

  escaped_preset = sdldisplay_shader_settings_escape(
                     sdldisplay_runtime_shader_preset );
  if( !escaped_preset ) return;

  length += strlen( escaped_preset ) + 2;
  serialized = libspectrum_new( char, length );
  cursor = serialized;
  cursor += snprintf( cursor, length, "%s|", escaped_preset );
  libspectrum_free( escaped_preset );

  for( i = 0; i < sdldisplay_runtime_shader_parameter_count; i++ ) {
    char *escaped_name;

    if( !sdldisplay_runtime_shader_parameters[i].has_value ) continue;
    escaped_name = sdldisplay_shader_settings_escape(
                     sdldisplay_runtime_shader_parameters[i].name );
    if( !escaped_name ) {
      libspectrum_free( serialized );
      return;
    }

    cursor += snprintf( cursor, length - ( cursor - serialized ),
                        "%s%s=%.9g",
                        count > 0 && cursor[-1] != '|' ? ";" : "",
                        escaped_name,
                        sdldisplay_runtime_shader_parameters[i].initial_value );
    libspectrum_free( escaped_name );
  }

  settings_set_string( &settings_current.startup_shader_parameters, serialized );
  libspectrum_free( serialized );
}

static int
sdldisplay_set_runtime_shader_parameter_value( const char *name, float value,
                                               int persist_setting,
                                               char **error_text )
{
  const char *preset_path = settings_current.startup_shader;
  int backend_index;
  float preset_value;

  if( !preset_path || !*preset_path ) {
    preset_path = sdldisplay_shader_preset.preset_path;
  }

  backend_index = sdldisplay_find_backend_shader_parameter_index( name );
  if( backend_index < 0 ) {
    sdldisplay_set_shader_error( error_text,
                                 "shader parameter is no longer available" );
    return 1;
  }

  preset_value = sdldisplay_shader_parameter_preset_value(
                   &sdldisplay_glsl_backend.parameters[ backend_index ] );

  if( sdldisplay_shader_parameter_values_equal( value, preset_value ) ) {
    sdldisplay_remove_runtime_shader_parameter_value( name );
  } else if( sdldisplay_store_runtime_shader_parameter_value( preset_path, name,
                                                              value, error_text ) ) {
    return 1;
  }

  if( persist_setting ) sdldisplay_sync_shader_parameter_settings();

  return 0;
}

static void
sdldisplay_restore_saved_shader_parameter_overrides( void )
{
  const char *preset_path = settings_current.startup_shader;
  const char *serialized = settings_current.startup_shader_parameters;
  const char *separator;
  char *decoded_preset;
  char *payload;
  char *cursor;

  if( !preset_path || !*preset_path ) {
    sdldisplay_clear_runtime_shader_parameters();
    return;
  }

  if( sdldisplay_runtime_shader_preset &&
      !strcmp( sdldisplay_runtime_shader_preset, preset_path ) ) {
    return;
  }

  sdldisplay_clear_runtime_shader_parameters();

  if( !serialized || !*serialized ) return;

  separator = strchr( serialized, '|' );
  if( !separator ) return;

  decoded_preset = sdldisplay_shader_settings_unescape( serialized,
                                                        separator - serialized );
  if( !decoded_preset ) return;

  if( strcmp( decoded_preset, preset_path ) ) {
    libspectrum_free( decoded_preset );
    return;
  }

  sdldisplay_runtime_shader_preset = decoded_preset;
  payload = utils_safe_strdup( separator + 1 );
  if( !payload ) return;

  cursor = payload;
  while( cursor && *cursor ) {
    char *entry_end = strchr( cursor, ';' );
    char *equals = strchr( cursor, '=' );

    if( entry_end ) *entry_end = '\0';

    if( equals ) {
      char *decoded_name;
      char *end;
      float value;

      *equals = '\0';
      decoded_name = sdldisplay_shader_settings_unescape( cursor,
                                                          strlen( cursor ) );
      if( decoded_name ) {
        value = strtof( equals + 1, &end );
        if( end != equals + 1 && !*end ) {
          sdldisplay_store_runtime_shader_parameter_value( preset_path,
                                                           decoded_name, value,
                                                           NULL );
        }
        libspectrum_free( decoded_name );
      }
    }

    if( !entry_end ) break;
    cursor = entry_end + 1;
  }

  libspectrum_free( payload );
}

static int
sdldisplay_apply_runtime_shader_parameter_overrides( char **error_text )
{
  size_t i;
  const char *preset_path = settings_current.startup_shader;

  (void)error_text;

  if( !preset_path || !*preset_path ) {
    preset_path = sdldisplay_shader_preset.preset_path;
  }

  if( !sdldisplay_runtime_shader_preset || !preset_path ||
      strcmp( sdldisplay_runtime_shader_preset, preset_path ) ) {
    return 0;
  }

  for( i = 0; i < sdldisplay_runtime_shader_parameter_count; i++ ) {
    int parameter_index;

    if( !sdldisplay_runtime_shader_parameters[i].has_value ) continue;

    parameter_index = sdldisplay_find_backend_shader_parameter_index(
                        sdldisplay_runtime_shader_parameters[i].name );
    if( parameter_index < 0 ) continue;

    sdldisplay_glsl_backend.parameters[ parameter_index ].initial_value =
      sdldisplay_runtime_shader_parameters[i].initial_value;
  }

  return 0;
}

static void
sdldisplay_report_shader_notice( const char *format, ... )
{
  va_list ap;

  if( settings_current.startup_shader && sdldisplay_shader_notice_preset &&
      !strcmp( settings_current.startup_shader,
               sdldisplay_shader_notice_preset ) ) {
    return;
  }

  if( sdldisplay_shader_notice_preset ) {
    libspectrum_free( sdldisplay_shader_notice_preset );
    sdldisplay_shader_notice_preset = NULL;
  }

  if( settings_current.startup_shader ) {
    sdldisplay_shader_notice_preset =
      utils_safe_strdup( settings_current.startup_shader );
  }

  va_start( ap, format );
  vfprintf( stderr, format, ap );
  va_end( ap );
}

static void
sdldisplay_free_window( void )
{
  sdlglsl_backend_free( sdldisplay_window, &sdldisplay_glsl_backend );

  if( sdldisplay_texture ) {
    SDL_DestroyTexture( sdldisplay_texture );
    sdldisplay_texture = NULL;
  }

  if( sdldisplay_renderer ) {
    SDL_DestroyRenderer( sdldisplay_renderer );
    sdldisplay_renderer = NULL;
  }

  if( sdldisplay_window ) {
    SDL_DestroyWindow( sdldisplay_window );
    sdldisplay_window = NULL;
  }

  sdldisplay_window_surface = NULL;

  sdlshader_preset_free( &sdldisplay_shader_preset );

  sdldisplay_window_visible = 0;

  if( sdldisplay_gc ) {
    SDL_FreeSurface( sdldisplay_gc );
    sdldisplay_gc = NULL;
  }
}

static SDL_ScaleMode
sdldisplay_get_scale_mode( void )
{
  const char *mode = getenv( "FUSE_SDL_SCALE_MODE" );

  if( !mode || !*mode ) return SDL_SCALEMODE_NEAREST;

  if( !strcmp( mode, "linear" ) ) return SDL_SCALEMODE_LINEAR;
  if( !strcmp( mode, "pixelart" ) ) return SDL_SCALEMODE_PIXELART;

  return SDL_SCALEMODE_NEAREST;
}

static void
sdldisplay_get_render_rect( SDL_FRect *rect )
{
  int output_width = sdldisplay_gc ? sdldisplay_gc->w : 0;
  int output_height = sdldisplay_gc ? sdldisplay_gc->h : 0;
  float scale;

  if( sdldisplay_renderer &&
      !SDL_GetCurrentRenderOutputSize( sdldisplay_renderer, &output_width,
                                       &output_height ) ) {
    output_width = sdldisplay_gc ? sdldisplay_gc->w : 0;
    output_height = sdldisplay_gc ? sdldisplay_gc->h : 0;
  } else if( sdldisplay_use_glsl_backend && !sdldisplay_window_visible ) {
    output_width = sdldisplay_window_width;
    output_height = sdldisplay_window_height;
  } else if( sdldisplay_window ) {
    SDL_GetWindowSizeInPixels( sdldisplay_window, &output_width,
                               &output_height );
  }

  if( !sdldisplay_gc || output_width <= 0 || output_height <= 0 ) {
    rect->x = rect->y = 0.0f;
    rect->w = rect->h = 0.0f;
    return;
  }

  scale = SDL_min( output_width / (float)sdldisplay_gc->w,
                   output_height / (float)sdldisplay_gc->h );

  rect->w = sdldisplay_gc->w * scale;
  rect->h = sdldisplay_gc->h * scale;
  rect->x = ( output_width - rect->w ) / 2.0f;
  rect->y = ( output_height - rect->h ) / 2.0f;
}

static void
init_scalers( void )
{
  scaler_register_clear();

  scaler_register( SCALER_NORMAL );
  scaler_register( SCALER_2XSAI );
  scaler_register( SCALER_SUPER2XSAI );
  scaler_register( SCALER_SUPEREAGLE );
  scaler_register( SCALER_ADVMAME2X );
  scaler_register( SCALER_ADVMAME3X );
  scaler_register( SCALER_DOTMATRIX );
  scaler_register( SCALER_PALTV );
  scaler_register( SCALER_HQ2X );
  if( machine_current->timex ) {
    scaler_register( SCALER_HALF ); 
    scaler_register( SCALER_HALFSKIP );
    scaler_register( SCALER_TIMEXTV );
    scaler_register( SCALER_TIMEX1_5X );
    scaler_register( SCALER_TIMEX2X );
  } else {
    scaler_register( SCALER_DOUBLESIZE );
    scaler_register( SCALER_TRIPLESIZE );
    scaler_register( SCALER_QUADSIZE );
    scaler_register( SCALER_TV2X );
    scaler_register( SCALER_TV3X );
    scaler_register( SCALER_TV4X );
    scaler_register( SCALER_PALTV2X );
    scaler_register( SCALER_PALTV3X );
    scaler_register( SCALER_PALTV4X );
    scaler_register( SCALER_HQ3X );
    scaler_register( SCALER_HQ4X );
  }
  
  if( scaler_is_supported( current_scaler ) ) {
    scaler_select_scaler( current_scaler );
  } else {
    scaler_select_scaler( SCALER_NORMAL );
  }
}

static int
sdldisplay_prepare_shader_pipeline( void )
{
  char *error_text = NULL;
  const char *reason;

  sdldisplay_use_glsl_backend = 0;
  sdlshader_preset_free( &sdldisplay_shader_preset );
  sdldisplay_update_shader_menu_items();

  if( !settings_current.startup_shader || !*settings_current.startup_shader ) {
    return 0;
  }

  if( sdldisplay_glsl_backend_disabled ) {
    return 0;
  }

  if( sdlshader_preset_load( settings_current.startup_shader,
                             &sdldisplay_shader_preset, &error_text ) ) {
    sdldisplay_report_shader_notice(
             "%s: startup shader preset '%s' ignored: %s\n",
             fuse_progname, settings_current.startup_shader,
             error_text ? error_text : "unknown error" );
    if( error_text ) libspectrum_free( error_text );
    return 0;
  }

  sdldisplay_restore_saved_shader_parameter_overrides();

#if HAVE_OPENGL
  if( sdldisplay_shader_preset.shader_pass_count >= 1 ) {
    sdldisplay_use_glsl_backend = 1;
    return 0;
  }

  reason = "no RetroArch GLSL shader passes were loaded";
#else
  reason = "this build does not include OpenGL support for RetroArch GLSL presets";
#endif

  sdldisplay_report_shader_notice(
           "%s: startup shader preset '%s' resolved shader0 '%s', but shader execution is unavailable: %s; falling back to the standard SDL renderer path\n",
           fuse_progname, sdldisplay_shader_preset.preset_path,
           sdldisplay_shader_preset.passes[0].shader_path, reason );

  return 0;
}

static int
sdl_convert_icon( SDL_Surface *source, SDL_Surface **icon, int red )
{
  SDL_Surface *copy;   /* Copy with altered palette */
  SDL_Palette *palette;
  int i;

  palette = SDL_GetSurfacePalette( source );
  if( !palette ) return -1;

  SDL_Color colors[ palette->ncolors ];

  copy = SDL_ConvertSurface( source, source->format );
  if( !copy ) return -1;

  palette = SDL_GetSurfacePalette( copy );
  if( !palette ) {
    SDL_FreeSurface( copy );
    return -1;
  }

  for( i = 0; i < palette->ncolors; i++ ) {
    colors[i].r = red ? palette->colors[i].r : 0;
    colors[i].g = red ? 0 : palette->colors[i].g;
    colors[i].b = 0;
    colors[i].a = palette->colors[i].a;
  }

  if( !SDL_SetPaletteColors( palette, colors, 0, i ) ) {
    SDL_FreeSurface( copy );
    return -1;
  }

  icon[0] = SDL_ConvertSurface( copy, tmp_screen->format );

  SDL_FreeSurface( copy );
  if( !icon[0] ) return -1;

  icon[1] = SDL_CreateSurface( (icon[0]->w)<<1, (icon[0]->h)<<1,
                               icon[0]->format );
  if( !icon[1] ) return -1;

  ( scaler_get_proc16( SCALER_DOUBLESIZE ) )(
        (libspectrum_byte*)icon[0]->pixels,
        icon[0]->pitch,
        (libspectrum_byte*)icon[1]->pixels,
        icon[1]->pitch, icon[0]->w, icon[0]->h
      );

  return 0;
}

static int
sdl_load_status_icon( const char*filename, SDL_Surface **red, SDL_Surface **green )
{
  utils_file icon_file;
  SDL_IOStream *stream;
  SDL_Surface *temp;    /* Copy of image as loaded */

  if( utils_read_auxiliary_file( filename, &icon_file, UTILS_AUXILIARY_LIB ) ) {
    fprintf( stderr, "%s: Error loading icon asset \"%s\"\n", fuse_progname,
             filename );
    return -1;
  }

  stream = SDL_IOFromConstMem( icon_file.buffer, icon_file.length );
  if( !stream ) {
    utils_close_file( &icon_file );
    fprintf( stderr, "%s: Error creating icon stream for \"%s\": %s\n",
             fuse_progname, filename, SDL_GetError() );
    return -1;
  }

  if((temp = SDL_LoadBMP_IO( stream, true )) == NULL) {
    utils_close_file( &icon_file );
    fprintf( stderr, "%s: Error decoding icon \"%s\": %s\n", fuse_progname,
             filename, SDL_GetError() );
    return -1;
  }

  utils_close_file( &icon_file );

  if( SDL_GetSurfacePalette( temp ) == NULL ) {
    fprintf( stderr, "%s: Icon \"%s\" is not paletted\n", fuse_progname,
             filename );
    SDL_FreeSurface( temp );
    return -1;
  }

  if( sdl_convert_icon( temp, red, 1 ) || sdl_convert_icon( temp, green, 0 ) ) {
    SDL_FreeSurface( temp );
    return -1;
  }

  SDL_FreeSurface( temp );

  return 0;
}

int
uidisplay_init( int width, int height )
{
  SDL_DisplayMode **modes;
  const SDL_DisplayMode *desktop_mode;
  SDL_DisplayID display;
  int no_modes;
  int i = 0, mode_count = 0, mw = 0, mh = 0, mn = 0;

  display = SDL_GetPrimaryDisplay();
  desktop_mode = SDL_GetDesktopDisplayMode( display );
  modes = SDL_GetFullscreenDisplayModes( display, &mode_count );

  no_modes = modes == NULL || mode_count == 0;

  if( settings_current.sdl_fullscreen_mode &&
      strcmp( settings_current.sdl_fullscreen_mode, "list" ) == 0 ) {

    fprintf( stderr,
    "=====================================================================\n"
    " List of available SDL fullscreen modes:\n"
    "---------------------------------------------------------------------\n"
    "  No. width height\n"
    "---------------------------------------------------------------------\n"
    );
    if( no_modes ) {
      fprintf( stderr, "  ** The modes list is empty...\n" );
    } else {
      for( i = 0; i < mode_count; i++ ) {
        fprintf( stderr, "% 3d  % 5d % 5d\n", i + 1, modes[i]->w, modes[i]->h );
      }
    }
    fprintf( stderr,
    "=====================================================================\n");
    if( modes ) SDL_free( modes );
    fuse_exiting = 1;
    return 0;
  }

  if( !no_modes ) {
    i = mode_count;
  }

  if( settings_current.sdl_fullscreen_mode ) {
    if( sscanf( settings_current.sdl_fullscreen_mode, " %dx%d", &mw, &mh ) != 2 ) {
      if( !no_modes &&
          sscanf( settings_current.sdl_fullscreen_mode, " %d", &mn ) == 1 &&
          mn <= i ) {
        mw = modes[mn - 1]->w; mh = modes[mn - 1]->h;
      }
    }
  }

  /* Check if there are any modes available, or if our resolution is restricted
     at all */
  if( mh > 0 ) {
    /* set from command line */
    max_fullscreen_height = min_fullscreen_height = mh;
    fullscreen_width = mw;
  } else if( desktop_mode ) {
    /* Scale against the real desktop fullscreen target, not the smallest
       advertised legacy mode such as 320x200. */
    max_fullscreen_height = min_fullscreen_height = desktop_mode->h;
  } else if( no_modes ){
    /* Just try whatever we have and see what happens */
    max_fullscreen_height = 480;
    min_fullscreen_height = 240;
  } else {
    /* Record the largest supported fullscreen software mode */
    max_fullscreen_height = modes[0]->h;

    /* Record the smallest supported fullscreen software mode */
    for( i=0; modes[i]; ++i ) {
      min_fullscreen_height = modes[i]->h;
    }
  }

  image_width = width;
  image_height = height;

  timex = machine_current->timex;
  sdldisplay_glsl_backend_disabled = 0;
  sdlshader_preset_init( &sdldisplay_shader_preset );
  sdlglsl_backend_init( &sdldisplay_glsl_backend );

  init_scalers();

  if ( scaler_select_scaler( current_scaler ) )
    scaler_select_scaler( SCALER_NORMAL );

  if( sdldisplay_load_gfx_mode() ) {
    if( modes ) SDL_free( modes );
    return 1;
  }

  /* We can now output error messages to our output device */
  display_ui_initialised = 1;

  sdl_load_status_icon( "cassette.bmp", red_cassette, green_cassette );
  sdl_load_status_icon( "microdrive.bmp", red_mdr, green_mdr );
  sdl_load_status_icon( "plus3disk.bmp", red_disk, green_disk );

  if( modes ) SDL_free( modes );

  return 0;
}

static int
sdldisplay_allocate_colours( int numColours, Uint32 *colour_values,
                             Uint32 *bw_values )
{
  const SDL_PixelFormatDetails *format_details;
  const SDL_Palette *palette;
  int i;
  Uint8 red, green, blue, grey;

  format_details = SDL_GetPixelFormatDetails( tmp_screen->format );
  palette = SDL_GetSurfacePalette( tmp_screen );

  for( i = 0; i < numColours; i++ ) {

      red = colour_palette[i].r;
    green = colour_palette[i].g;
     blue = colour_palette[i].b;

    /* Addition of 0.5 is to avoid rounding errors */
    grey = ( 0.299 * red + 0.587 * green + 0.114 * blue ) + 0.5;

    colour_values[i] = SDL_MapRGB( format_details, palette, red, green, blue );
    bw_values[i]     = SDL_MapRGB( format_details, palette, grey, grey, grey );
  }

  return 0;
}

static void
sdldisplay_find_best_fullscreen_scaler( void )
{
  static int windowed_scaler = -1;
  static int searching_fullscreen_scaler = 0;

  /* Make sure we have at least more than half of the screen covered in
     fullscreen to avoid the "postage stamp" on machines that don't support
     320x240 anymore e.g. Mac notebooks */
  if( settings_current.full_screen ) {
    int i = 0;

    if( searching_fullscreen_scaler ) return;
    searching_fullscreen_scaler = 1;
    while( i < SCALER_NUM &&
           ( image_height*sdldisplay_current_size <= min_fullscreen_height/2 ||
             image_height*sdldisplay_current_size > max_fullscreen_height ) ) {
      if( windowed_scaler == -1) windowed_scaler = current_scaler;
      while( !scaler_is_supported(i) ) i++;
      scaler_set_scaler( i++ );
      sdldisplay_current_size = scaler_get_scaling_factor( current_scaler );
      /* if we failed to find a suitable size scaler, just use normal (what the
         user had originally may be too big) */
      if( image_height * sdldisplay_current_size <= min_fullscreen_height/2 ||
          image_height * sdldisplay_current_size > max_fullscreen_height ) {
        scaler_set_scaler( SCALER_NORMAL );
        sdldisplay_current_size = scaler_get_scaling_factor( current_scaler );
      }
    }
    searching_fullscreen_scaler = 0;
  } else {
    if( windowed_scaler != -1 ) {
      scaler_set_scaler( windowed_scaler );
      windowed_scaler = -1;
      sdldisplay_current_size = scaler_get_scaling_factor( current_scaler );
    }
  }
}

static int
sdldisplay_show_window( void )
{
  if( !sdldisplay_window || sdldisplay_window_visible ) return 0;

  if( !SDL_ShowWindow( sdldisplay_window ) ) return 1;
  if( !SDL_SyncWindow( sdldisplay_window ) ) return 1;

  sdldisplay_window_visible = 1;
  return 0;
}

static int
sdldisplay_load_gfx_mode( void )
{
  SDL_DisplayMode closest_mode;
  SDL_PixelFormat pixel_format;
  Uint16 *tmp_screen_pixels;
  SDL_WindowFlags window_flags;
  char *error_text = NULL;
  int logical_width, logical_height, window_width, window_height;

  sdldisplay_force_full_refresh = 1;
  sdldisplay_has_rendered_content = 0;

  /* Free the old surface */
  if( tmp_screen ) {
    free( tmp_screen->pixels );
    SDL_FreeSurface( tmp_screen );
    tmp_screen = NULL;
  }

  tmp_screen_width = (image_width + 3);

  sdldisplay_current_size = scaler_get_scaling_factor( current_scaler );

  sdldisplay_find_best_fullscreen_scaler();
  sdldisplay_current_size = scaler_get_scaling_factor( current_scaler );

  window_width = settings_current.full_screen && fullscreen_width ?
                 fullscreen_width : image_width * sdldisplay_current_size;
  window_height = settings_current.full_screen && fullscreen_width ?
                  max_fullscreen_height : image_height * sdldisplay_current_size;
  sdldisplay_window_width = window_width;
  sdldisplay_window_height = window_height;
  window_flags = SDL_WINDOW_HIDDEN;

  pixel_format = SDL_PIXELFORMAT_RGB565;
  scaler_select_bitformat( 565 );

  sdldisplay_free_window();

  if( sdldisplay_prepare_shader_pipeline() ) {
    fprintf( stderr, "%s: couldn't prepare SDL shader pipeline\n",
             fuse_progname );
    fuse_abort();
  }

  if( sdldisplay_use_glsl_backend ) {
    logical_width = image_width;
    logical_height = image_height;
  } else {
    logical_width = image_width * sdldisplay_current_size + 0.5f;
    logical_height = image_height * sdldisplay_current_size + 0.5f;
  }

  if( sdldisplay_use_glsl_backend ) {
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK,
                         SDL_GL_CONTEXT_PROFILE_COMPATIBILITY );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 2 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 1 );
    SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
    SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 8 );
    window_flags |= SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN;
  }

  sdldisplay_window = SDL_CreateWindow( FUSE_DOWNSTREAM_NAME, window_width,
                                        window_height, window_flags );
  if( !sdldisplay_window ) {
    fprintf( stderr, "%s: couldn't create SDL graphics context\n", fuse_progname );
    fuse_abort();
  }

  sdldisplay_window_visible = 0;

  SDL_WM_SetCaption( FUSE_DOWNSTREAM_NAME, FUSE_DOWNSTREAM_NAME );

  if( settings_current.full_screen ) {
    if( fullscreen_width && SDL_GetClosestFullscreenDisplayMode(
          SDL_GetPrimaryDisplay(), window_width, window_height, 0.0f, false,
          &closest_mode ) ) {
      if( !SDL_SetWindowFullscreenMode( sdldisplay_window, &closest_mode ) ) {
        fprintf( stderr, "%s: couldn't select SDL fullscreen mode\n", fuse_progname );
        fuse_abort();
      }
    } else {
      SDL_SetWindowFullscreenMode( sdldisplay_window, NULL );
    }

    if( !SDL_SetWindowFullscreen( sdldisplay_window, true ) ||
        ( !sdldisplay_use_glsl_backend &&
          !SDL_SyncWindow( sdldisplay_window ) ) ) {
      fprintf( stderr, "%s: couldn't enable SDL fullscreen mode\n", fuse_progname );
      fuse_abort();
    }
  }

  sdldisplay_gc = SDL_CreateSurface( logical_width, logical_height,
                                     pixel_format );
  if( !sdldisplay_gc ) {
    fprintf( stderr, "%s: couldn't create SDL backbuffer\n", fuse_progname );
    fuse_abort();
  }

  if( sdldisplay_use_glsl_backend ) {
    if( sdlglsl_backend_create( sdldisplay_window, logical_width,
                                logical_height, &sdldisplay_shader_preset,
                                &sdldisplay_glsl_backend, &error_text ) ) {
      sdldisplay_report_shader_notice(
               "%s: startup shader preset '%s' OpenGL setup failed: %s; falling back to the standard SDL renderer path\n",
               fuse_progname, sdldisplay_shader_preset.preset_path,
               error_text ? error_text : "unknown error" );
      if( error_text ) libspectrum_free( error_text );
      sdldisplay_glsl_backend_disabled = 1;
      sdldisplay_use_glsl_backend = 0;
      sdldisplay_free_window();
      return sdldisplay_load_gfx_mode();
    }

    if( sdldisplay_apply_runtime_shader_parameter_overrides( &error_text ) ) {
      sdldisplay_report_shader_notice(
               "%s: startup shader preset '%s' ignored: %s\n",
               fuse_progname, sdldisplay_shader_preset.preset_path,
               error_text ? error_text : "unknown error" );
      if( error_text ) libspectrum_free( error_text );
      sdldisplay_glsl_backend_disabled = 1;
      sdldisplay_use_glsl_backend = 0;
      sdldisplay_free_window();
      return sdldisplay_load_gfx_mode();
    }
  } else {
    sdldisplay_window_surface = SDL_GetWindowSurface( sdldisplay_window );
    if( !sdldisplay_window_surface ) {
      fprintf( stderr, "%s: couldn't get SDL window surface\n", fuse_progname );
      fuse_abort();
    }
  }

  sdldisplay_is_full_screen = settings_current.full_screen;

  /* Create the surface used for the graphics in 16 bit before scaling */

  /* Need some extra bytes around when using 2xSaI */
  tmp_screen_pixels = (Uint16*)calloc(tmp_screen_width*(image_height+3), sizeof(Uint16));
  tmp_screen = SDL_CreateSurfaceFrom( tmp_screen_width,
                                      image_height + 3,
                                      sdldisplay_gc->format,
                                      tmp_screen_pixels,
                                      tmp_screen_width * 2 );

  if( !tmp_screen ) {
    fprintf( stderr, "%s: couldn't create tmp_screen\n", fuse_progname );
    fuse_abort();
  }

  fullscreen_x_off = 0;
  fullscreen_y_off = 0;

  sdldisplay_allocate_colours( 16, colour_values, bw_values );

  /* Redraw the entire screen... */
  display_refresh_all();

  return 0;
}

static void
sdldisplay_copy_frame_region( const SDL_Rect *rect, Uint32 tmp_screen_pitch,
                              Uint32 dst_pitch )
{
  const int bytes_per_pixel = SDL_BYTESPERPIXEL( tmp_screen->format );
  const libspectrum_byte *src =
    (libspectrum_byte*)tmp_screen->pixels +
    ( rect->x + 1 ) * bytes_per_pixel + ( rect->y + 1 ) * tmp_screen_pitch;
  libspectrum_byte *dst =
    (libspectrum_byte*)sdldisplay_gc->pixels +
    rect->x * bytes_per_pixel + rect->y * dst_pitch;
  int row;

  for( row = 0; row < rect->h; row++ ) {
    memcpy( dst, src, rect->w * bytes_per_pixel );
    src += tmp_screen_pitch;
    dst += dst_pitch;
  }
}

static void
sdldisplay_scale_frame( void )
{
  SDL_Rect *r;
  Uint32 tmp_screen_pitch, dstPitch;
  SDL_Rect *last_rect;

  tmp_screen_pitch = tmp_screen->pitch;
  dstPitch = sdldisplay_gc->pitch;
  last_rect = updated_rects + num_rects;

  if( sdldisplay_use_glsl_backend ) {
    for( r = updated_rects; r != last_rect; r++ ) {
      sdldisplay_copy_frame_region( r, tmp_screen_pitch, dstPitch );
    }

    sdl_status_updated = 0;
    return;
  }

  for( r = updated_rects; r != last_rect; r++ ) {

    int dst_y = r->y * sdldisplay_current_size + fullscreen_y_off;
    int dst_h = r->h;
    int dst_x = r->x * sdldisplay_current_size + fullscreen_x_off;

    scaler_proc16(
      (libspectrum_byte*)tmp_screen->pixels +
                    (r->x+1) * SDL_BYTESPERPIXEL( tmp_screen->format ) +
                    (r->y+1)*tmp_screen_pitch,
      tmp_screen_pitch,
      (libspectrum_byte*)sdldisplay_gc->pixels +
                     dst_x * SDL_BYTESPERPIXEL( sdldisplay_gc->format ) +
                     dst_y*dstPitch,
      dstPitch, r->w, dst_h
    );

    r->x = dst_x;
    r->y = dst_y;
    r->w *= sdldisplay_current_size;
    r->h = dst_h * sdldisplay_current_size;
  }

  if( settings_current.statusbar ) {
    sdl_icon_overlay( tmp_screen_pitch, dstPitch );
  }
}

static int
sdldisplay_upload_frame( void )
{
  char *error_text = NULL;

  if( sdldisplay_use_glsl_backend ) {
    if( sdlglsl_backend_upload( sdldisplay_window, &sdldisplay_glsl_backend,
                                sdldisplay_gc, &error_text ) ) {
      if( error_text ) {
        fprintf( stderr, "%s: %s\n", fuse_progname, error_text );
        libspectrum_free( error_text );
      }
      return 1;
    }

    return 0;
  }

  return 0;
}

static int
sdldisplay_present_frame( void )
{
  SDL_Rect dst_rect;
  SDL_FRect render_rect;
  char *error_text = NULL;
  int was_window_visible = sdldisplay_window_visible;

  sdldisplay_get_render_rect( &render_rect );

  if( sdldisplay_use_glsl_backend ) {
    if( render_rect.w <= 0.0f || render_rect.h <= 0.0f ) return 0;

    if( sdlglsl_backend_present( sdldisplay_window, &sdldisplay_glsl_backend,
                   sdldisplay_window_visible ? 0 : sdldisplay_window_width,
                   sdldisplay_window_visible ? 0 : sdldisplay_window_height,
                                 render_rect.x + 0.5f, render_rect.y + 0.5f,
                                 render_rect.w + 0.5f, render_rect.h + 0.5f,
                                 &error_text ) ) {
      if( error_text ) {
        fprintf( stderr, "%s: %s\n", fuse_progname, error_text );
        libspectrum_free( error_text );
      }
      return 1;
    }

    if( sdldisplay_show_window() ) {
      return 1;
    }

    if( !was_window_visible && sdldisplay_window_visible ) {
      if( sdlglsl_backend_present( sdldisplay_window, &sdldisplay_glsl_backend,
                                   0, 0,
                                   render_rect.x + 0.5f, render_rect.y + 0.5f,
                                   render_rect.w + 0.5f, render_rect.h + 0.5f,
                                   &error_text ) ) {
        if( error_text ) {
          fprintf( stderr, "%s: %s\n", fuse_progname, error_text );
          libspectrum_free( error_text );
        }
        return 1;
      }
    }

    if( !sdldisplay_window_visible ) return 0;

    return 0;
  }

  if( !sdldisplay_window_visible ) {
    if( sdldisplay_show_window() ) {
      return 1;
    }
  }

  sdldisplay_window_surface = SDL_GetWindowSurface( sdldisplay_window );
  if( !sdldisplay_window_surface ) {
    return 1;
  }

  if( !SDL_ClearSurface( sdldisplay_window_surface, 0.0f, 0.0f, 0.0f, 1.0f ) ) {
    return 1;
  }

  if( render_rect.w > 0.0f && render_rect.h > 0.0f ) {
    dst_rect.x = render_rect.x + 0.5f;
    dst_rect.y = render_rect.y + 0.5f;
    dst_rect.w = render_rect.w + 0.5f;
    dst_rect.h = render_rect.h + 0.5f;

    if( !SDL_BlitSurfaceScaled( sdldisplay_gc, NULL, sdldisplay_window_surface,
                                &dst_rect, sdldisplay_get_scale_mode() ) ) {
      return 1;
    }
  }

  if( !SDL_UpdateWindowSurface( sdldisplay_window ) ) {
    return 1;
  }

  return 0;
}

int
uidisplay_hotswap_gfx_mode( void )
{
  fuse_emulation_pause();

  /* Free the old surface */
  if( tmp_screen ) {
    free( tmp_screen->pixels );
    SDL_FreeSurface( tmp_screen ); tmp_screen = NULL;
  }

  /* Setup the new GFX mode */
  if( sdldisplay_load_gfx_mode() ) return 1;

  /* Mac OS X resets the state of the cursor after a switch to full screen
     mode */
  if ( settings_current.full_screen || ui_mouse_grabbed ) {
    SDL_ShowCursor( SDL_DISABLE );
    SDL_WarpMouse( 128, 128 );
  } else {
    SDL_ShowCursor( SDL_ENABLE );
  }

  fuse_emulation_unpause();

  return 0;
}

SDL_Surface *saved = NULL;

void
uidisplay_frame_save( void )
{
  if( saved ) {
    SDL_FreeSurface( saved );
    saved = NULL;
  }

  saved = SDL_ConvertSurface( tmp_screen, tmp_screen->format );
}

void
uidisplay_frame_restore( void )
{
  if( saved ) {
    SDL_BlitSurface( saved, NULL, tmp_screen, NULL );
    sdldisplay_has_rendered_content = 1;
    sdldisplay_force_full_refresh = 1;
  }
}

static void
sdl_blit_icon( SDL_Surface **icon,
               SDL_Rect *r, Uint32 tmp_screen_pitch,
               Uint32 dstPitch )
{
  int x, y, w, h, dst_x, dst_y, dst_h;

  if( timex ) {
    r->x<<=1;
    r->y<<=1;
    r->w<<=1;
    r->h<<=1;
  }

  x = r->x;
  y = r->y;
  w = r->w;
  h = r->h;
  r->x++;
  r->y++;

  if( !SDL_BlitSurface( icon[timex], NULL, tmp_screen, r ) ) return;

  /* Extend the dirty region by 1 pixel for scalers
     that "smear" the screen, e.g. 2xSAI */
  if( scaler_flags & SCALER_FLAGS_EXPAND )
    scaler_expander( &x, &y, &w, &h, image_width, image_height );

  dst_y = y * sdldisplay_current_size + fullscreen_y_off;
  dst_h = h;
  dst_x = x * sdldisplay_current_size + fullscreen_x_off;

  scaler_proc16(
	(libspectrum_byte*)tmp_screen->pixels +
      (x+1) * SDL_BYTESPERPIXEL( tmp_screen->format ) +
	                (y+1) * tmp_screen_pitch,
	tmp_screen_pitch,
	(libspectrum_byte*)sdldisplay_gc->pixels +
      dst_x * SDL_BYTESPERPIXEL( sdldisplay_gc->format ) +
			dst_y * dstPitch,
	dstPitch, w, dst_h
  );

  if( num_rects == MAX_UPDATE_RECT ) {
    sdldisplay_force_full_refresh = 1;
    return;
  }

  /* Adjust rects for the destination rect size */
  updated_rects[num_rects].x = dst_x;
  updated_rects[num_rects].y = dst_y;
  updated_rects[num_rects].w = w * sdldisplay_current_size;
  updated_rects[num_rects].h = dst_h * sdldisplay_current_size;

  num_rects++;
}

static void
sdl_icon_overlay( Uint32 tmp_screen_pitch, Uint32 dstPitch )
{
  SDL_Rect r = { 243, 218, red_disk[0]->w, red_disk[0]->h };

  switch( sdl_disk_state ) {
  case UI_STATUSBAR_STATE_ACTIVE:
    sdl_blit_icon( green_disk, &r, tmp_screen_pitch, dstPitch );
    break;
  case UI_STATUSBAR_STATE_INACTIVE:
    sdl_blit_icon( red_disk, &r, tmp_screen_pitch, dstPitch );
    break;
  case UI_STATUSBAR_STATE_NOT_AVAILABLE:
    break;
  }

  r.x = 264;
  r.y = 218;
  r.w = red_mdr[0]->w;
  r.h = red_mdr[0]->h;

  switch( sdl_mdr_state ) {
  case UI_STATUSBAR_STATE_ACTIVE:
    sdl_blit_icon( green_mdr, &r, tmp_screen_pitch, dstPitch );
    break;
  case UI_STATUSBAR_STATE_INACTIVE:
    sdl_blit_icon( red_mdr, &r, tmp_screen_pitch, dstPitch );
    break;
  case UI_STATUSBAR_STATE_NOT_AVAILABLE:
    break;
  }

  r.x = 285;
  r.y = 220;
  r.w = red_cassette[0]->w;
  r.h = red_cassette[0]->h;

  switch( sdl_tape_state ) {
  case UI_STATUSBAR_STATE_ACTIVE:
    sdl_blit_icon( green_cassette, &r, tmp_screen_pitch, dstPitch );
    break;
  case UI_STATUSBAR_STATE_INACTIVE:
  case UI_STATUSBAR_STATE_NOT_AVAILABLE:
    sdl_blit_icon( red_cassette, &r, tmp_screen_pitch, dstPitch );
    break;
  }

  sdl_status_updated = 0;
}

/* Set one pixel in the display */
void
uidisplay_putpixel( int x, int y, int colour )
{
  libspectrum_word *dest_base, *dest;
  Uint32 *palette_values = settings_current.bw_tv ? bw_values :
                           colour_values;

  Uint32 palette_colour = palette_values[ colour ];

  sdldisplay_has_rendered_content = 1;

  if( machine_current->timex ) {
    x <<= 1; y <<= 1;
    dest_base = dest =
      (libspectrum_word*)( (libspectrum_byte*)tmp_screen->pixels +
                           (x+1) * SDL_BYTESPERPIXEL( tmp_screen->format ) +
                           (y+1) * tmp_screen->pitch);

    *(dest++) = palette_colour;
    *(dest++) = palette_colour;
    dest = (libspectrum_word*)
      ( (libspectrum_byte*)dest_base + tmp_screen->pitch);
    *(dest++) = palette_colour;
    *(dest++) = palette_colour;
  } else {
    dest =
      (libspectrum_word*)( (libspectrum_byte*)tmp_screen->pixels +
                           (x+1) * SDL_BYTESPERPIXEL( tmp_screen->format ) +
                           (y+1) * tmp_screen->pitch);

    *dest = palette_colour;
  }
}

/* Print the 8 pixels in `data' using ink colour `ink' and paper
   colour `paper' to the screen at ( (8*x) , y ) */
void
uidisplay_plot8( int x, int y, libspectrum_byte data,
	         libspectrum_byte ink, libspectrum_byte paper )
{
  libspectrum_word *dest;
  Uint32 *palette_values = settings_current.bw_tv ? bw_values :
                           colour_values;

  Uint32 palette_ink = palette_values[ ink ];
  Uint32 palette_paper = palette_values[ paper ];

  sdldisplay_has_rendered_content = 1;

  if( machine_current->timex ) {
    int i;
    libspectrum_word *dest_base;

    x <<= 4; y <<= 1;

    dest_base =
      (libspectrum_word*)( (libspectrum_byte*)tmp_screen->pixels +
                           (x+1) * SDL_BYTESPERPIXEL( tmp_screen->format ) +
                           (y+1) * tmp_screen->pitch);

    for( i=0; i<2; i++ ) {
      dest = dest_base;

      *(dest++) = ( data & 0x80 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x80 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x40 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x40 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x20 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x20 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x10 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x10 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x08 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x08 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x04 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x04 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x02 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x02 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x01 ) ? palette_ink : palette_paper;
      *dest     = ( data & 0x01 ) ? palette_ink : palette_paper;

      dest_base = (libspectrum_word*)
        ( (libspectrum_byte*)dest_base + tmp_screen->pitch);
    }
  } else {
    x <<= 3;

    dest =
      (libspectrum_word*)( (libspectrum_byte*)tmp_screen->pixels +
                           (x+1) * SDL_BYTESPERPIXEL( tmp_screen->format ) +
                           (y+1) * tmp_screen->pitch);

    *(dest++) = ( data & 0x80 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x40 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x20 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x10 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x08 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x04 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x02 ) ? palette_ink : palette_paper;
    *dest     = ( data & 0x01 ) ? palette_ink : palette_paper;
  }
}

/* Print the 16 pixels in `data' using ink colour `ink' and paper
   colour `paper' to the screen at ( (16*x) , y ) */
void
uidisplay_plot16( int x, int y, libspectrum_word data,
		  libspectrum_byte ink, libspectrum_byte paper )
{
  libspectrum_word *dest_base, *dest;
  int i;
  Uint32 *palette_values = settings_current.bw_tv ? bw_values :
                           colour_values;
  Uint32 palette_ink = palette_values[ ink ];
  Uint32 palette_paper = palette_values[ paper ];

  sdldisplay_has_rendered_content = 1;

  x <<= 4; y <<= 1;

  dest_base =
    (libspectrum_word*)( (libspectrum_byte*)tmp_screen->pixels +
                         (x+1) * SDL_BYTESPERPIXEL( tmp_screen->format ) +
                         (y+1) * tmp_screen->pitch);

  for( i=0; i<2; i++ ) {
    dest = dest_base;

    *(dest++) = ( data & 0x8000 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x4000 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x2000 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x1000 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0800 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0400 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0200 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0100 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0080 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0040 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0020 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0010 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0008 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0004 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0002 ) ? palette_ink : palette_paper;
    *dest     = ( data & 0x0001 ) ? palette_ink : palette_paper;

    dest_base = (libspectrum_word*)
      ( (libspectrum_byte*)dest_base + tmp_screen->pitch);
  }
}

void
uidisplay_frame_end( void )
{
  /* We check for a switch to fullscreen here to give systems with a
     windowed-only UI a chance to free menu etc. resources before
     the switch to fullscreen (e.g. Mac OS X) */
  if( sdldisplay_is_full_screen != settings_current.full_screen &&
      uidisplay_hotswap_gfx_mode() ) {
    fprintf( stderr, "%s: Error switching to fullscreen\n", fuse_progname );
    fuse_abort();
  }

  /* Force a full redraw if requested */
  if ( sdldisplay_force_full_refresh ) {
    num_rects = 1;

    updated_rects[0].x = 0;
    updated_rects[0].y = 0;
    updated_rects[0].w = image_width;
    updated_rects[0].h = image_height;
  }

  if ( !(ui_widget_level >= 0) && num_rects == 0 && !sdl_status_updated )
    return;

  if( sdldisplay_use_glsl_backend && !sdldisplay_has_rendered_content )
    return;

  if( SDL_MUSTLOCK( sdldisplay_gc ) ) SDL_LockSurface( sdldisplay_gc );
  sdldisplay_scale_frame();

  if( SDL_MUSTLOCK( sdldisplay_gc ) ) SDL_UnlockSurface( sdldisplay_gc );

  if( sdldisplay_upload_frame() ) {
    fprintf( stderr, "%s: couldn't update SDL texture\n", fuse_progname );
    fuse_abort();
  }

  if( sdldisplay_present_frame() ) {
    fprintf( stderr, "%s: couldn't render SDL texture\n", fuse_progname );
    fuse_abort();
  }

  num_rects = 0;
  sdldisplay_force_full_refresh = 0;
}

void
uidisplay_area( int x, int y, int width, int height )
{
  if ( sdldisplay_force_full_refresh )
    return;

  if( num_rects == MAX_UPDATE_RECT ) {
    sdldisplay_force_full_refresh = 1;
    return;
  }

  /* Extend the dirty region by 1 pixel for scalers
     that "smear" the screen, e.g. 2xSAI */
  if( scaler_flags & SCALER_FLAGS_EXPAND )
    scaler_expander( &x, &y, &width, &height, image_width, image_height );

  updated_rects[num_rects].x = x;
  updated_rects[num_rects].y = y;
  updated_rects[num_rects].w = width;
  updated_rects[num_rects].h = height;

  num_rects++;
}

int
uidisplay_end( void )
{
  int i;

  display_ui_initialised = 0;

  if ( tmp_screen ) {
    free( tmp_screen->pixels );
    SDL_FreeSurface( tmp_screen ); tmp_screen = NULL;
  }

  sdldisplay_free_window();

  if( saved ) {
    SDL_FreeSurface( saved ); saved = NULL;
  }

  if( sdldisplay_shader_notice_preset ) {
    libspectrum_free( sdldisplay_shader_notice_preset );
    sdldisplay_shader_notice_preset = NULL;
  }

  sdldisplay_clear_runtime_shader_parameters();

  for( i=0; i<2; i++ ) {
    if ( red_cassette[i] ) {
      SDL_FreeSurface( red_cassette[i] ); red_cassette[i] = NULL;
    }
    if ( green_cassette[i] ) {
      SDL_FreeSurface( green_cassette[i] ); green_cassette[i] = NULL;
    }
    if ( red_mdr[i] ) {
      SDL_FreeSurface( red_mdr[i] ); red_mdr[i] = NULL;
    }
    if ( green_mdr[i] ) {
      SDL_FreeSurface( green_mdr[i] ); green_mdr[i] = NULL;
    }
    if ( red_disk[i] ) {
      SDL_FreeSurface( red_disk[i] ); red_disk[i] = NULL;
    }
    if ( green_disk[i] ) {
      SDL_FreeSurface( green_disk[i] ); green_disk[i] = NULL;
    }
  }

  return 0;
}

size_t
sdldisplay_shader_parameter_count( void )
{
  if( !sdldisplay_use_glsl_backend || !sdldisplay_glsl_backend.active ) return 0;

  return sdldisplay_glsl_backend.parameter_count;
}

int
sdldisplay_shader_parameter_get_info( size_t index,
                                      sdldisplay_shader_parameter_info *info )
{
  if( !sdldisplay_use_glsl_backend || !sdldisplay_glsl_backend.active ||
      !info || index >= sdldisplay_glsl_backend.parameter_count ) {
    return 1;
  }

  info->name = sdldisplay_glsl_backend.parameters[index].name;
  info->label = sdldisplay_glsl_backend.parameters[index].label ?
                  sdldisplay_glsl_backend.parameters[index].label :
                  sdldisplay_glsl_backend.parameters[index].name;
  info->value = sdldisplay_glsl_backend.parameters[index].initial_value;
  info->default_value = sdldisplay_glsl_backend.parameters[index].default_value;
  info->preset_value = sdldisplay_shader_parameter_preset_value(
                         &sdldisplay_glsl_backend.parameters[index] );
  info->minimum_value = sdldisplay_glsl_backend.parameters[index].minimum_value;
  info->maximum_value = sdldisplay_glsl_backend.parameters[index].maximum_value;
  info->step_value = sdldisplay_glsl_backend.parameters[index].step_value;
  info->has_metadata = sdldisplay_glsl_backend.parameters[index].has_metadata;

  return 0;
}

int
sdldisplay_shader_parameter_set( size_t index, float value )
{
  char *error_text = NULL;
  const char *name;

  if( !sdldisplay_use_glsl_backend || !sdldisplay_glsl_backend.active ||
      index >= sdldisplay_glsl_backend.parameter_count ) {
    return 1;
  }

  name = sdldisplay_glsl_backend.parameters[index].name;
  sdldisplay_glsl_backend.parameters[index].initial_value = value;

  if( sdldisplay_set_runtime_shader_parameter_value( name, value, 1,
                                                     &error_text ) ) {
    if( error_text ) libspectrum_free( error_text );
    return 1;
  }

  sdldisplay_force_full_refresh = 1;

  return 0;
}

int
sdldisplay_shader_parameter_reset_to_default( size_t index )
{
  char *error_text = NULL;
  const char *name;
  float value;

  if( !sdldisplay_use_glsl_backend || !sdldisplay_glsl_backend.active ||
      index >= sdldisplay_glsl_backend.parameter_count ) {
    return 1;
  }

  name = sdldisplay_glsl_backend.parameters[index].name;
  value = sdldisplay_glsl_backend.parameters[index].default_value;
  sdldisplay_glsl_backend.parameters[index].initial_value = value;

  if( sdldisplay_set_runtime_shader_parameter_value( name, value, 1,
                                                     &error_text ) ) {
    if( error_text ) libspectrum_free( error_text );
    return 1;
  }

  sdldisplay_force_full_refresh = 1;
  return 0;
}

int
sdldisplay_shader_parameter_reset_to_preset( size_t index )
{
  char *error_text = NULL;
  const char *name;
  float value;

  if( !sdldisplay_use_glsl_backend || !sdldisplay_glsl_backend.active ||
      index >= sdldisplay_glsl_backend.parameter_count ) {
    return 1;
  }

  name = sdldisplay_glsl_backend.parameters[index].name;
  value = sdldisplay_shader_parameter_preset_value(
            &sdldisplay_glsl_backend.parameters[index] );
  sdldisplay_glsl_backend.parameters[index].initial_value = value;

  if( sdldisplay_set_runtime_shader_parameter_value( name, value, 1,
                                                     &error_text ) ) {
    if( error_text ) libspectrum_free( error_text );
    return 1;
  }

  sdldisplay_force_full_refresh = 1;
  return 0;
}

/* The statusbar handling function */
int
ui_statusbar_update( ui_statusbar_item item, ui_statusbar_state state )
{
  switch( item ) {

  case UI_STATUSBAR_ITEM_DISK:
    sdl_disk_state = state;
    sdl_status_updated = 1;
    return 0;

  case UI_STATUSBAR_ITEM_PAUSED:
    /* We don't support pausing this version of Fuse */
    return 0;

  case UI_STATUSBAR_ITEM_TAPE:
    sdl_tape_state = state;
    sdl_status_updated = 1;
    return 0;

  case UI_STATUSBAR_ITEM_MICRODRIVE:
    sdl_mdr_state = state;
    sdl_status_updated = 1;
    return 0;

  case UI_STATUSBAR_ITEM_MOUSE:
    /* We don't support showing a grab icon */
    return 0;

  }

  ui_error( UI_ERROR_ERROR, "Attempt to update unknown statusbar item %d",
            item );
  return 1;
}
