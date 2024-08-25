// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Containers/ArrayView.h"
#include "VVMFalse.h"
#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMType.h"

namespace Verse
{
struct FOpResult;

using FNativeCallResult = FOpResult;

// A function that is implemented in C++
struct VNativeFunction : VHeapValue
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VHeapValue);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	const uint32 NumParameters;

	// Interface between VerseVM and C++
	using Args = TArrayView<VValue>;
	using FThunkFn = FNativeCallResult (*)(FRunningContext, VValue, Args /* Arguments */);

	// The C++ function to call
	FThunkFn Thunk;

	TWriteBarrier<VValue> ParentScope;

	static VNativeFunction& New(FAllocationContext Context, uint32 NumParameters, FThunkFn Thunk)
	{
		return *new (Context.AllocateFastCell(sizeof(VNativeFunction))) VNativeFunction(Context, NumParameters, Thunk, GlobalFalse());
	}

	VNativeFunction& Bind(FAllocationContext Context, VValue InParentScope)
	{
		return *new (Context.AllocateFastCell(sizeof(VNativeFunction))) VNativeFunction(Context, NumParameters, Thunk, InParentScope);
	}

private:
	VNativeFunction(FAllocationContext Context, uint32 InNumParameters, FThunkFn InThunk, VValue InParentScope)
		: VHeapValue(Context, &GlobalTrivialEmergentType.Get(Context))
		, NumParameters(InNumParameters)
		, Thunk(InThunk)
		, ParentScope(Context, InParentScope)
	{
	}
};

} // namespace Verse
#endif // WITH_VERSE_VM
