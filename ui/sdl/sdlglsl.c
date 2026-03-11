/* sdlglsl.c: OpenGL backend helpers for RetroArch GLSL presets
   Copyright (c) 2026

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
*/

#include <config.h>

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libspectrum.h>

#include "utils.h"

#include "sdlglsl.h"

#if HAVE_OPENGL
#define GL_GLEXT_PROTOTYPES
#include <SDL3/SDL_opengl.h>
#endif

#if USE_LIBPNG
#include <png.h>
#endif

static void
sdlglsl_set_error( char **error_text, const char *format, ... )
{
  char buffer[1024];
  va_list ap;

  if( !error_text ) return;

  if( *error_text ) libspectrum_free( *error_text );

  va_start( ap, format );
  vsnprintf( buffer, sizeof( buffer ), format, ap );
  va_end( ap );

  *error_text = utils_safe_strdup( buffer );
}

#if HAVE_OPENGL

static int
sdlglsl_debug_enabled( void )
{
  static int cached = -1;

  if( cached == -1 ) {
    const char *value = getenv( "FUSE_SDL_GLSL_DEBUG" );
    cached = value && *value && strcmp( value, "0" ) ? 1 : 0;
  }

  return cached;
}

static void
sdlglsl_debug_log( const char *format, ... )
{
  FILE *file;
  va_list ap;

  if( !sdlglsl_debug_enabled() ) return;

  file = fopen( "/tmp/fuse-glsl-runtime.log", "a" );
  if( !file ) return;

  va_start( ap, format );
  fprintf( file, "fuse-glsl: " );
  vfprintf( file, format, ap );
  fprintf( file, "\n" );
  fflush( file );
  va_end( ap );

  fclose( file );
}

static int
sdlglsl_check_gl_error( const char *step, char **error_text )
{
  GLenum error = glGetError();

  if( error == GL_NO_ERROR ) return 0;

  sdlglsl_set_error( error_text, "%s failed with OpenGL error 0x%04x", step,
                     (unsigned int)error );
  return 1;
}

static int
sdlglsl_make_current( SDL_Window *window, sdlglsl_backend *backend,
                      char **error_text )
{
  if( !SDL_GL_MakeCurrent( window, backend->context ) ) {
    sdlglsl_set_error( error_text, "could not make OpenGL context current: %s",
                       SDL_GetError() );
    return 1;
  }

  return 0;
}

static int
sdlglsl_check_shader( GLuint shader, const char *stage, char **error_text )
{
  GLint compiled = GL_FALSE;

  glGetShaderiv( shader, GL_COMPILE_STATUS, &compiled );
  if( compiled ) return 0;

  {
    GLint log_length = 0;
    char *log;

    glGetShaderiv( shader, GL_INFO_LOG_LENGTH, &log_length );
    log = libspectrum_new( char, log_length > 1 ? log_length : 1 );
    if( log_length > 1 ) {
      glGetShaderInfoLog( shader, log_length, NULL, log );
    } else {
      log[0] = '\0';
    }

    sdlglsl_set_error( error_text, "%s shader compilation failed: %s", stage,
                       log );
    libspectrum_free( log );
  }

  return 1;
}

static int
sdlglsl_check_program( GLuint program, char **error_text )
{
  GLint linked = GL_FALSE;

  glGetProgramiv( program, GL_LINK_STATUS, &linked );
  if( linked ) return 0;

  {
    GLint log_length = 0;
    char *log;

    glGetProgramiv( program, GL_INFO_LOG_LENGTH, &log_length );
    log = libspectrum_new( char, log_length > 1 ? log_length : 1 );
    if( log_length > 1 ) {
      glGetProgramInfoLog( program, log_length, NULL, log );
    } else {
      log[0] = '\0';
    }

    sdlglsl_set_error( error_text, "GLSL program link failed: %s", log );
    libspectrum_free( log );
  }

  return 1;
}

static int
sdlglsl_compile_program( sdlshader_source *shader_source, GLuint *program,
                        char **error_text )
{
  char *vertex_source = NULL, *fragment_source = NULL;
  const char *vertex_sources[1], *fragment_sources[1];
  GLuint vertex_shader = 0, fragment_shader = 0;
  GLint attribute_location;

  if( sdlshader_source_build_stage( shader_source, "VERTEX",
                                    &vertex_source, error_text ) ) {
    return 1;
  }

  if( sdlshader_source_build_stage( shader_source, "FRAGMENT",
                                    &fragment_source, error_text ) ) {
    libspectrum_free( vertex_source );
    return 1;
  }

  vertex_shader = glCreateShader( GL_VERTEX_SHADER );
  fragment_shader = glCreateShader( GL_FRAGMENT_SHADER );
  *program = glCreateProgram();

  vertex_sources[0] = vertex_source;
  fragment_sources[0] = fragment_source;
  glShaderSource( vertex_shader, 1, vertex_sources, NULL );
  glShaderSource( fragment_shader, 1, fragment_sources, NULL );
  glCompileShader( vertex_shader );
  if( sdlglsl_check_shader( vertex_shader, "vertex", error_text ) ) goto error;
  glCompileShader( fragment_shader );
  if( sdlglsl_check_shader( fragment_shader, "fragment", error_text ) ) goto error;

  glAttachShader( *program, vertex_shader );
  glAttachShader( *program, fragment_shader );

  attribute_location = 0;
  glBindAttribLocation( *program, attribute_location++, "VertexCoord" );
  glBindAttribLocation( *program, attribute_location++, "TexCoord" );
  glBindAttribLocation( *program, attribute_location, "COLOR" );

  glLinkProgram( *program );
  if( sdlglsl_check_program( *program, error_text ) ) goto error;

  glDeleteShader( vertex_shader );
  glDeleteShader( fragment_shader );
  libspectrum_free( vertex_source );
  libspectrum_free( fragment_source );
  return 0;

error:
  if( vertex_shader ) glDeleteShader( vertex_shader );
  if( fragment_shader ) glDeleteShader( fragment_shader );
  if( *program ) {
    glDeleteProgram( *program );
    *program = 0;
  }
  libspectrum_free( vertex_source );
  libspectrum_free( fragment_source );
  return 1;
}

