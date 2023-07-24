/*
 * Copyright 1995, 1996 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*
 * Standard headers
 *
 * Note: where both a NEED_XXX and HAVE_XXX are recognized, the form to
 * use is:
 *
 *   # ifdef OS_YYY
 *   #   define HAVE_XXX
 *   #   ifdef NEED_XXX
 *   #     include <xxx.h>
 *   #   endif
 *   # endif
 *
 *  This causes the HAVE_XXX macro to be defined even if the NEED_XXX macro 
 *  is not; this protects us from problems caused by #ifdef HAVE_XXX in
 *  header files.
 */

# ifndef P4STDHDRS_H
# define P4STDHDRS_H

# ifdef OS_VMS
# define _POSIX_EXIT  // to get exit status right from stdlib.h
# endif	

# include <stdio.h>
# include <string.h>
# include <stdlib.h>

# if !defined( OS_QNX ) && !defined( OS_VMS )
# include <memory.h>
# endif
# ifdef OS_NEXT
# include <libc.h>
# endif

/*
 * NEED_ACCESS - access()
 * NEED_BRK - brk()/sbrk()
 * NEED_CHDIR - chdir()
 * NEED_CRITSEC - Critical Sections, just Windows for now
 * NEED_DBGBREAK - DebugBreak(), just Windows for now
 * NEED_EBCDIC - __etoa, __atoe
 * NEED_ERRNO - errno, strerror
 * NEED_FILE - write(), unlink(), etc
 * NEED_FCNTL - O_XXX flags
 * NEED_FLOCK - LOCK_UN, etc
 * NEED_FORK - fork(), waitpid()
 * NEED_FSYNC - fsync()
 * NEED_GETPID - getpid()
 * NEED_GETUID - getuid(),setuid() etc.
 * NEED_IOCTL - ioctl() call and flags for UNIX
 * NEED_MKDIR - mkdir()
 * NEED_MIMALLOC - mimalloc initialization
 * NEED_MMAP - mmap()
 * NEED_OPENDIR - opendir(), etc
 * NEED_POPEN - popen(), pclose()
 * NEED_READLINK - readlink()
 * NEED_SIGNAL - signal()
 * NEED_SLEEP - Sleep()
 * NEED_SMARTHEAP - Smartheap Initialization
 * NEED_STAT - stat()
 * NEED_STATFS - statfs()
 * NEED_STATVFS - statvfs()
 * NEED_SOCKETPAIR - pipe(), socketpair()
 * NEED_SOCKET_IO - various socket stuff
 * NEED_SRWLOCK - headers for authwinldapconn.cc
 * NEED_SYSLOG - syslog()
 * NEED_TIME - time(), etc
 * NEED_TIME_HP - High Precision time, such as gettimeofday, clock_gettime, etc.
 * NEED_TYPES - off_t, etc (always set)
 * NEED_UTIME - utime()
 * NEED_WIN32FIO - Native Windows file I/O
 */

# if defined( NEED_ACCESS ) || \
	defined( NEED_CHDIR ) || \
	defined( NEED_EBCDIC ) || \
	defined( NEED_FILE ) || \
	defined( NEED_FSYNC ) || \
	defined( NEED_FORK ) || \
	defined( NEED_GETCWD ) || \
	defined( NEED_GETPID ) || \
	defined( NEED_GETPWUID ) || \
	defined( NEED_GETUID ) || \
	defined( NEED_BRK ) || \
	defined( NEED_READLINK ) || \
	defined( NEED_SOCKET_IO ) || \
	defined( NEED_SLEEP )

# ifndef OS_NT
# include <unistd.h>
# endif

# ifdef OS_VAXVMS
# include <unixio.h>
# endif

# endif

# if defined( NEED_BRK )
# if !defined( OS_NT ) && !defined( MAC_MWPEF ) && \
     !defined( OS_AS400 ) && !defined( OS_MVS ) && \
     !defined( OS_LINUX ) && !defined( OS_DARWIN ) && \
     !defined( OS_MACOSX )
