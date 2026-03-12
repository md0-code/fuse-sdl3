/* sdlshader.c: SDL shader preset parsing helpers
   Copyright (c) 2026

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
*/

#include <config.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_LIBGEN_H
#include <libgen.h>
#endif

#include <libspectrum.h>

#include "strings.h"
#include "compat.h"
#include "utils.h"

#include "sdlshader.h"

static void
sdlshader_set_error( char **error_text, const char *message )
{
  if( !error_text ) return;

  if( *error_text ) libspectrum_free( *error_text );
  *error_text = utils_safe_strdup( message );
}

static char *
sdlshader_dup_trimmed( const char *start, size_t length )
{
  char *result;

  while( length && isspace( (unsigned char)*start ) ) {
    start++;
    length--;
  }

  while( length && isspace( (unsigned char)start[ length - 1 ] ) ) {
    length--;
  }

  if( length >= 2 &&
      ( ( start[0] == '"' && start[ length - 1 ] == '"' ) ||
        ( start[0] == '\'' && start[ length - 1 ] == '\'' ) ) ) {
    start++;
    length -= 2;
  }

  result = libspectrum_new( char, length + 1 );
  memcpy( result, start, length );
  result[ length ] = '\0';

  return result;
}

void
sdlshader_parameter_init( sdlshader_parameter *parameter )
{
  parameter->name = NULL;
  parameter->label = NULL;
  parameter->initial_value = 0.0f;
  parameter->default_value = 0.0f;
  parameter->minimum_value = 0.0f;
  parameter->maximum_value = 0.0f;
  parameter->step_value = 0.0f;
  parameter->has_value = 0;
  parameter->has_metadata = 0;
  parameter->has_preset_value = 0;
  parameter->preset_value = 0.0f;
}

void
sdlshader_parameter_free( sdlshader_parameter *parameter )
{
  if( parameter->name ) libspectrum_free( parameter->name );
  if( parameter->label ) libspectrum_free( parameter->label );

  sdlshader_parameter_init( parameter );
}

int
sdlshader_parameter_copy( sdlshader_parameter *dest,
                          const sdlshader_parameter *src )
{
  sdlshader_parameter_init( dest );

  if( src->name ) dest->name = utils_safe_strdup( src->name );
  if( src->label ) dest->label = utils_safe_strdup( src->label );

  if( ( src->name && !dest->name ) || ( src->label && !dest->label ) ) {
    sdlshader_parameter_free( dest );
    return 1;
  }

  dest->initial_value = src->initial_value;
  dest->default_value = src->default_value;
  dest->minimum_value = src->minimum_value;
  dest->maximum_value = src->maximum_value;
  dest->step_value = src->step_value;
  dest->has_value = src->has_value;
  dest->has_metadata = src->has_metadata;
  dest->has_preset_value = src->has_preset_value;
  dest->preset_value = src->preset_value;

  return 0;
}

static char *
sdlshader_resolve_path( const char *base_path, const char *value )
{
  char *directory, *path_copy, *resolved;
  int length;

  if( !value || !*value ) return utils_safe_strdup( value );
  if( compat_is_absolute_path( value ) ) return utils_safe_strdup( value );

  path_copy = utils_safe_strdup( base_path );
  directory = dirname( path_copy );
  length = snprintf( NULL, 0, "%s%s%s", directory, FUSE_DIR_SEP_STR, value );
  resolved = libspectrum_new( char, length + 1 );
  snprintf( resolved, length + 1, "%s%s%s", directory, FUSE_DIR_SEP_STR,
            value );

  libspectrum_free( path_copy );

  return resolved;
}

static int
sdlshader_parse_boolean( const char *value )
{
  return !strcasecmp( value, "true" ) || !strcmp( value, "1" );
}

static int
sdlshader_parse_float( const char *value, float *result )
{
  char *end;

  errno = 0;
  *result = strtof( value, &end );

  if( errno || end == value ) return 1;
  while( *end && isspace( (unsigned char)*end ) ) end++;

  return *end ? 1 : 0;
}

