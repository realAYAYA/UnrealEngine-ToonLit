// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuR/BlockCompression/Miro/Miro.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/MutableMemory.h"
#include "HAL/MallocAnsi.h"

namespace mu
{


//! Static pointers to the user provided malloc and free methods.
//! If they are null (default) the versions on the standard library will be used.
	static void* (*s_custom_malloc)(size_t, uint32) = nullptr;
	static void(*s_custom_free)(void*) = nullptr;


	static int s_initialized = 0;
	static int s_finalized = 0;


	//---------------------------------------------------------------------------------------------
	//! Call the system malloc or the user-provided replacement.
	//---------------------------------------------------------------------------------------------
	inline void* lowerlevel_malloc(size_t bytes)
	{
		void* pMem = 0;
		if (s_custom_malloc)
		{
			pMem = s_custom_malloc(bytes, 1);
		}
		else
		{
			pMem = malloc(bytes);
		}
		return pMem;
	}

	inline void* lowerlevel_malloc_aligned(size_t bytes, uint32 alignment)
	{
		void* pMem = 0;
		if (s_custom_malloc)
		{
			pMem = s_custom_malloc(bytes, alignment);
		}
		else
		{
			size_t padding = bytes % alignment;
			bytes += padding ? alignment - padding : 0;
			pMem = AnsiMalloc(bytes, alignment);
		}
		return pMem;
	}

	//-------------------------------------------------------------------------------------------------
	//! Call the system free or the user-provided replacement.
	//-------------------------------------------------------------------------------------------------
	inline void lowerlevel_free(void* ptr)
	{
		if (s_custom_free)
		{
			s_custom_free(ptr);
		}
		else
		{
			free(ptr);
		}
	}


	//-------------------------------------------------------------------------------------------------
	inline void lowerlevel_free_aligned(void* ptr)
	{
		if (s_custom_free)
		{
			s_custom_free(ptr);
		}
		else
		{
			AnsiFree(ptr);
		}
	}


	//-------------------------------------------------------------------------------------------------
	//! Memory management functions to be used inside the library. No other memory allocation is
	//! allowed.
	//-------------------------------------------------------------------------------------------------
	void* mutable_malloc(size_t size)
	{
		if (!size)
		{
			return nullptr;
		}

		void* pMem = nullptr;

		pMem = lowerlevel_malloc(size);

		return pMem;
	}


	void* mutable_malloc_aligned(size_t size, uint32 alignment)
	{
		if (!size)
		{
			return nullptr;
		}

		void* pMem = nullptr;

		pMem = lowerlevel_malloc_aligned(size, alignment);

		return pMem;
	}


	void mutable_free(void* ptr)
	{
		if (!ptr)
		{
			return;
		}

		lowerlevel_free(ptr);
	}


	void mutable_free(void* ptr, size_t size)
	{
		if (!ptr || !size)
		{
			return;
		}

		lowerlevel_free(ptr);
	}


	void mutable_free_aligned(void* ptr, size_t size)
	{
		if (!size || !ptr)
		{
			return;
		}

		lowerlevel_free_aligned(ptr);
	}


	//-------------------------------------------------------------------------------------------------
	void Initialize
	(
		void* (*customMalloc)(size_t, uint32),
		void (*customFree)(void*)
	)
	{
		if (!s_initialized)
		{
			s_initialized = 1;
			s_finalized = 0;

			s_custom_malloc = customMalloc;
			s_custom_free = customFree;

			miro::initialize();
		}
	}


	//-------------------------------------------------------------------------------------------------
	void Finalize()
	{
		if (s_initialized && !s_finalized)
		{
			miro::finalize();

			s_finalized = 1;
			s_initialized = 0;
			s_custom_malloc = nullptr;
			s_custom_free = nullptr;
		}
	}


}
