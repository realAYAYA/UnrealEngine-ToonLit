// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Containers/Map.h"
#include "CoreTypes.h"
#include "VVMCell.h"
#include "VVMGlobalTrivialEmergentTypePtr.h"

namespace Verse
{

struct VWeakCellMap : VCell
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VCell);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	static VWeakCellMap& New(FAllocationContext Context)
	{
		return *new (Context.Allocate(FHeap::DestructorAndCensusSpace, sizeof(VWeakCellMap))) VWeakCellMap(Context);
	}

	VCell* Find(FAccessContext Context, VCell* Key);
	void Add(FAccessContext Context, VCell* Key, VCell* Value);
	void Remove(VCell* Key);

	// This function is a test-only function because it has a very limited kind of meaning. Requesting the size (or
	// checking the emptiness) of a weak map gives a kind of upper bound: it means that the map has at most this many
	// entries. But we cannot tell you which of those entries are real. When you query them, you are likely to find
	// fewer entries.
	bool IsEmpty() const { return Map.IsEmpty(); }

	size_t GetAllocatedSize() const
	{
		return Map.GetAllocatedSize();
	}

private:
	VWeakCellMap(FAllocationContext Context);
	~VWeakCellMap();

	void ConductCensusImpl();

	TMap<VCell*, VCell*> Map;
};

} // namespace Verse
#endif // WITH_VERSE_VM