static int
sdlshader_parse_scale_type( const char *value,
                            sdlshader_pass_scale_type *result )
{
  if( !strcasecmp( value, "source" ) ) {
    *result = SDLSHADER_PASS_SCALE_SOURCE;
    return 0;
  }

  if( !strcasecmp( value, "viewport" ) ) {
    *result = SDLSHADER_PASS_SCALE_VIEWPORT;
    return 0;
  }

  if( !strcasecmp( value, "absolute" ) ) {
    *result = SDLSHADER_PASS_SCALE_ABSOLUTE;
    return 0;
  }

  return 1;
}

static int
sdlshader_parse_pass_index( const char *key, const char *prefix )
{
  size_t prefix_length = strlen( prefix );
  char *end;
  long index;

  if( strncmp( key, prefix, prefix_length ) ) return -1;
  if( !key[ prefix_length ] ) return -1;

  index = strtol( key + prefix_length, &end, 10 );
  if( *end || index < 0 || index > 1024 ) return -1;

  return (int)index;
}

static int
sdlshader_find_parameter_index( const sdlshader_parameter *parameters,
                                size_t parameter_count, const char *name )
{
  size_t i;

  for( i = 0; i < parameter_count; i++ ) {
    if( !strcmp( parameters[i].name, name ) ) return (int)i;
  }

  return -1;
}

static int
sdlshader_find_texture_index( const sdlshader_preset_texture *textures,
                              size_t texture_count, const char *name )
{
  size_t i;

  for( i = 0; i < texture_count; i++ ) {
    if( !strcmp( textures[i].name, name ) ) return (int)i;
  }

  return -1;
}

static int
sdlshader_append_preset_parameter( sdlshader_preset *preset, const char *name )
{
  size_t new_count;
  sdlshader_parameter *new_parameters;

  if( sdlshader_find_parameter_index( preset->parameters,
                                      preset->parameter_count, name ) >= 0 ) {
    return 0;
  }

  new_count = preset->parameter_count + 1;
  new_parameters = realloc( preset->parameters,
                            new_count * sizeof( *new_parameters ) );
  if( !new_parameters ) return 1;

  preset->parameters = new_parameters;
  sdlshader_parameter_init( &preset->parameters[ preset->parameter_count ] );
  preset->parameters[ preset->parameter_count ].name = utils_safe_strdup( name );
  preset->parameter_count = new_count;

  return 0;
}

static int
sdlshader_append_preset_texture( sdlshader_preset *preset, const char *name )
{
  size_t new_count;
  sdlshader_preset_texture *new_textures;

  if( sdlshader_find_texture_index( preset->textures,
                                    preset->texture_count, name ) >= 0 ) {
    return 0;
  }

  new_count = preset->texture_count + 1;
  new_textures = realloc( preset->textures,
                          new_count * sizeof( *new_textures ) );
  if( !new_textures ) return 1;

  preset->textures = new_textures;
  preset->textures[ preset->texture_count ].name = utils_safe_strdup( name );
  preset->textures[ preset->texture_count ].path = NULL;
  preset->texture_count = new_count;

  return 0;
}

static int
sdlshader_parse_parameter_list( sdlshader_preset *preset, const char *value )
{
  const char *cursor = value;

  while( *cursor ) {
    const char *separator = strchr( cursor, ';' );
    size_t length = separator ? (size_t)( separator - cursor ) : strlen( cursor );
    char *name = sdlshader_dup_trimmed( cursor, length );
    int error = 0;

    if( *name ) error = sdlshader_append_preset_parameter( preset, name );

    libspectrum_free( name );
    if( error ) return 1;
    if( !separator ) break;
    cursor = separator + 1;
  }

  return 0;
}

static int
sdlshader_parse_texture_list( sdlshader_preset *preset, const char *value )
{
  const char *cursor = value;

  while( *cursor ) {
    const char *separator = strchr( cursor, ';' );
    size_t length = separator ? (size_t)( separator - cursor ) : strlen( cursor );
    char *name = sdlshader_dup_trimmed( cursor, length );
    int error = 0;

    if( *name ) error = sdlshader_append_preset_texture( preset, name );

    libspectrum_free( name );
    if( error ) return 1;
    if( !separator ) break;
    cursor = separator + 1;
  }

  return 0;
}

