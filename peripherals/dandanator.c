/* dandanator.c: Dandanator cartridge handling routines */

#include <config.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dandanator.h"
#include "event.h"
#include "if2.h"
#include "infrastructure/startup_manager.h"
#include "machine.h"
#include "settings.h"
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
#define DANDANATOR_HIDDEN_SLOT DANDANATOR_SLOT_COUNT
#define DANDANATOR_COMMANDS_LOCKED 0x04
#define DANDANATOR_COMMANDS_DISABLED 0x08
#define DANDANATOR_FLAG_C 0x01
#define DANDANATOR_FLAG_Z 0x40
#define DANDANATOR_COMMAND_DELAY 35

static memory_page
  dandanator_memory_map_romcs[DANDANATOR_SLOT_COUNT * MEMORY_PAGES_IN_16K];
static memory_page dandanator_memory_map_shadow[MEMORY_PAGES_IN_16K];

int dandanator_active = 0;

static int dandanator_memory_source;
static int dandanator_shadow_memory_source;
static char *dandanator_filename;
static libspectrum_byte *dandanator_romset;
static libspectrum_byte dandanator_shadow_ram[DANDANATOR_SLOT_SIZE];
static libspectrum_byte dandanator_extended_ram[DANDANATOR_SLOT_SIZE];
static libspectrum_byte dandanator_extended_valid[DANDANATOR_SLOT_SIZE];
static int dandanator_inserted;
static int dandanator_programming_enabled;
static int dandanator_current_slot;
static int dandanator_sleep_state;
static int dandanator_pending_return_slot;
static int dandanator_eeprom_sector;
static int dandanator_bridge_base;
static libspectrum_byte dandanator_jedec_data_1555;
static libspectrum_byte dandanator_jedec_data_2aaa;
static int dandanator_jedec_data_1555_valid;
static int dandanator_jedec_data_2aaa_valid;
static libspectrum_word dandanator_extended_pointer0;
static libspectrum_word dandanator_extended_pointer1;
static libspectrum_word dandanator_extended_mode;
static libspectrum_byte dandanator_pic_ram[256];
static libspectrum_byte dandanator_command_bytes[4];
static int dandanator_command_stage;
static int dandanator_command_trap;
static libspectrum_byte dandanator_last_opcode;
static libspectrum_word dandanator_last_opcode_pc;
static int dandanator_pending_apply_mapping;
static int dandanator_pending_cpu_reset;
static int dandanator_pending_cpu_nmi;
static libspectrum_dword dandanator_apply_mapping_at;


static void dandanator_reset( int hard_reset );
static void dandanator_memory_map( void );
static int dandanator_detect_blank_buffer( const libspectrum_byte *buffer,
                                           size_t length );
static void dandanator_reset_runtime_state( void );
static void dandanator_update_romcs_state( void );
static void dandanator_apply_mapping( void );
static libspectrum_word dandanator_slot_checksum( int slot,
                                                  libspectrum_word start,
                                                  libspectrum_word end );
static void dandanator_schedule_apply_mapping( int reset_cpu );
static void dandanator_clear_command_state( void );
static void dandanator_finish_command( void );
static int dandanator_is_trap_opcode( libspectrum_byte opcode );
static libspectrum_word dandanator_branch_target( libspectrum_word pc,
                                                  libspectrum_byte opcode );
static int dandanator_should_continue_trap( libspectrum_word target );
static void dandanator_commit_eeprom_bridge( void );
static void dandanator_reset_extended_overlay( void );
static void dandanator_store_extended_overlay( int command, int value,
                                               int offset );
static void dandanator_select_slot( int slot );
static int dandanator_write_back_romset( void );

static void dandanator_finish_insert( const char *filename,
                                      int programming_enabled );

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
  dandanator_shadow_memory_source = memory_source_register( "Dandanator shadow" );
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
dandanator_detect_blank_buffer( const libspectrum_byte *buffer, size_t length )
{
  size_t i;

  if( !buffer || length != DANDANATOR_ROMSET_SIZE ) return 0;

  for( i = 0; i < length; i++ ) {
    if( buffer[i] != 0xff ) return 0;
  }

  return 1;
}

