// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMHeapClientDataGuard.h"
#include "VVMWeakKeyMap.h"

namespace Verse
{

struct FWeakKeyMapGuard final
{
	FWeakKeyMapGuard(FHeapPageHeader* Header)
		: Guard(Header)
	{
	}

	FWeakKeyMap* TryGet()
	{
		return GetRef();
	}

	FWeakKeyMap* Get()
	{
		if (GetRef())
		{
			return GetRef();
		}
		else
		{
			return GetSlow();
		}
	}

	// Returns true if this page no longer has a weak key map.
	// If this returns false, then it also accounts for the native memory usage of this map.
	bool ConductCensus();

private:
	FWeakKeyMap* GetSlow();

	FWeakKeyMap*& GetRef() const
	{
		return *reinterpret_cast<FWeakKeyMap**>(Guard.GetClientDataPtr());
	}

	FHeapClientDataGuard Guard;
};

} // namespace Verse
#endif // WITH_VERSE_VM
