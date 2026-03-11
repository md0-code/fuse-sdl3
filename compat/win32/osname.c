/* osname.c: Get a representation of the OS we're running on
   Copyright (c) 1999-2007 Philip Kendall
   Copyright (c) 2015 Sergio Baldoví

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

   Author contact information:

   E-mail: philip-fuse@shadowmagic.org.uk

*/

#include <config.h>

#include <stdio.h>
#include <windows.h>
#include <winternl.h>

#include "ui/ui.h"

int compat_osname( char *osname, size_t length )
{
  typedef LONG (WINAPI *rtl_get_version_fn)( PRTL_OSVERSIONINFOW );

  RTL_OSVERSIONINFOW buf;
  HMODULE ntdll;
  rtl_get_version_fn rtl_get_version;
  char service_pack[ 128 ] = "";

  ZeroMemory( &buf, sizeof( buf ) );
  buf.dwOSVersionInfoSize = sizeof( buf );

  ntdll = GetModuleHandleW( L"ntdll.dll" );
  rtl_get_version = ntdll ?
    (rtl_get_version_fn)GetProcAddress( ntdll, "RtlGetVersion" ) : NULL;

  if( !rtl_get_version || rtl_get_version( &buf ) != 0 ) {
    ui_error( UI_ERROR_ERROR, "error getting system information." );
    return 1;
  }

  if( buf.szCSDVersion[ 0 ] ) {
    WideCharToMultiByte( CP_UTF8, 0, buf.szCSDVersion, -1,
                         service_pack, sizeof( service_pack ), NULL, NULL );
  }

  snprintf( osname, length, "Windows NT %i.%i build %i%s%s",
	    (int)buf.dwMajorVersion, (int)buf.dwMinorVersion,
	    (int)buf.dwBuildNumber,
	    service_pack[ 0 ] ? " " : "", service_pack );

  return 0;
}
