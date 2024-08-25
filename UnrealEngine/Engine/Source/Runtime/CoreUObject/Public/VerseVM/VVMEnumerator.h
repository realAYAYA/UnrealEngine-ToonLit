// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMCell.h"
#include "VVMGlobalTrivialEmergentTypePtr.h"

namespace Verse
{

struct VEnumerator : VHeapValue
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VHeapValue);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	static VEnumerator& New(FAllocationContext Context)
	{
		return *new (Context.AllocateFastCell(sizeof(VEnumerator))) VEnumerator(Context);
	}

private:
	COREUOBJECT_API uint32 GetTypeHashImpl();

	VEnumerator(FAllocationContext Context)
		: VHeapValue(Context, &GlobalTrivialEmergentType.Get(Context))
	{
	}
};

} // namespace Verse
#endif // WITH_VERSE_VM
