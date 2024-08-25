// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMNativeFunction.h"
#include "VVMType.h"

namespace Verse
{

// A special heap value to store all intrinsic VNativeFunction objects
struct VIntrinsics : VHeapValue
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VHeapValue);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	TWriteBarrier<VNativeFunction> Abs;
	TWriteBarrier<VNativeFunction> Ceil;
	TWriteBarrier<VNativeFunction> Floor;
	TWriteBarrier<VNativeFunction> ConcatenateMaps;

	static VIntrinsics& New(FAllocationContext Context)
	{
		return *new (Context.AllocateFastCell(sizeof(VIntrinsics))) VIntrinsics(Context);
	}

private:
	COREUOBJECT_API static FNativeCallResult AbsImpl(FRunningContext Context, VValue Scope, VNativeFunction::Args Arguments);
	COREUOBJECT_API static FNativeCallResult CeilImpl(FRunningContext Context, VValue Scope, VNativeFunction::Args Arguments);
	COREUOBJECT_API static FNativeCallResult FloorImpl(FRunningContext Context, VValue Scope, VNativeFunction::Args Arguments);
	COREUOBJECT_API static FNativeCallResult ConcatenateMapsImpl(FRunningContext Context, VValue Scope, VNativeFunction::Args Arguments);

	VIntrinsics(FAllocationContext Context)
		: VHeapValue(Context, &GlobalTrivialEmergentType.Get(Context))
		, Abs(Context, VNativeFunction::New(Context, 1, &AbsImpl))
		, Ceil(Context, VNativeFunction::New(Context, 1, &CeilImpl))
		, Floor(Context, VNativeFunction::New(Context, 1, &FloorImpl))
		, ConcatenateMaps(Context, VNativeFunction::New(Context, 2, &ConcatenateMapsImpl))
	{
	}
};

} // namespace Verse
#endif // WITH_VERSE_VM