# define HAVE_BRK
# endif
# endif

# if defined( NEED_LSOF )
# if defined( OS_LINUX ) || defined( OS_DARWIN ) || defined( OS_MACOSX )
# define HAVE_LSOF
# endif
# endif

# if defined( NEED_GETUID )
# if defined ( OS_MACOSX ) || defined ( OS_DARWIN ) || defined ( __unix__ )
# define HAVE_GETUID
# endif
# endif

# if defined( NEED_EBCDIC ) 
# if defined( OS_AS400 )
# include <ebcdic.h>
# endif
# endif

# if defined( NEED_ACCESS ) || defined( NEED_CHDIR )
# if defined( OS_NT ) || defined( OS_OS2 )
# include <direct.h>
# endif
# endif

# if defined( NEED_ERRNO )
# ifdef OS_AS400
extern int errno;
# endif
# include <errno.h>
# endif

# if defined(NEED_FILE) || defined(NEED_FSYNC)
# ifdef OS_NT
# include <io.h>
# endif
# endif

# ifdef NEED_FCNTL
# include <fcntl.h>
# endif

// This must be one of the first occurrences for including windows.h
// so that _WIN32_WINNT will flavor definitions.
# ifdef OS_NT
# define WIN32_LEAN_AND_MEAN
// current default is Win7; IPv6 code needs these macros >= WinVista
// before including this file (see net/netportipv6.h for details)
# if !defined(NTDDI_VERSION) || (NTDDI_VERSION < 0x0501000)
#   undef NTDDI_VERSION
#   define NTDDI_VERSION    0x06010000
# endif // NTDDI_VERSION
# if !defined(_WIN32_WINNT) || (_WIN32_WINNT < 0x0601)
#   undef _WIN32_WINNT
#   define _WIN32_WINNT     0x0601
# endif // _WIN32_WINNT
# if !defined(WINVER) || (WINVER < _WIN32_WINNT)
#   undef WINVER
#   define WINVER           _WIN32_WINNT
# endif // WINVER
# endif // OS_NT

# ifdef OS_NT
# define HAVE_CRITSEC
# ifdef NEED_CRITSEC
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# endif // NEED_CRITSEC
# endif // OS_NT

// Definitions for AcquireSRWLock and ReleaseSRWLock.
//
# ifdef OS_NT
# define HAVE_SRWLOCK
# ifdef NEED_SRWLOCK
# if (_MSC_VER >= 1800) && (!defined(_WIN32_WINNT) || (_WIN32_WINNT < 0x0600))
# undef _WIN32_WINNT
# define _WIN32_WINNT 0x0600
# endif // _MSC_VER
# include <windows.h>
# include <winldap.h>
# include <winber.h>
# include <wincrypt.h>
# include <rpc.h>
# include <rpcdce.h>
# endif // NEED_SRWLOCK
# endif // OS_NT

# ifdef OS_NT
# define HAVE_DBGBREAK
# ifdef NEED_DBGBREAK
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# endif // NEED_DBGBREAK
# endif // OS_NT

# include "malloc_override.h"

# ifdef MEM_DEBUG
#   ifdef USE_MIMALLOC
#     define NEED_MIMALLOC
#   endif
#   ifdef USE_SMARTHEAP
#     define NEED_SMARTHEAP
#   endif
# endif

// Mimalloc instrumentation.
# ifdef NEED_MIMALLOC
#   if defined( USE_MIMALLOC )
#     define HAVE_MIMALLOC
#   endif // USE_MIMALLOC
# endif // NEED_MIMALLOC

// Smart Heap instrumentation.
# ifdef NEED_SMARTHEAP
#   if defined( USE_SMARTHEAP )
#     ifdef OS_NT
#       include <windows.h>
#     endif // OS_NT
#     include <smrtheap.h>
#     define HAVE_SMARTHEAP
#   endif // USE_SMARTHEAP
# endif // NEED_SMARTHEAP

