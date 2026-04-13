#include <config.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libspectrum.h>

#include "display.h"
#include "frontend/runtime.h"
#include "fuse.h"
#include "libretro/libretro.h"
#include "libretro/libretro_frontend.h"
#include "machine.h"
#include "memory_pages.h"
#include "periph.h"
#include "peripherals/disk/fdd.h"
#include "peripherals/joystick.h"
#include "pokefinder/pokemem.h"
#include "settings.h"
#include "snapshot.h"
#include "sound.h"
#include "spectrum.h"
#include "tape.h"
#include "ui/uimedia.h"
#include "utils.h"

#define FUSE_LIBRETRO_STATE_MAGIC 0x46534C52u
#define FUSE_LIBRETRO_STATE_VERSION 1u

typedef struct libretro_state_header {
  uint32_t magic;
  uint32_t version;
  uint32_t payload_size;
  uint32_t snapshot_type;
  char machine_id[ 32 ];
} libretro_state_header;

typedef struct libretro_cheat_entry {
  int enabled;
  int valid;
  libspectrum_byte bank;
  libspectrum_word address;
  libspectrum_byte value;
} libretro_cheat_entry;

typedef struct libretro_disk_image {
  char *path;
  char *label;
} libretro_disk_image;

typedef struct libretro_disk_state {
  libretro_disk_image *images;
  size_t image_count;
  size_t current_index;
  size_t initial_index;
  char *initial_path;
  int initial_index_valid;
  int active;
  int ejected;
  int controller;
  int drive;
} libretro_disk_state;

typedef struct libretro_drive_candidate {
  int controller;
  int drive;
} libretro_drive_candidate;

enum {
  LIBRETRO_FRONTEND_ACTION_RESET = 0,
  LIBRETRO_FRONTEND_ACTION_TAPE_TOGGLE,
  LIBRETRO_FRONTEND_ACTION_TAPE_REWIND,
  LIBRETRO_FRONTEND_ACTION_DISK_EJECT,
  LIBRETRO_FRONTEND_ACTION_DISK_PREVIOUS,
  LIBRETRO_FRONTEND_ACTION_DISK_NEXT,
  LIBRETRO_FRONTEND_ACTION_COUNT
};

static retro_environment_t environment_callback;
static retro_video_refresh_t video_refresh_callback;
static retro_audio_sample_t audio_sample_callback;
static retro_audio_sample_batch_t audio_batch_callback;
static retro_input_poll_t input_poll_callback;
static retro_input_state_t input_state_callback;

static int runtime_initialised;
static int game_loaded;
static int options_dirty;
static int geometry_dirty;
static int memory_map_dirty;
static libretro_disk_state disk_state = {
  NULL, 0, 0, 0, NULL, 0, 0, 0, -1, -1
};
static unsigned libretro_port_devices[ 2 ] = {
  RETRO_DEVICE_JOYPAD,
  RETRO_DEVICE_JOYPAD,
};
static int libretro_frontend_action_previous[ LIBRETRO_FRONTEND_ACTION_COUNT ];
static uint64_t libretro_memory_map_signature;

static libretro_cheat_entry *cheat_entries;
static size_t cheat_entry_count;
static struct retro_memory_descriptor
  libretro_memory_descriptors[ MEMORY_PAGES_IN_64K + 1 ];
static struct retro_memory_map libretro_memory_map = {
  libretro_memory_descriptors,
  0,
};
static struct retro_input_descriptor libretro_input_descriptors[ 40 ];
static char libretro_joypad_descriptor_text[ 2 ][ 10 ][ 64 ];

static const struct retro_controller_description
  libretro_port0_controller_types[] = {
    { "RetroPad", RETRO_DEVICE_JOYPAD },
    { "Keyboard", RETRO_DEVICE_KEYBOARD },
  };

static const struct retro_controller_description
  libretro_port1_controller_types[] = {
    { "RetroPad", RETRO_DEVICE_JOYPAD },
  };

static const struct retro_controller_info libretro_controller_info[] = {
  { libretro_port0_controller_types,
    ARRAY_SIZE( libretro_port0_controller_types ) },
  { libretro_port1_controller_types,
    ARRAY_SIZE( libretro_port1_controller_types ) },
  { NULL, 0 },
};

static const libretro_drive_candidate libretro_drive_candidates[] = {
  { UI_MEDIA_CONTROLLER_PLUS3, 0 },
  { UI_MEDIA_CONTROLLER_PLUS3, 1 },
  { UI_MEDIA_CONTROLLER_BETA, 0 },
  { UI_MEDIA_CONTROLLER_BETA, 1 },
  { UI_MEDIA_CONTROLLER_BETA, 2 },
  { UI_MEDIA_CONTROLLER_BETA, 3 },
  { UI_MEDIA_CONTROLLER_PLUSD, 0 },
  { UI_MEDIA_CONTROLLER_PLUSD, 1 },
  { UI_MEDIA_CONTROLLER_DISCIPLE, 0 },
  { UI_MEDIA_CONTROLLER_DISCIPLE, 1 },
  { UI_MEDIA_CONTROLLER_DIDAKTIK, 0 },
  { UI_MEDIA_CONTROLLER_DIDAKTIK, 1 },
  { UI_MEDIA_CONTROLLER_OPUS, 0 },
  { UI_MEDIA_CONTROLLER_OPUS, 1 },
};

static struct retro_core_option_v2_category libretro_option_categories[] = {
  { "system", "System", "Machine and core emulation settings." },
  { "media", "Media", "Tape, disk, and autoload behavior." },
  { "video", "Video", "Display and TV presentation options." },
  { "audio", "Audio", "Sound generation and output options." },
  { "input", "Input", "Joystick and mouse mapping options." },
  { "peripherals", "Peripherals", "Optional hardware interfaces for disk workflows." },
  { NULL, NULL, NULL }
};

static struct retro_core_option_v2_definition libretro_option_defs_v2[] = {
  {
    "fuse_machine", "Machine model", "Machine",
    "Selects the emulated machine model. Changing this resets the machine.",
    "Selects the emulated machine model. Changing this resets the machine.",
    "system",
    {
      { "48", "48K" },
      { "16", "16K" },
      { "128", "128K" },
      { "plus2", "+2" },
      { "plus2a", "+2A" },
      { "plus3", "+3" },
      { "plus3e", "+3e" },
      { "48_ntsc", "48K NTSC" },
      { "se", "SE" },
      { "pentagon", "Pentagon 128" },
      { "pentagon512", "Pentagon 512" },
      { "pentagon1024", "Pentagon 1024" },
      { "scorpion", "Scorpion" },
      { "2048", "TC2048" },
      { "2068", "TC2068" },
      { "ts2068", "TS2068" },
      { NULL, NULL },
    },
    "48"
  },
  {
    "fuse_issue2", "Issue 2 keyboard", "Issue 2 keyboard",
    "Emulates the original Issue 2 keyboard behavior on applicable 48K-era machines.",
    NULL,
    "system",
    {
      { "disabled", NULL },
      { "enabled", NULL },
      { NULL, NULL },
    },
    "disabled"
  },
  {
    "fuse_late_timings", "Late timings", "Late timings",
    "Applies late ULA timing behavior. Changing this resets the machine.",
    NULL,
    "system",
    {
      { "disabled", NULL },
      { "enabled", NULL },
      { NULL, NULL },
    },
    "disabled"
  },
  {
    "fuse_fastload", "Fastload", "Fastload",
    "Enables Fuse fastload handling for supported loaders.",
    NULL,
    "media",
    {
      { "disabled", NULL },
      { "enabled", NULL },
      { NULL, NULL },
    },
    "disabled"
  },
  {
    "fuse_tape_traps", "Tape traps", "Tape traps",
    "Enables tape loader traps for supported tape routines.",
    NULL,
    "media",
    {
      { "disabled", NULL },
      { "enabled", NULL },
      { NULL, NULL },
    },
    "disabled"
  },
  {
    "fuse_auto_load", "Auto-load media", "Auto-load media",
    "Automatically starts compatible tape and disk content after insertion.",
    NULL,
    "media",
    {
      { "enabled", NULL },
      { "disabled", NULL },
      { NULL, NULL },
    },
    "enabled"
  },
  {
    "fuse_detect_loader", "Detect tape loaders", "Detect tape loaders",
    "Detects custom tape loaders to improve loading behavior.",
    NULL,
    "media",
    {
      { "enabled", NULL },
      { "disabled", NULL },
      { NULL, NULL },
    },
    "enabled"
  },
  {
    "fuse_accelerate_loader", "Accelerate tape loaders", "Accelerate tape loaders",
    "Speeds up compatible custom tape loaders.",
    NULL,
    "media",
    {
      { "enabled", NULL },
      { "disabled", NULL },
      { NULL, NULL },
    },
    "enabled"
  },
  {
    "fuse_bw_tv", "Black and white TV", "Black and white TV",
    "Displays the Spectrum palette using monochrome TV colors.",
    NULL,
    "video",
    {
      { "disabled", NULL },
      { "enabled", NULL },
      { NULL, NULL },
    },
    "disabled"
  },
  {
    "fuse_pal_tv2x", "PAL TV 2x", "PAL TV 2x",
    "Enables PAL TV filtering behavior in the software scaler path.",
    NULL,
    "video",
    {
      { "disabled", NULL },
      { "enabled", NULL },
      { NULL, NULL },
    },
    "disabled"
  },
  {
    "fuse_sound", "Sound", "Sound",
    "Globally enables or disables sound output.",
    NULL,
    "audio",
    {
      { "enabled", NULL },
      { "disabled", NULL },
      { NULL, NULL },
    },
    "enabled"
  },
  {
    "fuse_loading_sound", "Loading sound", "Loading sound",
    "Keeps the tape loading tone audible while loading software.",
    NULL,
    "audio",
    {
      { "enabled", NULL },
      { "disabled", NULL },
      { NULL, NULL },
    },
    "enabled"
  },
  {
    "fuse_ay_stereo", "AY stereo separation", "AY stereo separation",
    "Selects the AY stereo channel layout.",
    NULL,
    "audio",
    {
      { "none", "None" },
      { "acb", "ACB" },
      { "abc", "ABC" },
      { NULL, NULL },
    },
    "none"
  },
  {
    "fuse_speaker_type", "Speaker filter", "Speaker filter",
    "Chooses the beeper/speaker output model.",
    NULL,
    "audio",
    {
      { "tv_speaker", "TV speaker" },
      { "beeper", "Beeper" },
      { "unfiltered", "Unfiltered" },
      { NULL, NULL },
    },
    "tv_speaker"
  },
  {
    "fuse_joystick_port_1", "Joystick port 1", "Joystick port 1",
    "Maps the first RetroPad to the selected Spectrum joystick interface.",
    NULL,
    "input",
    {
      { "kempston", "Kempston" },
      { "cursor", "Cursor" },
      { "sinclair1", "Sinclair 1" },
      { "sinclair2", "Sinclair 2" },
      { "fuller", "Fuller" },
      { "none", "None" },
      { NULL, NULL },
    },
    "kempston"
  },
  {
    "fuse_joystick_port_2", "Joystick port 2", "Joystick port 2",
    "Maps the second RetroPad to the selected Spectrum joystick interface.",
    NULL,
    "input",
    {
      { "none", "None" },
      { "kempston", "Kempston" },
      { "cursor", "Cursor" },
      { "sinclair1", "Sinclair 1" },
      { "sinclair2", "Sinclair 2" },
      { "fuller", "Fuller" },
      { NULL, NULL },
    },
    "none"
  },
  {
    "fuse_kempston_mouse", "Kempston mouse", "Kempston mouse",
    "Enables the Kempston mouse interface.",
    NULL,
    "input",
    {
      { "disabled", NULL },
      { "enabled", NULL },
      { NULL, NULL },
    },
    "disabled"
  },
  {
    "fuse_disk_peripheral", "Disk peripheral", "Disk peripheral",
    "Enables an optional floppy-disk interface for generic disk images when the machine itself does not provide one.",
    NULL,
    "peripherals",
    {
      { "none", "None" },
      { "beta128", "Beta 128" },
      { "plusd", "+D" },
      { "disciple", "DISCiPLE" },
      { "didaktik80", "Didaktik 80" },
      { "opus", "Opus Discovery" },
      { NULL, NULL },
    },
    "none"
  },
  { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL }
};