static int
sdlshader_ensure_pass_capacity( sdlshader_preset *preset, int pass_index )
{
  sdlshader_preset_pass *new_passes;
  int old_count, i;

  if( pass_index < preset->shader_pass_count ) return 0;

  old_count = preset->shader_pass_count;
  new_passes = realloc( preset->passes,
                        ( pass_index + 1 ) * sizeof( *new_passes ) );
  if( !new_passes ) return 1;

  preset->passes = new_passes;
  for( i = old_count; i <= pass_index; i++ ) {
    preset->passes[i].shader_path = NULL;
    preset->passes[i].filter_linear = 0;
    preset->passes[i].mipmap_input = 0;
    preset->passes[i].scale_type_x = SDLSHADER_PASS_SCALE_SOURCE;
    preset->passes[i].scale_type_y = SDLSHADER_PASS_SCALE_SOURCE;
    preset->passes[i].scale_x = 1.0f;
    preset->passes[i].scale_y = 1.0f;
  }

  preset->shader_pass_count = pass_index + 1;
  return 0;
}

static int
sdlshader_parse_parameter_line( const char *line, size_t length,
                                sdlshader_parameter *parameter )
{
  const char *cursor = line;
  const char *label_start;
  const char *label_end;
  char *end;
  float minimum_value, maximum_value, step_value;

  while( length && isspace( (unsigned char)*cursor ) ) {
    cursor++;
    length--;
  }

  if( length < 17 || strncmp( cursor, "#pragma parameter", 17 ) ) return 1;
  cursor += 17;

  while( isspace( (unsigned char)*cursor ) ) cursor++;

  end = (char *)cursor;
  while( *end && !isspace( (unsigned char)*end ) ) end++;
  parameter->name = sdlshader_dup_trimmed( cursor, end - cursor );

  label_start = strchr( end, '"' );
  if( !label_start ) return 1;
  label_end = strchr( label_start + 1, '"' );
  if( !label_end ) return 1;
  parameter->label = sdlshader_dup_trimmed( label_start + 1,
                                            label_end - label_start - 1 );
  cursor = label_end + 1;

  while( isspace( (unsigned char)*cursor ) ) cursor++;

  errno = 0;
  parameter->initial_value = strtof( cursor, &end );
  parameter->default_value = parameter->initial_value;
  parameter->has_value = 1;

  if( errno || end == cursor ) return 1;

  cursor = end;
  while( isspace( (unsigned char)*cursor ) ) cursor++;

  if( *cursor ) {
    errno = 0;
    minimum_value = strtof( cursor, &end );
    if( !errno && end != cursor ) {
      cursor = end;
      while( isspace( (unsigned char)*cursor ) ) cursor++;

      errno = 0;
      maximum_value = strtof( cursor, &end );
      if( !errno && end != cursor ) {
        cursor = end;
        while( isspace( (unsigned char)*cursor ) ) cursor++;

        errno = 0;
        step_value = strtof( cursor, &end );
        if( !errno && end != cursor ) {
          parameter->minimum_value = minimum_value;
          parameter->maximum_value = maximum_value;
          parameter->step_value = step_value;
          parameter->has_metadata = 1;
        }
      }
    }
  }

  return 0;
}

static int
sdlshader_append_parameter( sdlshader_source *source,
                            sdlshader_parameter *parameter )
{
  size_t new_count = source->parameter_count + 1;
  sdlshader_parameter *new_parameters;

  new_parameters = realloc( source->parameters,
                            new_count * sizeof( *new_parameters ) );
  if( !new_parameters ) return 1;

  source->parameters = new_parameters;
  sdlshader_parameter_init( &source->parameters[ source->parameter_count ] );
  source->parameters[ source->parameter_count ] = *parameter;
  source->parameter_count = new_count;

  return 0;
}

static int
sdlshader_insert_defines( const char *source, const char *stage_define,
                          char **stage_source )
{
  const char *version_end;
  const char *defines = "#define PARAMETER_UNIFORM\n";
  int prefix_length, defines_length, total_length;

  prefix_length = 0;
  version_end = NULL;
  if( !strncmp( source, "#version", 8 ) ) {
    version_end = strchr( source, '\n' );
    prefix_length = version_end ? ( version_end - source + 1 ) : strlen( source );
  }

  defines_length = snprintf( NULL, 0, "#define %s\n%s", stage_define,
                             defines );
  total_length = prefix_length + defines_length + strlen( source + prefix_length );
  *stage_source = libspectrum_new( char, total_length + 1 );

  memcpy( *stage_source, source, prefix_length );
  snprintf( *stage_source + prefix_length, defines_length + 1,
            "#define %s\n%s", stage_define, defines );
  strcpy( *stage_source + prefix_length + defines_length,
          source + prefix_length );

  return 0;
}

