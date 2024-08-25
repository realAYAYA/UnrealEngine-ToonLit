// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "HAL/Platform.h"

namespace Verse
{
struct FIOContext;

struct FCollectionCycleRequest
{
	FCollectionCycleRequest() = default;

	COREUOBJECT_API bool IsDone() const;

	// This needs an IO context because waiting for the GC in a running context is sure to deadlock.
	//
	// Waiting on one of these requests is totally optional.
	COREUOBJECT_API void Wait(FIOContext Context) const;

	FCollectionCycleRequest Previous() const
	{
		if (!RequestedCycleVersion)
		{
			return *this;
		}
		else
		{
			return FCollectionCycleRequest(RequestedCycleVersion - 1);
		}
	}

private:
	friend class FHeap;

	explicit FCollectionCycleRequest(uint64 InRequestedCycleVersion)
		: RequestedCycleVersion(InRequestedCycleVersion)
	{
	}

	uint64 RequestedCycleVersion = 0;
};

} // namespace Verse
#endif // WITH_VERSE_VM
