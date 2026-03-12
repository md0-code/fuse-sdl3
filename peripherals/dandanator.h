/* dandanator.h: Dandanator cartridge handling routines */

#ifndef FUSE_DANDANATOR_H
#define FUSE_DANDANATOR_H

#include <libspectrum.h>

extern int dandanator_active;

void dandanator_register_startup( void );
int dandanator_available( void );
int dandanator_detect_buffer( const libspectrum_byte *buffer, size_t length );
int dandanator_insert( const char *filename );
void dandanator_eject( void );
void dandanator_pre_opcode( libspectrum_word pc, libspectrum_byte opcode );
void dandanator_memory_read( libspectrum_word address );
void dandanator_memory_write( libspectrum_word address );

#endif			/* #ifndef FUSE_DANDANATOR_H */