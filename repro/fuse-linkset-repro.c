#include <stdio.h>

#include <dlfcn.h>

#include <SDL3/SDL.h>

typedef struct preload_library_t {
  const char *soname;
  void *handle;
} preload_library_t;

static int
preload_library( preload_library_t *library )
{
  library->handle = dlopen( library->soname, RTLD_NOW | RTLD_GLOBAL );
  if( !library->handle ) {
    fprintf( stderr, "dlopen(%s) failed: %s\n", library->soname, dlerror() );
    return 1;
  }

  return 0;
}

static void
close_library( preload_library_t *library )
{
  if( library->handle ) dlclose( library->handle );
}

int
main( void )
{
  SDL_Window *window;
  size_t i;
  preload_library_t libraries[] = {
    { "libspectrum.so.9", NULL },
    { "libxml2.so.16", NULL },
    { "libpng16.so.16", NULL },
    { "libOpenGL.so.0", NULL },
  };

  for( i = 0; i < SDL_arraysize( libraries ); i++ ) {
    if( preload_library( &libraries[i] ) ) {
      while( i-- > 0 ) close_library( &libraries[i] );
      return 1;
    }
  }

  if( !SDL_Init( SDL_INIT_VIDEO ) ) {
    fprintf( stderr, "SDL_Init(SDL_INIT_VIDEO) failed: %s\n", SDL_GetError() );
    for( i = SDL_arraysize( libraries ); i-- > 0; ) close_library( &libraries[i] );
    return 1;
  }

  window = SDL_CreateWindow( "fuse-linkset-repro", 320, 240, 0 );
  if( !window ) {
    fprintf( stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError() );
    SDL_Quit();
    for( i = SDL_arraysize( libraries ); i-- > 0; ) close_library( &libraries[i] );
    return 1;
  }

  printf( "SDL video driver: %s\n", SDL_GetCurrentVideoDriver() );
  SDL_Delay( 100 );
  SDL_DestroyWindow( window );
  SDL_Quit();

  for( i = SDL_arraysize( libraries ); i-- > 0; ) close_library( &libraries[i] );

  return 0;
}