static struct retro_core_options_v2 libretro_core_options = {
  libretro_option_categories,
  libretro_option_defs_v2,
};

static struct retro_core_option_definition libretro_option_defs_v1[] = {
  {
    "fuse_machine", "Machine model",
    "Selects the emulated machine model. Changing this resets the machine.",
    {
      { "48", "48K" },
      { "16", "16K" },
      { "128", "128K" },
      { "plus2", "+2" },
      { "plus2a", "+2A" },
      { "plus3", "+3" },
      { "plus3e", "+3e" },
      { "48_ntsc", "48K NTSC" },
      { "se", "SE" },
      { "pentagon", "Pentagon 128" },
      { "pentagon512", "Pentagon 512" },
      { "pentagon1024", "Pentagon 1024" },
      { "scorpion", "Scorpion" },
      { "2048", "TC2048" },
      { "2068", "TC2068" },
      { "ts2068", "TS2068" },
      { NULL, NULL },
    },
    "48"
  },
  {
    "fuse_issue2", "Issue 2 keyboard",
    "Emulates the original Issue 2 keyboard behavior on applicable 48K-era machines.",
    {
      { "disabled", NULL },
      { "enabled", NULL },
      { NULL, NULL },
    },
    "disabled"
  },
  {
    "fuse_late_timings", "Late timings",
    "Applies late ULA timing behavior. Changing this resets the machine.",
    {
      { "disabled", NULL },
      { "enabled", NULL },
      { NULL, NULL },
    },
    "disabled"
  },
  {
    "fuse_fastload", "Fastload",
    "Enables Fuse fastload handling for supported loaders.",
    {
      { "disabled", NULL },
      { "enabled", NULL },
      { NULL, NULL },
    },
    "disabled"
  },
  {
    "fuse_tape_traps", "Tape traps",
    "Enables tape loader traps for supported tape routines.",
    {
      { "disabled", NULL },
      { "enabled", NULL },
      { NULL, NULL },
    },
    "disabled"
  },
  {
    "fuse_auto_load", "Auto-load media",
    "Automatically starts compatible tape and disk content after insertion.",
    {
      { "enabled", NULL },
      { "disabled", NULL },
      { NULL, NULL },
    },
    "enabled"
  },
  {
    "fuse_detect_loader", "Detect tape loaders",
    "Detects custom tape loaders to improve loading behavior.",
    {
      { "enabled", NULL },
      { "disabled", NULL },
      { NULL, NULL },
    },
    "enabled"
  },
  {
    "fuse_accelerate_loader", "Accelerate tape loaders",
    "Speeds up compatible custom tape loaders.",
    {
      { "enabled", NULL },
      { "disabled", NULL },
      { NULL, NULL },
    },
    "enabled"
  },
  {
    "fuse_bw_tv", "Black and white TV",
    "Displays the Spectrum palette using monochrome TV colors.",
    {
      { "disabled", NULL },
      { "enabled", NULL },
      { NULL, NULL },
    },
    "disabled"
  },
  {
    "fuse_pal_tv2x", "PAL TV 2x",
    "Enables PAL TV filtering behavior in the software scaler path.",
    {
      { "disabled", NULL },
      { "enabled", NULL },
      { NULL, NULL },
    },
    "disabled"
  },
  {
    "fuse_sound", "Sound",
    "Globally enables or disables sound output.",
    {
      { "enabled", NULL },
      { "disabled", NULL },
      { NULL, NULL },
    },
    "enabled"
  },
  {
    "fuse_loading_sound", "Loading sound",
    "Keeps the tape loading tone audible while loading software.",
    {
      { "enabled", NULL },
      { "disabled", NULL },
      { NULL, NULL },
    },
    "enabled"
  },
  {
    "fuse_ay_stereo", "AY stereo separation",
    "Selects the AY stereo channel layout.",
    {
      { "none", "None" },
      { "acb", "ACB" },
      { "abc", "ABC" },
      { NULL, NULL },
    },
    "none"
  },
  {
    "fuse_speaker_type", "Speaker filter",
    "Chooses the beeper/speaker output model.",
    {
      { "tv_speaker", "TV speaker" },
      { "beeper", "Beeper" },
      { "unfiltered", "Unfiltered" },
      { NULL, NULL },
    },
    "tv_speaker"
  },
  {
    "fuse_joystick_port_1", "Joystick port 1",
    "Maps the first RetroPad to the selected Spectrum joystick interface.",
    {
      { "kempston", "Kempston" },
      { "cursor", "Cursor" },
      { "sinclair1", "Sinclair 1" },
      { "sinclair2", "Sinclair 2" },
      { "fuller", "Fuller" },
      { "none", "None" },
      { NULL, NULL },
    },
    "kempston"
  },
  {
    "fuse_joystick_port_2", "Joystick port 2",
    "Maps the second RetroPad to the selected Spectrum joystick interface.",
    {
      { "none", "None" },
      { "kempston", "Kempston" },
      { "cursor", "Cursor" },
      { "sinclair1", "Sinclair 1" },
      { "sinclair2", "Sinclair 2" },
      { "fuller", "Fuller" },
      { NULL, NULL },
    },
    "none"
  },
  {
    "fuse_kempston_mouse", "Kempston mouse",
    "Enables the Kempston mouse interface.",
    {
      { "disabled", NULL },
      { "enabled", NULL },
      { NULL, NULL },
    },
    "disabled"
  },
  {
    "fuse_disk_peripheral", "Disk peripheral",
    "Enables an optional floppy-disk interface for generic disk images when the machine itself does not provide one.",
    {
      { "none", "None" },
      { "beta128", "Beta 128" },
      { "plusd", "+D" },
      { "disciple", "DISCiPLE" },
      { "didaktik80", "Didaktik 80" },
      { "opus", "Opus Discovery" },
      { NULL, NULL },
    },
    "none"
  },
  { NULL, NULL, NULL, { { NULL, NULL } }, NULL }
};

static const struct retro_variable libretro_option_defs_v0[] = {
  { "fuse_machine", "Machine model; 48|16|128|plus2|plus2a|plus3|plus3e|48_ntsc|se|pentagon|pentagon512|pentagon1024|scorpion|2048|2068|ts2068" },
  { "fuse_issue2", "Issue 2 keyboard; disabled|enabled" },
  { "fuse_late_timings", "Late timings; disabled|enabled" },
  { "fuse_fastload", "Fastload; disabled|enabled" },
  { "fuse_tape_traps", "Tape traps; disabled|enabled" },
  { "fuse_auto_load", "Auto-load media; enabled|disabled" },
  { "fuse_detect_loader", "Detect tape loaders; enabled|disabled" },
  { "fuse_accelerate_loader", "Accelerate tape loaders; enabled|disabled" },
  { "fuse_bw_tv", "Black and white TV; disabled|enabled" },
  { "fuse_pal_tv2x", "PAL TV 2x; disabled|enabled" },
  { "fuse_sound", "Sound; enabled|disabled" },
  { "fuse_loading_sound", "Loading sound; enabled|disabled" },
  { "fuse_ay_stereo", "AY stereo separation; none|acb|abc" },
  { "fuse_speaker_type", "Speaker filter; tv_speaker|beeper|unfiltered" },
  { "fuse_joystick_port_1", "Joystick port 1; kempston|cursor|sinclair1|sinclair2|fuller|none" },
  { "fuse_joystick_port_2", "Joystick port 2; none|kempston|cursor|sinclair1|sinclair2|fuller" },
  { "fuse_kempston_mouse", "Kempston mouse; disabled|enabled" },
  { "fuse_disk_peripheral", "Disk peripheral; none|beta128|plusd|disciple|didaktik80|opus" },
  { NULL, NULL }
};

