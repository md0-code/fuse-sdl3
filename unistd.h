#ifndef FUSE_WIN32_UNISTD_H
#define FUSE_WIN32_UNISTD_H

#ifdef WIN32

#include <BaseTsd.h>
#include <stdlib.h>
#include <io.h>
#include <process.h>
#include <direct.h>

#ifndef _SSIZE_T_DEFINED
typedef SSIZE_T ssize_t;
#define _SSIZE_T_DEFINED
#endif

#ifndef F_OK
#define F_OK 0
#endif

#ifndef W_OK
#define W_OK 2
#endif

#ifndef R_OK
#define R_OK 4
#endif

#ifndef X_OK
#define X_OK 0
#endif

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif

#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

#define access _access
#define chdir _chdir
#define close _close
#define dup _dup
#define dup2 _dup2
#define fileno _fileno
#define fsync _commit
#define getcwd _getcwd
#define getpid _getpid
#define isatty _isatty
#define lseek _lseek
#define read _read
#define rmdir _rmdir
#define unlink _unlink
#define write _write

static inline int geteuid(void)
{
	return 1;
}

static inline int getuid(void)
{
	return 1;
}

static inline int setuid(int uid)
{
	(void)uid;
	return 0;
}

static inline int setenv(const char *name, const char *value, int overwrite)
{
	if (!overwrite && getenv(name)) {
		return 0;
	}

	return _putenv_s(name, value ? value : "");
}

#endif

#endif
