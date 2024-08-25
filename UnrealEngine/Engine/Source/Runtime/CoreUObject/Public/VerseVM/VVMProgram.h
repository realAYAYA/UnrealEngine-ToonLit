// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMCell.h"
#include "VVMIntrinsics.h"
#include "VVMPackage.h"
#include "VerseVM/VVMNameValueMap.h"

namespace Verse
{
struct VProgram : VCell
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VCell);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	uint32 Num() const { return Map.Num(); }
	const VUTF8String& GetName(uint32 Index) const { return Map.GetName(Index); }
	VPackage& GetPackage(uint32 Index) const { return Map.GetCell<VPackage>(Index); }
	void AddPackage(FAllocationContext Context, VUTF8String& Name, VPackage& Package) { Map.AddValue(Context, Name, Package); }
	VPackage* LookupPackage(FUtf8StringView VersePackageName) const { return Map.LookupCell<VPackage>(VersePackageName); }

	const VIntrinsics& GetIntrinsics() const { return *Intrinsics.Get(); }

	static VProgram& New(FAllocationContext Context, uint32 Capacity)
	{
		return *new (Context.AllocateFastCell(sizeof(VProgram))) VProgram(Context, Capacity);
	}

private:
	VProgram(FAllocationContext Context, uint32 Capacity)
		: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
		, Map(Context, Capacity)
		, Intrinsics(Context, &VIntrinsics::New(Context))
	{
	}

	VNameValueMap Map;
	TWriteBarrier<VIntrinsics> Intrinsics;
};
} // namespace Verse
#endif // WITH_VERSE_VM