static void RETRO_CALLCONV libretro_keyboard_callback( bool down,
                                                       unsigned keycode,
                                                       uint32_t character,
                                                       uint16_t key_modifiers );
static void libretro_register_core_options( void );
static void libretro_register_controller_info( void );
static void libretro_register_input_descriptors( void );
static void libretro_register_disk_control( void );
static void libretro_apply_variables( int force );
static int libretro_apply_bool_variable( const char *key, int *field );
static void libretro_apply_machine_variable( void );
static void libretro_apply_stereo_variable( void );
static void libretro_apply_speaker_variable( void );
static void libretro_apply_sound_variable( void );
static void libretro_apply_joystick_variables( void );
static void libretro_apply_disk_peripheral_variable( void );
static int libretro_get_variable( const char *key, const char **value );
static int libretro_string_enabled( const char *value );
static int libretro_build_state( unsigned char **buffer, size_t *length );
static void libretro_rebuild_cheats( void );
static int libretro_parse_cheat( const char *code, libretro_cheat_entry *entry );
static void libretro_reset_cheats( void );
static void libretro_update_geometry( struct retro_system_av_info *info );
static const char *libretro_machine_id_from_value( const char *value );
static int libretro_joystick_type_from_value( const char *value );
static int libretro_apply_machine_change( const char *id );
static void libretro_refresh_video_settings( void );
static void libretro_reconfigure_audio( void );
static int libretro_load_content( const char *path );
static int libretro_is_m3u_path( const char *path );
static int libretro_identify_path( const char *path, libspectrum_id_t *type,
                                   libspectrum_class_t *classp );
static int libretro_is_disk_class( libspectrum_class_t classp );
static void libretro_disk_clear_images( void );
static void libretro_disk_reset_content( int preserve_initial );
static void libretro_disk_clear_initial( void );
static int libretro_disk_append_image( const char *path );
static char *libretro_disk_make_label( const char *path );
static int libretro_parse_m3u_playlist( const char *playlist_path );
static char *libretro_resolve_playlist_entry( const char *playlist_path,
                                              const char *entry );
static int libretro_disk_select_initial_image( void );
static int libretro_disk_resolve_loaded_drive( const char *path );
static int libretro_disk_insert_current_image( void );
static const char *libretro_disk_image_path( size_t index );
static const char *libretro_disk_peripheral_from_value( const char *value );
static bool RETRO_CALLCONV libretro_disk_set_eject_state( bool ejected );
static bool RETRO_CALLCONV libretro_disk_get_eject_state( void );
static unsigned RETRO_CALLCONV libretro_disk_get_image_index( void );
static bool RETRO_CALLCONV libretro_disk_set_image_index( unsigned index );
static unsigned RETRO_CALLCONV libretro_disk_get_num_images( void );
static bool RETRO_CALLCONV libretro_disk_replace_image_index(
  unsigned index, const struct retro_game_info *info );
static bool RETRO_CALLCONV libretro_disk_add_image_index( void );
static bool RETRO_CALLCONV libretro_disk_set_initial_image( unsigned index,
                                                            const char *path );
static bool RETRO_CALLCONV libretro_disk_get_image_path( unsigned index,
                                                         char *s, size_t len );
static bool RETRO_CALLCONV libretro_disk_get_image_label( unsigned index,
                                                          char *s, size_t len );
static void libretro_show_message( const char *message );
static void libretro_set_controller_port_device_internal( unsigned port,
                                                          unsigned device );
static void libretro_reset_frontend_actions( void );
static void libretro_after_machine_state_change( int rebuild_cheats );
static void libretro_handle_frontend_actions( void );
static int libretro_disk_cycle_image( int direction );
static void libretro_mark_memory_map_dirty( void );
static void libretro_refresh_memory_map( void );
static uint64_t libretro_calculate_memory_map_signature( void );
static size_t libretro_build_memory_descriptors( void );
static uint64_t libretro_memory_flags_for_page( const memory_page *page );
static int libretro_memory_pages_mergeable( const memory_page *first,
                                            const memory_page *next,
                                            uint64_t first_flags,
                                            uint64_t next_flags );
static const char *libretro_joystick_label( size_t port );
static void libretro_append_input_descriptor( size_t *count,
                                              unsigned port,
                                              unsigned device,
                                              unsigned index,
                                              unsigned id,
                                              const char *description );

unsigned RETRO_API RETRO_CALLCONV
retro_api_version( void )
{
  return RETRO_API_VERSION;
}

void RETRO_API RETRO_CALLCONV
retro_set_environment( retro_environment_t cb )
{
  struct retro_keyboard_callback keyboard_callback;
  bool supports_no_game = true;

  environment_callback = cb;
  libretro_frontend_set_environment( cb );
  libretro_register_core_options();
  libretro_register_controller_info();

  keyboard_callback.callback = libretro_keyboard_callback;
  if( environment_callback ) {
    environment_callback( RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME,
                          &supports_no_game );
    environment_callback( RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK,
                          &keyboard_callback );
  }
}

void RETRO_API RETRO_CALLCONV
retro_set_video_refresh( retro_video_refresh_t cb )
{
  video_refresh_callback = cb;
}

void RETRO_API RETRO_CALLCONV
retro_set_audio_sample( retro_audio_sample_t cb )
{
  audio_sample_callback = cb;
}

void RETRO_API RETRO_CALLCONV
retro_set_audio_sample_batch( retro_audio_sample_batch_t cb )
{
  audio_batch_callback = cb;
}

void RETRO_API RETRO_CALLCONV
retro_set_input_poll( retro_input_poll_t cb )
{
  input_poll_callback = cb;
  libretro_frontend_set_input_callbacks( input_poll_callback,
                                         input_state_callback );
}

void RETRO_API RETRO_CALLCONV
retro_set_input_state( retro_input_state_t cb )
{
  input_state_callback = cb;
  libretro_frontend_set_input_callbacks( input_poll_callback,
                                         input_state_callback );
}

void RETRO_API RETRO_CALLCONV
retro_set_controller_port_device( unsigned port, unsigned device )
{
  if( port >= ARRAY_SIZE( libretro_port_devices ) ) return;

  libretro_set_controller_port_device_internal( port, device );
  if( port == 0 && libretro_port_devices[ port ] != RETRO_DEVICE_JOYPAD ) {
    libretro_reset_frontend_actions();
  }

  libretro_register_input_descriptors();
}

void RETRO_API RETRO_CALLCONV
retro_get_system_info( struct retro_system_info *info )
{
  if( !info ) return;

  memset( info, 0, sizeof( *info ) );
  info->library_name = "Fuse SDL3";
  info->library_version = FUSE_VERSION;
  info->need_fullpath = true;
  info->block_extract = false;
  info->valid_extensions =
    "szx|z80|sna|tap|tzx|pzx|csw|dsk|trd|fdi|udi|mgt|img|opd|opu|hdf|mdr|dck|rom|rzx|pok|m3u";
}

static void
libretro_update_geometry( struct retro_system_av_info *info )
{
  int width = libretro_frontend_get_video_width();
  int height = libretro_frontend_get_video_height();
  double fps = 50.0;

  if( !info ) return;

  if( width <= 0 ) width = DISPLAY_ASPECT_WIDTH;
  if( height <= 0 ) height = DISPLAY_SCREEN_HEIGHT;

  if( machine_current ) {
    fps = (double)machine_current->timings.processor_speed /
          (double)machine_current->timings.tstates_per_frame;
  }

  info->geometry.base_width = (unsigned)width;
  info->geometry.base_height = (unsigned)height;
  info->geometry.max_width = DISPLAY_SCREEN_WIDTH;
  info->geometry.max_height = DISPLAY_SCREEN_HEIGHT * 2;
  info->geometry.aspect_ratio = height > 0 ? (float)width / (float)height
                                           : 4.0f / 3.0f;
  info->timing.fps = fps;
  info->timing.sample_rate = libretro_frontend_get_audio_sample_rate();
}

void RETRO_API RETRO_CALLCONV
retro_get_system_av_info( struct retro_system_av_info *info )
{
  if( !info ) return;
  memset( info, 0, sizeof( *info ) );
  libretro_update_geometry( info );
}

void RETRO_API RETRO_CALLCONV
retro_init( void )
{
  runtime_initialised = 0;
  game_loaded = 0;
  options_dirty = 1;
  geometry_dirty = 0;
  memory_map_dirty = 1;
  libretro_memory_map_signature = 0;
  libretro_disk_reset_content( 0 );
  libretro_set_controller_port_device_internal( 0, RETRO_DEVICE_JOYPAD );
  libretro_set_controller_port_device_internal( 1, RETRO_DEVICE_JOYPAD );
  libretro_reset_frontend_actions();
  libretro_register_input_descriptors();
  libretro_register_disk_control();
}

void RETRO_API RETRO_CALLCONV
retro_deinit( void )
{
  if( runtime_initialised ) {
    fuse_runtime_shutdown();
    runtime_initialised = 0;
    game_loaded = 0;
  }

  libretro_disk_reset_content( 0 );
  libretro_reset_frontend_actions();
  libretro_reset_cheats();
}

void RETRO_API RETRO_CALLCONV
retro_reset( void )
{
  if( !runtime_initialised ) return;

  fuse_runtime_reset( 1 );
  libretro_after_machine_state_change( 1 );
}

