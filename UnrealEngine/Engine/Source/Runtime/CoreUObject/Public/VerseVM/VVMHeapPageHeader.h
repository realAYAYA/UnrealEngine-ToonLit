// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "verse_heap_page_header_ue.h"
#include "verse_heap_ue.h"

namespace Verse
{

struct FHeapPageHeader final
{
	static FHeapPageHeader* Get(const void* Ptr)
	{
		return reinterpret_cast<FHeapPageHeader*>(verse_heap_get_page_header(reinterpret_cast<uintptr_t>(Ptr)));
	}

	void** LockClientData()
	{
		return verse_heap_page_header_lock_client_data(reinterpret_cast<verse_heap_page_header*>(this));
	}

	void UnlockClientData()
	{
		verse_heap_page_header_unlock_client_data(reinterpret_cast<verse_heap_page_header*>(this));
	}
};

} // namespace Verse
#endif // WITH_VERSE_VM