void
sdlshader_preset_init( sdlshader_preset *preset )
{
  preset->preset_path = NULL;
  preset->shader_pass_count = 0;
  preset->passes = NULL;
  preset->textures = NULL;
  preset->texture_count = 0;
  preset->parameters = NULL;
  preset->parameter_count = 0;
}

void
sdlshader_preset_free( sdlshader_preset *preset )
{
  int i;

  if( preset->preset_path ) libspectrum_free( preset->preset_path );

  for( i = 0; i < preset->shader_pass_count; i++ ) {
    if( preset->passes[i].shader_path ) {
      libspectrum_free( preset->passes[i].shader_path );
    }
  }
  free( preset->passes );

  for( i = 0; i < (int)preset->texture_count; i++ ) {
    if( preset->textures[i].name ) libspectrum_free( preset->textures[i].name );
    if( preset->textures[i].path ) libspectrum_free( preset->textures[i].path );
  }
  free( preset->textures );

  for( i = 0; i < (int)preset->parameter_count; i++ ) {
    sdlshader_parameter_free( &preset->parameters[i] );
  }
  free( preset->parameters );

  sdlshader_preset_init( preset );
}

int
sdlshader_preset_parse( const char *preset_path, const char *buffer,
                        sdlshader_preset *preset, char **error_text )
{
  const char *cursor, *line_end;
  int declared_pass_count = -1;

  if( error_text ) *error_text = NULL;

  sdlshader_preset_free( preset );
  preset->preset_path = utils_safe_strdup( preset_path );

  cursor = buffer;
  while( *cursor ) {
    const char *line = cursor;
    const char *equals;
    char *key, *value;

    line_end = strchr( cursor, '\n' );
    if( !line_end ) line_end = cursor + strlen( cursor );
    cursor = *line_end ? line_end + 1 : line_end;

    while( line < line_end && isspace( (unsigned char)*line ) ) line++;
    if( line == line_end || *line == '#' || *line == ';' ) continue;
    if( line_end - line >= 2 && line[0] == '/' && line[1] == '/' ) continue;

    equals = memchr( line, '=', line_end - line );
    if( !equals ) continue;

    key = sdlshader_dup_trimmed( line, equals - line );
    value = sdlshader_dup_trimmed( equals + 1, line_end - equals - 1 );

    if( !strcmp( key, "shaders" ) ) {
      declared_pass_count = atoi( value );
    } else if( !strcmp( key, "textures" ) ) {
      if( sdlshader_parse_texture_list( preset, value ) ) {
        libspectrum_free( key );
        libspectrum_free( value );
        sdlshader_set_error( error_text,
                             "could not store preset texture metadata" );
        sdlshader_preset_free( preset );
        return 1;
      }
    } else if( !strcmp( key, "parameters" ) ) {
      if( sdlshader_parse_parameter_list( preset, value ) ) {
        libspectrum_free( key );
        libspectrum_free( value );
        sdlshader_set_error( error_text,
                             "could not store preset parameter metadata" );
        sdlshader_preset_free( preset );
        return 1;
      }
    } else {
      int pass_index = sdlshader_parse_pass_index( key, "shader" );

      if( pass_index >= 0 ) {
        if( sdlshader_ensure_pass_capacity( preset, pass_index ) ) {
          libspectrum_free( key );
          libspectrum_free( value );
          sdlshader_set_error( error_text,
                               "could not store preset pass definitions" );
          sdlshader_preset_free( preset );
          return 1;
        }

        if( preset->passes[ pass_index ].shader_path ) {
          libspectrum_free( preset->passes[ pass_index ].shader_path );
        }
        preset->passes[ pass_index ].shader_path =
          sdlshader_resolve_path( preset_path, value );
      } else {
        pass_index = sdlshader_parse_pass_index( key, "filter_linear" );
        if( pass_index >= 0 ) {
          if( sdlshader_ensure_pass_capacity( preset, pass_index ) ) {
            libspectrum_free( key );
            libspectrum_free( value );
            sdlshader_set_error( error_text,
                                 "could not store preset pass filters" );
            sdlshader_preset_free( preset );
            return 1;
          }

          preset->passes[ pass_index ].filter_linear =
            sdlshader_parse_boolean( value );
        } else {
          pass_index = sdlshader_parse_pass_index( key, "mipmap_input" );
          if( pass_index >= 0 ) {
            if( sdlshader_ensure_pass_capacity( preset, pass_index ) ) {
              libspectrum_free( key );
              libspectrum_free( value );
              sdlshader_set_error( error_text,
                                   "could not store preset pass mipmap flags" );
              sdlshader_preset_free( preset );
              return 1;
            }

            preset->passes[ pass_index ].mipmap_input =
              sdlshader_parse_boolean( value );
          } else {
          pass_index = sdlshader_parse_pass_index( key, "scale_type_x" );
          if( pass_index >= 0 ) {
            if( sdlshader_ensure_pass_capacity( preset, pass_index ) ) {
              libspectrum_free( key );
              libspectrum_free( value );
              sdlshader_set_error( error_text,
                                   "could not store preset pass scale types" );
              sdlshader_preset_free( preset );
              return 1;
            }

            if( sdlshader_parse_scale_type( value,
                                            &preset->passes[ pass_index ].scale_type_x ) ) {
              libspectrum_free( key );
              libspectrum_free( value );
              sdlshader_set_error( error_text,
                                   "preset uses an unsupported scale_type_x value" );
              sdlshader_preset_free( preset );
              return 1;
            }
          } else {
            pass_index = sdlshader_parse_pass_index( key, "scale_type_y" );
            if( pass_index >= 0 ) {
              if( sdlshader_ensure_pass_capacity( preset, pass_index ) ) {
                libspectrum_free( key );
                libspectrum_free( value );
                sdlshader_set_error( error_text,
                                     "could not store preset pass scale types" );
                sdlshader_preset_free( preset );
                return 1;
              }

              if( sdlshader_parse_scale_type( value,
                                              &preset->passes[ pass_index ].scale_type_y ) ) {
                libspectrum_free( key );
                libspectrum_free( value );
                sdlshader_set_error( error_text,
                                     "preset uses an unsupported scale_type_y value" );
                sdlshader_preset_free( preset );
                return 1;
              }
            } else {
              pass_index = sdlshader_parse_pass_index( key, "scale_type" );
              if( pass_index >= 0 ) {
                sdlshader_pass_scale_type scale_type;

                if( sdlshader_ensure_pass_capacity( preset, pass_index ) ) {
                  libspectrum_free( key );
                  libspectrum_free( value );
                  sdlshader_set_error( error_text,
                                       "could not store preset pass scale types" );
                  sdlshader_preset_free( preset );
                  return 1;
                }

                if( sdlshader_parse_scale_type( value, &scale_type ) ) {
                  libspectrum_free( key );
                  libspectrum_free( value );
                  sdlshader_set_error( error_text,
                                       "preset uses an unsupported scale_type value" );
                  sdlshader_preset_free( preset );
                  return 1;
                }

                preset->passes[ pass_index ].scale_type_x = scale_type;
                preset->passes[ pass_index ].scale_type_y = scale_type;
              } else {
                pass_index = sdlshader_parse_pass_index( key, "scale_x" );
                if( pass_index >= 0 ) {
                  float scale_value;

                  if( sdlshader_ensure_pass_capacity( preset, pass_index ) ) {
                    libspectrum_free( key );
                    libspectrum_free( value );
                    sdlshader_set_error( error_text,
                                         "could not store preset pass scale values" );
                    sdlshader_preset_free( preset );
                    return 1;
                  }

                  if( sdlshader_parse_float( value, &scale_value ) ||
                      scale_value <= 0.0f ) {
                    libspectrum_free( key );
                    libspectrum_free( value );
                    sdlshader_set_error( error_text,
                                         "preset uses an invalid scale_x value" );
                    sdlshader_preset_free( preset );
                    return 1;
                  }

                  preset->passes[ pass_index ].scale_x = scale_value;
                } else {
                  pass_index = sdlshader_parse_pass_index( key, "scale_y" );
                  if( pass_index >= 0 ) {
                    float scale_value;

                    if( sdlshader_ensure_pass_capacity( preset, pass_index ) ) {
                      libspectrum_free( key );
                      libspectrum_free( value );
                      sdlshader_set_error( error_text,
                                           "could not store preset pass scale values" );
                      sdlshader_preset_free( preset );
                      return 1;
                    }

                    if( sdlshader_parse_float( value, &scale_value ) ||
                        scale_value <= 0.0f ) {
                      libspectrum_free( key );
                      libspectrum_free( value );
                      sdlshader_set_error( error_text,
                                           "preset uses an invalid scale_y value" );
                      sdlshader_preset_free( preset );
                      return 1;
                    }

                    preset->passes[ pass_index ].scale_y = scale_value;
                  } else {
                    pass_index = sdlshader_parse_pass_index( key, "scale" );
                    if( pass_index >= 0 ) {
                      float scale_value;

                      if( sdlshader_ensure_pass_capacity( preset, pass_index ) ) {
                        libspectrum_free( key );
                        libspectrum_free( value );
                        sdlshader_set_error( error_text,
                                             "could not store preset pass scale values" );
                        sdlshader_preset_free( preset );
                        return 1;
                      }

                      if( sdlshader_parse_float( value, &scale_value ) ||
                          scale_value <= 0.0f ) {
                        libspectrum_free( key );
                        libspectrum_free( value );
                        sdlshader_set_error( error_text,
                                             "preset uses an invalid scale value" );
                        sdlshader_preset_free( preset );
                        return 1;
                      }

                      preset->passes[ pass_index ].scale_x = scale_value;
                      preset->passes[ pass_index ].scale_y = scale_value;
                    }
                  }
                }
              }
            }
          }
          }
        }

        if( pass_index < 0 ) {
          int texture_index = sdlshader_find_texture_index( preset->textures,
                                                            preset->texture_count,
                                                            key );
          if( texture_index >= 0 ) {
            if( preset->textures[ texture_index ].path ) {
              libspectrum_free( preset->textures[ texture_index ].path );
            }

            preset->textures[ texture_index ].path =
              sdlshader_resolve_path( preset_path, value );
          } else {
            int parameter_index = sdlshader_find_parameter_index( preset->parameters,
                                                                  preset->parameter_count,
                                                                  key );
            if( parameter_index >= 0 ) {
              float parameter_value;

              if( sdlshader_parse_float( value, &parameter_value ) ) {
                libspectrum_free( key );
                libspectrum_free( value );
                sdlshader_set_error( error_text,
                                     "preset uses an invalid parameter value" );
                sdlshader_preset_free( preset );
                return 1;
              }

              preset->parameters[ parameter_index ].initial_value = parameter_value;
              preset->parameters[ parameter_index ].has_value = 1;
            }
          }
        }
      }
    }

    libspectrum_free( key );
    libspectrum_free( value );
  }

  if( declared_pass_count <= 0 ) {
    sdlshader_set_error( error_text,
                         "preset does not declare a positive shaders count" );
    sdlshader_preset_free( preset );
    return 1;
  }

  if( preset->shader_pass_count < declared_pass_count ) {
    if( sdlshader_ensure_pass_capacity( preset, declared_pass_count - 1 ) ) {
      sdlshader_set_error( error_text,
                           "could not finalise preset pass storage" );
      sdlshader_preset_free( preset );
      return 1;
    }
  }

  preset->shader_pass_count = declared_pass_count;

  {
    int i;

    for( i = 0; i < preset->shader_pass_count; i++ ) {
      if( !preset->passes[i].shader_path || !*preset->passes[i].shader_path ) {
        char message[64];

        snprintf( message, sizeof( message ),
                  "preset does not define shader%d", i );
        sdlshader_set_error( error_text, message );
        sdlshader_preset_free( preset );
        return 1;
      }
    }

    for( i = 0; i < (int)preset->texture_count; i++ ) {
      if( !preset->textures[i].path || !*preset->textures[i].path ) {
        char message[96];

        snprintf( message, sizeof( message ),
                  "preset does not define texture path for %s",
                  preset->textures[i].name );
        sdlshader_set_error( error_text, message );
        sdlshader_preset_free( preset );
        return 1;
      }
    }
  }

  return 0;
}