bool RETRO_API RETRO_CALLCONV
retro_load_game( const struct retro_game_info *game )
{
  enum retro_pixel_format pixel_format = RETRO_PIXEL_FORMAT_XRGB8888;

  if( environment_callback ) {
    environment_callback( RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &pixel_format );
  }

  if( runtime_initialised ) {
    fuse_runtime_shutdown();
    runtime_initialised = 0;
    game_loaded = 0;
  }

  libretro_disk_reset_content( 1 );

  if( fuse_runtime_init( "fuse-libretro" ) ) {
    libretro_log_message( RETRO_LOG_ERROR,
                          "libretro: failed to initialise emulator runtime\n" );
    return false;
  }

  runtime_initialised = 1;
  libretro_apply_variables( 1 );

  if( game && game->path && game->path[0] ) {
    if( libretro_load_content( game->path ) ) {
      libretro_log_message( RETRO_LOG_ERROR,
                            "libretro: failed to load content '%s'\n",
                            game->path );
      fuse_runtime_shutdown();
      runtime_initialised = 0;
      libretro_disk_reset_content( 0 );
      return false;
    }
  } else {
    libretro_log_message( RETRO_LOG_INFO,
                          "libretro: starting without content\n" );
  }

  fuse_runtime_refresh_display();
  libretro_frontend_reset_runtime_state();
  libretro_reset_frontend_actions();
  libretro_mark_memory_map_dirty();
  libretro_refresh_memory_map();
  libretro_register_input_descriptors();
  geometry_dirty = 0;
  game_loaded = 1;
  return true;
}

void RETRO_API RETRO_CALLCONV
retro_unload_game( void )
{
  if( runtime_initialised ) {
    fuse_runtime_shutdown();
  }

  runtime_initialised = 0;
  game_loaded = 0;
  libretro_disk_reset_content( 0 );
  libretro_reset_cheats();
}

unsigned RETRO_API RETRO_CALLCONV
retro_get_region( void )
{
  if( machine_current && machine_current->id &&
      !strcmp( machine_current->id, "48_ntsc" ) ) {
    return RETRO_REGION_NTSC;
  }

  return RETRO_REGION_PAL;
}

void RETRO_API RETRO_CALLCONV
retro_run( void )
{
  const void *video_data;
  const int16_t *audio_data;
  size_t pitch;
  size_t frames;
  bool updated = false;
  struct retro_system_av_info info;

  if( !runtime_initialised || !game_loaded ) return;

  if( environment_callback ) {
    environment_callback( RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated );
    if( updated || options_dirty ) libretro_apply_variables( updated || options_dirty );

    if( geometry_dirty ) {
      memset( &info, 0, sizeof( info ) );
      libretro_update_geometry( &info );
      environment_callback( RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &info );
      geometry_dirty = 0;
    }
  }

  libretro_frontend_clear_audio();
  libretro_frontend_capture_input();
  libretro_handle_frontend_actions();
  fuse_runtime_run_frame();
  libretro_refresh_memory_map();

  video_data = libretro_frontend_get_video_data( &pitch );
  if( video_refresh_callback ) {
    video_refresh_callback( video_data,
                            (unsigned)libretro_frontend_get_video_width(),
                            (unsigned)libretro_frontend_get_video_height(),
                            pitch );
  }

  audio_data = libretro_frontend_get_audio_data( &frames );
  if( audio_batch_callback && audio_data && frames ) {
    audio_batch_callback( audio_data, frames );
  } else if( audio_sample_callback && audio_data ) {
    size_t i;
    int channels = libretro_frontend_get_audio_channels();
    for( i = 0; i < frames; i++ ) {
      int16_t left = audio_data[ i * channels ];
      int16_t right = channels > 1 ? audio_data[ i * channels + 1 ] : left;
      audio_sample_callback( left, right );
    }
  }
}

size_t RETRO_API RETRO_CALLCONV
retro_serialize_size( void )
{
  unsigned char *buffer = NULL;
  size_t length = 0;

  if( !runtime_initialised ) return 0;

  if( libretro_build_state( &buffer, &length ) ) return 0;
  libspectrum_free( buffer );
  return length;
}

bool RETRO_API RETRO_CALLCONV
retro_serialize( void *data, size_t size )
{
  unsigned char *buffer = NULL;
  size_t length = 0;

  if( !runtime_initialised ) return false;
  if( libretro_build_state( &buffer, &length ) ) return false;

  if( size < length ) {
    libspectrum_free( buffer );
    return false;
  }

  memcpy( data, buffer, length );
  libspectrum_free( buffer );
  return true;
}

bool RETRO_API RETRO_CALLCONV
retro_unserialize( const void *data, size_t size )
{
  const libretro_state_header *header = data;
  const unsigned char *payload;

  if( !runtime_initialised || !data || size < sizeof( *header ) ) return false;

  if( header->magic != FUSE_LIBRETRO_STATE_MAGIC ||
      header->version != FUSE_LIBRETRO_STATE_VERSION ) {
    return false;
  }

  if( sizeof( *header ) + header->payload_size > size ) return false;

  payload = (const unsigned char *)data + sizeof( *header );

  if( snapshot_read_buffer( payload, header->payload_size,
                            (libspectrum_id_t)header->snapshot_type ) ) {
    return false;
  }

  libretro_after_machine_state_change( 1 );
  return true;
}

void RETRO_API RETRO_CALLCONV
retro_cheat_reset( void )
{
  libretro_reset_cheats();
}

void RETRO_API RETRO_CALLCONV
retro_cheat_set( unsigned index, bool enabled, const char *code )
{
  libretro_cheat_entry entry;
  libretro_cheat_entry *new_entries;
  size_t new_count;
  size_t i;

  memset( &entry, 0, sizeof( entry ) );
  entry.enabled = enabled ? 1 : 0;

  if( enabled && code ) {
    entry.valid = !libretro_parse_cheat( code, &entry );
    if( !entry.valid ) {
      libretro_log_message( RETRO_LOG_WARN,
                            "libretro: ignoring unsupported cheat '%s'\n",
                            code );
    }
  }

  if( index >= cheat_entry_count ) {
    new_count = index + 1;
    new_entries = realloc( cheat_entries, new_count * sizeof( *new_entries ) );
    if( !new_entries ) return;

    for( i = cheat_entry_count; i < new_count; i++ ) {
      memset( &new_entries[ i ], 0, sizeof( new_entries[ i ] ) );
    }

    cheat_entries = new_entries;
    cheat_entry_count = new_count;
  }

  cheat_entries[ index ] = entry;
  libretro_rebuild_cheats();
}

bool RETRO_API RETRO_CALLCONV
retro_load_game_special( unsigned special_type,
                         const struct retro_game_info *info,
                         size_t num_info )
{
  (void)special_type;
  (void)info;
  (void)num_info;
  return false;
}

RETRO_API void *RETRO_CALLCONV
retro_get_memory_data( unsigned id )
{
  (void)id;
  return NULL;
}

size_t RETRO_API RETRO_CALLCONV
retro_get_memory_size( unsigned id )
{
  (void)id;
  return 0;
}

static void
libretro_show_message( const char *message )
{
  struct retro_message msg;

  if( !environment_callback || !message || !message[ 0 ] ) return;

  msg.msg = message;
  msg.frames = 180;
  environment_callback( RETRO_ENVIRONMENT_SET_MESSAGE, &msg );
}

static void
libretro_set_controller_port_device_internal( unsigned port, unsigned device )
{
  unsigned selected = RETRO_DEVICE_JOYPAD;

  if( port >= ARRAY_SIZE( libretro_port_devices ) ) return;

  if( port == 0 ) {
    if( device == RETRO_DEVICE_KEYBOARD || device == RETRO_DEVICE_NONE ) {
      selected = device;
    }
  } else {
    if( device == RETRO_DEVICE_NONE ) selected = RETRO_DEVICE_NONE;
  }

  libretro_port_devices[ port ] = selected;
  libretro_frontend_set_controller_port_device( port, selected );
}

static void
libretro_reset_frontend_actions( void )
{
  memset( libretro_frontend_action_previous, 0,
          sizeof( libretro_frontend_action_previous ) );
}

static void
libretro_after_machine_state_change( int rebuild_cheats )
{
  fuse_runtime_refresh_display();
  libretro_frontend_reset_runtime_state();
  if( rebuild_cheats ) libretro_rebuild_cheats();

  geometry_dirty = 1;
  libretro_mark_memory_map_dirty();
  libretro_refresh_memory_map();
}

static int
libretro_disk_cycle_image( int direction )
{
  char message[ 128 ];
  int was_ejected;
  size_t next_index;

  if( disk_state.image_count <= 1 ) {
    libretro_show_message( "No alternate disk image available" );
    return 1;
  }

  was_ejected = disk_state.ejected;
  if( !was_ejected && !libretro_disk_set_eject_state( true ) ) return 1;

  next_index = disk_state.current_index;
  if( direction > 0 ) {
    next_index = ( next_index + 1 ) % disk_state.image_count;
  } else if( next_index == 0 ) {
    next_index = disk_state.image_count - 1;
  } else {
    next_index--;
  }

  disk_state.current_index = next_index;

  if( !was_ejected && !libretro_disk_set_eject_state( false ) ) return 1;

  snprintf( message, sizeof( message ),
            was_ejected ? "Selected disk %u: %s" : "Inserted disk %u: %s",
            (unsigned)( next_index + 1 ),
            disk_state.images[ next_index ].label
              ? disk_state.images[ next_index ].label
              : disk_state.images[ next_index ].path );
  libretro_show_message( message );
  return 0;
}

