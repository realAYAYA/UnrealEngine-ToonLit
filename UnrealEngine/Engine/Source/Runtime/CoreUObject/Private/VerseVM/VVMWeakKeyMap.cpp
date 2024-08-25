// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMWeakKeyMap.h"
#include "VerseVM/VVMAbstractVisitor.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include "VerseVM/VVMNativeAllocationGuard.h"

namespace Verse
{

FWeakKeyMap::FWeakKeyMap()
{
	FHeap::ReportAllocatedNativeBytes(GetAllocatedSize());
}

FWeakKeyMap::~FWeakKeyMap()
{
	FHeap::ReportDeallocatedNativeBytes(GetAllocatedSize());
}

bool FWeakKeyMap::HasEntriesForKey(VCell* Key) const
{
	const TMap<VCell*, VCell*>* MapMap = InternalMap.Find(Key);
	if (MapMap)
	{
		return !MapMap->IsEmpty();
	}
	else
	{
		return false;
	}
}

void FWeakKeyMap::Add(VCell* Key, VCell* Map, VCell* Value)
{
	TNativeAllocationGuard NativeAllocationGuard(this);
	InternalMap.Emplace(Key).Add(Map, Value);
}

void FWeakKeyMap::Remove(VCell* Key, VCell* Map)
{
	TNativeAllocationGuard NativeAllocationGuard(this);
	if (TMap<VCell*, VCell*>* MapMap = InternalMap.Find(Key))
	{
		MapMap->Remove(Map);
		// Note that if the MapMap became empty, then the next census will remove it from the map.
		// We just leave it in case someone calls Add again in the near future.
	}
}

template <typename TVisitor>
void FWeakKeyMap::Visit(VCell* Key, TVisitor& Visitor)
{
	if constexpr (TVisitor::bIsAbstractVisitor)
	{
		if (TMap<VCell*, VCell*>* MapMap = InternalMap.Find(Key))
		{
			uint64 ScratchNumElements = MapMap->Num();
			Visitor.BeginMap(TEXT("Values"), ScratchNumElements);
			for (auto It = MapMap->CreateIterator(); It; ++It)
			{
				// It->Key is a weak map in this case.
				Visitor.BeginObject();
				if (Visitor.IsMarked(It->Key, TEXT("Key")))
				{
					Visitor.VisitNonNull(It->Value, TEXT("Value"));
				}
				Visitor.EndObject();
			}
			Visitor.EndMap();
		}
	}
	else
	{
		if (TMap<VCell*, VCell*>* MapMap = InternalMap.Find(Key))
		{
			for (auto It = MapMap->CreateIterator(); It; ++It)
			{
				// It->Key is a weak map in this case.
				if (Visitor.IsMarked(It->Key, TEXT("Key")))
				{
					Visitor.VisitNonNull(It->Value, TEXT("Value"));
				}
			}
		}
	}
}

template void FWeakKeyMap::Visit(VCell* Key, FMarkStackVisitor& Visitor);
template void FWeakKeyMap::Visit(VCell* Key, FAbstractVisitor& Visitor);

void FWeakKeyMap::ConductCensus()
{
	TNativeAllocationGuard NativeAllocationGuard(this);
	for (auto OuterIt = InternalMap.CreateIterator(); OuterIt; ++OuterIt)
	{
		bool bDidMarkAny = false;
		if (FHeap::IsMarked(OuterIt->Key))
		{
			for (auto InnerIt = OuterIt->Value.CreateIterator(); InnerIt; ++InnerIt)
			{
				// Here are the possibilities:
				// Key Is Live, Value Is Live -> OK, keep entry.
				// Key Is Live, Value Is Not -> Cannot happen.
				// Key Is Not, Value Is Live -> OK, delete entry (key could have died but value was referenced by something other than the map).
				// Key Is Not, Value Is Not -> OK, delete entry.
				if (FHeap::IsMarked(InnerIt->Key))
				{
					V_DIE_UNLESS(FHeap::IsMarked(InnerIt->Value));
					bDidMarkAny = true;
				}
				else
				{
					InnerIt.RemoveCurrent();
				}
			}
		}
		if (!bDidMarkAny)
		{
			OuterIt.RemoveCurrent();
		}
	}
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
