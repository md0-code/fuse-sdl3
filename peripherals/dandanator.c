/* dandanator.c: Dandanator cartridge handling routines */

#include <config.h>

#include <ctype.h>
#include <string.h>

#include "dandanator.h"
#include "if2.h"
#include "infrastructure/startup_manager.h"
#include "machine.h"
#include "memory_pages.h"
#include "module.h"
#include "spectrum.h"
#include "ui/ui.h"
#include "utils.h"
#include "z80/z80.h"

#define DANDANATOR_SLOT_COUNT 32
#define DANDANATOR_SLOT_SIZE 0x4000
#define DANDANATOR_ROMSET_SIZE ( DANDANATOR_SLOT_COUNT * DANDANATOR_SLOT_SIZE )
#define DANDANATOR_VERSION_OFFSET 0x3fe0
#define DANDANATOR_HIDDEN_SLOT -1
#define DANDANATOR_SLEEP_UNTIL_COMMAND 0x04
#define DANDANATOR_SLEEP_UNTIL_RESET 0x08

static memory_page
  dandanator_memory_map_romcs[DANDANATOR_SLOT_COUNT * MEMORY_PAGES_IN_16K];

int dandanator_active = 0;

static int dandanator_memory_source;
static char *dandanator_filename;
static libspectrum_byte *dandanator_romset;
static int dandanator_inserted;
static int dandanator_current_slot;
static int dandanator_reset_slot;
static int dandanator_sleep_state;
static int dandanator_pending_return_slot;
static int dandanator_command_active;
static int dandanator_command_token_index;
static int dandanator_command_token_open;
static libspectrum_byte dandanator_command_bytes[4];

static void dandanator_reset( int hard_reset );
static void dandanator_memory_map( void );
static void dandanator_clear_protocol_state( void );
static void dandanator_apply_mapping( void );
static void dandanator_set_visible_slot( int slot );
static void dandanator_finish_command( void );
static void dandanator_complete_return_switch( void );

static module_info_t dandanator_module_info = {

  /* .reset = */ dandanator_reset,
  /* .romcs = */ dandanator_memory_map,
  /* .snapshot_enabled = */ NULL,
  /* .snapshot_from = */ NULL,
  /* .snapshot_to = */ NULL,

};

static int
dandanator_init( void *context GCC_UNUSED )
{
  dandanator_memory_source = memory_source_register( "Dandanator" );
  module_register( &dandanator_module_info );
  return 0;
}

void
dandanator_register_startup( void )
{
  startup_manager_module dependencies[] = { STARTUP_MANAGER_MODULE_MEMORY };
  startup_manager_register( STARTUP_MANAGER_MODULE_DANDANATOR, dependencies,
                            ARRAY_SIZE( dependencies ), dandanator_init,
                            NULL, NULL );
}

int
dandanator_available( void )
{
  return 1;
}

int
dandanator_detect_buffer( const libspectrum_byte *buffer, size_t length )
{
  size_t i;

  if( !buffer || length != DANDANATOR_ROMSET_SIZE ) return 0;

  if( buffer[DANDANATOR_VERSION_OFFSET] != 'v' &&
      buffer[DANDANATOR_VERSION_OFFSET] != 'V' )
    return 0;

  for( i = 1; i < 8; i++ ) {
    libspectrum_byte value = buffer[DANDANATOR_VERSION_OFFSET + i];
    if( value == 0 ) break;
    if( !isprint( value ) ) return 0;
  }

  return 1;
}

static int
dandanator_load_romset( const char *filename )
{
  utils_file romset;
  size_t slot, page;

  if( utils_read_file( filename, &romset ) ) return 1;

  if( !dandanator_detect_buffer( romset.buffer, romset.length ) ) {
    ui_error( UI_ERROR_ERROR,
              "'%s' is not a recognised Dandanator romset", filename );
    utils_close_file( &romset );
    return 1;
  }

  if( dandanator_romset ) libspectrum_free( dandanator_romset );

  dandanator_romset = libspectrum_new( libspectrum_byte, romset.length );
  memcpy( dandanator_romset, romset.buffer, romset.length );

  for( slot = 0; slot < DANDANATOR_SLOT_COUNT; slot++ ) {
    for( page = 0; page < MEMORY_PAGES_IN_16K; page++ ) {
      memory_page *mapping =
        &dandanator_memory_map_romcs[slot * MEMORY_PAGES_IN_16K + page];
      size_t offset = slot * DANDANATOR_SLOT_SIZE + page * MEMORY_PAGE_SIZE;

      mapping->page = dandanator_romset + offset;
      mapping->writable = 0;
      mapping->contended = 0;
      mapping->source = dandanator_memory_source;
      mapping->save_to_snapshot = 0;
      mapping->page_num = slot;
      mapping->offset = page * MEMORY_PAGE_SIZE;
    }
  }

  utils_close_file( &romset );
  return 0;
}

