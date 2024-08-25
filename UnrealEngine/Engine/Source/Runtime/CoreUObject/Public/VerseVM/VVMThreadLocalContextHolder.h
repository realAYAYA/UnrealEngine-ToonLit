// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMLog.h"

namespace Verse
{
struct FContextImpl;

// For use by FContextImpl.
struct FThreadLocalContextHolder
{
	FThreadLocalContextHolder() = default;
	COREUOBJECT_API ~FThreadLocalContextHolder();

	void Set(FContextImpl* InContext)
	{
		V_DIE_IF(Context);
		V_DIE_UNLESS(InContext);
		Context = InContext;
	}

	FContextImpl* Get() const
	{
		return Context;
	}

private:
	FContextImpl* Context = nullptr;
};

} // namespace Verse
#endif // WITH_VERSE_VM