# ifdef NEED_FLOCK
# ifdef OS_NT
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <io.h>
# endif
# ifdef __unix__
# include <sys/file.h>
# ifdef LOCK_UN
extern "C" int flock( int, int );
# endif
# endif
# endif

# if !defined( OS_OS2 ) && !defined( MAC_MWPEF ) && \
     !defined( OS_NT )  && !defined( OS_AS400 ) && \
     !defined( OS_VMS )
# define HAVE_FORK
# ifdef NEED_FORK
# ifdef OS_MACHTEN
# include "/usr/include/sys/wait.h"
# endif 
# include <sys/wait.h>
# endif
# endif

# if !defined( OS_BEOS ) && !defined( OS_NT ) && \
     !defined( OS_OS2 )
# define HAVE_FSYNC
# endif

# ifdef NEED_GETCWD
# ifdef OS_NEXT
# define getcwd( b, s ) getwd( b )
# endif
# if defined(OS_OS2) || defined(OS_NT)
extern "C" char *getcwd( char *buf, size_t size );
# endif
# ifdef OS_VMS
# include <unixlib.h>
# endif
# endif 

# if !defined(OS_OS2)
# define HAVE_GETHOSTNAME

# ifdef NEED_GETHOSTNAME

# ifdef OS_BEOS
# include <net/netdb.h>
# endif

# ifdef OS_VMS
# include <socket.h>
# endif

# if defined(OS_PTX) || \
	defined(OS_QNX) || \
	defined(OS_AIX32) || \
	defined(OS_NCR) || \
	defined(OS_UNIXWARE2)

extern "C" int gethostname( char * name, int namelen );

# endif

# if defined(OS_NT) 
extern "C" int __stdcall gethostname( char * name, int namelen );
# endif

# endif /* NEED_GETHOSTNAME */

# endif

# ifdef NEED_GETPID
# ifdef OS_NT
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# else
# if defined(OS_OS2)
# include <process.h>
# endif /* OS2 */
# endif	/* NT */
# endif	/* GETPID */

# if !defined( OS_VMS ) && !defined( OS_NT ) && !defined( OS_BEOS ) && \
	!defined( MAC_MWPEF ) && !defined( OS_OS2 )
# define HAVE_GETPWUID
# ifdef NEED_GETPWUID
# include <pwd.h>
# endif 
# endif /* UNIX */

# ifdef NEED_IOCTL
# ifndef OS_NT
# include <sys/ioctl.h>
# endif /* NT */
# endif /* IOCTL */

# ifdef ACCESSPERMS
    #define PERMSMASK ACCESSPERMS
# else
    #ifdef  S_IAMB
        #define PERMSMASK S_IAMB
    #else
        #define PERMSMASK 0x1FF
    #endif
# endif
# if defined(NEED_MKDIR) || defined(NEED_STAT) || defined(NEED_CHMOD)

# ifdef OS_OS2
# include <direct.h>
# endif

# include <sys/stat.h>

# ifndef S_ISLNK /* NCR */
# define S_ISLNK(m) (((m)&S_IFMT)==S_IFLNK)
# endif

# ifndef S_ISDIR /* NEXT */
# define S_ISDIR(m) (((m)&S_IFMT)==S_IFDIR)
# define S_ISREG(m) (((m)&S_IFMT)==S_IFREG)
# endif

# ifdef OS_NT
# define PERM_0222 (S_IWRITE)
# define PERM_0266 (S_IWRITE)
# define PERM_0666 (S_IRUSR|S_IWRITE)
# define PERM_0777 (S_IRUSR|S_IWRITE|S_IEXEC)
# define PERM_0700 ( S_IRUSR | S_IWUSR | S_IXUSR )
# define PERM_0600 ( S_IRUSR | S_IWUSR )
# define PERM_0500 ( S_IRUSR | S_IXUSR )
# define PERM_0400 ( S_IRUSR )
# ifndef S_IRUSR
# define S_IRUSR S_IREAD
# define S_IWUSR S_IWRITE
# define S_IXUSR S_IEXEC
# endif	/* S_IRUSR */
# endif

