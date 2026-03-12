#include <stdio.h>

#include <SDL3/SDL.h>

int
main( void )
{
  SDL_Window *window;

  if( !SDL_Init( SDL_INIT_VIDEO ) ) {
    fprintf( stderr, "SDL_Init(SDL_INIT_VIDEO) failed: %s\n", SDL_GetError() );
    return 1;
  }

  window = SDL_CreateWindow( "libdecor-wayland-repro", 320, 240, 0 );
  if( !window ) {
    fprintf( stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError() );
    SDL_Quit();
    return 1;
  }

  printf( "SDL video driver: %s\n", SDL_GetCurrentVideoDriver() );
  SDL_Delay( 100 );
  SDL_DestroyWindow( window );
  SDL_Quit();
  return 0;
}