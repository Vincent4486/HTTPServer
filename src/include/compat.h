/* Small portability header to smooth differences between Windows (MSVC/MinGW) and POSIX */
#ifndef HTTP_COMPAT_H
#define HTTP_COMPAT_H

#include <stdint.h>
#include <stddef.h>

#ifdef _WIN32
/* Use pointer-sized signed type for ssize_t on Windows */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <io.h>
#include <direct.h>
#include <sys/stat.h>
#include <errno.h>

typedef intptr_t ssize_t;

#define PATH_SEPARATOR '\\'
#define PATH_SEPARATOR_STR "\\"

/* map common POSIX file I/O names to MSVCRT underscored names */
#define read _read
#define write _write
#define close _close
#define open _open
#define O_RDONLY _O_RDONLY
#define strcasecmp _stricmp

/* realpath -> _fullpath on Windows */
#ifndef realpath
#define realpath(N,R) _fullpath((R),(N),_MAX_PATH)
#endif

/* mkdir wrapper: map to _mkdir which ignores mode */
#ifndef mkdir
#define mkdir(p,mode) _mkdir(p)
#endif

/* socket helpers */
#define CLOSE_SOCKET(s) closesocket(s)
#define SOCKET_ERRNO() WSAGetLastError()

#else /* POSIX */
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
/* sockets for POSIX */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PATH_SEPARATOR '/'
#define PATH_SEPARATOR_STR "/"

typedef off_t compat_off_t;

#define CLOSE_SOCKET(s) close(s)
#define SOCKET_ERRNO() errno

#endif /* _WIN32 */

#endif /* HTTP_COMPAT_H */