static void
libretro_handle_frontend_actions( void )
{
  static const unsigned action_ids[ LIBRETRO_FRONTEND_ACTION_COUNT ] = {
    RETRO_DEVICE_ID_JOYPAD_START,
    RETRO_DEVICE_ID_JOYPAD_SELECT,
    RETRO_DEVICE_ID_JOYPAD_L2,
    RETRO_DEVICE_ID_JOYPAD_R2,
    RETRO_DEVICE_ID_JOYPAD_L3,
    RETRO_DEVICE_ID_JOYPAD_R3,
  };
  unsigned i;

  if( !input_state_callback || libretro_port_devices[ 0 ] != RETRO_DEVICE_JOYPAD ) {
    libretro_reset_frontend_actions();
    return;
  }

  for( i = 0; i < LIBRETRO_FRONTEND_ACTION_COUNT; i++ ) {
    int current = input_state_callback( 0, RETRO_DEVICE_JOYPAD, 0,
                                        action_ids[ i ] ) ? 1 : 0;

    if( current && !libretro_frontend_action_previous[ i ] ) {
      switch( i ) {
      case LIBRETRO_FRONTEND_ACTION_RESET:
        fuse_runtime_reset( 1 );
        libretro_after_machine_state_change( 1 );
        libretro_show_message( "Machine reset" );
        break;

      case LIBRETRO_FRONTEND_ACTION_TAPE_TOGGLE:
        if( !tape_present() ) {
          libretro_show_message( "No tape loaded" );
        } else if( !tape_toggle_play( 0 ) ) {
          libretro_show_message( tape_is_playing() ? "Tape playing"
                                                   : "Tape stopped" );
        }
        break;

      case LIBRETRO_FRONTEND_ACTION_TAPE_REWIND:
        if( !tape_present() ) {
          libretro_show_message( "No tape loaded" );
        } else if( !tape_rewind() ) {
          libretro_show_message( "Tape rewound" );
        }
        break;

      case LIBRETRO_FRONTEND_ACTION_DISK_EJECT:
        if( !disk_state.image_count ) {
          libretro_show_message( "No disk loaded" );
        } else {
          libretro_disk_set_eject_state( disk_state.ejected ? false : true );
        }
        break;

      case LIBRETRO_FRONTEND_ACTION_DISK_PREVIOUS:
        libretro_disk_cycle_image( -1 );
        break;

      case LIBRETRO_FRONTEND_ACTION_DISK_NEXT:
        libretro_disk_cycle_image( 1 );
        break;
      }
    }

    libretro_frontend_action_previous[ i ] = current;
  }
}

static void
libretro_mark_memory_map_dirty( void )
{
  memory_map_dirty = 1;
}

static uint64_t
libretro_memory_flags_for_page( const memory_page *page )
{
  uint64_t flags = 0;

  if( !page || !page->page ) return 0;

  if( page->writable ) {
    flags |= RETRO_MEMDESC_SYSTEM_RAM;
  } else {
    flags |= RETRO_MEMDESC_CONST;
  }

  return flags;
}

static int
libretro_memory_pages_mergeable( const memory_page *first,
                                 const memory_page *next,
                                 uint64_t first_flags,
                                 uint64_t next_flags )
{
  if( !first || !next ) return 0;
  if( first->page == NULL || next->page == NULL ) return 0;
  if( first_flags != next_flags ) return 0;
  if( first->source != next->source ) return 0;
  if( first->writable != next->writable ) return 0;

  return first->page + MEMORY_PAGE_SIZE == next->page;
}

static size_t
libretro_build_memory_descriptors( void )
{
  size_t count = 0;
  size_t page_index = 0;

  while( page_index < MEMORY_PAGES_IN_64K ) {
    const memory_page *first = &memory_map_read[ page_index ];
    libretro_memory_descriptors[ count ].flags =
      libretro_memory_flags_for_page( first );
    libretro_memory_descriptors[ count ].ptr = first->page;
    libretro_memory_descriptors[ count ].offset = 0;
    libretro_memory_descriptors[ count ].start = page_index * MEMORY_PAGE_SIZE;
    libretro_memory_descriptors[ count ].select = 0;
    libretro_memory_descriptors[ count ].disconnect = 0;
    libretro_memory_descriptors[ count ].len = MEMORY_PAGE_SIZE;
    libretro_memory_descriptors[ count ].addrspace = "Z80";
    count++;

    page_index++;
  }

  libretro_memory_descriptors[ count ].flags = 0;
  libretro_memory_descriptors[ count ].ptr = NULL;
  libretro_memory_descriptors[ count ].offset = 0;
  libretro_memory_descriptors[ count ].start = 0;
  libretro_memory_descriptors[ count ].select = 0xffff;
  libretro_memory_descriptors[ count ].disconnect = 0;
  libretro_memory_descriptors[ count ].len = 0;
  libretro_memory_descriptors[ count ].addrspace = "Z80";
  count++;

  libretro_memory_map.num_descriptors = (unsigned)count;
  return count;
}

static uint64_t
libretro_calculate_memory_map_signature( void )
{
  uint64_t signature = 1469598103934665603ULL;
  size_t i;

  for( i = 0; i < MEMORY_PAGES_IN_64K; i++ ) {
    const memory_page *page = &memory_map_read[ i ];
    uintptr_t ptr_value = (uintptr_t)page->page;

    signature ^= (uint64_t)( ptr_value >> 4 );
    signature *= 1099511628211ULL;
    signature ^= (uint64_t)page->source;
    signature *= 1099511628211ULL;
    signature ^= (uint64_t)page->writable;
    signature *= 1099511628211ULL;
    signature ^= (uint64_t)page->offset;
    signature *= 1099511628211ULL;
  }

  return signature;
}

static void
libretro_refresh_memory_map( void )
{
  uint64_t signature;

  if( !environment_callback || !runtime_initialised ) return;

  signature = libretro_calculate_memory_map_signature();
  if( !memory_map_dirty && signature == libretro_memory_map_signature ) return;

  libretro_build_memory_descriptors();
  environment_callback( RETRO_ENVIRONMENT_SET_MEMORY_MAPS,
                        &libretro_memory_map );
  libretro_memory_map_signature = signature;
  memory_map_dirty = 0;
}

static const char *
libretro_joystick_label( size_t port )
{
  int joystick_type;

  if( runtime_initialised ) {
    joystick_type = port == 0 ? settings_current.joystick_1_output
                              : settings_current.joystick_2_output;
  } else {
    joystick_type = port == 0 ? JOYSTICK_TYPE_KEMPSTON
                              : JOYSTICK_TYPE_NONE;
  }

  if( joystick_type < 0 || joystick_type >= JOYSTICK_TYPE_COUNT ) {
    return "Joystick";
  }

  return joystick_name[ joystick_type ];
}

static void
libretro_append_input_descriptor( size_t *count, unsigned port,
                                  unsigned device, unsigned index,
                                  unsigned id, const char *description )
{
  if( !count || !description ) return;
  if( *count + 1 >= ARRAY_SIZE( libretro_input_descriptors ) ) return;

  libretro_input_descriptors[ *count ].port = port;
  libretro_input_descriptors[ *count ].device = device;
  libretro_input_descriptors[ *count ].index = index;
  libretro_input_descriptors[ *count ].id = id;
  libretro_input_descriptors[ *count ].description = description;
  ( *count )++;
}

static void RETRO_CALLCONV
libretro_keyboard_callback( bool down, unsigned keycode, uint32_t character,
                            uint16_t key_modifiers )
{
  if( !runtime_initialised ) return;
  libretro_frontend_keyboard_event( down, keycode, character, key_modifiers );
}

static void
libretro_register_core_options( void )
{
  unsigned version = 0;

  if( !environment_callback ) return;

  if( !environment_callback( RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION,
                             &version ) ) {
    version = 0;
  }

  if( version >= 2 ) {
    environment_callback( RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2,
                          &libretro_core_options );
  } else if( version >= 1 ) {
    environment_callback( RETRO_ENVIRONMENT_SET_CORE_OPTIONS,
                          libretro_option_defs_v1 );
  } else {
    environment_callback( RETRO_ENVIRONMENT_SET_VARIABLES,
                          (void*)libretro_option_defs_v0 );
  }
}

static void
libretro_register_controller_info( void )
{
  if( !environment_callback ) return;

  environment_callback( RETRO_ENVIRONMENT_SET_CONTROLLER_INFO,
                        (void*)libretro_controller_info );
}

static void
libretro_register_input_descriptors( void )
{
  static const unsigned joypad_ids[] = {
    RETRO_DEVICE_ID_JOYPAD_UP,
    RETRO_DEVICE_ID_JOYPAD_DOWN,
    RETRO_DEVICE_ID_JOYPAD_LEFT,
    RETRO_DEVICE_ID_JOYPAD_RIGHT,
    RETRO_DEVICE_ID_JOYPAD_B,
    RETRO_DEVICE_ID_JOYPAD_A,
    RETRO_DEVICE_ID_JOYPAD_Y,
    RETRO_DEVICE_ID_JOYPAD_X,
    RETRO_DEVICE_ID_JOYPAD_L,
    RETRO_DEVICE_ID_JOYPAD_R,
  };
  static const char *joypad_suffixes[] = {
    "Up",
    "Down",
    "Left",
    "Right",
    "Fire 1",
    "Fire 2",
    "Fire 3",
    "Fire 4",
    "Fire 5",
    "Fire 6",
  };
  static const struct {
    unsigned id;
    const char *description;
  } keyboard_descriptors[] = {
    { RETROK_LSHIFT, "Caps Shift" },
    { RETROK_LCTRL, "Symbol Shift" },
    { RETROK_SPACE, "Space / BREAK" },
    { RETROK_RETURN, "Enter / NEW LINE" },
    { RETROK_1, "Number 1" },
    { RETROK_0, "Number 0" },
    { RETROK_q, "Q" },
    { RETROK_a, "A" },
    { RETROK_p, "P" },
    { RETROK_m, "M" },
  };
  size_t count = 0;
  size_t i;

  if( !environment_callback ) return;

  memset( libretro_input_descriptors, 0, sizeof( libretro_input_descriptors ) );

  for( i = 0; i < ARRAY_SIZE( keyboard_descriptors ); i++ ) {
    libretro_append_input_descriptor( &count, 0, RETRO_DEVICE_KEYBOARD, 0,
                                      keyboard_descriptors[ i ].id,
                                      keyboard_descriptors[ i ].description );
  }

  for( i = 0; i < ARRAY_SIZE( libretro_port_devices ); i++ ) {
    size_t joy_index;

    if( libretro_port_devices[ i ] != RETRO_DEVICE_JOYPAD ) continue;

    for( joy_index = 0; joy_index < ARRAY_SIZE( joypad_ids ); joy_index++ ) {
      snprintf( libretro_joypad_descriptor_text[ i ][ joy_index ],
                sizeof( libretro_joypad_descriptor_text[ i ][ joy_index ] ),
                "%s %s",
                libretro_joystick_label( i ),
                joypad_suffixes[ joy_index ] );
      libretro_append_input_descriptor( &count, (unsigned)i,
                                        RETRO_DEVICE_JOYPAD, 0,
                                        joypad_ids[ joy_index ],
                                        libretro_joypad_descriptor_text[ i ][ joy_index ] );
    }
  }

  if( libretro_port_devices[ 0 ] == RETRO_DEVICE_JOYPAD ) {
    libretro_append_input_descriptor( &count, 0, RETRO_DEVICE_JOYPAD, 0,
                                      RETRO_DEVICE_ID_JOYPAD_START,
                                      "Reset machine" );
    libretro_append_input_descriptor( &count, 0, RETRO_DEVICE_JOYPAD, 0,
                                      RETRO_DEVICE_ID_JOYPAD_SELECT,
                                      "Tape play / stop" );
    libretro_append_input_descriptor( &count, 0, RETRO_DEVICE_JOYPAD, 0,
                                      RETRO_DEVICE_ID_JOYPAD_L2,
                                      "Tape rewind" );
    libretro_append_input_descriptor( &count, 0, RETRO_DEVICE_JOYPAD, 0,
                                      RETRO_DEVICE_ID_JOYPAD_R2,
                                      "Disk eject / insert" );
    libretro_append_input_descriptor( &count, 0, RETRO_DEVICE_JOYPAD, 0,
                                      RETRO_DEVICE_ID_JOYPAD_L3,
                                      "Previous disk" );
    libretro_append_input_descriptor( &count, 0, RETRO_DEVICE_JOYPAD, 0,
                                      RETRO_DEVICE_ID_JOYPAD_R3,
                                      "Next disk" );
  }

  environment_callback( RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS,
                        libretro_input_descriptors );
}

