/* about.c: about dialog box
   Copyright (c) 2016 Sergio Baldoví

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

   E-mail: serbalgi@gmail.com

*/

#include <config.h>

#include <stdio.h>

#include "display.h"
#include "widget.h"
#include "widget_internals.h"

int
widget_about_draw( void *data GCC_UNUSED )
{
  char buffer[80];
  static const char display_url[] = "github.com/md0-code/fuse-sdl3";
  int dialog_cols, dialog_left_edge_x, string_width, x, line;

  dialog_cols = 35;
  dialog_left_edge_x =
    ( DISPLAY_SCREEN_WIDTH_COLS - dialog_cols + 1 ) / 2 -
    DISPLAY_BORDER_WIDTH_COLS;
  line = 0;

  widget_dialog_with_border( dialog_left_edge_x, 2, dialog_cols, 7+2 );
  widget_printstring( dialog_left_edge_x * 8 + 2, 16, WIDGET_COLOUR_TITLE,
                      "About " FUSE_DOWNSTREAM_NAME );

  string_width = widget_stringwidth( "The Free Unix Spectrum Emulator" );
  x = dialog_left_edge_x * 8 + ( dialog_cols * 8 - string_width ) / 2;
  widget_printstring( x, ++line * 8 + 24, WIDGET_COLOUR_FOREGROUND,
                      "The Free Unix Spectrum Emulator" );

  snprintf( buffer, 80, "%s %s", FUSE_DOWNSTREAM_NAME, VERSION );
  string_width = widget_stringwidth( buffer );
  x = dialog_left_edge_x * 8 + ( dialog_cols * 8 - string_width ) / 2;
  widget_printstring( x, ++line * 8 + 24, WIDGET_COLOUR_FOREGROUND, buffer );

  ++line;

  string_width = widget_stringwidth( FUSE_COPYRIGHT );
  x = dialog_left_edge_x * 8 + ( dialog_cols * 8 - string_width ) / 2;
  widget_printstring( x, ++line * 8 + 24, WIDGET_COLOUR_FOREGROUND,
                      FUSE_COPYRIGHT );

  ++line;

  string_width = widget_stringwidth( display_url );
  x = dialog_left_edge_x * 8 + ( dialog_cols * 8 - string_width ) / 2;
  widget_printstring( x, ++line * 8 + 24, 0x09, display_url );

  widget_display_lines( 2, line + 3 );

  return 0;
}

void
widget_about_keyhandler( input_key key )
{
  switch( key ) {

  case INPUT_KEY_Escape:
  case INPUT_JOYSTICK_FIRE_2:
    widget_end_widget( WIDGET_FINISHED_CANCEL );
    return;

  case INPUT_KEY_Return:
  case INPUT_KEY_KP_Enter:
  case INPUT_JOYSTICK_FIRE_1:
    widget_end_widget( WIDGET_FINISHED_OK );
    return;

  default:	/* Keep gcc happy */
    break;

  }
}
