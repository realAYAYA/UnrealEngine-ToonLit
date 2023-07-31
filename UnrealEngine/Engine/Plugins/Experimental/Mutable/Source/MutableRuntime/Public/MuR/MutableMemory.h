// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#include <cstring>	// for size_t in some platforms
#include <new>

namespace mu
{
	//! \defgroup runtime Run-time classes
    //! This module contains the only classes necessary to integrate the system into an application
    //! loading models and building instances by setting parameter values.

    //! \brief Set the memory management methods for the library.
    //! The library will always manage the heap memory using these methods.
	//! This should be called only before starting to use the library or any of its data.
	//! If this method is not called, the standard C library malloc and free will be used.
	//! \todo: Notes about thread safety
	//! \param customMalloc Function pointer to a function taking the size in bytes to allocate.
    //!        The second parameter is the required alignment in bytes.
	//! 	   This function must return a valid heap pointer or 0 in case of allocation failure.
	//! \param customFree Function pointer to a function taking a pointer obtained reviously with
	//! 	   customMalloc, that needs to be freed.
	//! \ingroup runtime
	MUTABLERUNTIME_API extern void Initialize
		(
            void* (*customMalloc)( size_t, uint32 ),
            void (*customFree)( void* )
		);

    //! \brief Shutdown everything related to mutable.
    //! This should be called when no other mutable objects will be used, created or destroyed,
    //! and all threads involving mutable tasks have been terminated.
	MUTABLERUNTIME_API extern void Finalize();

	//! Memory management functions to be used inside the library. No other memory allocation is
	//! allowed.
	MUTABLERUNTIME_API extern void* mutable_malloc(size_t size);
	MUTABLERUNTIME_API extern void* mutable_malloc_aligned(size_t size, uint32 alignment);
	MUTABLERUNTIME_API extern void mutable_free(void* ptr);
	MUTABLERUNTIME_API extern void mutable_free(void* ptr, size_t size);
	MUTABLERUNTIME_API extern void mutable_free_aligned(void* ptr, size_t size);

    //! \brief %Base class from which all library classes inherit to control memory allocation.
	//! For internal use only.
	//! \ingroup runtime
	class MUTABLERUNTIME_API Base
	{
	public:

		inline void* operator new(size_t s)
		{
			return mutable_malloc(s);
		}

		inline void operator delete(void* p, size_t size)
		{
			mutable_free(p, size);
		}
    };

}