static void
libretro_register_disk_control( void )
{
  unsigned version = 0;

  if( !environment_callback ) return;

  if( environment_callback( RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION,
                            &version ) && version >= 1 ) {
    struct retro_disk_control_ext_callback disk_control = {
      libretro_disk_set_eject_state,
      libretro_disk_get_eject_state,
      libretro_disk_get_image_index,
      libretro_disk_set_image_index,
      libretro_disk_get_num_images,
      libretro_disk_replace_image_index,
      libretro_disk_add_image_index,
      libretro_disk_set_initial_image,
      libretro_disk_get_image_path,
      libretro_disk_get_image_label,
    };
    environment_callback( RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE,
                          &disk_control );
  } else {
    struct retro_disk_control_callback disk_control = {
      libretro_disk_set_eject_state,
      libretro_disk_get_eject_state,
      libretro_disk_get_image_index,
      libretro_disk_set_image_index,
      libretro_disk_get_num_images,
      libretro_disk_replace_image_index,
      libretro_disk_add_image_index,
    };
    environment_callback( RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE,
                          &disk_control );
  }
}

static int
libretro_get_variable( const char *key, const char **value )
{
  struct retro_variable variable;

  if( value ) *value = NULL;
  if( !environment_callback ) return 1;

  variable.key = key;
  variable.value = NULL;
  if( !environment_callback( RETRO_ENVIRONMENT_GET_VARIABLE, &variable ) ||
      !variable.value ) {
    return 1;
  }

  if( value ) *value = variable.value;
  return 0;
}

static int
libretro_string_enabled( const char *value )
{
  return value && !strcmp( value, "enabled" );
}

static int
libretro_apply_bool_variable( const char *key, int *field )
{
  const char *value;
  int enabled;

  if( !field ) return 0;
  if( libretro_get_variable( key, &value ) ) return 0;

  enabled = libretro_string_enabled( value );
  if( *field == enabled ) return 0;

  *field = enabled;
  return 1;
}

static const char *
libretro_machine_id_from_value( const char *value )
{
  if( !value || !value[0] ) return "48";
  return value;
}

static int
libretro_joystick_type_from_value( const char *value )
{
  if( !value || !strcmp( value, "kempston" ) ) return JOYSTICK_TYPE_KEMPSTON;
  if( !strcmp( value, "cursor" ) ) return JOYSTICK_TYPE_CURSOR;
  if( !strcmp( value, "sinclair1" ) ) return JOYSTICK_TYPE_SINCLAIR_1;
  if( !strcmp( value, "sinclair2" ) ) return JOYSTICK_TYPE_SINCLAIR_2;
  if( !strcmp( value, "fuller" ) ) return JOYSTICK_TYPE_FULLER;
  if( !strcmp( value, "none" ) ) return JOYSTICK_TYPE_NONE;
  return JOYSTICK_TYPE_KEMPSTON;
}

static void
libretro_apply_machine_variable( void )
{
  const char *value;
  const char *id;

  if( libretro_get_variable( "fuse_machine", &value ) ) return;

  id = libretro_machine_id_from_value( value );
  if( settings_current.start_machine && !strcmp( settings_current.start_machine, id ) ) {
    return;
  }

  libretro_apply_machine_change( id );
}

static void
libretro_apply_stereo_variable( void )
{
  const char *value;
  const char *setting_value = "None";

  if( libretro_get_variable( "fuse_ay_stereo", &value ) ) return;

  if( value && !strcmp( value, "acb" ) ) {
    setting_value = "ACB";
  } else if( value && !strcmp( value, "abc" ) ) {
    setting_value = "ABC";
  }

  if( settings_current.stereo_ay && !strcmp( settings_current.stereo_ay,
                                             setting_value ) ) {
    return;
  }

  settings_set_string( &settings_current.stereo_ay, setting_value );
  if( runtime_initialised ) libretro_reconfigure_audio();
}

static void
libretro_apply_speaker_variable( void )
{
  const char *value;
  const char *speaker_value = "TV speaker";

  if( libretro_get_variable( "fuse_speaker_type", &value ) ) return;

  if( value && !strcmp( value, "beeper" ) ) {
    speaker_value = "Beeper";
  } else if( value && !strcmp( value, "unfiltered" ) ) {
    speaker_value = "Unfiltered";
  }

  if( settings_current.speaker_type && !strcmp( settings_current.speaker_type,
                                                speaker_value ) ) {
    return;
  }

  settings_set_string( &settings_current.speaker_type, speaker_value );
  if( runtime_initialised ) libretro_reconfigure_audio();
}

static void
libretro_apply_sound_variable( void )
{
  const char *value;
  int enabled;

  if( libretro_get_variable( "fuse_sound", &value ) ) return;

  enabled = libretro_string_enabled( value );
  if( settings_current.sound == enabled ) return;

  settings_current.sound = enabled;
  if( runtime_initialised ) libretro_reconfigure_audio();
}

static void
libretro_apply_joystick_variables( void )
{
  const char *value;
  int changed = 0;

  if( !libretro_get_variable( "fuse_joystick_port_1", &value ) ) {
    int joystick_type = libretro_joystick_type_from_value( value );
    if( settings_current.joystick_1_output != joystick_type ) {
      settings_current.joystick_1_output = joystick_type;
      changed = 1;
    }
  }

  if( !libretro_get_variable( "fuse_joystick_port_2", &value ) ) {
    int joystick_type = libretro_joystick_type_from_value( value );
    if( settings_current.joystick_2_output != joystick_type ) {
      settings_current.joystick_2_output = joystick_type;
      changed = 1;
    }
  }

  if( libretro_apply_bool_variable( "fuse_kempston_mouse",
                                    &settings_current.kempston_mouse ) ) {
    changed = 1;
  }

  if( changed && runtime_initialised ) periph_posthook();

  if( changed ) {
    libretro_mark_memory_map_dirty();
    libretro_refresh_memory_map();
  }
}

static const char *
libretro_disk_peripheral_from_value( const char *value )
{
  if( !value || !value[0] ) return "none";
  return value;
}

static void
libretro_apply_disk_peripheral_variable( void )
{
  const char *value;
  const char *selected;
  int beta = 0, plusd = 0, disciple = 0, didaktik80 = 0, opus = 0;
  int changed = 0;

  if( libretro_get_variable( "fuse_disk_peripheral", &value ) ) return;

  selected = libretro_disk_peripheral_from_value( value );

  if( !strcmp( selected, "beta128" ) ) {
    beta = 1;
  } else if( !strcmp( selected, "plusd" ) ) {
    plusd = 1;
  } else if( !strcmp( selected, "disciple" ) ) {
    disciple = 1;
  } else if( !strcmp( selected, "didaktik80" ) ) {
    didaktik80 = 1;
  } else if( !strcmp( selected, "opus" ) ) {
    opus = 1;
  }

  if( settings_current.beta128 != beta ) {
    settings_current.beta128 = beta;
    changed = 1;
  }
  if( settings_current.plusd != plusd ) {
    settings_current.plusd = plusd;
    changed = 1;
  }
  if( settings_current.disciple != disciple ) {
    settings_current.disciple = disciple;
    changed = 1;
  }
  if( settings_current.didaktik80 != didaktik80 ) {
    settings_current.didaktik80 = didaktik80;
    changed = 1;
  }
  if( settings_current.opus != opus ) {
    settings_current.opus = opus;
    changed = 1;
  }

  if( changed && runtime_initialised ) periph_posthook();

  if( changed ) {
    libretro_mark_memory_map_dirty();
    libretro_refresh_memory_map();
  }
}

