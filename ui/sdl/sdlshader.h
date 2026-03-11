/* sdlshader.h: SDL shader preset parsing helpers
   Copyright (c) 2026

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
*/

#ifndef FUSE_SDLSHADER_H
#define FUSE_SDLSHADER_H

#include <stddef.h>

typedef struct sdlshader_parameter {

  char *name;
  char *label;
  float initial_value;
  float default_value;
  float minimum_value;
  float maximum_value;
  float step_value;
  int has_value;
  int has_metadata;
  int has_preset_value;
  float preset_value;

} sdlshader_parameter;

typedef struct sdlshader_preset_texture {

  char *name;
  char *path;

} sdlshader_preset_texture;

typedef struct sdlshader_source {

  char *path;
  char *source;
  sdlshader_parameter *parameters;
  size_t parameter_count;

} sdlshader_source;

typedef enum sdlshader_pass_scale_type {

  SDLSHADER_PASS_SCALE_SOURCE = 0,
  SDLSHADER_PASS_SCALE_VIEWPORT,
  SDLSHADER_PASS_SCALE_ABSOLUTE

} sdlshader_pass_scale_type;

typedef struct sdlshader_preset_pass {

  char *shader_path;
  int filter_linear;
  int mipmap_input;
  sdlshader_pass_scale_type scale_type_x;
  sdlshader_pass_scale_type scale_type_y;
  float scale_x;
  float scale_y;

} sdlshader_preset_pass;

typedef struct sdlshader_preset {

  char *preset_path;
  int shader_pass_count;
  sdlshader_preset_pass *passes;
  sdlshader_preset_texture *textures;
  size_t texture_count;
  sdlshader_parameter *parameters;
  size_t parameter_count;

} sdlshader_preset;

void sdlshader_parameter_init( sdlshader_parameter *parameter );
void sdlshader_parameter_free( sdlshader_parameter *parameter );
int sdlshader_parameter_copy( sdlshader_parameter *dest,
                              const sdlshader_parameter *src );

void sdlshader_preset_init( sdlshader_preset *preset );
void sdlshader_preset_free( sdlshader_preset *preset );

int sdlshader_preset_parse( const char *preset_path, const char *buffer,
                            sdlshader_preset *preset, char **error_text );
int sdlshader_preset_load( const char *preset_path, sdlshader_preset *preset,
                           char **error_text );

void sdlshader_source_init( sdlshader_source *source );
void sdlshader_source_free( sdlshader_source *source );

int sdlshader_source_load( const char *shader_path, sdlshader_source *source,
                           char **error_text );
int sdlshader_source_build_stage( const sdlshader_source *source,
                                  const char *stage_define,
                                  char **stage_source,
                                  char **error_text );

#endif