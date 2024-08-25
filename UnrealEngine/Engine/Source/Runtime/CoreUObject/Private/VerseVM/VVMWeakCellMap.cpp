// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMWeakCellMap.h"
#include "Async/ExternalMutex.h"
#include "Async/UniqueLock.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include "VerseVM/VVMNativeAllocationGuard.h"
#include "VerseVM/VVMWriteBarrier.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VWeakCellMap);
TGlobalTrivialEmergentTypePtr<&VWeakCellMap::StaticCppClassInfo> VWeakCellMap::GlobalTrivialEmergentType;

VCell* VWeakCellMap::Find(FAccessContext Context, VCell* Key)
{
	V_DIE_UNLESS(Key);
	UE::FExternalMutex ExternalMutex(Mutex);
	UE::TUniqueLock Lock(ExternalMutex);
	VCell** Result = Map.Find(Key);
	if (Result)
	{
		return Context.RunWeakReadBarrier(*Result);
	}
	else
	{
		return nullptr;
	}
}

void VWeakCellMap::Add(FAccessContext Context, VCell* Key, VCell* Value)
{
	V_DIE_UNLESS(Key);
	V_DIE_UNLESS(Value);
	UE::FExternalMutex ExternalMutex(Mutex);
	UE::TUniqueLock Lock(ExternalMutex);
	TNativeAllocationGuard NativeAllocationGuard(this);
	Context.RunWriteBarrierNonNull(Value);
	Key->AddWeakMapping(this, Value);
	Map.Add(Key, Value);
}

void VWeakCellMap::Remove(VCell* Key)
{
	V_DIE_UNLESS(Key);
	UE::FExternalMutex ExternalMutex(Mutex);
	UE::TUniqueLock Lock(ExternalMutex);
	TNativeAllocationGuard NativeAllocationGuard(this);
	Key->RemoveWeakMapping(this);
	Map.Remove(Key);
}

template <typename TVisitor>
void VWeakCellMap::VisitReferencesImpl(TVisitor& Visitor)
{
	UE::FExternalMutex ExternalMutex(Mutex);
	UE::TUniqueLock Lock(ExternalMutex);
	if constexpr (TVisitor::bIsAbstractVisitor)
	{
		uint64 ScratchNumElements = Map.Num();
		Visitor.BeginMap(TEXT("Values"), ScratchNumElements);
		for (auto It = Map.CreateIterator(); It; ++It)
		{
			Visitor.BeginObject();
			if (Visitor.IsMarked(It->Key, TEXT("Key")))
			{
				Visitor.VisitNonNull(It->Value, TEXT("Value"));
			}
			Visitor.EndObject();
		}
		Visitor.EndMap();
	}
	else
	{
		for (auto It = Map.CreateIterator(); It; ++It)
		{
			if (Visitor.IsMarked(It->Key, TEXT("Key")))
			{
				Visitor.VisitNonNull(It->Value, TEXT("Value"));
			}
		}
	}
	Visitor.ReportNativeBytes(GetAllocatedSize());
}

void VWeakCellMap::ConductCensusImpl()
{
	UE::FExternalMutex ExternalMutex(Mutex);
	UE::TUniqueLock Lock(ExternalMutex);
	TNativeAllocationGuard NativeAllocationGuard(this);
	V_DIE_UNLESS(FHeap::IsMarked(this));
	for (auto It = Map.CreateIterator(); It; ++It)
	{
		// Here are the possibilities:
		// Key Is Live, Value Is Live -> OK, keep entry.
		// Key Is Live, Value Is Not -> Cannot happen.
		// Key Is Not, Value Is Live -> OK, delete entry (key could have died but value was referenced by something other than the map).
		// Key Is Not, Value Is Not -> OK, delete entry.
		if (FHeap::IsMarked(It->Key))
		{
			V_DIE_UNLESS(FHeap::IsMarked(It->Value));
		}
		else
		{
			It.RemoveCurrent();
		}
	}
}

VWeakCellMap::VWeakCellMap(FAllocationContext Context)
	: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
{
	FHeap::ReportAllocatedNativeBytes(GetAllocatedSize());
}

VWeakCellMap::~VWeakCellMap()
{
	FHeap::ReportDeallocatedNativeBytes(GetAllocatedSize());
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