static void
sdlglsl_set_uniform_int( GLuint program, const char *name, int value )
{
  GLint location = glGetUniformLocation( program, name );
  if( location >= 0 ) glUniform1i( location, value );
}

static void
sdlglsl_set_uniform_float( GLuint program, const char *name, float value )
{
  GLint location = glGetUniformLocation( program, name );
  if( location >= 0 ) glUniform1f( location, value );
}

static void
sdlglsl_set_uniform_vec2( GLuint program, const char *name, float x, float y )
{
  GLint location = glGetUniformLocation( program, name );
  if( location >= 0 ) glUniform2f( location, x, y );
}

static void
sdlglsl_set_uniform_mat4( GLuint program, const char *name,
                          const float *matrix )
{
  GLint location = glGetUniformLocation( program, name );
  if( location >= 0 ) glUniformMatrix4fv( location, 1, GL_FALSE, matrix );
}

static int
sdlglsl_find_parameter_index( const sdlshader_parameter *parameters,
                              size_t parameter_count, const char *name )
{
  size_t i;

  for( i = 0; i < parameter_count; i++ ) {
    if( !strcmp( parameters[i].name, name ) ) return (int)i;
  }

  return -1;
}

static int
sdlglsl_append_parameter( sdlshader_parameter **parameters,
                          size_t *parameter_count,
                          const sdlshader_parameter *parameter,
                          char **error_text )
{
  sdlshader_parameter *new_parameters;
  size_t new_count;

  new_count = *parameter_count + 1;
  new_parameters = realloc( *parameters, new_count * sizeof( *new_parameters ) );
  if( !new_parameters ) {
    sdlglsl_set_error( error_text, "could not allocate shader parameter state" );
    return 1;
  }

  *parameters = new_parameters;
  sdlshader_parameter_init( &(*parameters)[ *parameter_count ] );
  if( sdlshader_parameter_copy( &(*parameters)[ *parameter_count ], parameter ) ) {
    sdlglsl_set_error( error_text, "could not allocate shader parameter state" );
    return 1;
  }
  *parameter_count = new_count;

  return 0;
}

static int
sdlglsl_collect_parameters_from_source( sdlglsl_backend *backend,
                                        const sdlshader_source *shader_source,
                                        char **error_text )
{
  size_t i;

  for( i = 0; i < shader_source->parameter_count; i++ ) {
    if( sdlglsl_find_parameter_index( backend->parameters,
                                      backend->parameter_count,
                                      shader_source->parameters[i].name ) >= 0 ) {
      continue;
    }

    if( sdlglsl_append_parameter( &backend->parameters,
                                  &backend->parameter_count,
                                  &shader_source->parameters[i],
                                  error_text ) ) {
      return 1;
    }
  }

  return 0;
}

static int
sdlglsl_apply_preset_parameter_overrides( sdlglsl_backend *backend,
                                          const sdlshader_preset *preset,
                                          char **error_text )
{
  size_t i;

  for( i = 0; i < preset->parameter_count; i++ ) {
    int parameter_index = sdlglsl_find_parameter_index( backend->parameters,
                                                        backend->parameter_count,
                                                        preset->parameters[i].name );

    if( !preset->parameters[i].has_value ) continue;

    if( parameter_index < 0 ) {
      sdlshader_parameter parameter;

      if( sdlshader_parameter_copy( &parameter, &preset->parameters[i] ) ) {
        sdlglsl_set_error( error_text, "could not allocate shader parameter state" );
        return 1;
      }
      parameter.default_value = parameter.initial_value;
      parameter.has_preset_value = 1;
      parameter.preset_value = parameter.initial_value;

      if( sdlglsl_append_parameter( &backend->parameters,
                                    &backend->parameter_count,
                                    &parameter,
                                    error_text ) ) {
        sdlshader_parameter_free( &parameter );
        return 1;
      }
      sdlshader_parameter_free( &parameter );
    } else {
      backend->parameters[ parameter_index ].has_preset_value = 1;
      backend->parameters[ parameter_index ].preset_value =
        preset->parameters[i].initial_value;
      backend->parameters[ parameter_index ].initial_value =
        preset->parameters[i].initial_value;
    }
  }

  return 0;
}

static void
sdlglsl_configure_texture_filter( unsigned int texture, int filter_linear,
                                  int use_mipmaps )
{
  GLint min_filter, mag_filter;

  min_filter = filter_linear ? GL_LINEAR : GL_NEAREST;
  mag_filter = filter_linear ? GL_LINEAR : GL_NEAREST;

  if( use_mipmaps ) {
    min_filter = filter_linear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST;
  }

  glBindTexture( GL_TEXTURE_2D, texture );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter );
}