# ifndef PERM_0222
# define PERM_0222 (S_IWUSR | S_IWGRP | S_IWOTH)
# define PERM_0266 (S_IWUSR | (S_IRGRP|S_IWGRP) | (S_IROTH|S_IWOTH))
# define PERM_0666 ((S_IRUSR|S_IWUSR) | (S_IRGRP|S_IWGRP) | (S_IROTH|S_IWOTH))
# define PERM_0777 (S_IRWXU | S_IRWXG | S_IRWXO)
# define PERM_0700 ( S_IRWXU )
# define PERM_0600 ( S_IRUSR | S_IWUSR )
# define PERM_0500 ( S_IRUSR | S_IXUSR )
# define PERM_0400 ( S_IRUSR )
# endif

# endif

# if defined(NEED_STATVFS)

# ifdef OS_NT
# else
# include <sys/statvfs.h>
# endif

# ifdef OS_SOLARIS
# define HAVE_STATVFS_BASETYPE
# endif

# endif

# if defined(NEED_STATFS)

# ifdef OS_LINUX
# define HAVE_STATFS
# include <sys/statfs.h>
# endif

# if defined(OS_DARWIN) || defined(OS_MACOSX) || defined(OS_FREEBSD)
# define HAVE_STATFS
# define HAVE_STATFS_FSTYPENAME
# include <sys/param.h>
# include <sys/mount.h>
# endif

# endif /* NEED_STATFS */

/* Many users don't define NEED_MMAP -- so we always find out */
/* added AIX 5.3 - mmap region getting corrupted */

# if !defined( OS_AS400 ) && !defined( OS_BEOS ) && \
	!defined( OS_HPUX68K ) && \
	!defined( OS_MACHTEN ) && !defined( OS_MVS ) && \
	!defined( OS_VMS62 ) && !defined( OS_OS2 ) && \
	!defined( OS_NEXT ) && !defined( OS_NT ) && \
	!defined( OS_QNX ) && !defined( OS_UNICOS ) && \
	!defined( OS_MPEIX ) && !defined( OS_QNXNTO ) && \
	!defined( OS_ZETA ) && \
	!defined( OS_AIX53 ) && !defined( OS_LINUXIA64 )

# define HAVE_MMAP
# ifdef NEED_MMAP
# ifdef OS_HPUX9
extern "C" caddr_t mmap(const caddr_t, size_t, int, int, int, off_t);
extern "C" int munmap(const caddr_t, size_t);
# endif /* HPUX9 */
# include <sys/mman.h>
# define HAVE_MMAP_BTREES
# endif /* NEED_MMAP */
# endif /* HAVE_MMAP */

# ifdef NEED_OPENDIR
# include <dirent.h>
# endif

# ifdef NEED_POPEN
# ifdef OS_NT
# define popen _popen
# define pclose _pclose
# endif
# ifdef OS_MVS
extern "C" int pclose(FILE *);
extern "C" FILE *popen(const char *, const char *);
# endif
# endif

# ifdef NEED_SIGNAL
# ifdef OS_OSF
# include "/usr/include/sys/signal.h"
# else
# include <signal.h>
# endif
# if defined( OS_NEXT ) || defined( OS_MPEIX )
// broken under gcc 2.5.8
# undef SIG_IGN
# undef SIG_DFL
# define SIG_DFL         (void (*)(int))0
# define SIG_IGN         (void (*)(int))1
# endif
# endif

/*
 * This definition differs from the conventional approach because we test
 * on AF_UNIX and that's not defined until after we include socket.h. So,
 * we use the old scheme of only defining HAVE_SOCKETPAIR if NEED_SOCKETPAIR
 * is set and the call exists.
 */