int
dandanator_insert( const char *filename )
{
  if( if2_active ) {
    ui_error( UI_ERROR_ERROR,
              "Eject the Interface 2 cartridge before inserting a Dandanator" );
    return 1;
  }

  if( dandanator_load_romset( filename ) ) return 1;

  if( dandanator_filename ) libspectrum_free( dandanator_filename );
  dandanator_filename = utils_safe_strdup( filename );

  dandanator_inserted = 1;
  dandanator_current_slot = 0;
  dandanator_reset_slot = 0;
  dandanator_sleep_state = 0;
  dandanator_pending_return_slot = 0;
  dandanator_clear_protocol_state();

  ui_menu_activate( UI_MENU_ITEM_MEDIA_CARTRIDGE_DANDANATOR_EJECT, 1 );

  machine_reset( 0 );

  return 0;
}

void
dandanator_eject( void )
{
  if( dandanator_filename ) {
    libspectrum_free( dandanator_filename );
    dandanator_filename = NULL;
  }

  if( dandanator_romset ) {
    libspectrum_free( dandanator_romset );
    dandanator_romset = NULL;
  }

  dandanator_inserted = 0;
  dandanator_active = 0;
  dandanator_current_slot = 0;
  dandanator_reset_slot = 0;
  dandanator_sleep_state = 0;
  dandanator_pending_return_slot = 0;
  dandanator_clear_protocol_state();

  machine_current->ram.romcs = 0;

  ui_menu_activate( UI_MENU_ITEM_MEDIA_CARTRIDGE_DANDANATOR_EJECT, 0 );

  machine_reset( 0 );
}

static void
dandanator_reset( int hard_reset )
{
  dandanator_active = 0;
  dandanator_clear_protocol_state();

  if( !dandanator_inserted || !dandanator_romset ) {
    ui_menu_activate( UI_MENU_ITEM_MEDIA_CARTRIDGE_DANDANATOR_EJECT, 0 );
    return;
  }

  if( hard_reset ) {
    dandanator_current_slot = 0;
    dandanator_reset_slot = 0;
    dandanator_sleep_state = 0;
  } else {
    dandanator_current_slot = dandanator_reset_slot;
  }

  dandanator_active = 1;
  dandanator_pending_return_slot = 0;
  dandanator_apply_mapping();

  ui_menu_activate( UI_MENU_ITEM_MEDIA_CARTRIDGE_DANDANATOR_EJECT, 1 );
}

static void
dandanator_memory_map( void )
{
  if( !dandanator_active || dandanator_current_slot == DANDANATOR_HIDDEN_SLOT )
    return;

  memory_map_romcs_full(
    &dandanator_memory_map_romcs[dandanator_current_slot * MEMORY_PAGES_IN_16K]
  );
}

static void
dandanator_clear_protocol_state( void )
{
  dandanator_command_active = 0;
  dandanator_command_token_index = 0;
  dandanator_command_token_open = 0;
  memset( dandanator_command_bytes, 0, sizeof( dandanator_command_bytes ) );
}

static void
dandanator_apply_mapping( void )
{
  if( !dandanator_inserted ) return;

  if( dandanator_current_slot == DANDANATOR_HIDDEN_SLOT ) {
    machine_current->ram.romcs = 0;
    if( machine_current->memory_map ) machine_current->memory_map();
  } else {
    machine_current->ram.romcs = 1;
    memory_romcs_map();
  }
}

static void
dandanator_set_visible_slot( int slot )
{
  if( slot < 0 || slot >= DANDANATOR_SLOT_COUNT ) return;

  dandanator_current_slot = slot;
  dandanator_apply_mapping();
}