#if USE_LIBPNG
static int
sdlglsl_load_png_pixels( const char *path, unsigned char **pixels,
                         int *width, int *height, char **error_text )
{
  FILE *file = NULL;
  png_structp png_ptr = NULL;
  png_infop info_ptr = NULL;
  png_bytep *rows = NULL;
  unsigned char *image = NULL;
  png_uint_32 png_width, png_height;
  int rowbytes;
  int y;

  *pixels = NULL;
  *width = 0;
  *height = 0;

  file = fopen( path, "rb" );
  if( !file ) {
    sdlglsl_set_error( error_text, "could not open auxiliary texture %s", path );
    return 1;
  }

  png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
  info_ptr = png_ptr ? png_create_info_struct( png_ptr ) : NULL;
  if( !png_ptr || !info_ptr ) {
    sdlglsl_set_error( error_text,
                       "could not allocate libpng state for auxiliary texture" );
    goto error;
  }

  if( setjmp( png_jmpbuf( png_ptr ) ) ) {
    sdlglsl_set_error( error_text, "could not decode auxiliary PNG texture %s", path );
    goto error;
  }

  png_init_io( png_ptr, file );
  png_read_info( png_ptr, info_ptr );
  png_get_IHDR( png_ptr, info_ptr, &png_width, &png_height, NULL, NULL,
                NULL, NULL, NULL );

  if( png_width == 0 || png_height == 0 ) {
    sdlglsl_set_error( error_text, "auxiliary texture %s has invalid dimensions", path );
    goto error;
  }

  if( png_get_bit_depth( png_ptr, info_ptr ) == 16 ) png_set_strip_16( png_ptr );
  if( png_get_color_type( png_ptr, info_ptr ) == PNG_COLOR_TYPE_PALETTE ) {
    png_set_palette_to_rgb( png_ptr );
  }
  if( png_get_color_type( png_ptr, info_ptr ) == PNG_COLOR_TYPE_GRAY &&
      png_get_bit_depth( png_ptr, info_ptr ) < 8 ) {
    png_set_expand_gray_1_2_4_to_8( png_ptr );
  }
  if( png_get_valid( png_ptr, info_ptr, PNG_INFO_tRNS ) ) {
    png_set_tRNS_to_alpha( png_ptr );
  }
  if( png_get_color_type( png_ptr, info_ptr ) == PNG_COLOR_TYPE_GRAY ||
      png_get_color_type( png_ptr, info_ptr ) == PNG_COLOR_TYPE_GRAY_ALPHA ) {
    png_set_gray_to_rgb( png_ptr );
  }
  if( !( png_get_color_type( png_ptr, info_ptr ) & PNG_COLOR_MASK_ALPHA ) ) {
    png_set_filler( png_ptr, 0xff, PNG_FILLER_AFTER );
  }

  png_read_update_info( png_ptr, info_ptr );

  rowbytes = png_get_rowbytes( png_ptr, info_ptr );
  image = libspectrum_new( unsigned char, rowbytes * png_height );
  rows = libspectrum_new( png_bytep, png_height );

  for( y = 0; y < (int)png_height; y++ ) {
    rows[y] = image + y * rowbytes;
  }

  png_read_image( png_ptr, rows );
  png_read_end( png_ptr, NULL );

  *pixels = image;
  *width = (int)png_width;
  *height = (int)png_height;

  libspectrum_free( rows );
  png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
  fclose( file );
  return 0;

error:
  if( rows ) libspectrum_free( rows );
  if( image ) libspectrum_free( image );
  if( png_ptr || info_ptr ) png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
  if( file ) fclose( file );
  return 1;
}
#endif

static int
sdlglsl_load_auxiliary_texture( const sdlshader_preset_texture *preset_texture,
                                sdlglsl_texture *texture,
                                char **error_text )
{
#if !USE_LIBPNG
  (void)preset_texture;
  (void)texture;
  sdlglsl_set_error( error_text,
                     "preset auxiliary textures require libpng support in this build" );
  return 1;
#else
  unsigned char *pixels = NULL;
  int width = 0, height = 0;
  const char *extension;

  extension = strrchr( preset_texture->path, '.' );
  if( !extension || strcasecmp( extension, ".png" ) ) {
    sdlglsl_set_error( error_text,
                       "unsupported auxiliary texture format for %s",
                       preset_texture->path );
    return 1;
  }

  if( sdlglsl_load_png_pixels( preset_texture->path, &pixels, &width, &height,
                               error_text ) ) {
    return 1;
  }

  texture->name = utils_safe_strdup( preset_texture->name );
  texture->width = width;
  texture->height = height;
  texture->texture = 0;

  glGenTextures( 1, &texture->texture );
  glBindTexture( GL_TEXTURE_2D, texture->texture );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
  glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
  glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
                GL_UNSIGNED_BYTE, pixels );
  glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );

  libspectrum_free( pixels );

  if( sdlglsl_check_gl_error( "OpenGL auxiliary texture upload", error_text ) ) {
    if( texture->texture ) {
      glDeleteTextures( 1, &texture->texture );
      texture->texture = 0;
    }
    libspectrum_free( texture->name );
    texture->name = NULL;
    texture->width = 0;
    texture->height = 0;
    return 1;
  }

  return 0;
#endif
}

