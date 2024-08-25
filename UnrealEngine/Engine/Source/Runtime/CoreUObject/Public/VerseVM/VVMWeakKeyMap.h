// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Containers/Map.h"

namespace Verse
{

struct FMarkStack;
struct VCell;

// This is a collection of "transposed" weak maps.
//
// A transposed weak map maps maps to values. I.e. while a weak map is a Key -> Value map, a transposed weak map
// is a Map -> Value map.
//
// A weak key map is a collection of transposed weak maps keyed by key. I.e. a weak key map is a Key -> Map -> Value
// map.
//
// For every weak map entry in the world of the form Key -> Value where Map is the owning map (the one that would do
// the logic in VisitReferences that says "if Key is marked then mark Value"), there must be a weak key map entry
// of the form Key -> Map -> Value. This ensures that if Key is not yet marked before the Map runs its
// VisitReferences, then the weak key map that owns the Key will do the marking by running "if Map is marked then
// mark Value" logic.
//
// You could imagine there being one FWeakKeyMap globally for the whole GC. That would work. But we hang FWeakKeyMap
// off libpas's verse_heap_page_header (aka Verse::FHeapPageHeader). There is one of those per "page" for objects
// in segregated storage, and one globally for all large objects. Libpas manages the synchronization that protects
// these maps.
//
// Most of this is "owned" by VCell. VCell accesses this via FWeakKeyMapGuard.
struct FWeakKeyMap final
{
	FWeakKeyMap();
	~FWeakKeyMap();

	bool IsEmpty() const
	{
		return InternalMap.IsEmpty();
	}

	// This is just to support testing.
	bool HasEntriesForKey(VCell* Key) const;

	void Add(VCell* Key, VCell* Map, VCell* Value);
	void Remove(VCell* Key, VCell* Map);

	template <typename TVisitor>
	void Visit(VCell* Key, TVisitor& Visitor);

	void ConductCensus();

	size_t GetAllocatedSize() const
	{
		return InternalMap.GetAllocatedSize();
	}

private:
	TMap<VCell*, TMap<VCell*, VCell*>> InternalMap;
};

} // namespace Verse
#endif // WITH_VERSE_VM
