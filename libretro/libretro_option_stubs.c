#include <config.h>

#include <string.h>

#include <libspectrum.h>

#include "settings.h"
#include "unittests/unittests.h"

static int
option_index( const char *value, const char *const *choices, size_t count,
              int default_index )
{
  size_t i;

  if( !value || !*value ) return default_index;

  for( i = 0; i < count; i++ ) {
    if( !strcmp( value, choices[ i ] ) ) return (int)i;
  }

  return default_index;
}

int
option_enumerate_movie_movie_compr( void )
{
  static const char *const choices[] = { "None", "Lossless", "High" };
  return option_index( settings_current.movie_compr, choices,
                       sizeof( choices ) / sizeof( choices[0] ), 1 );
}

int
option_enumerate_sound_stereo_ay( void )
{
  static const char *const choices[] = { "None", "ACB", "ABC" };
  return option_index( settings_current.stereo_ay, choices,
                       sizeof( choices ) / sizeof( choices[0] ), 0 );
}

int
option_enumerate_sound_speaker_type( void )
{
  static const char *const choices[] = { "TV speaker", "Beeper", "Unfiltered" };
  return option_index( settings_current.speaker_type, choices,
                       sizeof( choices ) / sizeof( choices[0] ), 0 );
}

int
option_enumerate_diskoptions_drive_plus3a_type( void )
{
  static const char *const choices[] = {
    "Single-sided 40 track", "Double-sided 40 track",
    "Single-sided 80 track", "Double-sided 80 track"
  };
  return option_index( settings_current.drive_plus3a_type, choices,
                       sizeof( choices ) / sizeof( choices[0] ), 0 );
}

int
option_enumerate_diskoptions_drive_plus3b_type( void )
{
  static const char *const choices[] = {
    "Disabled", "Single-sided 40 track", "Double-sided 40 track",
    "Single-sided 80 track", "Double-sided 80 track"
  };
  return option_index( settings_current.drive_plus3b_type, choices,
                       sizeof( choices ) / sizeof( choices[0] ), 4 );
}

int
option_enumerate_diskoptions_drive_beta128a_type( void )
{
  static const char *const choices[] = {
    "Single-sided 40 track", "Double-sided 40 track",
    "Single-sided 80 track", "Double-sided 80 track"
  };
  return option_index( settings_current.drive_beta128a_type, choices,
                       sizeof( choices ) / sizeof( choices[0] ), 3 );
}

int
option_enumerate_diskoptions_drive_beta128b_type( void )
{
  static const char *const choices[] = {
    "Disabled", "Single-sided 40 track", "Double-sided 40 track",
    "Single-sided 80 track", "Double-sided 80 track"
  };
  return option_index( settings_current.drive_beta128b_type, choices,
                       sizeof( choices ) / sizeof( choices[0] ), 4 );
}

int
option_enumerate_diskoptions_drive_beta128c_type( void )
{
  static const char *const choices[] = {
    "Disabled", "Single-sided 40 track", "Double-sided 40 track",
    "Single-sided 80 track", "Double-sided 80 track"
  };
  return option_index( settings_current.drive_beta128c_type, choices,
                       sizeof( choices ) / sizeof( choices[0] ), 4 );
}

int
option_enumerate_diskoptions_drive_beta128d_type( void )
{
  static const char *const choices[] = {
    "Disabled", "Single-sided 40 track", "Double-sided 40 track",
    "Single-sided 80 track", "Double-sided 80 track"
  };
  return option_index( settings_current.drive_beta128d_type, choices,
                       sizeof( choices ) / sizeof( choices[0] ), 4 );
}

int
option_enumerate_diskoptions_drive_plusd1_type( void )
{
  static const char *const choices[] = {
    "Single-sided 40 track", "Double-sided 40 track",
    "Single-sided 80 track", "Double-sided 80 track"
  };
  return option_index( settings_current.drive_plusd1_type, choices,
                       sizeof( choices ) / sizeof( choices[0] ), 3 );
}