static void
sdlglsl_bind_uniforms( GLuint program,
                       const sdlglsl_texture *textures,
                       size_t texture_count,
                       const sdlshader_parameter *parameters,
                       size_t parameter_count,
                       int frame_count, int original_width,
                       int original_height, int input_width, int input_height,
                       int output_width, int output_height )
{
  static const float identity_matrix[16] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
  };
  size_t i;
  static const char *history_sampler_names[ SDLGLSL_HISTORY_TEXTURE_COUNT ] = {
    "PrevTexture",
    "Prev1Texture",
    "Prev2Texture",
    "Prev3Texture",
    "Prev4Texture",
    "Prev5Texture",
    "Prev6Texture"
  };

  sdlglsl_set_uniform_mat4( program, "MVPMatrix", identity_matrix );
  sdlglsl_set_uniform_vec2( program, "OutputSize",
                            (float)output_width, (float)output_height );
  sdlglsl_set_uniform_vec2( program, "TextureSize",
                            (float)input_width, (float)input_height );
  sdlglsl_set_uniform_vec2( program, "InputSize",
                            (float)input_width, (float)input_height );
  sdlglsl_set_uniform_vec2( program, "OrigTextureSize",
                            (float)original_width, (float)original_height );
  sdlglsl_set_uniform_int( program, "FrameDirection", 1 );
  sdlglsl_set_uniform_int( program, "FrameCount", frame_count );
  sdlglsl_set_uniform_int( program, "Texture", 0 );
  sdlglsl_set_uniform_int( program, "source", 0 );

  for( i = 0; i < texture_count; i++ ) {
    sdlglsl_set_uniform_int( program, textures[i].name, (int)i + 1 );
  }

  for( i = 0; i < SDLGLSL_HISTORY_TEXTURE_COUNT; i++ ) {
    int texture_unit = (int)texture_count + 1 + (int)i;

    sdlglsl_set_uniform_int( program, history_sampler_names[i], texture_unit );
  }

  for( i = 0; i < parameter_count; i++ ) {
    sdlglsl_set_uniform_float( program,
                               parameters[i].name,
                               parameters[i].initial_value );
  }
}

static void
sdlglsl_pass_init( sdlglsl_pass *pass )
{
  pass->program = 0;
  pass->texture = 0;
  pass->framebuffer = 0;
  pass->texture_width = 0;
  pass->texture_height = 0;
  pass->filter_linear = 0;
  pass->mipmap_input = 0;
  pass->texture_filter_linear = -1;
  pass->scale_type_x = SDLSHADER_PASS_SCALE_SOURCE;
  pass->scale_type_y = SDLSHADER_PASS_SCALE_SOURCE;
  pass->scale_x = 1.0f;
  pass->scale_y = 1.0f;
  sdlshader_source_init( &pass->shader_source );
}

static void
sdlglsl_pass_free( sdlglsl_pass *pass )
{
  if( pass->framebuffer ) {
    glDeleteFramebuffers( 1, &pass->framebuffer );
    pass->framebuffer = 0;
  }

  if( pass->texture ) {
    glDeleteTextures( 1, &pass->texture );
    pass->texture = 0;
  }

  if( pass->program ) {
    glDeleteProgram( pass->program );
    pass->program = 0;
  }

  sdlshader_source_free( &pass->shader_source );
  pass->texture_width = 0;
  pass->texture_height = 0;
  pass->texture_filter_linear = -1;
}

static void
sdlglsl_history_texture_free( sdlglsl_history_texture *texture )
{
  if( texture->texture ) {
    glDeleteTextures( 1, &texture->texture );
    texture->texture = 0;
  }

  texture->width = 0;
  texture->height = 0;
}

static int
sdlglsl_ensure_history_textures( sdlglsl_backend *backend, int texture_width,
                                 int texture_height, char **error_text )
{
  unsigned char *blank = NULL;
  size_t pixel_count;
  size_t i;

  if( texture_width <= 0 || texture_height <= 0 ) return 1;

  if( backend->history_textures[0].texture &&
      backend->history_textures[0].width == texture_width &&
      backend->history_textures[0].height == texture_height ) {
    return 0;
  }

  for( i = 0; i < SDLGLSL_HISTORY_TEXTURE_COUNT; i++ ) {
    sdlglsl_history_texture_free( &backend->history_textures[i] );
  }

  pixel_count = (size_t)texture_width * (size_t)texture_height;
  blank = calloc( pixel_count, 4 );
  if( !blank ) {
    sdlglsl_set_error( error_text,
                       "could not allocate previous-frame texture storage" );
    return 1;
  }

  for( i = 0; i < SDLGLSL_HISTORY_TEXTURE_COUNT; i++ ) {
    glGenTextures( 1, &backend->history_textures[i].texture );
    glBindTexture( GL_TEXTURE_2D, backend->history_textures[i].texture );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, texture_width, texture_height, 0,
                  GL_RGBA, GL_UNSIGNED_BYTE, blank );
    backend->history_textures[i].width = texture_width;
    backend->history_textures[i].height = texture_height;
  }

  free( blank );

  backend->history_index = 0;
  backend->history_valid_count = 0;

  return sdlglsl_check_gl_error( "OpenGL previous-frame texture setup",
                                 error_text );
}