# ifdef NEED_SOCKETPAIR
# if defined( OS_NT )
# define WINDOWS_LEAN_AND_MEAN
# include <windows.h>
# include <process.h>
# elif defined( OS_BEOS )
# include <net/socket.h>
# else
# include <sys/socket.h>
# endif
# if defined( AF_UNIX ) && \
    !defined( OS_AS400 ) && \
    !defined( OS_NT ) && \
    !defined( OS_QNXNTO ) && \
    !defined( OS_OS2 ) && \
    !defined( OS_VMS )
# define HAVE_SOCKETPAIR
# if defined(OS_MACHTEN) || defined(OS_AIX32) || defined(OS_MVS)
extern "C" int socketpair(int, int, int, int*);
# endif
# endif
# endif

# ifdef NEED_SYSLOG
#  if defined( __unix__ )
#   define HAVE_SYSLOG
#   include <syslog.h>
#  elif defined( OS_NT )
#   define HAVE_EVENT_LOG
#   include <windows.h>
#  endif
# endif

# if defined(NEED_TIME) || defined(NEED_UTIME)
# include <time.h>
# endif

# if defined(NEED_TIME_HP)
#    if defined( OS_LINUX )
#       define HAVE_CLOCK_GETTIME
#if defined(__GLIBC__) && defined(__GLIBC_PREREQ)
#       if ( __GLIBC_PREREQ( 2, 10 ) && \
             ( defined(_BSD_SOURCE) || \
               _XOPEN_SOURCE >= 700  || \
               _POSIX_C_SOURCE >= 200809L ) ) || \
           ( !__GLIBC_PREREQ( 2, 10 ) && \
             __GLIBC_PREREQ( 2, 6 ) && \
             defined(_ATFILE_SOURCE) )
#            define HAVE_UTIMENSAT
#       else
#           define HAVE_GETTIMEOFDAY
#           include <sys/time.h>
#       endif
#else
#            define HAVE_UTIMENSAT
#endif
#    elif defined( OS_NT )
#       define WIN32_LEAN_AND_MEAN
#       include <windows.h>
#       define HAVE_GETSYSTEMTIME
#    else
#       define HAVE_GETTIMEOFDAY
#       include <sys/time.h>
#    endif
# endif

# if defined(NEED_TYPES) || 1

# if defined ( MAC_MWPEF )
# include <stat.h>
// because time_t is __std(time_t)
using namespace std;
# else
# include <sys/types.h>
# endif

# endif

# ifndef OS_VMS
# define HAVE_UTIME
# ifdef NEED_UTIME
# if ( defined( OS_NT ) || defined( OS_OS2 ) ) && !defined(__BORLANDC__)
# include <sys/utime.h>
# else
# include <utime.h>
# endif
# endif

# define HAVE_UTIMES
# ifdef NEED_UTIMES
# if ( defined( OS_NT ) || defined( OS_OS2 ) ) && !defined(__BORLANDC__)
# include <sys/utime.h>
# else
# include <sys/types.h>
# include <utime.h>
# endif
# endif
# endif

# ifdef NEED_SLEEP
# ifdef OS_NT
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# define sleep(x) Sleep((x) * 1000)
# define msleep(x) Sleep(x)
# ifndef OS_MINGW
typedef unsigned long useconds_t;
# endif
# else
// Assumeing usleep exists everywhere other than Windows
# define msleep(x) usleep((x) * 1000)
# endif
# endif

# ifdef NEED_WINDOWSH
# ifdef OS_NT
# include <windows.h>
# endif
# endif

/*
 * HAVE_TRUNCATE -- working truncate() call
 * HAVE_SYMLINKS -- OS supports SYMLINKS
 */

# define HAVE_SYMLINKS
# if defined( OS_OS2 ) || \
	defined ( MAC_MWPEF ) || \
	defined( OS_VMS ) || \
	defined( OS_INTERIX )
# undef HAVE_SYMLINKS
# endif