static void
libretro_apply_variables( int force )
{
  int refresh_video = 0;

  libretro_apply_machine_variable();
  libretro_apply_sound_variable();
  libretro_apply_disk_peripheral_variable();

  libretro_apply_bool_variable( "fuse_fastload", &settings_current.fastload );
  libretro_apply_bool_variable( "fuse_tape_traps", &settings_current.tape_traps );
  libretro_apply_bool_variable( "fuse_auto_load", &settings_current.auto_load );
  libretro_apply_bool_variable( "fuse_detect_loader", &settings_current.detect_loader );
  libretro_apply_bool_variable( "fuse_accelerate_loader", &settings_current.accelerate_loader );
  libretro_apply_bool_variable( "fuse_loading_sound", &settings_current.sound_load );
  if( libretro_apply_bool_variable( "fuse_issue2", &settings_current.issue2 ) ) {
    refresh_video = 1;
  }
  if( libretro_apply_bool_variable( "fuse_bw_tv", &settings_current.bw_tv ) ) {
    refresh_video = 1;
  }
  if( libretro_apply_bool_variable( "fuse_pal_tv2x", &settings_current.pal_tv2x ) ) {
    refresh_video = 1;
  }

  if( libretro_apply_bool_variable( "fuse_late_timings",
                                    &settings_current.late_timings ) ) {
    if( runtime_initialised && machine_current && machine_current->id ) {
      libretro_apply_machine_change( machine_current->id );
    }
  }

  libretro_apply_stereo_variable();
  libretro_apply_speaker_variable();
  libretro_apply_joystick_variables();

  if( force || refresh_video ) libretro_refresh_video_settings();

  libretro_register_input_descriptors();

  options_dirty = 0;
}

static int
libretro_apply_machine_change( const char *id )
{
  settings_set_string( &settings_current.start_machine, id );

  if( runtime_initialised ) {
    if( fuse_runtime_select_machine( id ) ) return 1;
    libretro_after_machine_state_change( 1 );
  }

  return 0;
}

static void
libretro_refresh_video_settings( void )
{
  if( runtime_initialised ) fuse_runtime_refresh_display();
}

static void
libretro_reconfigure_audio( void )
{
  if( !runtime_initialised ) return;

  sound_end();
  sound_init( settings_current.sound_device );
}

static int
libretro_is_m3u_path( const char *path )
{
  const char *extension;

  if( !path ) return 0;

  extension = strrchr( path, '.' );
  return extension && !strcmp( extension, ".m3u" );
}

static int
libretro_identify_path( const char *path, libspectrum_id_t *type,
                        libspectrum_class_t *classp )
{
  utils_file file;
  int error;

  memset( &file, 0, sizeof( file ) );
  error = utils_read_file( path, &file );
  if( error ) return error;

  error = libspectrum_identify_file_with_class( type, classp, path,
                                                file.buffer, file.length );
  utils_close_file( &file );

  return error;
}

static int
libretro_is_disk_class( libspectrum_class_t classp )
{
  switch( classp ) {
  case LIBSPECTRUM_CLASS_DISK_PLUS3:
  case LIBSPECTRUM_CLASS_DISK_DIDAKTIK:
  case LIBSPECTRUM_CLASS_DISK_PLUSD:
  case LIBSPECTRUM_CLASS_DISK_OPUS:
  case LIBSPECTRUM_CLASS_DISK_TRDOS:
  case LIBSPECTRUM_CLASS_DISK_GENERIC:
    return 1;
  default:
    return 0;
  }
}

static void
libretro_disk_clear_images( void )
{
  size_t i;

  for( i = 0; i < disk_state.image_count; i++ ) {
    libspectrum_free( disk_state.images[ i ].path );
    libspectrum_free( disk_state.images[ i ].label );
  }

  free( disk_state.images );
  disk_state.images = NULL;
  disk_state.image_count = 0;
}

static void
libretro_disk_clear_initial( void )
{
  libspectrum_free( disk_state.initial_path );
  disk_state.initial_path = NULL;
  disk_state.initial_index = 0;
  disk_state.initial_index_valid = 0;
}

static void
libretro_disk_reset_content( int preserve_initial )
{
  libretro_disk_clear_images();
  disk_state.current_index = 0;
  disk_state.active = 0;
  disk_state.ejected = 0;
  disk_state.controller = -1;
  disk_state.drive = -1;

  if( !preserve_initial ) libretro_disk_clear_initial();
}

static char *
libretro_disk_make_label( const char *path )
{
  const char *basename;
  const char *extension;
  size_t length;
  char *label;

  if( !path ) return NULL;

  basename = strrchr( path, '/' );
  if( !basename ) basename = strrchr( path, '\\' );
  basename = basename ? basename + 1 : path;

  extension = strrchr( basename, '.' );
  length = extension && extension > basename ? (size_t)( extension - basename )
                                             : strlen( basename );

  label = libspectrum_new( char, length + 1 );
  memcpy( label, basename, length );
  label[ length ] = '\0';
  return label;
}

static int
libretro_disk_append_image( const char *path )
{
  libretro_disk_image *images;
  size_t new_count = disk_state.image_count + 1;

  images = realloc( disk_state.images, new_count * sizeof( *images ) );
  if( !images ) return 1;

  disk_state.images = images;
  disk_state.images[ disk_state.image_count ].path = utils_safe_strdup( path );
  disk_state.images[ disk_state.image_count ].label = libretro_disk_make_label( path );
  disk_state.image_count = new_count;
  return 0;
}

static char *
libretro_resolve_playlist_entry( const char *playlist_path, const char *entry )
{
  const char *separator;
  size_t directory_length;
  size_t entry_length;
  char *resolved;

  if( !entry || !entry[0] ) return NULL;
  if( compat_is_absolute_path( entry ) || entry[0] == '/' || entry[0] == '\\' ) {
    return utils_safe_strdup( entry );
  }

  separator = strrchr( playlist_path, '/' );
  if( !separator ) separator = strrchr( playlist_path, '\\' );
  if( !separator ) return utils_safe_strdup( entry );

  directory_length = (size_t)( separator - playlist_path );
  entry_length = strlen( entry );
  resolved = libspectrum_new( char, directory_length + 1 + entry_length + 1 );

  memcpy( resolved, playlist_path, directory_length );
  resolved[ directory_length ] = FUSE_DIR_SEP_CHR;
  memcpy( resolved + directory_length + 1, entry, entry_length + 1 );

  return resolved;
}

static int
libretro_parse_m3u_playlist( const char *playlist_path )
{
  FILE *playlist;
  char line[ PATH_MAX * 2 ];

  playlist = fopen( playlist_path, "r" );
  if( !playlist ) return 1;

  while( fgets( line, sizeof( line ), playlist ) ) {
    char *entry = line;
    char *end;
    char *resolved;
    libspectrum_id_t type;
    libspectrum_class_t classp;

    while( *entry == ' ' || *entry == '\t' ) entry++;
    if( *entry == '\0' || *entry == '\n' || *entry == '\r' || *entry == '#' ) {
      continue;
    }

    end = entry + strlen( entry );
    while( end > entry && ( end[-1] == '\n' || end[-1] == '\r' ||
                            end[-1] == ' ' || end[-1] == '\t' ) ) {
      *--end = '\0';
    }

    resolved = libretro_resolve_playlist_entry( playlist_path, entry );
    if( !resolved ) {
      fclose( playlist );
      return 1;
    }

    if( libretro_identify_path( resolved, &type, &classp ) ||
        !libretro_is_disk_class( classp ) ||
        libretro_disk_append_image( resolved ) ) {
      libspectrum_free( resolved );
      fclose( playlist );
      return 1;
    }

    libspectrum_free( resolved );
  }

  fclose( playlist );
  return disk_state.image_count ? 0 : 1;
}

static int
libretro_disk_select_initial_image( void )
{
  size_t index = 0;

  if( disk_state.image_count == 0 ) {
    libretro_disk_clear_initial();
    return 1;
  }

  if( disk_state.initial_index_valid &&
      disk_state.initial_index < disk_state.image_count ) {
    index = disk_state.initial_index;
    if( disk_state.initial_path &&
        strcmp( disk_state.images[ index ].path, disk_state.initial_path ) ) {
      size_t i;
      for( i = 0; i < disk_state.image_count; i++ ) {
        if( !strcmp( disk_state.images[ i ].path, disk_state.initial_path ) ) {
          index = i;
          break;
        }
      }
      if( i == disk_state.image_count ) index = 0;
    }
  }

  disk_state.current_index = index;
  libretro_disk_clear_initial();
  return 0;
}

static int
libretro_disk_resolve_loaded_drive( const char *path )
{
  size_t i;

  disk_state.active = 0;
  disk_state.controller = -1;
  disk_state.drive = -1;

  for( i = 0; i < ARRAY_SIZE( libretro_drive_candidates ); i++ ) {
    ui_media_drive_info_t *drive = ui_media_drive_find(
      libretro_drive_candidates[ i ].controller,
      libretro_drive_candidates[ i ].drive
    );

    if( !drive || !drive->fdd || !drive->fdd->loaded ||
        !drive->fdd->disk.filename ) {
      continue;
    }

    if( !strcmp( drive->fdd->disk.filename, path ) ) {
      disk_state.controller = libretro_drive_candidates[ i ].controller;
      disk_state.drive = libretro_drive_candidates[ i ].drive;
      disk_state.active = 1;
      return 0;
    }
  }

  return 1;
}

static const char *
libretro_disk_image_path( size_t index )
{
  if( index >= disk_state.image_count ) return NULL;
  return disk_state.images[ index ].path;
}

static int
libretro_disk_insert_current_image( void )
{
  const char *path = libretro_disk_image_path( disk_state.current_index );

  if( !path ) return 0;

  if( disk_state.active ) {
    ui_media_drive_info_t *drive = ui_media_drive_find( disk_state.controller,
                                                        disk_state.drive );
    if( drive ) {
      int error = ui_media_drive_insert( drive, path, settings_current.auto_load );
      if( !error ) {
        fuse_runtime_refresh_display();
        libretro_disk_resolve_loaded_drive( path );
      }
      return error;
    }
  }

  if( fuse_runtime_load_file( path ) ) return 1;

  libretro_disk_resolve_loaded_drive( path );
  fuse_runtime_refresh_display();
  return 0;
}