static int
sdlglsl_capture_history_frame( sdlglsl_backend *backend, int texture_width,
                               int texture_height, char **error_text )
{
  int next_index;

  if( sdlglsl_ensure_history_textures( backend, texture_width, texture_height,
                                       error_text ) ) {
    return 1;
  }

  next_index = ( backend->history_index + 1 ) % SDLGLSL_HISTORY_TEXTURE_COUNT;

  glBindFramebuffer( GL_FRAMEBUFFER, 0 );
  glReadBuffer( GL_BACK );
  glBindTexture( GL_TEXTURE_2D, backend->history_textures[ next_index ].texture );
  glCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, texture_width,
                       texture_height );

  backend->history_index = next_index;
  if( backend->history_valid_count < SDLGLSL_HISTORY_TEXTURE_COUNT ) {
    backend->history_valid_count++;
  }

  return sdlglsl_check_gl_error( "OpenGL previous-frame capture", error_text );
}

static int
sdlglsl_create_pass_target( sdlglsl_pass *pass, int texture_width,
                            int texture_height, int filter_linear,
                            char **error_text )
{
  GLenum status;

  pass->texture_width = texture_width;
  pass->texture_height = texture_height;
  pass->texture_filter_linear = filter_linear;

  glGenTextures( 1, &pass->texture );
  sdlglsl_configure_texture_filter( pass->texture, filter_linear, 0 );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
  glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, texture_width, texture_height, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, NULL );

  glGenFramebuffers( 1, &pass->framebuffer );
  glBindFramebuffer( GL_FRAMEBUFFER, pass->framebuffer );
  glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                          GL_TEXTURE_2D, pass->texture, 0 );

  status = glCheckFramebufferStatus( GL_FRAMEBUFFER );
  glBindFramebuffer( GL_FRAMEBUFFER, 0 );

  if( status != GL_FRAMEBUFFER_COMPLETE ) {
    sdlglsl_set_error( error_text,
                       "OpenGL framebuffer setup failed with status 0x%04x",
                       (unsigned int)status );
    return 1;
  }

  return 0;
}

static int
sdlglsl_ensure_pass_target( sdlglsl_pass *pass, int texture_width,
                            int texture_height, int filter_linear,
                            char **error_text )
{
  if( pass->texture && pass->framebuffer &&
      pass->texture_width == texture_width &&
      pass->texture_height == texture_height &&
      pass->texture_filter_linear == filter_linear ) {
    return 0;
  }

  if( pass->framebuffer ) {
    glDeleteFramebuffers( 1, &pass->framebuffer );
    pass->framebuffer = 0;
  }

  if( pass->texture ) {
    glDeleteTextures( 1, &pass->texture );
    pass->texture = 0;
  }

  return sdlglsl_create_pass_target( pass, texture_width, texture_height,
                                     filter_linear, error_text );
}

static int
sdlglsl_draw_pass( sdlglsl_backend *backend, sdlglsl_pass *pass,
                   unsigned int input_texture, int input_width,
                   int input_height, int framebuffer, int viewport_x,
                   int viewport_y, int viewport_width, int viewport_height,
                   int window_height, int flip_texcoords,
                   char **error_text )
{
  static const GLfloat vertices[] = {
    -1.0f, -1.0f, 0.0f, 1.0f,
     1.0f, -1.0f, 0.0f, 1.0f,
    -1.0f,  1.0f, 0.0f, 1.0f,
     1.0f,  1.0f, 0.0f, 1.0f,
  };
  static const GLfloat flipped_texcoords[] = {
    0.0f, 1.0f, 0.0f, 1.0f,
    1.0f, 1.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 0.0f, 1.0f,
    1.0f, 0.0f, 0.0f, 1.0f,
  };
  static const GLfloat texcoords[] = {
    0.0f, 0.0f, 0.0f, 1.0f,
    1.0f, 0.0f, 0.0f, 1.0f,
    0.0f, 1.0f, 0.0f, 1.0f,
    1.0f, 1.0f, 0.0f, 1.0f,
  };
  static const GLfloat colours[] = {
    1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f,
  };
  GLint vertex_location, texcoord_location, colour_location;

  glBindFramebuffer( GL_FRAMEBUFFER, framebuffer );
  if( framebuffer ) {
    glViewport( viewport_x, viewport_y, viewport_width, viewport_height );
  } else {
    glViewport( viewport_x,
                window_height - viewport_y - viewport_height,
                viewport_width, viewport_height );
  }

  glUseProgram( pass->program );
  glActiveTexture( GL_TEXTURE0 );
  glBindTexture( GL_TEXTURE_2D, input_texture );
  for( size_t texture_index = 0; texture_index < backend->texture_count;
       texture_index++ ) {
    glActiveTexture( GL_TEXTURE0 + (GLenum)texture_index + 1 );
    glBindTexture( GL_TEXTURE_2D, backend->textures[ texture_index ].texture );
  }
  for( size_t history_index = 0; history_index < SDLGLSL_HISTORY_TEXTURE_COUNT;
       history_index++ ) {
    glActiveTexture( GL_TEXTURE0 + (GLenum)backend->texture_count +
                     (GLenum)history_index + 1 );
    glBindTexture( GL_TEXTURE_2D,
                   backend->history_textures[ history_index ].texture );
  }
  glActiveTexture( GL_TEXTURE0 );
  sdlglsl_bind_uniforms( pass->program,
                         backend->textures, backend->texture_count,
                         backend->parameters, backend->parameter_count,
                         backend->frame_count,
                         backend->texture_width, backend->texture_height,
                         input_width, input_height,
                         viewport_width, viewport_height );

  vertex_location = glGetAttribLocation( pass->program, "VertexCoord" );
  texcoord_location = glGetAttribLocation( pass->program, "TexCoord" );
  colour_location = glGetAttribLocation( pass->program, "COLOR" );

  if( vertex_location >= 0 ) {
    glEnableVertexAttribArray( vertex_location );
    glVertexAttribPointer( vertex_location, 4, GL_FLOAT, GL_FALSE, 0, vertices );
  }

  if( texcoord_location >= 0 ) {
    glEnableVertexAttribArray( texcoord_location );
    glVertexAttribPointer( texcoord_location, 4, GL_FLOAT, GL_FALSE, 0,
                           flip_texcoords ? flipped_texcoords : texcoords );
  }

  if( colour_location >= 0 ) {
    glEnableVertexAttribArray( colour_location );
    glVertexAttribPointer( colour_location, 4, GL_FLOAT, GL_FALSE, 0, colours );
  }

  glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE );
  glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );
  glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );

  if( vertex_location >= 0 ) glDisableVertexAttribArray( vertex_location );
  if( texcoord_location >= 0 ) glDisableVertexAttribArray( texcoord_location );
  if( colour_location >= 0 ) glDisableVertexAttribArray( colour_location );

  glUseProgram( 0 );

  if( sdlglsl_check_gl_error( "OpenGL draw", error_text ) ) return 1;

  return 0;
}

