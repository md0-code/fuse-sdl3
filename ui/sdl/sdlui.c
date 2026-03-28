/* sdlui.c: Routines for dealing with the SDL user interface
   Copyright (c) 2000-2002 Philip Kendall, Matan Ziv-Av, Fredrick Meunier
   Copyright (c) 2015 Stuart Brady

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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if defined( __linux__ )
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "sdlcompat.h"

#include "display.h"
#include "fuse.h"
#include "ui/ui.h"
#include "ui/uidisplay.h"
#include "settings.h"
#include "sdldisplay.h"
#include "sdljoystick.h"
#include "sdlkeyboard.h"
#include "ui/scaler/scaler.h"
#include "menu.h"

#if defined( __linux__ )
static int
sdlui_env_enabled( const char *name )
{
  const char *value = getenv( name );

  return value && value[0] && strcmp( value, "0" );
}

static int
sdlui_wayland_requested( const char **video_driver_out )
{
  const char *legacy_video_driver = getenv( "SDL_VIDEO_DRIVER" );
  const char *video_driver = getenv( "SDL_VIDEODRIVER" );
  const char *wayland_display = getenv( "WAYLAND_DISPLAY" );

  if( !video_driver && legacy_video_driver && legacy_video_driver[0] ) {
    if( setenv( "SDL_VIDEODRIVER", legacy_video_driver, 0 ) ) {
      fprintf( stderr,
               "%s: warning: couldn't map SDL_VIDEO_DRIVER to SDL_VIDEODRIVER\n",
               fuse_progname );
    } else {
      video_driver = getenv( "SDL_VIDEODRIVER" );
    }
  }

  if( video_driver_out ) *video_driver_out = video_driver;

  if( video_driver ) return !strcmp( video_driver, "wayland" );

  return wayland_display && wayland_display[0];
}

static int
sdlui_probe_wayland_libdecor_crash( void )
{
  /* Note: The original crash probe was unreliable due to environment
     differences between parent and child processes. For now, we use
     a simplified approach that assumes Wayland has compatibility issues
     and automatically switches to X11 when available. */
  return 1;  /* Always assume crash risk on Wayland */
}

static void
sdlui_configure_video_environment( void )
{
  const char *video_driver = getenv( "SDL_VIDEODRIVER" );
  const char *display = getenv( "DISPLAY" );

  if( !sdlui_wayland_requested( &video_driver ) ) return;

  if( getenv( "LIBDECOR_PLUGIN_DIR" ) ||
      sdlui_env_enabled( "FUSE_SDL_DISABLE_LIBDECOR_WORKAROUND" ) ) {
    return;
  }

  /* Apply X11 workaround when Wayland is detected to avoid libdecor crashes */
  if( display && display[0] ) {
    if( setenv( "SDL_VIDEODRIVER", "x11", 1 ) ) {
      fprintf( stderr,
               "%s: warning: couldn't switch SDL video backend to x11 to avoid known Wayland libdecor crashes\n",
               fuse_progname );
      return;
    }

    fprintf( stderr,
             "%s: automatically switching SDL video backend to x11 to avoid known Wayland libdecor crashes\n",
             fuse_progname );
    return;
  }

  /* No X11 available, try disabling libdecor plugins as fallback */
  if( setenv( "LIBDECOR_PLUGIN_DIR", "/nonexistent", 0 ) ) {
    fprintf( stderr,
             "%s: warning: couldn't disable libdecor plugins to avoid Wayland crashes\n",
             fuse_progname );
    return;
  }

  fprintf( stderr,
           "%s: warning: no X11 display available; disabled libdecor plugins to reduce Wayland crash risk\n",
           fuse_progname );
}
#else
static void
sdlui_configure_video_environment( void )
{
}
#endif

static void
atexit_proc( void )
{ 
  SDL_ShowCursor( SDL_ENABLE );
  SDL_Quit();
}