int
sdlshader_preset_load( const char *preset_path, sdlshader_preset *preset,
                       char **error_text )
{
  const char *extension;
  utils_file file;
  char *buffer;
  int error;

  if( error_text ) *error_text = NULL;

  extension = strrchr( preset_path, '.' );
  if( !extension || strcmp( extension, ".glslp" ) ) {
    sdlshader_set_error( error_text,
                         "startup shader presets must use the .glslp extension" );
    return 1;
  }

  error = utils_read_file( preset_path, &file );
  if( error ) {
    sdlshader_set_error( error_text, "could not read preset file" );
    return 1;
  }

  buffer = libspectrum_new( char, file.length + 1 );
  memcpy( buffer, file.buffer, file.length );
  buffer[ file.length ] = '\0';

  error = sdlshader_preset_parse( preset_path, buffer, preset, error_text );

  libspectrum_free( buffer );
  utils_close_file( &file );

  if( error ) return error;

  {
    int i;

    for( i = 0; i < preset->shader_pass_count; i++ ) {
      if( !compat_file_exists( preset->passes[i].shader_path ) ) {
        char message[80];

        snprintf( message, sizeof( message ),
                  "preset shader%d file could not be found", i );
        sdlshader_set_error( error_text, message );
        sdlshader_preset_free( preset );
        return 1;
      }
    }

    for( i = 0; i < (int)preset->texture_count; i++ ) {
      if( !compat_file_exists( preset->textures[i].path ) ) {
        char message[96];

        snprintf( message, sizeof( message ),
                  "preset texture %s file could not be found",
                  preset->textures[i].name );
        sdlshader_set_error( error_text, message );
        sdlshader_preset_free( preset );
        return 1;
      }
    }
  }

  return 0;
}

