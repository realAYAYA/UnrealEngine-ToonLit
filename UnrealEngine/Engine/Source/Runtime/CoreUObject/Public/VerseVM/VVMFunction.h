// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMGlobalTrivialEmergentTypePtr.h"

namespace Verse
{
struct VProcedure;

struct VFunction : VHeapValue
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VHeapValue);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	using Args = TArray<VValue, TInlineAllocator<8>>;

	TWriteBarrier<VProcedure> Procedure;
	TWriteBarrier<VValue> ParentScope; // Either VObject or a UObject

	// Upon failure, returns an uninitialized VValue
	COREUOBJECT_API VValue InvokeInTransaction(FRunningContext Context, VValue Argument);
	COREUOBJECT_API VValue InvokeInTransaction(FRunningContext Context, Args&& Args);

	static VFunction& New(FAllocationContext Context, VProcedure& Procedure, VValue ParentScope)
	{
		return *new (Context.AllocateFastCell(sizeof(VFunction))) VFunction(Context, Procedure, ParentScope);
	}

	VProcedure& GetProcedure() { return *Procedure.Get(); }

	COREUOBJECT_API void ToStringImpl(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter);

private:
	VFunction(FAllocationContext Context, VProcedure& InFunction, VValue InParentScope)
		: VHeapValue(Context, &GlobalTrivialEmergentType.Get(Context))
		, Procedure(Context, &InFunction)
		, ParentScope(Context, InParentScope)
	{
	}
};
} // namespace Verse
#endif // WITH_VERSE_VM