int 
ui_init( int *argc, char ***argv )
{
  if( ui_widget_init() ) return 1;

/* Comment out to Work around a bug in OS X 10.1 related to OpenGL in windowed
   mode */
  atexit(atexit_proc);

  sdlui_configure_video_environment();

  if( !SDL_Init( SDL_INIT_VIDEO ) ) {
    ui_error( UI_ERROR_ERROR, "failed to initialise SDL video subsystem: %s",
              SDL_GetError() );
    return 1;
  }

#ifndef __MORPHOS__
  SDL_EnableUNICODE( 1 );
#endif				/* #ifndef __MORPHOS__ */

  sdlkeyboard_init();

  ui_mouse_present = 1;

  return 0;
}

int 
ui_event( void )
{
  SDL_Event event;

  while ( SDL_PollEvent( &event ) ) {
    switch ( event.type ) {
    case SDL_EVENT_KEY_DOWN:
      sdlkeyboard_keypress( &(event.key) );
      break;
    case SDL_EVENT_KEY_UP:
      sdlkeyboard_keyrelease( &(event.key) );
      break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
      ui_mouse_button( event.button.button, 1 );
      break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
      ui_mouse_button( event.button.button, 0 );
      break;
    case SDL_EVENT_MOUSE_MOTION:
      if( ui_mouse_grabbed ) {
        ui_mouse_motion( event.motion.x - 128, event.motion.y - 128 );
        if( event.motion.x != 128 || event.motion.y != 128 )
          SDL_WarpMouse( 128, 128 );
      }	
      break;

#if USE_JOYSTICK && !HAVE_JSW_H

    case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
      sdljoystick_buttonpress( &(event.jbutton) );
      break;
    case SDL_EVENT_JOYSTICK_BUTTON_UP:
      sdljoystick_buttonrelease( &(event.jbutton) );
      break;
    case SDL_EVENT_JOYSTICK_AXIS_MOTION:
      sdljoystick_axismove( &(event.jaxis) );
      break;
    case SDL_EVENT_JOYSTICK_HAT_MOTION:
      sdljoystick_hatmove( &(event.jhat) );
      break;

#endif			/* if USE_JOYSTICK && !HAVE_JSW_H */

    case SDL_EVENT_QUIT:
      fuse_emulation_pause();
      menu_file_exit(0);
      fuse_emulation_unpause();
      break;
    case SDL_EVENT_WINDOW_EXPOSED:
      display_refresh_all();
      break;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
      ui_mouse_resume();
      break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
      ui_mouse_suspend();
      break;
    default:
      break;
    }
  }

  return 0;
}

int 
ui_end( void )
{
  int error;

  error = uidisplay_end();
  if ( error )
    return error;

  sdlkeyboard_end();

  SDL_Quit();

  ui_widget_end();

  return 0;
}

int
ui_statusbar_update_speed( float speed )
{
  char buffer[24];
  const char fuse[] = "Fuse SDL3";

  snprintf( buffer, sizeof( buffer ), "%s - %3.0f%%", fuse, speed );

  /* FIXME: Icon caption should be snapshot name? */
  SDL_WM_SetCaption( buffer, fuse );

  return 0;
}

int
ui_mouse_grab( int startup )
{
  if( settings_current.full_screen ) {
    SDL_WarpMouse( 128, 128 );
    return 1;
  }
  if( startup ) return 0;

  switch( SDL_WM_GrabInput( SDL_GRAB_ON ) ) {
  case SDL_GRAB_ON:
  case SDL_GRAB_FULLSCREEN:
    SDL_ShowCursor( SDL_DISABLE );
    SDL_WarpMouse( 128, 128 );
    return 1;
  default:
    ui_error( UI_ERROR_WARNING, "Mouse grab failed" );
    return 0;
  }
}

int
ui_mouse_release( int suspend )
{
  if( settings_current.full_screen ) return !suspend;

  SDL_WM_GrabInput( SDL_GRAB_OFF );
  SDL_ShowCursor( SDL_ENABLE );
  return 0;
}
