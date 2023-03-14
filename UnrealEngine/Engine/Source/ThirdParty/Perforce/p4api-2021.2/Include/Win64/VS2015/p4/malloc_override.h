/*
 * Copyright 1995, 2019 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*

Note that for P4API users, the definitions in this file will always
default to the plain malloc/free definitions - the P4API need not be
built with a memory manager.

*/

# pragma once

# if defined(MALLOC_OVERRIDE)
# define HAS_MALLOC_OVERRIDE

#  ifdef USE_MIMALLOC
// Note that mimalloc/Jamfile has a copy of these.
#    define MI_STATIC_LIB
#    ifndef NDEBUG
#      define NDEBUG
#    endif
#    define MI_STAT 1
#    define MI_NO_ENVIRO 1
#    include <mimalloc.h>

#    define HAS_MIMALLOC
#    define P4_MALLOC  mi_malloc
#    define P4_CALLOC  mi_calloc
#    define P4_REALLOC mi_realloc
#    define P4_FREE    mi_cfree
// These functions can crash if passed a non MiMalloc pointer to free.
// See job107801
# ifdef NO_DEF
#    define P4_SIZED_DELETE(ptr, size)  mi_free_size( ptr, size )
#    define P4_SIZED_DELETE_ARR(ptr, size) mi_free_size( ptr, size )
# endif

#  endif // USE_MIMALLOC

#  ifdef USE_JEMALLOC
// Note that jemalloc/Jamfile has a copy of these.
#    define JEMALLOC_NO_PRIVATE_NAMESPACE
#    define _REENTRANT
#    define JEMALLOC_EXPORT
#    define _LIB
#    define JEMALLOC_NO_RENAME
#    include <jemalloc.h>

#    define HAS_JEMALLOC
#    define P4_MALLOC  je_malloc
#    define P4_CALLOC  je_calloc
#    define P4_REALLOC je_realloc
#    define P4_FREE    je_free
#    define P4_SIZED_DELETE(ptr, size)  je_free(ptr)
#    define P4_SIZED_DELETE_ARR(ptr, size) je_free(ptr)


# endif // USE_JEMALLOC

#  ifdef USE_RPMALLOC
#    include <rpmalloc.h>

#    define HAS_RPMALLOC
#    define P4_MALLOC  rpmalloc
#    define P4_CALLOC  rpcalloc
#    define P4_REALLOC rprealloc
#    define P4_FREE    rpfree
#    define P4_SIZED_DELETE(ptr, size)  rpfree( ptr )
#    define P4_SIZED_DELETE_ARR(ptr, size) rpfree( ptr )

#  endif // USE_RPMALLOC

# ifdef USE_SMARTHEAP
# include <smrtheap.h>
# define HAS_SMARTHEAP
# if 0
#  if (_MSC_VER >= 1900)
#    define P4_MALLOC  SH_malloc
#    define P4_CALLOC  SH_calloc
#    define P4_REALLOC SH_realloc
#    define P4_FREE    SH_free
#    define P4_SIZED_DELETE(ptr, size)  SH_free( ptr )
#    define P4_SIZED_DELETE_ARR(ptr, size) SH_free( ptr )
#  endif // _MSC_VER < 1900
# endif // 0
# endif // USE_SMARTHEAP

# define NEEDS_OPERATOR_NEW_OVERRIDE

// In Visual Studio 2015 and lower on Windows, SmartHeap can override
// new/delete/malloc/free via linking.  Newer versions can't.
# if (_MSC_VER < 1900) && defined(HAS_SMARTHEAP)
#  undef NEEDS_OPERATOR_NEW_OVERRIDE
# endif

# endif // MALLOC_OVERRIDE

# ifndef P4_MALLOC // None
#    define P4_MALLOC  malloc
#    define P4_CALLOC  calloc
#    define P4_REALLOC realloc
#    define P4_FREE    free
#    define P4_SIZED_DELETE(ptr, size)  free( ptr )
#    define P4_SIZED_DELETE_ARR(ptr, size) free( ptr )
# endif