# define HAVE_TRUNCATE
# if defined( OS_OS2 ) || \
	defined( MAC_MWPEF ) || \
	defined( OS_BEOS ) || \
	defined( OS_QNX ) || \
	defined( OS_SCO ) || \
	defined( OS_VMS ) || \
	defined( OS_INTERIX ) || \
	defined( OS_MVS ) || \
	defined( OS_MPEIX ) || \
	defined( OS_AS400 )
# undef HAVE_TRUNCATE
# endif

/* These systems have no memccpy() or a broken one */

# if defined( OS_AS400 ) || defined( OS_BEOS ) || defined( OS_FREEBSD ) || \
	defined( OS_MACHTEN ) || defined( OS_MVS ) || \
	defined( OS_VMS62 ) || defined( OS_RHAPSODY ) || defined( OS_ZETA )
	
# define BAD_MEMCCPY
extern "C" void *memccpy(void *, const void *, int, size_t);
# endif

/* SUNOS has old headers, bless its heart */

# ifdef OS_SUNOS
# define memmove(d, s, c) bcopy(s, d, c)

extern "C" {
void bcopy(const void *src, void *dst, size_t len);
int closelog();
int fsync(int fd);
int ftruncate(int fd, off_t length);
int gethostname(char *name, int namelen);
int getrusage(int who, struct rusage *rusage);
int gettimeofday(struct timeval *tp, struct timezone *tzp);
int lstat(const char *path, struct stat *sb);
int munmap(void *addr, size_t len);
int openlog(const char *ident, int logopt, int facility);
int readlink(const char *path, char *buf, int bufsiz);
caddr_t sbrk(int inc);
int socketpair(int d, int type, int protocol, int *sv);
int symlink(const char *name1, const char *name2);
int syslog(int priority, const char *message ... );
int tolower(int c);
int toupper(int c);
int truncate(const char *path, off_t length);
} ;

# endif

/*
 * MT_STATIC - static multithreaded data
 */

# ifdef OS_NT
#   define MT_STATIC static __declspec(thread)
#   define P4MT __declspec(thread)
# elif !defined( OS_BEOS ) && \
       !defined( OS_AS400 ) && \
       !defined( OS_VMS )
#   ifdef NEED_THREADS
#     define HAVE_PTHREAD
#     include <pthread.h>
#   endif
#   if ( defined( OS_DARWIN ) && OS_VER < 140 ) || \
       ( defined( OS_MACOSX ) && OS_VER < 1010 )
#     define MT_STATIC static
#     define P4MT
#   else
#     define MT_STATIC static __thread
#     define P4MT __thread
#   endif
# else
#   define MT_STATIC static
#   define P4MT
# endif

/*
 * Illegal characters in a filename, includes %
 * as escape character.  Used when creating an
 * archive file in the spec depot
 */

# ifdef OS_NT
# define BadSpecFileCharList "%/<>:|\\"
# else
# define BadSpecFileCharList "%/"
# endif

/*
 * LineType - LF (raw), CR, CRLF, lfcrlf (LF/CRLF)
 */

enum LineType { LineTypeRaw, LineTypeCr, LineTypeCrLf, LineTypeLfcrlf };

# ifdef USE_CR
# define LineTypeLocal LineTypeCr
# endif
# ifdef USE_CRLF
# define LineTypeLocal LineTypeCrLf
# endif
# ifndef LineTypeLocal
# define LineTypeLocal LineTypeRaw
# endif

/*
 * P4INT64 - a 64 bit int
 */

# if !defined( OS_MVS ) && \
     !defined( OS_OS2 ) && \
     !defined( OS_QNX )
# define HAVE_INT64
# ifdef OS_NT
# define P4INT64 __int64
# else
# define P4INT64 long long
# endif
# endif

# ifndef P4INT64
# define P4INT64 int
# endif

/*
 * offL_t - size of files or offsets into files
 */

typedef P4INT64 offL_t;

/*
 * We use p4size_t rather than size_t in space-critical places such as
 * StrPtr and StrBuf. 4GB ought to be enough for anyone, says Mr. Gates...
 */

