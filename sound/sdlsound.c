/* sdlsound.c: SDL sound I/O
   Copyright (c) 2002-2015 Alexander Yurchenko, Russell Marks, Philip Kendall,
			   Fredrick Meunier

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

*/

#include <config.h>

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#include "sdlcompat.h"

#include "settings.h"
#include "sound.h"
#include "ui/ui.h"

static SDL_AudioStream *audio_stream;
static int audio_buffer_limit;

/* Number of Spectrum frames audio latency to use */
#define NUM_FRAMES 2

/* Records sound writer status information */
static int audio_output_started;

int
sound_lowlevel_init( const char *device, int *freqptr, int *stereoptr )
{
  SDL_AudioSpec requested;
  int error;
  float hz;
  int sound_framesiz;

#ifndef __MORPHOS__    
  /* I'd rather just use setenv, but Windows doesn't have it */
  if( device ) {
    const char *environment = "SDL_AUDIODRIVER=";
    char *command = libspectrum_new( char, strlen( environment ) +
                                           strlen( device ) + 1 );
    strcpy( command, environment );
    strcat( command, device );
    error = putenv( command );
    libspectrum_free( command );
    if( error ) { 
      settings_current.sound = 0;
      ui_error( UI_ERROR_ERROR, "Couldn't set SDL_AUDIODRIVER: %s",
                strerror ( error ) );
      return 1;
    }
  }
#endif			/* #ifndef __MORPHOS__ */

  SDL_InitSubSystem( SDL_INIT_AUDIO );

  memset( &requested, 0, sizeof( SDL_AudioSpec ) );

  requested.freq = *freqptr;
  requested.channels = *stereoptr ? 2 : 1;
  requested.format = SDL_AUDIO_S16;

  /* Adjust relative processor speed to deal with adjusting sound generation
     frequency against emulation speed (more flexible than adjusting generated
     sample rate) */
  hz = (float)sound_get_effective_processor_speed() /
              machine_current->timings.tstates_per_frame;
  /* Amount of audio data we will accumulate before yielding back to the OS.
     Not much point having more than 100Hz playback, we probably get
     downgraded by the OS as being a hog too (unlimited Hz limits playback
     speed to about 2000% on my Mac, 100Hz allows up to 5000% for me) */
  if( hz > 100.0 ) hz = 100.0;
  sound_framesiz = *freqptr / hz;
#ifdef __FreeBSD__
  requested.samples = pow( 2.0, floor( log2( sound_framesiz ) ) );
#else			/* #ifdef __FreeBSD__ */
  requested.samples = sound_framesiz;
#endif			/* #ifdef __FreeBSD__ */

  audio_stream = SDL_OpenAudioDeviceStream( SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                            &requested, NULL, NULL );
  if( !audio_stream ) {
    settings_current.sound = 0;
    ui_error( UI_ERROR_ERROR, "Couldn't open sound device: %s",
              SDL_GetError() );
    return 1;
  }

  sound_framesiz = *freqptr / hz;
  sound_framesiz <<= 1;

  audio_buffer_limit = NUM_FRAMES * requested.channels * sound_framesiz + 1;

  /* wait to run sound until we have some sound to play */
  audio_output_started = 0;

  return 0;
}

void
sound_lowlevel_end( void )
{
  if( audio_stream ) {
    SDL_ClearAudioStream( audio_stream );
    SDL_DestroyAudioStream( audio_stream );
    audio_stream = NULL;
  }

  SDL_QuitSubSystem( SDL_INIT_AUDIO );
}

/* Copy data to fifo */
void
sound_lowlevel_frame( libspectrum_signed_word *data, int len )
{
  int chunk;
  int queued;

  /* Convert to bytes */
  libspectrum_signed_byte* bytes = (libspectrum_signed_byte*)data;
  len <<= 1;

  while( len && audio_stream ) {
    queued = SDL_GetAudioStreamQueued( audio_stream );
    if( queued < 0 ) {
      ui_error( UI_ERROR_ERROR, "Couldn't query SDL audio queue: %s",
                SDL_GetError() );
      return;
    }

    chunk = MIN( len, audio_buffer_limit - queued );
    if( chunk <= 0 ) {
      SDL_Delay(10);
      continue;
    }

    if( !SDL_PutAudioStreamData( audio_stream, bytes, chunk ) ) {
      ui_error( UI_ERROR_ERROR, "Couldn't queue SDL audio: %s",
                SDL_GetError() );
      return;
    }

    bytes += chunk;
    len -= chunk;
  }

  if( audio_stream && !audio_output_started ) {
    SDL_ResumeAudioStreamDevice( audio_stream );
    audio_output_started = 1;
  }
}

#ifndef MIN
#define MIN(a,b)    (((a) < (b)) ? (a) : (b))
#endif