#endif

int
sdlglsl_pass_uses_flipped_input( int pass_index )
{
  return pass_index == 0;
}

static int
sdlglsl_scale_dimension( sdlshader_pass_scale_type scale_type, float scale,
                         int source_dimension, int viewport_dimension )
{
  float size;

  switch( scale_type ) {
  case SDLSHADER_PASS_SCALE_ABSOLUTE:
    size = scale;
    break;
  case SDLSHADER_PASS_SCALE_VIEWPORT:
    size = viewport_dimension * scale;
    break;
  case SDLSHADER_PASS_SCALE_SOURCE:
  default:
    size = source_dimension * scale;
    break;
  }

  if( size < 1.0f ) return 1;

  return (int)( size + 0.5f );
}

int
sdlglsl_calculate_pass_size( sdlshader_pass_scale_type scale_type_x,
                             sdlshader_pass_scale_type scale_type_y,
                             float scale_x, float scale_y,
                             int source_width, int source_height,
                             int viewport_width, int viewport_height,
                             int *output_width, int *output_height )
{
  if( !output_width || !output_height ) return 1;
  if( source_width <= 0 || source_height <= 0 ) return 1;
  if( viewport_width <= 0 || viewport_height <= 0 ) return 1;
  if( scale_x <= 0.0f || scale_y <= 0.0f ) return 1;

  *output_width = sdlglsl_scale_dimension( scale_type_x, scale_x,
                                           source_width, viewport_width );
  *output_height = sdlglsl_scale_dimension( scale_type_y, scale_y,
                                            source_height, viewport_height );

  return 0;
}

void
sdlglsl_backend_init( sdlglsl_backend *backend )
{
  backend->active = 0;

#if HAVE_OPENGL
  backend->context = NULL;
  backend->input_texture = 0;
  backend->frame_count = 0;
  backend->has_uploaded_frame = 0;
  backend->texture_width = 0;
  backend->texture_height = 0;
  backend->history_index = 0;
  backend->history_valid_count = 0;
  backend->textures = NULL;
  backend->texture_count = 0;
  memset( backend->history_textures, 0, sizeof( backend->history_textures ) );
  backend->parameters = NULL;
  backend->parameter_count = 0;
  backend->pass_count = 0;
  backend->passes = NULL;
#endif
}

void
sdlglsl_backend_free( SDL_Window *window, sdlglsl_backend *backend )
{
#if HAVE_OPENGL
  int i;

  if( backend->context && window ) SDL_GL_MakeCurrent( window, backend->context );

  if( backend->input_texture ) {
    glDeleteTextures( 1, &backend->input_texture );
    backend->input_texture = 0;
  }

  for( i = 0; i < (int)backend->texture_count; i++ ) {
    if( backend->textures[i].texture ) {
      glDeleteTextures( 1, &backend->textures[i].texture );
    }
    if( backend->textures[i].name ) libspectrum_free( backend->textures[i].name );
  }
  free( backend->textures );
  backend->textures = NULL;
  backend->texture_count = 0;

  for( i = 0; i < SDLGLSL_HISTORY_TEXTURE_COUNT; i++ ) {
    sdlglsl_history_texture_free( &backend->history_textures[i] );
  }
  backend->history_index = 0;
  backend->history_valid_count = 0;

  for( i = 0; i < backend->pass_count; i++ ) {
    sdlglsl_pass_free( &backend->passes[i] );
  }
  free( backend->passes );
  backend->passes = NULL;
  backend->pass_count = 0;

  backend->texture_width = 0;
  backend->texture_height = 0;
  backend->has_uploaded_frame = 0;

  for( i = 0; i < (int)backend->parameter_count; i++ ) {
    sdlshader_parameter_free( &backend->parameters[i] );
  }
  free( backend->parameters );
  backend->parameters = NULL;
  backend->parameter_count = 0;

  if( backend->context ) {
    SDL_GL_DeleteContext( backend->context );
    backend->context = NULL;
  }
#else
  (void)window;
#endif

  backend->active = 0;
}