int
option_enumerate_diskoptions_drive_plusd2_type( void )
{
  static const char *const choices[] = {
    "Disabled", "Single-sided 40 track", "Double-sided 40 track",
    "Single-sided 80 track", "Double-sided 80 track"
  };
  return option_index( settings_current.drive_plusd2_type, choices,
                       sizeof( choices ) / sizeof( choices[0] ), 4 );
}

int
option_enumerate_diskoptions_drive_didaktik80a_type( void )
{
  static const char *const choices[] = {
    "Single-sided 40 track", "Double-sided 40 track",
    "Single-sided 80 track", "Double-sided 80 track"
  };
  return option_index( settings_current.drive_didaktik80a_type, choices,
                       sizeof( choices ) / sizeof( choices[0] ), 3 );
}

int
option_enumerate_diskoptions_drive_didaktik80b_type( void )
{
  static const char *const choices[] = {
    "Disabled", "Single-sided 40 track", "Double-sided 40 track",
    "Single-sided 80 track", "Double-sided 80 track"
  };
  return option_index( settings_current.drive_didaktik80b_type, choices,
                       sizeof( choices ) / sizeof( choices[0] ), 4 );
}

int
option_enumerate_diskoptions_drive_disciple1_type( void )
{
  static const char *const choices[] = {
    "Single-sided 40 track", "Double-sided 40 track",
    "Single-sided 80 track", "Double-sided 80 track"
  };
  return option_index( settings_current.drive_disciple1_type, choices,
                       sizeof( choices ) / sizeof( choices[0] ), 3 );
}

int
option_enumerate_diskoptions_drive_disciple2_type( void )
{
  static const char *const choices[] = {
    "Disabled", "Single-sided 40 track", "Double-sided 40 track",
    "Single-sided 80 track", "Double-sided 80 track"
  };
  return option_index( settings_current.drive_disciple2_type, choices,
                       sizeof( choices ) / sizeof( choices[0] ), 4 );
}

int
option_enumerate_diskoptions_drive_opus1_type( void )
{
  static const char *const choices[] = {
    "Single-sided 40 track", "Double-sided 40 track",
    "Single-sided 80 track", "Double-sided 80 track"
  };
  return option_index( settings_current.drive_opus1_type, choices,
                       sizeof( choices ) / sizeof( choices[0] ), 0 );
}

int
option_enumerate_diskoptions_drive_opus2_type( void )
{
  static const char *const choices[] = {
    "Disabled", "Single-sided 40 track", "Double-sided 40 track",
    "Single-sided 80 track", "Double-sided 80 track"
  };
  return option_index( settings_current.drive_opus2_type, choices,
                       sizeof( choices ) / sizeof( choices[0] ), 1 );
}

int
option_enumerate_diskoptions_disk_try_merge( void )
{
  static const char *const choices[] = {
    "Never", "With single-sided drives", "Always"
  };
  return option_index( settings_current.disk_try_merge, choices,
                       sizeof( choices ) / sizeof( choices[0] ), 1 );
}

int
unittests_run( void )
{
  return 0;
}

int
unittests_assert_2k_page( libspectrum_word base, int source, int page )
{
  (void)base;
  (void)source;
  (void)page;
  return 0;
}

int
unittests_assert_4k_page( libspectrum_word base, int source, int page )
{
  (void)base;
  (void)source;
  (void)page;
  return 0;
}

int
unittests_assert_8k_page( libspectrum_word base, int source, int page )
{
  (void)base;
  (void)source;
  (void)page;
  return 0;
}

int
unittests_assert_16k_page( libspectrum_word base, int source, int page )
{
  (void)base;
  (void)source;
  (void)page;
  return 0;
}

int
unittests_assert_16k_ram_page( libspectrum_word base, int page )
{
  (void)base;
  (void)page;
  return 0;
}

int
unittests_paging_test_48( int ram8000 )
{
  (void)ram8000;
  return 0;
}