typedef unsigned int p4size_t;

# if defined(OS_MACOSX) && OS_VER < 1010
# define FOUR_CHAR_CONSTANT(_a, _b, _c, _d)       \
        ((UInt32)                                 \
        ((UInt32) (_a) << 24) |                   \
        ((UInt32) (_b) << 16) |                   \
        ((UInt32) (_c) <<  8) |                   \
        ((UInt32) (_d)))
# endif

/* 
 * B&R's NTIA64 build machine doesn't define vsnprintf, 
 * but it does define _vsnprintf. Use that one instead.
 */
# ifdef OS_NTIA64
# define vsnprintf _vsnprintf 
# endif

# if defined(_MSC_VER) && _MSC_VER < 1900 || \
     !defined(_MSC_VER) && defined(_MSC_FULL_VER)
# define strtoll _strtoi64
# endif

// C++11 or higher
# if __cplusplus >= 201103L
#  ifndef HAS_CPP11
#   define HAS_CPP11
#  endif
# endif

// C++14 or higher
# if __cplusplus >= 201402L
#   define HAS_CPP14
# endif

// C++17 or higher
# if __cplusplus >= 201703L
#   define HAS_CPP17
# endif

# if defined(_MSC_VER) && _MSC_VER < 1900
#  define HAS_BROKEN_CPP11
# endif

# ifdef HAS_CPP11
#   define HAS_PARALLEL_SYNC_THREADS
# endif

# if defined(HAS_CPP14) && defined(USE_EXTENSIONS) && USE_EXTENSIONS == 1
#   define HAS_EXTENSIONS
# endif

# include "sanitizers.h"

# ifdef __GNUC__
# define GCC_VERSION (__GNUC__ * 10000 \
                     + __GNUC_MINOR__ * 100 \
                     + __GNUC_PATCHLEVEL__)
// E.g.  GCC > 3.2.0
// #if GCC_VERSION > 30200
# endif

# ifdef OS_NT

# if defined( NEED_WIN32FIO )
# define HAVE_WIN32FIO

// Must define _AMD64_, _X86_, _IA64_ or _ARM_ in order to use winnt.h
// directly without the "No Target Architecture" error.  Use a macro
// Perforce defined to caused one of those 4 to be defined.
//
# ifdef OS_NTX64
# define _AMD64_
# endif // OS_NTX64
# ifdef OS_NTX86
# define _X86_
# endif // OS_NTX86

# ifndef OS_MINGW
# ifndef NOMINMAX
#   define NOMINMAX
# endif // !NOMINMAX
# include <windef.h>
# include <winnt.h>
# else
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# endif // !OS_MINGW

// Don't like defining our own handle value here.  The problem is
// that this is defined in the header Pdh.h, which then causes the
// Windows header rpc.h to be included.  The P4 rpc.h is found
// instead and things get very messy.
//
# ifndef INVALID_HANDLE_VALUE
# define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)
# endif

// This is a special token for invalid handle values when running
// as a Windows Service.  See fdutil.cc for more detail.
//
# define INVALID_HANDLE_VALUE2 ((HANDLE)(LONG_PTR)-2)

typedef struct P4_FD
{
	HANDLE fh;
	int flags;
	int isStd;
	unsigned char *ptr;
	DWORD rcv;
	int iobuf_siz;
	unsigned char *iobuf;
} P4_FD;

typedef struct P4_FD* FD_TYPE;

# endif // NEED_WIN32FIO

# define FD_INIT NULL
# define FD_ERR NULL
typedef void* FD_PTR;

# else // OS_NT

# define FD_INIT -1
# define FD_ERR -1
typedef int FD_TYPE;
typedef int FD_PTR;

# endif // !OS_NT

# if !defined( HAS_CPP11 ) && !defined( LLONG_MIN )
#  define LLONG_MIN (-9223372036854775807LL - 1)
#  define LLONG_MAX   9223372036854775807LL
# endif

# endif // P4STDHDRS_H