void
sdlshader_source_init( sdlshader_source *source )
{
  source->path = NULL;
  source->source = NULL;
  source->parameters = NULL;
  source->parameter_count = 0;
}

void
sdlshader_source_free( sdlshader_source *source )
{
  size_t i;

  if( source->path ) libspectrum_free( source->path );
  if( source->source ) libspectrum_free( source->source );

  for( i = 0; i < source->parameter_count; i++ ) {
    sdlshader_parameter_free( &source->parameters[i] );
  }

  free( source->parameters );

  sdlshader_source_init( source );
}

int
sdlshader_source_load( const char *shader_path, sdlshader_source *source,
                       char **error_text )
{
  const char *cursor, *line_end;
  utils_file file;
  int error;

  if( error_text ) *error_text = NULL;

  sdlshader_source_free( source );
  source->path = utils_safe_strdup( shader_path );

  error = utils_read_file( shader_path, &file );
  if( error ) {
    sdlshader_set_error( error_text, "could not read GLSL shader file" );
    sdlshader_source_free( source );
    return 1;
  }

  source->source = libspectrum_new( char, file.length + 1 );
  memcpy( source->source, file.buffer, file.length );
  source->source[ file.length ] = '\0';
  utils_close_file( &file );

  cursor = source->source;
  while( *cursor ) {
    sdlshader_parameter parameter;

    line_end = strchr( cursor, '\n' );
    if( !line_end ) line_end = cursor + strlen( cursor );

    sdlshader_parameter_init( &parameter );

    if( !sdlshader_parse_parameter_line( cursor, line_end - cursor,
                                         &parameter ) ) {
      if( sdlshader_append_parameter( source, &parameter ) ) {
      sdlshader_parameter_free( &parameter );
        sdlshader_set_error( error_text,
                             "could not store GLSL shader parameter metadata" );
        sdlshader_source_free( source );
        return 1;
      }
    }

    cursor = *line_end ? line_end + 1 : line_end;
  }

  return 0;
}

int
sdlshader_source_build_stage( const sdlshader_source *source,
                              const char *stage_define,
                              char **stage_source,
                              char **error_text )
{
  if( error_text ) *error_text = NULL;

  if( !source->source ) {
    sdlshader_set_error( error_text, "shader source is not loaded" );
    return 1;
  }

  return sdlshader_insert_defines( source->source, stage_define,
                                   stage_source );
}