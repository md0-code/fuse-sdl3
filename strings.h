#ifndef FUSE_WIN32_STRINGS_H
#define FUSE_WIN32_STRINGS_H

#ifdef WIN32

#include <string.h>

#define strcasecmp _stricmp
#define strncasecmp _strnicmp

#else

#include_next <strings.h>

#endif

#endif
