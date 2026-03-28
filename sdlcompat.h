/* sdlcompat.h: temporary SDL3 compatibility shim for the SDL migration
   Copyright (c) 2026

   This file intentionally keeps the early SDL3 migration patches mechanical.
   It centralises the header-path switch and a small set of constant aliases so
   the backend files can move off legacy SDL includes before their runtime
   behaviour is fully ported.
*/

#ifndef FUSE_SDLCOMPAT_H
#define FUSE_SDLCOMPAT_H

#ifndef SDL_ENABLE_OLD_NAMES
#define SDL_ENABLE_OLD_NAMES
#endif

#include <SDL3/SDL.h>

#ifndef SDL_ENABLE
#define SDL_ENABLE 1
#endif

#ifndef SDL_DISABLE
#define SDL_DISABLE 0
#endif

#ifndef SDL_IGNORE
#define SDL_IGNORE 0
#endif

#ifndef SDL_SWSURFACE
#define SDL_SWSURFACE 0
#endif

#ifndef SDL_FULLSCREEN
#define SDL_FULLSCREEN SDL_WINDOW_FULLSCREEN
#endif

#ifndef SDL_NOFRAME
#define SDL_NOFRAME SDL_WINDOW_BORDERLESS
#endif

#ifndef SDL_SCALEMODE_PIXELART
#define SDL_SCALEMODE_PIXELART SDL_SCALEMODE_LINEAR
#endif

#ifndef SDL_GRAB_OFF
#define SDL_GRAB_OFF 0
#endif

#ifndef SDL_GRAB_ON
#define SDL_GRAB_ON 1
#endif

#ifndef SDL_GRAB_FULLSCREEN
#define SDL_GRAB_FULLSCREEN 2
#endif

#ifndef SDL_APPINPUTFOCUS
#define SDL_APPINPUTFOCUS 0x01
#endif

#ifndef SDL_DEFAULT_REPEAT_DELAY
#define SDL_DEFAULT_REPEAT_DELAY 500
#endif

#ifndef SDL_DEFAULT_REPEAT_INTERVAL
#define SDL_DEFAULT_REPEAT_INTERVAL 30
#endif

#ifndef SDLK_LSUPER
#define SDLK_LSUPER SDLK_LGUI
#endif

#ifndef SDLK_RSUPER
#define SDLK_RSUPER SDLK_RGUI
#endif

static inline int
fuse_sdl_enable_key_repeat( int delay, int interval )
{
   (void)delay;
   (void)interval;
   return 0;
}

static inline int
fuse_sdl_enable_unicode( int enable )
{
   (void)enable;
   return 0;
}

static inline bool
fuse_sdl_show_cursor_toggle( int toggle )
{
   return toggle ? SDL_ShowCursor() : SDL_HideCursor();
}

static inline bool
fuse_sdl_warp_mouse( float x, float y )
{
   SDL_Window *window;

   window = SDL_GetMouseFocus();
   if( !window ) window = SDL_GetKeyboardFocus();
   if( !window ) return false;

   SDL_WarpMouseInWindow( window, x, y );
   return true;
}

static inline int
fuse_sdl_wm_grab_input( int mode )
{
   SDL_Window *window;
   bool grabbed;

   window = SDL_GetMouseFocus();
   if( !window ) window = SDL_GetKeyboardFocus();
   if( !window ) return SDL_GRAB_OFF;

   grabbed = mode != SDL_GRAB_OFF;
   if( !SDL_SetWindowMouseGrab( window, grabbed ) ) return SDL_GRAB_OFF;
   if( !SDL_SetWindowKeyboardGrab( window, grabbed ) ) return SDL_GRAB_OFF;

   return mode;
}

static inline void
fuse_sdl_wm_set_caption( const char *title, const char *icon )
{
   SDL_Window *window;

   (void)icon;

   window = SDL_GetKeyboardFocus();
   if( !window ) window = SDL_GetMouseFocus();
   if( !window ) return;

   SDL_SetWindowTitle( window, title );
}

static inline int
fuse_sdl_num_joysticks( void )
{
   SDL_JoystickID *joysticks;
   int count;

   joysticks = SDL_GetJoysticks( &count );
   if( joysticks ) SDL_free( joysticks );

   return joysticks ? count : 0;
}

static inline SDL_Joystick*
fuse_sdl_joystick_open( int index )
{
   SDL_JoystickID *joysticks;
   SDL_Joystick *joystick;
   int count;

   joysticks = SDL_GetJoysticks( &count );
   if( !joysticks ) return NULL;
   if( index < 0 || index >= count ) {
      SDL_free( joysticks );
      return NULL;
   }

   joystick = SDL_OpenJoystick( joysticks[index] );
   SDL_free( joysticks );
   return joystick;
}

static inline int
fuse_sdl_joystick_event_state( int state )
{
   SDL_SetJoystickEventsEnabled( state != SDL_IGNORE );
   return SDL_JoystickEventsEnabled() ? SDL_ENABLE : SDL_IGNORE;
}

#ifndef AUDIO_S16SYS
#define AUDIO_S16SYS SDL_AUDIO_S16
#endif

#define SDL_EnableKeyRepeat(delay, interval) \
   fuse_sdl_enable_key_repeat( (delay), (interval) )
#define SDL_EnableUNICODE(enable) fuse_sdl_enable_unicode( (enable) )
#define SDL_ShowCursor(toggle) fuse_sdl_show_cursor_toggle( (toggle) )
#define SDL_WarpMouse(x, y) fuse_sdl_warp_mouse( (float)(x), (float)(y) )
#define SDL_WM_GrabInput(mode) fuse_sdl_wm_grab_input( (mode) )
#define SDL_WM_SetCaption(title, icon) fuse_sdl_wm_set_caption( (title), (icon) )
#define SDL_NumJoysticks() fuse_sdl_num_joysticks()
#ifdef SDL_JoystickOpen
#undef SDL_JoystickOpen
#endif
#define SDL_JoystickOpen(index) fuse_sdl_joystick_open( (index) )
#define SDL_JoystickEventState(state) fuse_sdl_joystick_event_state( (state) )

#endif