static int
dandanator_load_romset( const char *filename )
{
  utils_file romset;
  size_t slot, page;

  if( utils_read_file( filename, &romset ) ) return 1;

  if( !dandanator_detect_buffer( romset.buffer, romset.length ) &&
      !dandanator_detect_blank_buffer( romset.buffer, romset.length ) ) {
    ui_error( UI_ERROR_ERROR,
              "'%s' is not a recognised Dandanator EEPROM image", filename );
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

  for( page = 0; page < MEMORY_PAGES_IN_16K; page++ ) {
    memory_page *mapping = &dandanator_memory_map_shadow[page];

    mapping->page = dandanator_shadow_ram + page * MEMORY_PAGE_SIZE;
    mapping->writable = 1;
    mapping->contended = 0;
    mapping->source = dandanator_shadow_memory_source;
    mapping->save_to_snapshot = 0;
    mapping->page_num = 0;
    mapping->offset = page * MEMORY_PAGE_SIZE;
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

  dandanator_finish_insert( filename, 0 );

  machine_reset( 0 );

  return 0;
}

int
dandanator_insert_blank( const char *filename )
{
  libspectrum_byte *romset_buf;
  int error;

  romset_buf = libspectrum_new( libspectrum_byte, DANDANATOR_ROMSET_SIZE );
  memset( romset_buf, 0xff, DANDANATOR_ROMSET_SIZE );

  error = utils_write_file( filename, romset_buf, DANDANATOR_ROMSET_SIZE );
  libspectrum_free( romset_buf );
  if( error ) {
    ui_error( UI_ERROR_ERROR,
              "Couldn't create blank Dandanator image '%s'", filename );
    return 1;
  }

  if( if2_active ) {
    ui_error( UI_ERROR_ERROR,
              "Eject the Interface 2 cartridge before inserting a Dandanator" );
    return 1;
  }

  if( dandanator_load_romset( filename ) ) return 1;

  dandanator_finish_insert( filename, 1 );
  machine_reset( 0 );

  return 0;
}

int
dandanator_set_programming_enabled( int enabled )
{
  int state_changed;

  if( !dandanator_inserted ) {
    ui_error( UI_ERROR_ERROR, "Insert a Dandanator cartridge first" );
    return 1;
  }

  state_changed = dandanator_programming_enabled != !!enabled;
  dandanator_programming_enabled = !!enabled;
  dandanator_clear_command_state();

  if( dandanator_programming_enabled ) {
    dandanator_current_slot = DANDANATOR_HIDDEN_SLOT;
  } else {
    dandanator_select_slot( 0 );
  }

  dandanator_sleep_state &=
    ~( DANDANATOR_COMMANDS_LOCKED | DANDANATOR_COMMANDS_DISABLED );
  dandanator_pending_apply_mapping = 0;
  dandanator_pending_cpu_reset = 0;
  dandanator_pending_cpu_nmi = 0;
  dandanator_apply_mapping();

  if( state_changed ) machine_reset( 0 );

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

  if( settings_current.dandanator_file ) {
    libspectrum_free( settings_current.dandanator_file );
    settings_current.dandanator_file = NULL;
  }

  dandanator_inserted = 0;
  dandanator_active = 0;
  dandanator_programming_enabled = 0;
  dandanator_reset_runtime_state();

  machine_current->ram.romcs = 0;

  ui_menu_activate( UI_MENU_ITEM_MEDIA_CARTRIDGE_DANDANATOR_EJECT, 0 );

  machine_reset( 0 );
}

static void
dandanator_finish_insert( const char *filename, int programming_enabled )
{
  if( dandanator_filename ) libspectrum_free( dandanator_filename );
  dandanator_filename = utils_safe_strdup( filename );

  settings_set_string( &settings_current.dandanator_file, filename );

  dandanator_inserted = 1;
  dandanator_programming_enabled = !!programming_enabled;
  dandanator_reset_runtime_state();
  if( dandanator_programming_enabled ) {
    dandanator_current_slot = DANDANATOR_HIDDEN_SLOT;
  }

  ui_menu_activate( UI_MENU_ITEM_MEDIA_CARTRIDGE_DANDANATOR_EJECT, 1 );
}

static void
dandanator_reset( int hard_reset )
{
  hard_reset = hard_reset;
  dandanator_active = 0;

  if( !dandanator_inserted || !dandanator_romset ) {
    ui_menu_activate( UI_MENU_ITEM_MEDIA_CARTRIDGE_DANDANATOR_EJECT, 0 );
    return;
  }

  dandanator_reset_runtime_state();
  if( dandanator_programming_enabled ) {
    dandanator_current_slot = DANDANATOR_HIDDEN_SLOT;
  }

  dandanator_active = 1;
  dandanator_update_romcs_state();

  ui_menu_activate( UI_MENU_ITEM_MEDIA_CARTRIDGE_DANDANATOR_EJECT, 1 );
}

static void
dandanator_memory_map( void )
{
  if( !dandanator_active || !dandanator_inserted ) return;

  if( dandanator_current_slot >= DANDANATOR_HIDDEN_SLOT ) return;

  memory_map_16k_read_write(
    0x0000,
    &dandanator_memory_map_romcs[
      dandanator_current_slot * MEMORY_PAGES_IN_16K
    ],
    0,
    1,
    0
  );
  memory_map_16k_read_write( 0x0000, dandanator_memory_map_shadow, 0, 0, 1 );
}

static void
dandanator_reset_runtime_state( void )
{
  memset( dandanator_shadow_ram, 0, sizeof( dandanator_shadow_ram ) );
  memset( dandanator_extended_ram, 0, sizeof( dandanator_extended_ram ) );
  memset( dandanator_extended_valid, 0, sizeof( dandanator_extended_valid ) );
  dandanator_current_slot = 0;
  dandanator_sleep_state = 0;
  dandanator_pending_return_slot = 0;
  dandanator_eeprom_sector = 0;
  dandanator_bridge_base = 0;
  dandanator_jedec_data_1555 = 0;
  dandanator_jedec_data_2aaa = 0;
  dandanator_jedec_data_1555_valid = 0;
  dandanator_jedec_data_2aaa_valid = 0;
  dandanator_extended_pointer0 = 0;
  dandanator_extended_pointer1 = 0;
  dandanator_extended_mode = 0;
  memset( dandanator_pic_ram, 0, sizeof( dandanator_pic_ram ) );
  dandanator_last_opcode = 0;
  dandanator_last_opcode_pc = 0;
  dandanator_pending_apply_mapping = 0;
  dandanator_pending_cpu_reset = 0;
  dandanator_pending_cpu_nmi = 0;
  dandanator_clear_command_state();
}

static void
dandanator_reset_extended_overlay( void )
{
  memset( dandanator_extended_ram + 0x2700, 0, 0x0c00 );
  memset( dandanator_extended_valid + 0x2700, 0, 0x0c00 );
}

static void
dandanator_store_extended_overlay( int command, int value, int offset )
{
  libspectrum_word address;

  if( offset < 0 || offset > 0xff ) return;

  address = ( (libspectrum_word)command << 8 ) | ( offset & 0xff );
  if( address >= DANDANATOR_SLOT_SIZE ) return;

  dandanator_extended_ram[address] = value & 0xff;
  dandanator_extended_valid[address] = 1;

  if( command == 50 ) dandanator_pic_ram[offset & 0xff] = value & 0xff;

}

static void
dandanator_update_romcs_state( void )
{
  machine_current->ram.romcs = dandanator_current_slot < DANDANATOR_HIDDEN_SLOT;
}

static void
dandanator_apply_mapping( void )
{
  if( !dandanator_inserted || !machine_current || !machine_current->memory_map )
    return;

  dandanator_update_romcs_state();
  machine_current->memory_map();
}

static libspectrum_word
dandanator_slot_checksum( int slot, libspectrum_word start, libspectrum_word end )
{
  libspectrum_word checksum = 0;
  size_t base;
  libspectrum_word offset;

  if( !dandanator_romset ) return 0;
  if( slot < 0 || slot >= DANDANATOR_SLOT_COUNT ) return 0;
  if( start >= DANDANATOR_SLOT_SIZE ) return 0;

  if( end >= DANDANATOR_SLOT_SIZE ) end = DANDANATOR_SLOT_SIZE - 1;
  if( end < start ) return 0;

  base = (size_t)slot * DANDANATOR_SLOT_SIZE;
  for( offset = start; offset <= end; offset++ ) {
    checksum += dandanator_romset[base + offset];
  }

  return checksum;
}

static void
dandanator_schedule_apply_mapping( int reset_cpu )
{
  dandanator_pending_apply_mapping = 1;
  dandanator_apply_mapping_at = tstates + DANDANATOR_COMMAND_DELAY;
  if( reset_cpu ) dandanator_pending_cpu_reset = 1;
}

static void
dandanator_select_slot( int slot )
{
  if( slot < 0 ) slot = 0;

  dandanator_current_slot = slot;

  if( dandanator_current_slot == 0 ) {
    dandanator_sleep_state &=
      ~( DANDANATOR_COMMANDS_LOCKED | DANDANATOR_COMMANDS_DISABLED );
  }
}

void
dandanator_before_opcode_fetch( void )
{
  if( !dandanator_pending_apply_mapping ) return;

  /* Emulate PIC processing delay: on real hardware the ROM switch
     takes about 35 T-states after the confirmation pulse. */
  if( tstates < dandanator_apply_mapping_at ) return;

  dandanator_apply_mapping();

  if( dandanator_pending_cpu_nmi ) event_add( 0, z80_nmi_event );
  if( dandanator_pending_cpu_reset ) z80_reset( 0 );

  dandanator_pending_apply_mapping = 0;
  dandanator_pending_cpu_reset = 0;
  dandanator_pending_cpu_nmi = 0;
}

static libspectrum_word
dandanator_branch_target( libspectrum_word pc, libspectrum_byte opcode )
{
  libspectrum_signed_byte displacement = readbyte_internal( pc + 1 );

  switch( opcode ) {
  case 0x10:
    if( z80.bc.b.h == 1 ) return pc + 2;
    return pc + 2 + displacement;

  case 0x18:
    return pc + 2 + displacement;

  case 0x20:
    return ( z80.af.b.l & DANDANATOR_FLAG_Z ) ? pc + 2 : pc + 2 + displacement;

  case 0x28:
    return ( z80.af.b.l & DANDANATOR_FLAG_Z ) ? pc + 2 + displacement : pc + 2;

  case 0x30:
    return ( z80.af.b.l & DANDANATOR_FLAG_C ) ? pc + 2 : pc + 2 + displacement;

  case 0x38:
    return ( z80.af.b.l & DANDANATOR_FLAG_C ) ? pc + 2 + displacement : pc + 2;

  default:
    return pc + 1;
  }
}

static int
dandanator_is_trap_opcode( libspectrum_byte opcode )
{
  switch( opcode ) {
  case 0x10:
  case 0x18:
  case 0x20:
  case 0x28:
  case 0x30:
  case 0x38:
    return 1;

  default:
    return 0;
  }
}

static int
dandanator_should_continue_trap( libspectrum_word target )
{
  if( target <= dandanator_last_opcode_pc ) return 1;

  return 0;
}

static void
dandanator_clear_command_state( void )
{
  memset( dandanator_command_bytes, 0, sizeof( dandanator_command_bytes ) );
  dandanator_command_stage = 0;
  dandanator_command_trap = 0;
}

static void
dandanator_commit_eeprom_bridge( void )
{
  size_t rom_offset;

  if( !dandanator_programming_enabled ) {
    dandanator_eeprom_sector = 0;
    return;
  }

  if( dandanator_bridge_base < 0 ||
      dandanator_bridge_base > DANDANATOR_SLOT_SIZE - 0x1000 ||
      ( dandanator_bridge_base & 0x0fff ) ) {
    return;
  }

  rom_offset = (size_t)dandanator_eeprom_sector << 12;
  if( rom_offset + 0x1000 > DANDANATOR_ROMSET_SIZE ) {
    return;
  }

  memcpy( dandanator_romset + rom_offset,
          dandanator_shadow_ram + dandanator_bridge_base,
          0x1000 );

  /* The SST39SF040 byte-program sequence writes JEDEC unlock bytes to
     shadow RAM at 0x1555 (0xAA then 0xA0) and 0x2AAA (0x55) before each
     actual data byte.  After the last byte in the sector is programmed,
     the shadow RAM at those addresses contains protocol residue, not the
     real data.  Patch the committed sector with the captured real data
     values that were written during the programming loop. */
  if( dandanator_bridge_base <= 0x1555 && 0x1555 < dandanator_bridge_base + 0x1000
      && dandanator_jedec_data_1555_valid ) {
    dandanator_romset[rom_offset + ( 0x1555 - dandanator_bridge_base )] =
      dandanator_jedec_data_1555;
  }
  if( dandanator_bridge_base <= 0x2aaa && 0x2aaa < dandanator_bridge_base + 0x1000
      && dandanator_jedec_data_2aaa_valid ) {
    dandanator_romset[rom_offset + ( 0x2aaa - dandanator_bridge_base )] =
      dandanator_jedec_data_2aaa;
  }
  dandanator_jedec_data_1555_valid = 0;
  dandanator_jedec_data_2aaa_valid = 0;
  dandanator_write_back_romset();
  dandanator_eeprom_sector = 0;
}

static int
dandanator_write_back_romset( void )
{
  if( !dandanator_filename || !dandanator_romset ) return 1;

  if( utils_write_file( dandanator_filename, dandanator_romset,
                        DANDANATOR_ROMSET_SIZE ) ) {
    ui_error( UI_ERROR_ERROR,
              "Couldn't write Dandanator image '%s'", dandanator_filename );
    return 1;
  }

  return 0;
}

static void
dandanator_finish_command( void )
{
  int command = dandanator_command_bytes[0];
  int param1 = dandanator_command_bytes[1];
  int param2 = dandanator_command_bytes[2];

  if( dandanator_sleep_state & DANDANATOR_COMMANDS_DISABLED ) {
    dandanator_clear_command_state();
    return;
  }

  if( ( dandanator_sleep_state & DANDANATOR_COMMANDS_LOCKED ) &&
      command != 46 ) {
    dandanator_clear_command_state();
    return;
  }

  if( command == 46 ) {
    if( param1 == param2 ) {
      if( param1 == 1 ) {
        dandanator_sleep_state &= ~DANDANATOR_COMMANDS_DISABLED;
        dandanator_sleep_state |= DANDANATOR_COMMANDS_LOCKED;
      } else if( param1 == 16 ) {
        dandanator_sleep_state &=
          ~( DANDANATOR_COMMANDS_LOCKED | DANDANATOR_COMMANDS_DISABLED );
      } else if( param1 == 31 ) {
        dandanator_sleep_state &= ~DANDANATOR_COMMANDS_LOCKED;
        dandanator_sleep_state |= DANDANATOR_COMMANDS_DISABLED;
      }
    }
    dandanator_clear_command_state();
    return;
  }

  if( command > 0 && command < 34 ) {
    dandanator_select_slot( command - 1 );
    dandanator_apply_mapping();
    dandanator_clear_command_state();
    return;
  }

  switch( command ) {
  case 34:
    dandanator_current_slot = DANDANATOR_HIDDEN_SLOT;
    dandanator_sleep_state &= ~DANDANATOR_COMMANDS_DISABLED;
    dandanator_sleep_state |= DANDANATOR_COMMANDS_LOCKED;
    dandanator_apply_mapping();
    break;

  case 36:
    dandanator_schedule_apply_mapping( 1 );
    break;

  case 40:
    dandanator_select_slot( param1 ? param1 - 1 : 0 );

    if( param2 & 0x08 ) {
      dandanator_sleep_state &= ~DANDANATOR_COMMANDS_LOCKED;
      dandanator_sleep_state |= DANDANATOR_COMMANDS_DISABLED;
    } else if( param2 & 0x04 ) {
      dandanator_sleep_state &= ~DANDANATOR_COMMANDS_DISABLED;
      dandanator_sleep_state |= DANDANATOR_COMMANDS_LOCKED;
    }

    if( param2 & 0x02 ) dandanator_pending_cpu_nmi = 1;

    dandanator_schedule_apply_mapping( param2 & 0x01 );
    break;

  case 48:
    if( param1 == 16 ) {
      dandanator_bridge_base = -1;
    } else if( param1 == 32 ) {
      dandanator_eeprom_sector = param2 & 0x7f;
    }
    break;

  case 39:
    dandanator_extended_pointer0 = 0;
    dandanator_extended_pointer1 = 0;
    dandanator_extended_mode = 0;
    break;

  case 41:
    dandanator_store_extended_overlay( command, param1, param2 );
    break;

  case 42:
    dandanator_store_extended_overlay( command, param1, param2 );
    break;

  case 43:
    dandanator_store_extended_overlay( command, param1, param2 );
    break;

  case 45:
    dandanator_store_extended_overlay( command, param1, param2 );
    dandanator_extended_mode =
      ( (libspectrum_word)param1 << 8 ) | param2;
    break;

  case 49:
    dandanator_store_extended_overlay( command, param1, param2 );
    break;

  case 50:
    dandanator_store_extended_overlay( command, param1, param2 );
    break;

  default:
    break;
  }

  dandanator_clear_command_state();
}

void
dandanator_pre_opcode( libspectrum_word pc, libspectrum_byte opcode )
{
  libspectrum_word target;

  if( !dandanator_inserted ) return;

  if( opcode == 0xc9 && dandanator_shadow_ram[0x1555] == 0xa0 ) {
    dandanator_commit_eeprom_bridge();
    dandanator_shadow_ram[0x1555] = 0;
  }

  if( opcode == 0xc9 && dandanator_pending_return_slot ) {
    dandanator_current_slot = dandanator_pending_return_slot - 1;
    dandanator_pending_return_slot = 0;
    dandanator_apply_mapping();
  }

  if( opcode == 0xfb ) {
    /* If a special command already reached the confirmation phase, execute
       it before clearing state.  The real hardware fires on the address-0
       write, so we must not silently discard an accumulated command. */
    if( dandanator_command_bytes[0] >= 40 && dandanator_command_stage >= 6 ) {
      dandanator_finish_command();
    } else {
      dandanator_clear_command_state();
    }
  }

  if( dandanator_command_trap && dandanator_is_trap_opcode( opcode ) ) {
    dandanator_command_trap--;
    if( !dandanator_command_trap ) {
      target = dandanator_branch_target( pc, opcode );
      if( dandanator_should_continue_trap( target ) ) {
        dandanator_command_trap = 1;
      } else {
        if( dandanator_command_stage & 1 ) dandanator_command_stage++;

        if( dandanator_command_stage > 6 ||
                 ( dandanator_command_bytes[0] > 0 &&
                     dandanator_command_bytes[0] < 40 ) ) {
          dandanator_finish_command();
        }
      }
    }
  }

  dandanator_last_opcode = opcode;
  dandanator_last_opcode_pc = pc;
}

int
dandanator_memory_read( libspectrum_word address, libspectrum_byte *value )
{
  if( !dandanator_inserted || !value ) return 0;
  if( address >= DANDANATOR_SLOT_SIZE ) return 0;

  /* Overlay data is only relevant for the menu slot (slot 0).  When a game
     slot is mapped, reads must return the game ROM data unchanged. */
  if( dandanator_current_slot != 0 ) return 0;

  if( !dandanator_extended_valid[address] ) {
    if( address >= 0x2900 && address < 0x2a00 ) {
      *value = 0x00;
      return 1;
    }
    return 0;
  }

  *value = dandanator_extended_ram[address];

  return 1;
}

void
dandanator_memory_write( libspectrum_word address, libspectrum_byte value )
{
  int index;

  if( !dandanator_inserted ) return;

  /* The SST39SF040 byte-program sequence uses LD (BC),A (opcode 0x02) for
     JEDEC unlock writes and LD (DE),A (opcode 0x12) for the actual data
     byte.  Capture the real data when a data-phase write targets 0x1555
     or 0x2AAA so the EEPROM commit can patch out the JEDEC protocol
     residue that would otherwise corrupt those bytes. */
  if( dandanator_last_opcode == 0x12 ) {
    if( address == 0x1555 ) {
      dandanator_jedec_data_1555 = value;
      dandanator_jedec_data_1555_valid = 1;
    } else if( address == 0x2aaa ) {
      dandanator_jedec_data_2aaa = value;
      dandanator_jedec_data_2aaa_valid = 1;
    }
  }

  if( ( dandanator_last_opcode == 0x12 || dandanator_last_opcode == 0x77 ) &&
      dandanator_shadow_ram[0x1555] == 0xaa ) {
    dandanator_bridge_base = address;
    dandanator_shadow_ram[0x1555] = 0;
  }

  if( address >= 0x0004 ) return;

  if( dandanator_last_opcode != 0x32 && dandanator_last_opcode != 0x77 )
    return;

  /* When commands are disabled the PIC ignores all writes; skip command
     detection to avoid polluting state during game decompression. */
  if( dandanator_sleep_state & DANDANATOR_COMMANDS_DISABLED ) return;

  index = ( dandanator_command_stage | 1 ) / 2;
  if( index < 0 || index >= 4 ) return;

  dandanator_command_bytes[index]++;
  dandanator_command_stage |= 1;
  dandanator_command_trap = 1;

  /* A write to address 0 is the confirmation pulse for multi-byte commands.
     The real hardware triggers on this write immediately.  Some firmware
     (e.g. the dandanator-msft menusystem) does NOT follow the confirmation
     with a branch-based delay loop, so the branch-trap mechanism in
     dandanator_pre_opcode would never fire.  Finish the command now. */
  if( address == 0 && dandanator_command_bytes[0] >= 40 &&
      dandanator_command_stage >= 6 ) {
    dandanator_finish_command();
  }
}