int
sdlglsl_backend_create( SDL_Window *window, int texture_width,
                        int texture_height,
                        const sdlshader_preset *preset,
                        sdlglsl_backend *backend,
                        char **error_text )
{
#if !HAVE_OPENGL
  (void)window;
  (void)texture_width;
  (void)texture_height;
  (void)preset;
  sdlglsl_set_error( error_text, "OpenGL support is not available in this build" );
  return 1;
#else
  if( error_text ) *error_text = NULL;

  backend->context = SDL_GL_CreateContext( window );
  if( !backend->context ) {
    sdlglsl_set_error( error_text, "could not create SDL OpenGL context: %s",
                       SDL_GetError() );
    return 1;
  }

  if( sdlglsl_make_current( window, backend, error_text ) ) goto error;

  sdlglsl_debug_log( "GL_VENDOR=%s", glGetString( GL_VENDOR ) );
  sdlglsl_debug_log( "GL_RENDERER=%s", glGetString( GL_RENDERER ) );
  sdlglsl_debug_log( "GL_VERSION=%s", glGetString( GL_VERSION ) );
  sdlglsl_debug_log( "GLSL_VERSION=%s",
                     glGetString( GL_SHADING_LANGUAGE_VERSION ) );

  backend->passes = calloc( preset->shader_pass_count, sizeof( *backend->passes ) );
  if( !backend->passes ) {
    sdlglsl_set_error( error_text,
                       "could not allocate OpenGL shader pass state" );
    goto error;
  }

  backend->pass_count = preset->shader_pass_count;
  for( int i = 0; i < backend->pass_count; i++ ) {
    sdlglsl_pass_init( &backend->passes[i] );
    backend->passes[i].filter_linear = preset->passes[i].filter_linear;
    backend->passes[i].mipmap_input = preset->passes[i].mipmap_input;
    backend->passes[i].scale_type_x = preset->passes[i].scale_type_x;
    backend->passes[i].scale_type_y = preset->passes[i].scale_type_y;
    backend->passes[i].scale_x = preset->passes[i].scale_x;
    backend->passes[i].scale_y = preset->passes[i].scale_y;

    if( sdlshader_source_load( preset->passes[i].shader_path,
                               &backend->passes[i].shader_source,
                               error_text ) ) {
      goto error;
    }

    if( sdlglsl_collect_parameters_from_source( backend,
                                                &backend->passes[i].shader_source,
                                                error_text ) ) {
      goto error;
    }

    if( sdlglsl_compile_program( &backend->passes[i].shader_source,
                                 &backend->passes[i].program,
                                 error_text ) ) {
      goto error;
    }
  }

  if( sdlglsl_apply_preset_parameter_overrides( backend, preset, error_text ) ) {
    goto error;
  }

  if( preset->texture_count ) {
    backend->textures = calloc( preset->texture_count, sizeof( *backend->textures ) );
    if( !backend->textures ) {
      sdlglsl_set_error( error_text,
                         "could not allocate OpenGL auxiliary texture state" );
      goto error;
    }

    backend->texture_count = preset->texture_count;
    for( size_t i = 0; i < backend->texture_count; i++ ) {
      if( sdlglsl_load_auxiliary_texture( &preset->textures[i],
                                          &backend->textures[i],
                                          error_text ) ) {
        goto error;
      }
    }
  }

  glGenTextures( 1, &backend->input_texture );
  sdlglsl_configure_texture_filter( backend->input_texture,
                                    preset->passes[0].filter_linear,
                                    preset->passes[0].mipmap_input );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
  if( sdlglsl_check_gl_error( "OpenGL texture setup", error_text ) ) goto error;

  backend->frame_count = 0;
  backend->has_uploaded_frame = 0;
  backend->texture_width = texture_width;
  backend->texture_height = texture_height;
  backend->active = 1;

  return 0;

error:
  sdlglsl_backend_free( window, backend );
  return 1;
#endif
}

int
sdlglsl_backend_upload( SDL_Window *window, sdlglsl_backend *backend,
                        SDL_Surface *surface, char **error_text )
{
#if !HAVE_OPENGL
  (void)window;
  (void)backend;
  (void)surface;
  sdlglsl_set_error( error_text, "OpenGL support is not available in this build" );
  return 1;
#else
  if( sdlglsl_make_current( window, backend, error_text ) ) return 1;

  glActiveTexture( GL_TEXTURE0 );
  sdlglsl_configure_texture_filter( backend->input_texture,
                                    backend->pass_count ? backend->passes[0].filter_linear : 0,
                                    backend->pass_count ? backend->passes[0].mipmap_input : 0 );
  glPixelStorei( GL_UNPACK_ALIGNMENT, 2 );
  glPixelStorei( GL_UNPACK_ROW_LENGTH,
                 surface->pitch / SDL_BYTESPERPIXEL( surface->format ) );
  glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, surface->w, surface->h, 0, GL_RGB,
                GL_UNSIGNED_SHORT_5_6_5, surface->pixels );
  if( backend->pass_count && backend->passes[0].mipmap_input ) {
    glGenerateMipmap( GL_TEXTURE_2D );
  }
  glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );

  if( sdlglsl_debug_enabled() ) {
    const unsigned short *pixels = surface->pixels;
    sdlglsl_debug_log( "upload surface=%dx%d pitch=%d first-pixels=%04x,%04x,%04x,%04x",
                       surface->w, surface->h, surface->pitch,
                       pixels[0], pixels[1], pixels[2], pixels[3] );
  }

  backend->has_uploaded_frame = 1;

  if( sdlglsl_check_gl_error( "OpenGL texture upload", error_text ) ) return 1;

  return 0;
