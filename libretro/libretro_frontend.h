#ifndef FUSE_LIBRETRO_FRONTEND_H
#define FUSE_LIBRETRO_FRONTEND_H

#include <stddef.h>
#include <stdint.h>

#include "libretro/libretro.h"

void libretro_frontend_set_environment( retro_environment_t cb );
void libretro_frontend_set_input_callbacks( retro_input_poll_t poll_cb,
                                            retro_input_state_t state_cb );
void libretro_frontend_set_controller_port_device( unsigned port,
                                                   unsigned device );
void libretro_frontend_capture_input( void );
void libretro_frontend_keyboard_event( bool down, unsigned keycode,
                                       uint32_t character,
                                       uint16_t key_modifiers );

int libretro_frontend_get_video_width( void );
int libretro_frontend_get_video_height( void );
const void *libretro_frontend_get_video_data( size_t *pitch );

const int16_t *libretro_frontend_get_audio_data( size_t *frames );
void libretro_frontend_clear_audio( void );
int libretro_frontend_get_audio_sample_rate( void );
int libretro_frontend_get_audio_channels( void );
void libretro_frontend_reset_runtime_state( void );

void libretro_log_message( enum retro_log_level level, const char *fmt, ... );

#endif