static void
dandanator_complete_return_switch( void )
{
  if( dandanator_pending_return_slot <= 0 ) return;

  dandanator_set_visible_slot( dandanator_pending_return_slot - 1 );
  dandanator_pending_return_slot = 0;
}

static void
dandanator_finish_command( void )
{
  libspectrum_byte command = dandanator_command_bytes[0];
  libspectrum_byte arg1 = dandanator_command_bytes[1];
  libspectrum_byte arg2 = dandanator_command_bytes[2];

  if( !command ) {
    dandanator_clear_protocol_state();
    return;
  }

  if( command == 46 ) {
    if( arg1 == arg2 ) {
      if( arg1 == 1 ) {
        dandanator_sleep_state |= DANDANATOR_SLEEP_UNTIL_COMMAND;
      } else if( arg1 == 16 ) {
        dandanator_sleep_state &= ~DANDANATOR_SLEEP_UNTIL_COMMAND;
      } else if( arg1 == 31 ) {
        dandanator_sleep_state |= DANDANATOR_SLEEP_UNTIL_RESET;
      }
    }
    dandanator_clear_protocol_state();
    return;
  }

  if( dandanator_sleep_state & ( DANDANATOR_SLEEP_UNTIL_COMMAND |
                                 DANDANATOR_SLEEP_UNTIL_RESET ) ) {
    dandanator_clear_protocol_state();
    return;
  }

  if( command >= 1 && command <= DANDANATOR_SLOT_COUNT ) {
    dandanator_set_visible_slot( command - 1 );
  } else {
    switch( command ) {
    case 34:
      dandanator_current_slot = DANDANATOR_HIDDEN_SLOT;
      dandanator_sleep_state |= DANDANATOR_SLEEP_UNTIL_COMMAND;
      dandanator_apply_mapping();
      break;

    case 36:
      z80.iff1 = 0;
      z80.iff2 = 0;
      z80.halted = 0;
      z80.pc.w = 0;
      break;

    case 39:
      if( dandanator_current_slot >= 0 )
        dandanator_reset_slot = dandanator_current_slot;
      break;

    case 40:
      if( arg1 >= 1 && arg1 <= DANDANATOR_SLOT_COUNT ) {
        if( arg2 & 0x03 ) {
          dandanator_set_visible_slot( arg1 - 1 );
          z80.iff1 = 0;
          z80.iff2 = 0;
          z80.halted = 0;
          z80.pc.w = ( arg2 & 0x02 ) ? 0x0066 : 0x0000;
        } else {
          dandanator_pending_return_slot = arg1;
        }
      }
      dandanator_sleep_state = arg2 &
        ( DANDANATOR_SLEEP_UNTIL_COMMAND | DANDANATOR_SLEEP_UNTIL_RESET );
      break;

    case 41:
    case 42:
    case 43:
    case 48:
    case 49:
    case 52:
    default:
      break;
    }
  }

  dandanator_clear_protocol_state();
}

void
dandanator_pre_opcode( libspectrum_word pc, libspectrum_byte opcode )
{
  if( !dandanator_inserted ) return;

  if( opcode == 0xfb ) dandanator_clear_protocol_state();

  if( dandanator_command_active && dandanator_command_token_open ) {
    dandanator_command_token_open = 0;

    if( dandanator_command_bytes[0] > 0 && dandanator_command_bytes[0] < 40 ) {
      dandanator_finish_command();
    } else if( pc < 0x4000 && dandanator_command_token_index < 3 ) {
      dandanator_command_token_index++;
    } else if( dandanator_command_token_index < 3 ) {
      dandanator_clear_protocol_state();
    }
  }

  if( opcode == 0xc9 ) dandanator_complete_return_switch();
}

void
dandanator_memory_read( libspectrum_word address )
{
  address = address;
}

void
dandanator_memory_write( libspectrum_word address )
{
  if( !dandanator_inserted || address >= 0x0004 ) return;

  if( !dandanator_command_active ) {
    dandanator_command_active = 1;
    dandanator_command_token_index = 0;
    memset( dandanator_command_bytes, 0, sizeof( dandanator_command_bytes ) );
  }

  if( dandanator_command_token_index > 3 ) return;

  dandanator_command_token_open = 1;
  dandanator_command_bytes[dandanator_command_token_index]++;

  if( dandanator_command_token_index == 3 ) dandanator_finish_command();
}