#endif
}

int
sdlglsl_backend_present( SDL_Window *window, sdlglsl_backend *backend,
                         int output_width, int output_height,
                         int viewport_x, int viewport_y,
                         int viewport_width, int viewport_height,
                         char **error_text )
{
#if !HAVE_OPENGL
  (void)window;
  (void)backend;
  (void)output_width;
  (void)output_height;
  (void)viewport_x;
  (void)viewport_y;
  (void)viewport_width;
  (void)viewport_height;
  sdlglsl_set_error( error_text, "OpenGL support is not available in this build" );
  return 1;
#else
  static const GLfloat vertices[] = {
    -1.0f, -1.0f, 0.0f, 1.0f,
     1.0f, -1.0f, 0.0f, 1.0f,
    -1.0f,  1.0f, 0.0f, 1.0f,
     1.0f,  1.0f, 0.0f, 1.0f,
  };
  static const GLfloat texcoords[] = {
    0.0f, 1.0f, 0.0f, 1.0f,
    1.0f, 1.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 0.0f, 1.0f,
    1.0f, 0.0f, 0.0f, 1.0f,
  };
  static const GLfloat colours[] = {
    1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f,
  };
  static int logged_readback;
  unsigned int input_texture;
  int input_width, input_height;
  int i;

  if( sdlglsl_make_current( window, backend, error_text ) ) return 1;
  if( !backend->has_uploaded_frame ) return 0;

  if( output_width <= 0 || output_height <= 0 ) {
    SDL_GetWindowSizeInPixels( window, &output_width, &output_height );
  }
  if( output_width <= 0 || output_height <= 0 ) return 0;

  if( sdlglsl_ensure_history_textures( backend, output_width, output_height,
                                       error_text ) ) {
    return 1;
  }

  glDisable( GL_DEPTH_TEST );
  glDisable( GL_BLEND );
  glEnable( GL_TEXTURE_2D );
  glViewport( 0, 0, output_width, output_height );
  glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
  glClear( GL_COLOR_BUFFER_BIT );

  input_texture = backend->input_texture;
  input_width = backend->texture_width;
  input_height = backend->texture_height;

  for( i = 0; i < backend->pass_count; i++ ) {
    int framebuffer = 0;
    int pass_viewport_x = viewport_x;
    int pass_viewport_y = viewport_y;
    int pass_viewport_width = viewport_width;
    int pass_viewport_height = viewport_height;

    glActiveTexture( GL_TEXTURE0 );
    sdlglsl_configure_texture_filter( input_texture,
                                      backend->passes[i].filter_linear,
                                      backend->passes[i].mipmap_input );
    if( backend->passes[i].mipmap_input ) {
      glGenerateMipmap( GL_TEXTURE_2D );
    }

    if( i + 1 < backend->pass_count ) {
      if( sdlglsl_calculate_pass_size( backend->passes[i].scale_type_x,
                                       backend->passes[i].scale_type_y,
                                       backend->passes[i].scale_x,
                                       backend->passes[i].scale_y,
                                       input_width, input_height,
                                       viewport_width, viewport_height,
                                       &pass_viewport_width,
                                       &pass_viewport_height ) ) {
        sdlglsl_set_error( error_text,
                           "could not calculate render target size for pass %d",
                           i );
        return 1;
      }

      if( sdlglsl_ensure_pass_target( &backend->passes[i],
                                      pass_viewport_width,
                                      pass_viewport_height,
                                      backend->passes[i + 1].filter_linear,
                                      error_text ) ) {
        return 1;
      }

      framebuffer = backend->passes[i].framebuffer;
      pass_viewport_x = 0;
      pass_viewport_y = 0;
      pass_viewport_width = backend->passes[i].texture_width;
      pass_viewport_height = backend->passes[i].texture_height;
      glBindFramebuffer( GL_FRAMEBUFFER, framebuffer );
      glClear( GL_COLOR_BUFFER_BIT );
    }

    if( sdlglsl_draw_pass( backend, &backend->passes[i], input_texture,
                           input_width, input_height, framebuffer,
                           pass_viewport_x, pass_viewport_y,
                           pass_viewport_width, pass_viewport_height,
                           output_height,
                           sdlglsl_pass_uses_flipped_input( i ),
                           error_text ) ) {
      return 1;
    }

    if( i + 1 < backend->pass_count ) {
      input_texture = backend->passes[i].texture;
      input_width = backend->passes[i].texture_width;
      input_height = backend->passes[i].texture_height;
    }
  }

  glBindFramebuffer( GL_FRAMEBUFFER, 0 );

  if( sdlglsl_debug_enabled() && !logged_readback ) {
    unsigned char pixel[4] = { 0, 0, 0, 0 };
    glReadPixels( output_width / 2, output_height / 2, 1, 1, GL_RGBA,
                  GL_UNSIGNED_BYTE, pixel );
    sdlglsl_debug_log( "readback center rgba=%u,%u,%u,%u",
                       pixel[0], pixel[1], pixel[2], pixel[3] );
    logged_readback = 1;
  }

  if( sdlglsl_capture_history_frame( backend, output_width, output_height,
                                     error_text ) ) {
    return 1;
  }

  SDL_GL_SwapWindow( window );
  backend->frame_count++;

  return 0;
#endif
}