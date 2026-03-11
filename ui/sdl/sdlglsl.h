/* sdlglsl.h: OpenGL backend helpers for RetroArch GLSL presets
   Copyright (c) 2026

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
*/

#ifndef FUSE_SDLGLSL_H
#define FUSE_SDLGLSL_H

#include "sdlcompat.h"

#include "sdlshader.h"

typedef struct sdlglsl_pass {

  unsigned int program;
  unsigned int texture;
  unsigned int framebuffer;
  int texture_width;
  int texture_height;
  int filter_linear;
  int mipmap_input;
  int texture_filter_linear;
  sdlshader_pass_scale_type scale_type_x;
  sdlshader_pass_scale_type scale_type_y;
  float scale_x;
  float scale_y;
  sdlshader_source shader_source;

} sdlglsl_pass;

typedef struct sdlglsl_texture {

  char *name;
  unsigned int texture;
  int width;
  int height;

} sdlglsl_texture;

typedef struct sdlglsl_history_texture {

  unsigned int texture;
  int width;
  int height;

} sdlglsl_history_texture;

typedef struct sdlglsl_backend {

  int active;

#if HAVE_OPENGL
#define SDLGLSL_HISTORY_TEXTURE_COUNT 7
  SDL_GLContext context;
  unsigned int input_texture;
  int frame_count;
  int has_uploaded_frame;
  int texture_width;
  int texture_height;
  int history_index;
  int history_valid_count;
  sdlglsl_texture *textures;
  size_t texture_count;
  sdlglsl_history_texture history_textures[ SDLGLSL_HISTORY_TEXTURE_COUNT ];
  sdlshader_parameter *parameters;
  size_t parameter_count;
  int pass_count;
  sdlglsl_pass *passes;
#endif

} sdlglsl_backend;

int sdlglsl_pass_uses_flipped_input( int pass_index );
int sdlglsl_calculate_pass_size( sdlshader_pass_scale_type scale_type_x,
                                 sdlshader_pass_scale_type scale_type_y,
                                 float scale_x, float scale_y,
                                 int source_width, int source_height,
                                 int viewport_width, int viewport_height,
                                 int *output_width, int *output_height );

void sdlglsl_backend_init( sdlglsl_backend *backend );
void sdlglsl_backend_free( SDL_Window *window, sdlglsl_backend *backend );

int sdlglsl_backend_create( SDL_Window *window, int texture_width,
                            int texture_height,
                            const sdlshader_preset *preset,
                            sdlglsl_backend *backend,
                            char **error_text );
int sdlglsl_backend_upload( SDL_Window *window, sdlglsl_backend *backend,
                            SDL_Surface *surface, char **error_text );
int sdlglsl_backend_present( SDL_Window *window, sdlglsl_backend *backend,
                             int output_width, int output_height,
                             int viewport_x, int viewport_y,
                             int viewport_width, int viewport_height,
                             char **error_text );

#endif