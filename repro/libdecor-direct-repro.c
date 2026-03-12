#include <stdio.h>

#include <libdecor.h>
#include <wayland-client.h>

static void
libdecor_repro_error( struct libdecor *context, enum libdecor_error error,
                      const char *message )
{
  (void)context;

  fprintf( stderr, "libdecor error %d: %s\n", error,
           message ? message : "(no message)" );
}

int
main( void )
{
  struct wl_display *display;
  struct libdecor *context;
  struct libdecor_interface iface = {
    .error = libdecor_repro_error,
  };

  display = wl_display_connect( NULL );
  if( !display ) {
    fprintf( stderr, "wl_display_connect failed\n" );
    return 1;
  }

  context = libdecor_new( display, &iface );
  if( !context ) {
    fprintf( stderr, "libdecor_new returned NULL\n" );
    wl_display_disconnect( display );
    return 1;
  }

  puts( "libdecor_new succeeded" );
  libdecor_unref( context );
  wl_display_disconnect( display );
  return 0;
}