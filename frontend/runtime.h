#ifndef FUSE_FRONTEND_RUNTIME_H
#define FUSE_FRONTEND_RUNTIME_H

#include <stddef.h>

#include <libspectrum.h>

int fuse_runtime_init( const char *program_name );
int fuse_runtime_run_frame( void );
int fuse_runtime_load_file( const char *filename );
int fuse_runtime_reset( int hard_reset );
int fuse_runtime_select_machine( const char *id );
void fuse_runtime_refresh_display( void );
int fuse_runtime_shutdown( void );

#endif