static int
libretro_load_content( const char *path )
{
  libspectrum_id_t type = LIBSPECTRUM_ID_UNKNOWN;
  libspectrum_class_t classp = LIBSPECTRUM_CLASS_UNKNOWN;
  const char *content_path = path;

  if( libretro_is_m3u_path( path ) ) {
    if( libretro_parse_m3u_playlist( path ) ||
        libretro_disk_select_initial_image() ) {
      return 1;
    }

    content_path = libretro_disk_image_path( disk_state.current_index );
    if( !content_path ) return 1;

    libretro_log_message( RETRO_LOG_INFO,
                          "libretro: loaded disk playlist '%s' with %u image(s)\n",
                          path, (unsigned)disk_state.image_count );
  } else {
    if( libretro_identify_path( path, &type, &classp ) ) return 1;

    if( libretro_is_disk_class( classp ) ) {
      if( libretro_disk_append_image( path ) ) return 1;
      disk_state.current_index = 0;
    }
  }

  if( fuse_runtime_load_file( content_path ) ) return 1;

  if( disk_state.image_count ) {
    libretro_disk_resolve_loaded_drive( content_path );
    libretro_log_message( RETRO_LOG_INFO,
                          "libretro: inserted disk image %u: %s\n",
                          (unsigned)disk_state.current_index, content_path );
  }

  return 0;
}

static bool RETRO_CALLCONV
libretro_disk_set_eject_state( bool ejected )
{
  const char *message;

  if( !runtime_initialised ) return false;
  if( disk_state.ejected == ( ejected ? 1 : 0 ) ) return true;

  if( ejected ) {
    if( disk_state.active ) {
      if( ui_media_drive_eject( disk_state.controller, disk_state.drive ) ) {
        return false;
      }
      fuse_runtime_refresh_display();
    }
    disk_state.ejected = 1;
    libretro_log_message( RETRO_LOG_INFO, "libretro: disk tray opened\n" );
    libretro_show_message( "Disk tray opened" );
    return true;
  }

  if( disk_state.current_index < disk_state.image_count ) {
    if( libretro_disk_insert_current_image() ) return false;
  }

  disk_state.ejected = 0;
  libretro_log_message( RETRO_LOG_INFO, "libretro: disk tray closed\n" );
  message = disk_state.current_index < disk_state.image_count
    ? "Disk inserted"
    : "Disk tray closed";
  libretro_show_message( message );
  return true;
}

static bool RETRO_CALLCONV
libretro_disk_get_eject_state( void )
{
  return disk_state.ejected ? true : false;
}

static unsigned RETRO_CALLCONV
libretro_disk_get_image_index( void )
{
  return disk_state.current_index < disk_state.image_count
    ? (unsigned)disk_state.current_index
    : (unsigned)disk_state.image_count;
}

static bool RETRO_CALLCONV
libretro_disk_set_image_index( unsigned index )
{
  if( !disk_state.ejected ) return false;

  disk_state.current_index = index;
  libretro_log_message( RETRO_LOG_INFO,
                        "libretro: selected disk image index %u\n", index );
  return true;
}

static unsigned RETRO_CALLCONV
libretro_disk_get_num_images( void )
{
  return (unsigned)disk_state.image_count;
}

static bool RETRO_CALLCONV
libretro_disk_replace_image_index( unsigned index,
                                   const struct retro_game_info *info )
{
  size_t i;

  if( !disk_state.ejected ) return false;
  if( index >= disk_state.image_count ) return false;

  if( !info || !info->path || !info->path[0] ) {
    libspectrum_free( disk_state.images[ index ].path );
    libspectrum_free( disk_state.images[ index ].label );

    for( i = index + 1; i < disk_state.image_count; i++ ) {
      disk_state.images[ i - 1 ] = disk_state.images[ i ];
    }

    disk_state.image_count--;
    if( disk_state.image_count == 0 ) {
      free( disk_state.images );
      disk_state.images = NULL;
      disk_state.current_index = 0;
    } else {
      libretro_disk_image *images = realloc(
        disk_state.images, disk_state.image_count * sizeof( *images )
      );
      if( images ) disk_state.images = images;
      if( disk_state.current_index > index ) disk_state.current_index--;
      if( disk_state.current_index >= disk_state.image_count ) {
        disk_state.current_index = disk_state.image_count;
      }
    }

    return true;
  }

  libspectrum_free( disk_state.images[ index ].path );
  libspectrum_free( disk_state.images[ index ].label );
  disk_state.images[ index ].path = utils_safe_strdup( info->path );
  disk_state.images[ index ].label = libretro_disk_make_label( info->path );
  return true;
}

static bool RETRO_CALLCONV
libretro_disk_add_image_index( void )
{
  libretro_disk_image *images;
  size_t new_count = disk_state.image_count + 1;

  images = realloc( disk_state.images, new_count * sizeof( *images ) );
  if( !images ) return false;

  disk_state.images = images;
  disk_state.images[ disk_state.image_count ].path = NULL;
  disk_state.images[ disk_state.image_count ].label = NULL;
  disk_state.image_count = new_count;
  return true;
}

static bool RETRO_CALLCONV
libretro_disk_set_initial_image( unsigned index, const char *path )
{
  disk_state.initial_index = index;
  disk_state.initial_index_valid = 1;
  libspectrum_free( disk_state.initial_path );
  disk_state.initial_path = utils_safe_strdup( path );

  libretro_log_message( RETRO_LOG_INFO,
                        "libretro: requested initial disk image %u (%s)\n",
                        index, path ? path : "<null>" );
  return true;
}

static bool RETRO_CALLCONV
libretro_disk_get_image_path( unsigned index, char *s, size_t len )
{
  const char *path = libretro_disk_image_path( index );

  if( !s || !len ) return false;

  if( !disk_state.image_count && index == 0 ) {
    s[ 0 ] = '\0';
    return true;
  }

  if( !path ) return false;
  snprintf( s, len, "%s", path );
  return true;
}

static bool RETRO_CALLCONV
libretro_disk_get_image_label( unsigned index, char *s, size_t len )
{
  const char *label;

  if( index >= disk_state.image_count || !s || !len ) return false;

  label = disk_state.images[ index ].label ? disk_state.images[ index ].label
                                           : disk_state.images[ index ].path;
  if( !label ) return false;

  snprintf( s, len, "%s", label );
  return true;
}

static int
libretro_build_state( unsigned char **buffer, size_t *length )
{
  unsigned char *payload = NULL;
  size_t payload_length = 0;
  int flags = 0;
  size_t total_length;
  libretro_state_header *header;
  libspectrum_snap *snap;
  int error;

  if( buffer ) *buffer = NULL;
  if( length ) *length = 0;

  snap = libspectrum_snap_alloc();
  if( !snap ) return 1;

  error = snapshot_copy_to( snap );
  if( error ) {
    libspectrum_snap_free( snap );
    return error;
  }

  error = libspectrum_snap_write( &payload, &payload_length, &flags, snap,
                                  LIBSPECTRUM_ID_SNAPSHOT_SZX,
                                  fuse_creator, 0 );
  libspectrum_snap_free( snap );
  if( error ) return error;

  total_length = sizeof( *header ) + payload_length;
  *buffer = libspectrum_new( unsigned char, total_length );
  header = (libretro_state_header *)*buffer;

  header->magic = FUSE_LIBRETRO_STATE_MAGIC;
  header->version = FUSE_LIBRETRO_STATE_VERSION;
  header->payload_size = (uint32_t)payload_length;
  header->snapshot_type = LIBSPECTRUM_ID_SNAPSHOT_SZX;
  memset( header->machine_id, 0, sizeof( header->machine_id ) );
  if( machine_current && machine_current->id ) {
    strncpy( header->machine_id, machine_current->id,
             sizeof( header->machine_id ) - 1 );
  }

  memcpy( *buffer + sizeof( *header ), payload, payload_length );
  libspectrum_free( payload );

  if( length ) *length = total_length;
  return 0;
}

static int
libretro_parse_cheat( const char *code, libretro_cheat_entry *entry )
{
  char *working;
  char *value_part;
  char *bank_part = NULL;
  char *address_part;
  char *end;
  unsigned long parsed;

  if( !code || !entry ) return 1;

  working = utils_safe_strdup( code );
  if( !working ) return 1;

  value_part = strchr( working, '=' );
  if( !value_part ) value_part = strchr( working, ' ' );
  if( !value_part ) {
    libspectrum_free( working );
    return 1;
  }

  *value_part++ = '\0';
  while( *value_part == ' ' ) value_part++;

  address_part = working;
  bank_part = strchr( working, ':' );
  if( bank_part ) {
    *bank_part++ = '\0';
    address_part = bank_part;

    parsed = strtoul( working, &end, 0 );
    if( *end || parsed > 255 ) {
      libspectrum_free( working );
      return 1;
    }
    entry->bank = (libspectrum_byte)parsed;
  } else {
    entry->bank = 8;
  }

  parsed = strtoul( address_part, &end, 0 );
  if( *end || parsed > 65535 ) {
    libspectrum_free( working );
    return 1;
  }
  entry->address = (libspectrum_word)parsed;

  parsed = strtoul( value_part, &end, 0 );
  if( *end || parsed > 255 ) {
    libspectrum_free( working );
    return 1;
  }
  entry->value = (libspectrum_byte)parsed;

  libspectrum_free( working );
  return 0;
}

static void
libretro_rebuild_cheats( void )
{
  size_t i;

  pokemem_clear();
  if( !runtime_initialised ) return;

  for( i = 0; i < cheat_entry_count; i++ ) {
    trainer_t *trainer;
    if( !cheat_entries[ i ].enabled || !cheat_entries[ i ].valid ) continue;

    trainer = pokemem_trainer_list_add( cheat_entries[ i ].bank,
                                        cheat_entries[ i ].address,
                                        cheat_entries[ i ].value );
    if( trainer ) pokemem_trainer_activate( trainer );
  }
}

static void
libretro_reset_cheats( void )
{
  free( cheat_entries );
  cheat_entries = NULL;
  cheat_entry_count = 0;
  pokemem_clear();
}
