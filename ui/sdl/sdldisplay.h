/* sdldisplay.h: Routines for dealing with the SDL display
   Copyright (c) 2000-2003 Philip Kendall, Fredrick Meunier

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

#ifndef FUSE_SDLDISPLAY_H
#define FUSE_SDLDISPLAY_H

#include <stddef.h>

#include "sdlcompat.h"

extern SDL_Surface *sdldisplay_gc;    /* Hardware screen */

typedef struct sdldisplay_shader_parameter_info {

   const char *name;
   const char *label;
   float value;
   float default_value;
   float preset_value;
   float minimum_value;
   float maximum_value;
   float step_value;
   int has_metadata;

} sdldisplay_shader_parameter_info;

size_t sdldisplay_shader_parameter_count( void );
int sdldisplay_shader_parameter_get_info( size_t index,
                                                               sdldisplay_shader_parameter_info *info );
int sdldisplay_shader_parameter_set( size_t index, float value );
int sdldisplay_shader_parameter_reset_to_default( size_t index );
int sdldisplay_shader_parameter_reset_to_preset( size_t index );

#endif			/* #ifndef FUSE_SDLDISPLAY_H */
