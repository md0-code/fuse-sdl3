/* fuse_console_launcher.c: console wrapper for fuse.exe on Windows */

#include <windows.h>

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static const wchar_t*
skip_program_name( const wchar_t *command_line )
{
  int quoted = 0;

  while( *command_line ) {
    if( *command_line == L'"' ) {
      quoted = !quoted;
    } else if( !quoted && ( *command_line == L' ' || *command_line == L'\t' ) ) {
      break;
    }

    command_line++;
  }

  while( *command_line == L' ' || *command_line == L'\t' ) command_line++;

  return command_line;
}

int
wmain( void )
{
  const wchar_t *args;
  DWORD exit_code;
  PROCESS_INFORMATION process_information;
  STARTUPINFOW startup_information;
  wchar_t *child_command_line;
  wchar_t *exe_path;
  wchar_t module_path[MAX_PATH];
  size_t args_length, exe_length;

  if( !GetModuleFileNameW( NULL, module_path, MAX_PATH ) ) return 1;

  exe_path = wcsrchr( module_path, L'.' );
  if( !exe_path ) return 1;
  wcscpy( exe_path, L".exe" );

  args = skip_program_name( GetCommandLineW() );
  exe_length = wcslen( module_path );
  args_length = wcslen( args );

  child_command_line = (wchar_t*)malloc( sizeof(wchar_t) * ( exe_length + args_length + 4 ) );
  if( !child_command_line ) return 1;

  if( args_length ) {
    swprintf( child_command_line, exe_length + args_length + 4, L"\"%ls\" %ls",
              module_path, args );
  } else {
    swprintf( child_command_line, exe_length + 4, L"\"%ls\"", module_path );
  }

  ZeroMemory( &startup_information, sizeof( startup_information ) );
  startup_information.cb = sizeof( startup_information );
  GetStartupInfoW( &startup_information );

  ZeroMemory( &process_information, sizeof( process_information ) );

  if( !CreateProcessW( module_path, child_command_line, NULL, NULL, TRUE, 0,
                       NULL, NULL, &startup_information, &process_information ) ) {
    free( child_command_line );
    return 1;
  }

  free( child_command_line );

  WaitForSingleObject( process_information.hProcess, INFINITE );
  if( !GetExitCodeProcess( process_information.hProcess, &exit_code ) ) {
    exit_code = 1;
  }

  CloseHandle( process_information.hThread );
  CloseHandle( process_information.hProcess );

  return (int)exit_code;
}