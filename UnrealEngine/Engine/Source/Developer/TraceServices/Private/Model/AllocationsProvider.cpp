// Copyright Epic Games, Inc. All Rights Reserved.

#include "AllocationsProvider.h"

#include "Containers/ArrayView.h"
#include "Misc/PathViews.h"
#include "ProfilingDebugging/MemoryTrace.h"

// TraceServices
#include "AllocationsQuery.h"
#include "Common/ProviderLock.h"
#include "Common/Utils.h"
#include "Model/MetadataProvider.h"
#include "SbTree.h"
#include "TraceServices/Containers/Allocators.h"
#include "TraceServices/Model/Callstack.h"

#include <limits>

////////////////////////////////////////////////////////////////////////////////////////////////////

#define INSIGHTS_SLOW_CHECK(expr) //check(expr)

#define INSIGHTS_DEBUG_WATCH 0
// Action to be executed when a match is found.
#define INSIGHTS_DEBUG_WATCH_FOUND break // just log
//#define INSIGHTS_DEBUG_WATCH_FOUND return // log and ignore events
//#define INSIGHTS_DEBUG_WATCH_FOUND UE_DEBUG_BREAK(); break

// Initial reserved number for long living allocations.
#define INSIGHTS_LLA_RESERVE 0
//#define INSIGHTS_LLA_RESERVE (64 * 1024)

// Use optimized path for the short living allocations.
// If enabled, caches the short living allocations, as there is a very high chance to be freed in the next few "free" events.
// ~66% of all allocs are expected to have an "event distance" < 64 events
// ~70% of all allocs are expected to have an "event distance" < 512 events
#define INSIGHTS_USE_SHORT_LIVING_ALLOCS 1
#define INSIGHTS_SLA_USE_ADDRESS_MAP 0

// Use optimized path for the last alloc.
// If enabled, caches the last added alloc, as there is a high chance to be freed in the next "free" event.
// ~10% to ~30% of all allocs are expected to have an "event distance" == 1 event ("free" event follows the "alloc" event immediately)
#define INSIGHTS_USE_LAST_ALLOC 1

#define INSIGHTS_VALIDATE_ALLOC_EVENTS 0

#define INSIGHTS_DEBUG_METADATA 0

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace TraceServices
{

constexpr uint32 MaxLogMessagesPerErrorType = 100;

#if INSIGHTS_DEBUG_WATCH
static uint64 GWatchAddresses[] =
{
	// add here addresses to watch
	0x0ull,
};
#endif // INSIGHTS_DEBUG_WATCH

////////////////////////////////////////////////////////////////////////////////////////////////////
// FAllocationsProviderLock
////////////////////////////////////////////////////////////////////////////////////////////////////

thread_local FAllocationsProviderLock* GThreadCurrentAllocationsProviderLock;
thread_local int32 GThreadCurrentReadAllocationsProviderLockCount;
thread_local int32 GThreadCurrentWriteAllocationsProviderLockCount;

void FAllocationsProviderLock::ReadAccessCheck() const
{
	checkf(GThreadCurrentAllocationsProviderLock == this && (GThreadCurrentReadAllocationsProviderLockCount > 0 || GThreadCurrentWriteAllocationsProviderLockCount > 0),
		TEXT("Trying to READ from allocations provider outside of a READ scope"));
}

void FAllocationsProviderLock::WriteAccessCheck() const
{
	checkf(GThreadCurrentAllocationsProviderLock == this && GThreadCurrentWriteAllocationsProviderLockCount > 0,
		TEXT("Trying to WRITE to allocations provider outside of an EDIT/WRITE scope"));
}

void FAllocationsProviderLock::BeginRead()
{
	check(!GThreadCurrentAllocationsProviderLock || GThreadCurrentAllocationsProviderLock == this);
	checkf(GThreadCurrentWriteAllocationsProviderLockCount == 0, TEXT("Trying to lock allocations provider for READ while holding EDIT/WRITE access"));
	if (GThreadCurrentReadAllocationsProviderLockCount++ == 0)
	{
		GThreadCurrentAllocationsProviderLock = this;
		RWLock.ReadLock();
	}
}

void FAllocationsProviderLock::EndRead()
{
	check(GThreadCurrentReadAllocationsProviderLockCount > 0);
	if (--GThreadCurrentReadAllocationsProviderLockCount == 0)
	{
		RWLock.ReadUnlock();
		GThreadCurrentAllocationsProviderLock = nullptr;
	}
}

void FAllocationsProviderLock::BeginWrite()
{
	check(!GThreadCurrentAllocationsProviderLock || GThreadCurrentAllocationsProviderLock == this);
	checkf(GThreadCurrentReadAllocationsProviderLockCount == 0, TEXT("Trying to lock allocations provider for EDIT/WRITE while holding READ access"));
	if (GThreadCurrentWriteAllocationsProviderLockCount++ == 0)
	{
		GThreadCurrentAllocationsProviderLock = this;
		RWLock.WriteLock();
	}
}

void FAllocationsProviderLock::EndWrite()
{
	check(GThreadCurrentWriteAllocationsProviderLockCount > 0);
	if (--GThreadCurrentWriteAllocationsProviderLockCount == 0)
	{
		RWLock.WriteUnlock();
		GThreadCurrentAllocationsProviderLock = nullptr;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// FTagTracker
////////////////////////////////////////////////////////////////////////////////////////////////////

FTagTracker::FTagTracker(IAnalysisSession& InSession)
	: Session(InSession)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTagTracker::AddTagSpec(TagIdType InTag, TagIdType InParentTag, const TCHAR* InDisplay)
{
	if (InTag == InvalidTagId)
	{
		++NumErrors;
		if (NumErrors <= MaxLogMessagesPerErrorType)
		{
			UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] Cannot add Tag spec ('%s') with invalid id!"), InDisplay);
		}
		return;
	}

	if (ensure(!TagMap.Contains(InTag)))
	{
		FStringView Display(InDisplay);
		FString DisplayName;
		TStringBuilder<128> FullName;
		if (Display.Contains(TEXT("/")))
		{
			DisplayName = FPathViews::GetPathLeaf(Display);
			FullName = Display;
			// It is possible to define a child tag in runtime using only a string, even if the parent tag does not yet
			// exist. We need to find the correct parent or store it to the side until the parent tag is announced.
			if (InParentTag == InvalidTagId)
			{
				const FStringView Parent = FPathViews::GetPathLeaf(FPathViews::GetPath(Display));
				for (auto EntryPair : TagMap)
				{
					const auto Entry = EntryPair.Get<1>();
					const uint32 Id = EntryPair.Get<0>();
					if (Parent.Equals(Entry.Display))
					{
						InParentTag = Id;
						break;
					}
				}
				// If parent tag is still unknown here, create a temporary entry here
				if (InParentTag == InvalidTagId)
				{
					PendingTags.Emplace(InTag, Parent);
				}
			}
		}
		else
		{
			DisplayName = Display;
			BuildTagPath(FullName, Display, InParentTag);
		}

		const FTagEntry& Entry = TagMap.Emplace(InTag, FTagEntry{
			Session.StoreString(DisplayName),
			Session.StoreString(FullName.ToString()),
			InParentTag
		});

		// Check if this new tag has been referenced before by a child tag
		for (auto Pending : PendingTags)
		{
			const FString& Name = Pending.Get<1>();
			const TagIdType ReferencingId = Pending.Get<0>();
			if (Name.Equals(DisplayName))
			{
				TagMap[ReferencingId].ParentTag = InTag;
			}
		}

		if (!InDisplay || *InDisplay == TEXT('\0'))
		{
			UE_LOG(LogTraceServices, Warning, TEXT("[MemAlloc] Tag with id %u has invalid display name (ParentTag=%u)!"), InTag, InParentTag);
		}
		else
		{
			UE_LOG(LogTraceServices, Verbose, TEXT("[MemAlloc] Added Tag '%s' ('%s') with id %u."), Entry.Display, Entry.FullPath, InTag);
		}
	}
	else
	{
		++NumErrors;
		if (NumErrors <= MaxLogMessagesPerErrorType)
		{
			UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] Tag with id %u (ParentTag=%u, Display=%s) already added!"), InTag, InParentTag, InDisplay);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTagTracker::BuildTagPath(FStringBuilderBase& OutString, FStringView Name, TagIdType ParentTagId)
{
	if (const FTagEntry* ParentEntry = TagMap.Find(ParentTagId))
	{
		BuildTagPath(OutString, ParentEntry->Display, ParentEntry->ParentTag);
		OutString << TEXT("/");
	}
	OutString << Name;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTagTracker::PushTag(uint32 InThreadId, uint8 InTracker, TagIdType InTag)
{
	const uint32 TrackerThreadId = GetTrackerThreadId(InThreadId, InTracker);
	FThreadState& State = TrackerThreadStates.FindOrAdd(TrackerThreadId);
	if (InTag == InvalidTagId)
	{
		++NumErrors;
		if (NumErrors <= MaxLogMessagesPerErrorType)
		{
			UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] Invalid Tag on Thread %u (Tracker=%u)!"), InThreadId, uint32(InTracker));
		}
	}
	State.TagStack.Push({ InTag, ETagStackFlags::None });
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTagTracker::PopTag(uint32 InThreadId, uint8 InTracker)
{
	const uint32 TrackerThreadId = GetTrackerThreadId(InThreadId, InTracker);
	FThreadState* State = TrackerThreadStates.Find(TrackerThreadId);
	if (State && !State->TagStack.IsEmpty())
	{
		INSIGHTS_SLOW_CHECK(!State->TagStack.Top().IsPtrScope());
		State->TagStack.Pop();
	}
	else
	{
		++NumErrors;
		if (NumErrors <= MaxLogMessagesPerErrorType)
		{
			UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] Tag stack on Thread %u (Tracker=%u) is already empty!"), InThreadId, uint32(InTracker));
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TagIdType FTagTracker::GetCurrentTag(uint32 InThreadId, uint8 InTracker) const
{
	const uint32 TrackerThreadId = GetTrackerThreadId(InThreadId, InTracker);
	const FThreadState* State = TrackerThreadStates.Find(TrackerThreadId);
	if (!State || State->TagStack.IsEmpty())
	{
		return 0; // Untagged
	}
	return State->TagStack.Top().Tag;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const TCHAR* FTagTracker::GetTagString(TagIdType InTag) const
{
	const FTagEntry* Entry = TagMap.Find(InTag);
	return Entry ? Entry->Display : TEXT("Unknown");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const TCHAR* FTagTracker::GetTagFullPath(TagIdType InTag) const
{
	const FTagEntry* Entry = TagMap.Find(InTag);
	return Entry ? Entry->FullPath : TEXT("Unknown");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTagTracker::EnumerateTags(TFunctionRef<void (const TCHAR*, const TCHAR*, TagIdType, TagIdType)> Callback) const
{
	for (const auto& EntryPair : TagMap)
	{
		const TagIdType Id = EntryPair.Get<0>();
		const FTagEntry& Entry = EntryPair.Get<1>();
		Callback(Entry.Display, Entry.FullPath, Id, Entry.ParentTag);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTagTracker::PushTagFromPtr(uint32 InThreadId, uint8 InTracker, TagIdType InTag)
{
	const uint32 TrackerThreadId = GetTrackerThreadId(InThreadId, InTracker);
	FThreadState& State = TrackerThreadStates.FindOrAdd(TrackerThreadId);
	State.TagStack.Push({ InTag, ETagStackFlags::PtrScope });
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTagTracker::PopTagFromPtr(uint32 InThreadId, uint8 InTracker)
{
	const uint32 TrackerThreadId = GetTrackerThreadId(InThreadId, InTracker);
	FThreadState* State = TrackerThreadStates.Find(TrackerThreadId);
	if (State && !State->TagStack.IsEmpty())
	{
		INSIGHTS_SLOW_CHECK(State->TagStack.Top().IsPtrScope());
		State->TagStack.Pop();
	}
	else
	{
		++NumErrors;
		if (NumErrors <= MaxLogMessagesPerErrorType)
		{
			UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] Tag stack on Thread %u (Tracker=%u) is already empty!"), InThreadId, uint32(InTracker));
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FTagTracker::HasTagFromPtrScope(uint32 InThreadId, uint8 InTracker) const
{
	const uint32 TrackerThreadId = GetTrackerThreadId(InThreadId, InTracker);
	const FThreadState* State = TrackerThreadStates.Find(TrackerThreadId);
	return State && (State->TagStack.Num() > 0) && State->TagStack.Top().IsPtrScope();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// IAllocationsProvider::FAllocation
////////////////////////////////////////////////////////////////////////////////////////////////////

uint32 IAllocationsProvider::FAllocation::GetStartEventIndex() const
{
	const auto* Inner = (const FAllocationItem*)this;
	return Inner->StartEventIndex;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

uint32 IAllocationsProvider::FAllocation::GetEndEventIndex() const
{
	const auto* Inner = (const FAllocationItem*)this;
	return Inner->EndEventIndex;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

double IAllocationsProvider::FAllocation::GetStartTime() const
{
	const auto* Inner = (const FAllocationItem*)this;
	return Inner->StartTime;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

double IAllocationsProvider::FAllocation::GetEndTime() const
{
	const auto* Inner = (const FAllocationItem*)this;
	return Inner->EndTime;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

uint64 IAllocationsProvider::FAllocation::GetAddress() const
{
	const auto* Inner = (const FAllocationItem*)this;
	return Inner->Address;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

uint64 IAllocationsProvider::FAllocation::GetSize() const
{
	const auto* Inner = (const FAllocationItem*)this;
	return Inner->GetSize();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

uint32 IAllocationsProvider::FAllocation::GetAlignment() const
{
	const auto* Inner = (const FAllocationItem*)this;
	return Inner->GetAlignment();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
uint32 IAllocationsProvider::FAllocation::GetThreadId() const
{
	const auto* Inner = (const FAllocationItem*)this;
	return Inner->ThreadId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
uint32 IAllocationsProvider::FAllocation::GetCallstackId() const
{
	const auto* Inner = (const FAllocationItem*)this;
	return Inner->CallstackId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
uint32 IAllocationsProvider::FAllocation::GetFreeCallstackId() const
{
	const auto* Inner = (const FAllocationItem*)this;
	return Inner->FreeCallstackId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
uint32 IAllocationsProvider::FAllocation::GetMetadataId() const
{
	const auto* Inner = (const FAllocationItem*)this;
	return Inner->MetadataId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TagIdType IAllocationsProvider::FAllocation::GetTag() const
{
	const auto* Inner = (const FAllocationItem*)this;
	return Inner->Tag;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

HeapId IAllocationsProvider::FAllocation::GetRootHeap() const
{
	const auto* Inner = (const FAllocationItem*)this;
	return Inner->RootHeap;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool IAllocationsProvider::FAllocation::IsHeap() const
{
	const auto* Inner = (const FAllocationItem*)this;
	return Inner->IsHeap();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// IAllocationsProvider::FAllocations
////////////////////////////////////////////////////////////////////////////////////////////////////

void IAllocationsProvider::FAllocations::operator delete (void* Address)
{
	auto* Inner = (const FAllocationsImpl*)Address;
	delete Inner;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

uint32 IAllocationsProvider::FAllocations::Num() const
{
	auto* Inner = (const FAllocationsImpl*)this;
	return (uint32)(Inner->Items.Num());
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const IAllocationsProvider::FAllocation* IAllocationsProvider::FAllocations::Get(uint32 Index) const
{
	checkSlow(Index < Num());

	auto* Inner = (const FAllocationsImpl*)this;
	return (const FAllocation*)(Inner->Items[Index]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// IAllocationsProvider::FQueryStatus
////////////////////////////////////////////////////////////////////////////////////////////////////

IAllocationsProvider::FQueryResult IAllocationsProvider::FQueryStatus::NextResult() const
{
	auto* Inner = (FAllocationsImpl*)Handle;
	if (Inner == nullptr)
	{
		return nullptr;
	}

	Handle = UPTRINT(Inner->Next);

	auto* Ret = (FAllocations*)Inner;
	return FQueryResult(Ret);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// FShortLivingAllocs
////////////////////////////////////////////////////////////////////////////////////////////////////

FShortLivingAllocs::FShortLivingAllocs()
{
#if INSIGHTS_SLA_USE_ADDRESS_MAP
	AddressMap.Reserve(MaxAllocCount);
#endif

	AllNodes = new FNode[MaxAllocCount];

	// Build the "unused" simple linked list.
	FirstUnusedNode = AllNodes;
	for (int32 Index = 0; Index < MaxAllocCount - 1; ++Index)
	{
		AllNodes[Index].Next = &AllNodes[Index + 1];
	}
	AllNodes[MaxAllocCount - 1].Next = nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FShortLivingAllocs::~FShortLivingAllocs()
{
	Reset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FShortLivingAllocs::Reset()
{
#if INSIGHTS_SLA_USE_ADDRESS_MAP
	AddressMap.Reset();
#endif

	FNode* Node = LastAddedAllocNode;
	while (Node != nullptr)
	{
		delete Node->Alloc;
		Node = Node->Prev;
	}

	delete[] AllNodes;
	AllNodes = nullptr;

	LastAddedAllocNode = nullptr;
	OldestAllocNode = nullptr;
	FirstUnusedNode = nullptr;
	AllocCount = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FAllocationItem* FShortLivingAllocs::FindRef(uint64 Address)
{
#if INSIGHTS_SLA_USE_ADDRESS_MAP
	FNode** NodePtr = AddressMap.Find(Address);
	return NodePtr ? (*NodePtr)->Alloc : nullptr;
#else
	// Search linearly the list of allocations, backward, starting with most recent one.
	// As the probability of finding a match for a "free" event is higher for the recent allocs,
	// this could actually be faster than doing a O(log n) search in AddressMap.
	FNode* Node = LastAddedAllocNode;
	while (Node != nullptr)
	{
		if (Node->Alloc->Address == Address)
		{
			return Node->Alloc;
		}
		Node = Node->Prev;
	}
	return nullptr;
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FAllocationItem* FShortLivingAllocs::AddAndRemoveOldest(FAllocationItem* Alloc)
{
	if (FirstUnusedNode == nullptr)
	{
		// Collection is already full.
		INSIGHTS_SLOW_CHECK(AllocCount == MaxAllocCount);
		INSIGHTS_SLOW_CHECK(OldestAllocNode != nullptr);
		INSIGHTS_SLOW_CHECK(LastAddedAllocNode != nullptr);

		// Reuse the node of the oldest allocation for the new allocation.
		FNode* NewNode = OldestAllocNode;

		// Remove the oldest allocation.
		FAllocationItem* RemovedAlloc = OldestAllocNode->Alloc;
#if INSIGHTS_SLA_USE_ADDRESS_MAP
		AddressMap.Remove(RemovedAlloc->Address);
#endif
		OldestAllocNode = OldestAllocNode->Next;
		INSIGHTS_SLOW_CHECK(OldestAllocNode != nullptr);
		OldestAllocNode->Prev = nullptr;

		// Add the new node.
#if INSIGHTS_SLA_USE_ADDRESS_MAP
		AddressMap.Add(Alloc->Address, NewNode);
#endif
		NewNode->Alloc = Alloc;
		NewNode->Next = nullptr;
		NewNode->Prev = LastAddedAllocNode;
		LastAddedAllocNode->Next = NewNode;
		LastAddedAllocNode = NewNode;

		return RemovedAlloc;
	}
	else
	{
		INSIGHTS_SLOW_CHECK(AllocCount < MaxAllocCount);
		++AllocCount;

		FNode* NewNode = FirstUnusedNode;
		FirstUnusedNode = FirstUnusedNode->Next;

		// Add the new node.
#if INSIGHTS_SLA_USE_ADDRESS_MAP
		AddressMap.Add(Alloc->Address, NewNode);
#endif
		NewNode->Alloc = Alloc;
		NewNode->Next = nullptr;
		NewNode->Prev = LastAddedAllocNode;
		if (LastAddedAllocNode)
		{
			LastAddedAllocNode->Next = NewNode;
		}
		else
		{
			OldestAllocNode = NewNode;
		}
		LastAddedAllocNode = NewNode;

		return nullptr;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FAllocationItem* FShortLivingAllocs::Remove(uint64 Address)
{
#if INSIGHTS_SLA_USE_ADDRESS_MAP
	FNode* Node = nullptr;
	if (!AddressMap.RemoveAndCopyValue(Address, Node))
	{
		return nullptr;
	}
	INSIGHTS_SLOW_CHECK(Node && Node->Alloc && Node->Alloc->Address == Address);
#else
	// Search linearly the list of allocations, backward, starting with most recent one.
	// As the probability of finding a match for a "free" event is higher for the recent allocs,
	// this could actually be faster than doing a O(log n) search in AddressMap.
	FNode* Node = LastAddedAllocNode;
	while (Node != nullptr)
	{
		if (Node->Alloc->Address == Address)
		{
			break;
		}
		Node = Node->Prev;
	}
	if (!Node)
	{
		return nullptr;
	}
#endif // INSIGHTS_SLA_USE_ADDRESS_MAP

	INSIGHTS_SLOW_CHECK(AllocCount > 0);
	--AllocCount;

	// Remove node.
	if (Node == OldestAllocNode)
	{
		OldestAllocNode = OldestAllocNode->Next;
	}
	if (Node == LastAddedAllocNode)
	{
		LastAddedAllocNode = LastAddedAllocNode->Prev;
	}
	if (Node->Prev)
	{
		Node->Prev->Next = Node->Next;
	}
	if (Node->Next)
	{
		Node->Next->Prev = Node->Prev;
	}

	// Add the removed node to the "unused" list.
	Node->Next = FirstUnusedNode;
	FirstUnusedNode = Node;

	return Node->Alloc;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FShortLivingAllocs::Enumerate(TFunctionRef<void(const FAllocationItem& Alloc)> Callback) const
{
	FNode* Node = LastAddedAllocNode;
	while (Node != nullptr)
	{
		Callback(*(Node->Alloc));
		Node = Node->Prev;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
FAllocationItem* FShortLivingAllocs::FindRange(const uint64 Address) const
{
	//todo: Can we do better than iterating all the nodes?
	FNode* Node = LastAddedAllocNode;
	while (Node != nullptr)
	{
		if (Node->Alloc->IsContained(Address))
		{
			return Node->Alloc;
		}
		Node = Node->Prev;
	}
	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// FHeapAllocs
////////////////////////////////////////////////////////////////////////////////////////////////////

FHeapAllocs::FHeapAllocs()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FHeapAllocs::~FHeapAllocs()
{
	Reset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FHeapAllocs::Reset()
{
	for (auto& KV : AddressMap)
	{
		FList& List = KV.Value;
		const FNode* Node = List.First;
		while (Node != nullptr)
		{
			FNode* NextNode = Node->Next;
			delete Node->Alloc;
			delete Node;
			Node = NextNode;
		}
		List.First = nullptr;
		List.Last = nullptr;
	}
	AddressMap.Reset();
	AllocCount = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FAllocationItem* FHeapAllocs::FindRef(uint64 Address)
{
	FList* ListPtr = AddressMap.Find(Address);
	if (ListPtr)
	{
		// Search in reverse added order.
		for (const FNode* Node = ListPtr->Last; Node != nullptr; Node = Node->Prev)
		{
			if (Address == Node->Alloc->Address)
			{
				return Node->Alloc;
			}
		}
	}
	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FHeapAllocs::Add(FAllocationItem* Alloc)
{
	FNode* Node = new FNode();
	Node->Alloc = Alloc;
	Node->Next = nullptr;

	FList& List = AddressMap.FindOrAdd(Alloc->Address);
	if (List.Last == nullptr)
	{
		List.First = Node;
	}
	else
	{
		List.Last->Next = Node;
	}
	Node->Prev = List.Last;
	List.Last = Node;

	++AllocCount;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FAllocationItem* FHeapAllocs::Remove(uint64 Address)
{
	FList* ListPtr = AddressMap.Find(Address);
	if (ListPtr)
	{
		// Remove the last added heap alloc.
		check(ListPtr->Last != nullptr);
		FAllocationItem* Alloc = ListPtr->Last->Alloc;

		if (ListPtr->First == ListPtr->Last)
		{
			// Remove the entire linked list from the map (as it has only one node).
			delete ListPtr->Last;
			AddressMap.FindAndRemoveChecked(Address);
		}
		else
		{
			// Remove only the last node from the linked list of heap allocs with specified address.
			FNode* LastNode = ListPtr->Last;
			ListPtr->Last = LastNode->Prev;
			ListPtr->Last->Next = nullptr;
			delete LastNode;
		}

		--AllocCount;
		return Alloc;
	}
	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FHeapAllocs::Enumerate(TFunctionRef<void(const FAllocationItem& Alloc)> Callback) const
{
	for (const auto& KV : AddressMap)
	{
		// Iterate in same order as added.
		for (const FNode* Node = KV.Value.First; Node != nullptr; Node = Node->Next)
		{
			Callback(*Node->Alloc);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FAllocationItem* FHeapAllocs::FindRange(uint64 Address) const
{
	for (const auto& KV : AddressMap)
	{
		// Search in reverse added order.
		for (const FNode* Node = KV.Value.Last; Node != nullptr; Node = Node->Prev)
		{
			if (Node->Alloc->IsContained(Address))
			{
				return Node->Alloc;
			}
		}
	}
	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// FLiveAllocCollection
////////////////////////////////////////////////////////////////////////////////////////////////////

FLiveAllocCollection::FLiveAllocCollection()
{
#if INSIGHTS_LLA_RESERVE
	LongLivingAllocs.Reserve(INSIGHTS_LLA_RESERVE);
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FLiveAllocCollection::~FLiveAllocCollection()
{
	HeapAllocs.Reset();

	for (const auto& KV : LongLivingAllocs)
	{
		const FAllocationItem* Allocation = KV.Value;
		delete Allocation;
	}
	LongLivingAllocs.Reset();

	ShortLivingAllocs.Reset();

#if INSIGHTS_USE_LAST_ALLOC
	if (LastAlloc)
	{
		delete LastAlloc;
		LastAlloc = nullptr;
	}
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FAllocationItem* FLiveAllocCollection::FindRef(uint64 Address)
{
#if INSIGHTS_USE_LAST_ALLOC
	if (LastAlloc && LastAlloc->Address == Address)
	{
		return LastAlloc;
	}
#endif

#if INSIGHTS_USE_SHORT_LIVING_ALLOCS
	FAllocationItem* FoundShortLivingAlloc = ShortLivingAllocs.FindRef(Address);
	if (FoundShortLivingAlloc)
	{
		INSIGHTS_SLOW_CHECK(FoundShortLivingAlloc->Address == Address);
		return FoundShortLivingAlloc;
	}
#endif

	FAllocationItem* FoundLongLivingAlloc = LongLivingAllocs.FindRef(Address);
	if (FoundLongLivingAlloc)
	{
		INSIGHTS_SLOW_CHECK(FoundLongLivingAlloc->Address == Address);
		return FoundLongLivingAlloc;
	}

	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FAllocationItem* FLiveAllocCollection::FindHeapRef(uint64 Address)
{
	FAllocationItem* FoundHeapAlloc = HeapAllocs.FindRef(Address);
	if (FoundHeapAlloc)
	{
		INSIGHTS_SLOW_CHECK(FoundHeapAlloc->Address == Address);
		return FoundHeapAlloc;
	}

	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FAllocationItem* FLiveAllocCollection::FindByAddressRange(uint64 Address)
{
#if INSIGHTS_USE_LAST_ALLOC
	if (LastAlloc && LastAlloc->IsContained(Address))
	{
		return LastAlloc;
	}
#endif

#if INSIGHTS_USE_SHORT_LIVING_ALLOCS
	FAllocationItem* FoundShortLivingAlloc = ShortLivingAllocs.FindRange(Address);
	if (FoundShortLivingAlloc)
	{
		INSIGHTS_SLOW_CHECK(FoundShortLivingAlloc->IsContained(Address));
		return FoundShortLivingAlloc;
	}
#endif

	for (const auto& Alloc : LongLivingAllocs)
	{
		if (Alloc.Value->IsContained(Address))
		{
			return Alloc.Value;
		}
	}

	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FAllocationItem* FLiveAllocCollection::FindHeapByAddressRange(uint64 Address)
{
	FAllocationItem* FoundHeapAlloc = HeapAllocs.FindRange(Address);
	if (FoundHeapAlloc)
	{
		INSIGHTS_SLOW_CHECK(FoundHeapAlloc->IsContained(Address));
		return FoundHeapAlloc;
	}

	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FAllocationItem* FLiveAllocCollection::AddNew(uint64 Address)
{
	FAllocationItem* Alloc = new FAllocationItem();
	Alloc->Address = Address;

	Add(Alloc);
	return Alloc;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FLiveAllocCollection::Add(FAllocationItem* Alloc)
{
	++TotalAllocCount;
	if (TotalAllocCount > MaxAllocCount)
	{
		MaxAllocCount = TotalAllocCount;
	}

#if INSIGHTS_USE_LAST_ALLOC
	if (LastAlloc)
	{
		// We have a new "last allocation".
		// The previous one will be moved to the short living allocations.
		FAllocationItem* PrevLastAlloc = LastAlloc;
		LastAlloc = Alloc;
		Alloc = PrevLastAlloc;
	}
	else
	{
		LastAlloc = Alloc;
		return;
	}
#endif

#if INSIGHTS_USE_SHORT_LIVING_ALLOCS
	Alloc = ShortLivingAllocs.AddAndRemoveOldest(Alloc);
	if (Alloc == nullptr)
	{
		return;
	}
#endif

	LongLivingAllocs.Add(Alloc->Address, Alloc);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FAllocationItem* FLiveAllocCollection::AddNewHeap(uint64 Address)
{
	FAllocationItem* NewAlloc = new FAllocationItem();
	NewAlloc->Address = Address;

	AddHeap(NewAlloc);
	return NewAlloc;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FLiveAllocCollection::AddHeap(FAllocationItem* HeapAlloc)
{
	++TotalAllocCount;
	if (TotalAllocCount > MaxAllocCount)
	{
		MaxAllocCount = TotalAllocCount;
	}

	HeapAllocs.Add(HeapAlloc);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FAllocationItem* FLiveAllocCollection::Remove(uint64 Address)
{
#if INSIGHTS_USE_LAST_ALLOC
	if (LastAlloc && LastAlloc->Address == Address)
	{
		FAllocationItem* RemovedAlloc = LastAlloc;
		LastAlloc = nullptr;
		INSIGHTS_SLOW_CHECK(TotalAllocCount > 0);
		--TotalAllocCount;
		return RemovedAlloc;
	}
#endif

#if INSIGHTS_USE_SHORT_LIVING_ALLOCS
	FAllocationItem* RemovedShortLivingAlloc = ShortLivingAllocs.Remove(Address);
	if (RemovedShortLivingAlloc)
	{
		INSIGHTS_SLOW_CHECK(RemovedShortLivingAlloc->Address == Address);
		INSIGHTS_SLOW_CHECK(TotalAllocCount > 0);
		--TotalAllocCount;
		return RemovedShortLivingAlloc;
	}
#endif

	FAllocationItem* RemovedLongLivingAlloc;
	if (LongLivingAllocs.RemoveAndCopyValue(Address, RemovedLongLivingAlloc))
	{
		INSIGHTS_SLOW_CHECK(RemovedLongLivingAlloc->Address == Address);
		INSIGHTS_SLOW_CHECK(TotalAllocCount > 0);
		--TotalAllocCount;
		return RemovedLongLivingAlloc;
	}

	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FAllocationItem* FLiveAllocCollection::RemoveHeap(uint64 Address)
{
	FAllocationItem* RemovedHeapAlloc = HeapAllocs.Remove(Address);
	if (RemovedHeapAlloc)
	{
		INSIGHTS_SLOW_CHECK(RemovedHeapAlloc->Address == Address);
		INSIGHTS_SLOW_CHECK(TotalAllocCount > 0);
		--TotalAllocCount;
		return RemovedHeapAlloc;
	}

	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FLiveAllocCollection::Enumerate(TFunctionRef<void(const FAllocationItem& Alloc)> Callback) const
{
	HeapAllocs.Enumerate(Callback);

#if INSIGHTS_USE_LAST_ALLOC
	if (LastAlloc)
	{
		Callback(*LastAlloc);
	}
#endif

#if INSIGHTS_USE_SHORT_LIVING_ALLOCS
	ShortLivingAllocs.Enumerate(Callback);
#endif

	for (const auto& KV : LongLivingAllocs)
	{
		const FAllocationItem* Allocation = KV.Value;
		Callback(*Allocation);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// FAllocationsProvider
////////////////////////////////////////////////////////////////////////////////////////////////////

FAllocationsProvider::FAllocationsProvider(IAnalysisSession& InSession, FMetadataProvider& InMetadataProvider)
	: Session(InSession)
	, MetadataProvider(InMetadataProvider)
	, TagTracker(InSession)
	, Timeline(Session.GetLinearAllocator(), 1024)
	, MinTotalAllocatedMemoryTimeline(Session.GetLinearAllocator(), 1024)
	, MaxTotalAllocatedMemoryTimeline(Session.GetLinearAllocator(), 1024)
	, MinLiveAllocationsTimeline(Session.GetLinearAllocator(), 1024)
	, MaxLiveAllocationsTimeline(Session.GetLinearAllocator(), 1024)
	, AllocEventsTimeline(Session.GetLinearAllocator(), 1024)
	, FreeEventsTimeline(Session.GetLinearAllocator(), 1024)
{
	HeapSpecs.AddZeroed(256);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FAllocationsProvider::~FAllocationsProvider()
{
	for (uint8 RootHeapIdx = 0; RootHeapIdx < MaxRootHeaps; ++RootHeapIdx)
	{
		if (SbTree[RootHeapIdx])
		{
			delete SbTree[RootHeapIdx];
			SbTree[RootHeapIdx] = nullptr;
		}
		if (LiveAllocs[RootHeapIdx])
		{
			delete LiveAllocs[RootHeapIdx];
			LiveAllocs[RootHeapIdx] = nullptr;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::EditInit(double InTime, uint8 InMinAlignment)
{
	Lock.WriteAccessCheck();

	if (bInitialized)
	{
		// error: already initialized
		return;
	}

	InitTime = InTime;
	MinAlignment = InMinAlignment;

	// Create system root heap structures for backwards compatibility (before heap description events)
	AddHeapSpec(EMemoryTraceRootHeap::SystemMemory, 0, TEXT("System memory"), EMemoryTraceHeapFlags::Root);

	bInitialized = true;

	AdvanceTimelines(InTime);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::AddHeapSpec(HeapId Id, HeapId ParentId, const FStringView& Name, EMemoryTraceHeapFlags Flags)
{
	if (Id < MaxRootHeaps)
	{
		if (Id == 0 && SbTree[Id] != nullptr)
		{
			// "System" root heap is already created. See class constructor.
			check(LiveAllocs[Id] != nullptr);
		}
		else
		{
			check(SbTree[Id] == nullptr && LiveAllocs[Id] == nullptr);

			constexpr uint32 ColumnShift = 17; // 1<<17 = 128K
			SbTree[Id] = new FSbTree(Session.GetLinearAllocator(), ColumnShift);
			LiveAllocs[Id] = new FLiveAllocCollection();
		}

		FHeapSpec& RootHeapSpec = HeapSpecs[Id];
		RootHeapSpec.Name = Session.StoreString(Name);
		RootHeapSpec.Parent = nullptr;
		RootHeapSpec.Id = Id;
		RootHeapSpec.Flags = Flags;
	}
	else
	{
		FHeapSpec& ParentSpec = HeapSpecs[ParentId];
		if (ParentSpec.Name == nullptr)
		{
			UE_LOG(LogTraceServices, Warning, TEXT("[MemAlloc] Parent heap id (%u) used before it was announced for heap %s."), ParentId, *FString(Name));
		}
		FHeapSpec& NewSpec = HeapSpecs[Id];
		NewSpec.Name = Session.StoreString(Name);
		NewSpec.Parent = &ParentSpec;
		NewSpec.Id = Id;
		NewSpec.Flags = Flags;

		ParentSpec.Children.Add(&NewSpec);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::EditHeapSpec(HeapId Id, HeapId ParentId, const FStringView& Name, EMemoryTraceHeapFlags Flags)
{
	Lock.WriteAccessCheck();

	AddHeapSpec(Id, ParentId, Name, Flags);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::EditAlloc(double Time, uint32 CallstackId, uint64 Address, uint64 InSize, uint32 InAlignment, HeapId RootHeap)
{
	Lock.WriteAccessCheck();

	if (!bInitialized)
	{
		return;
	}

	if (Address == 0)
	{
		UE_LOG(LogTraceServices, Warning, TEXT("[MemAlloc] Alloc at address 0 : Size=%llu, RootHeap=%u, Time=%f, CallstackId=%u"), InSize, RootHeap, Time, CallstackId);
		return;
	}

	check(RootHeap < MaxRootHeaps);
	if (SbTree[RootHeap] == nullptr)
	{
		UE_LOG(LogTraceServices, Warning, TEXT("[MemAlloc] Invalid root heap id (%u). This heap has not yet been registered."), RootHeap);
		return;
	}

	SbTree[RootHeap]->SetTimeForEvent(EventIndex[RootHeap], Time);

	AdvanceTimelines(Time);

	const TagIdType Tag = TagTracker.GetCurrentTag(CurrentSystemThreadId, CurrentTracker);

#if INSIGHTS_DEBUG_WATCH
	for (int32 AddrIndex = 0; AddrIndex < UE_ARRAY_COUNT(GWatchAddresses); ++AddrIndex)
	{
		if (GWatchAddresses[AddrIndex] == Address)
		{
			UE_LOG(LogTraceServices, Warning, TEXT("[MemAlloc][%u] Alloc 0x%llX : Size=%llu, Tag=%u, RootHeap=%u, Time=%f, CallstackId=%u"), CurrentTraceThreadId, Address, InSize, Tag, RootHeap, Time, CallstackId);
			INSIGHTS_DEBUG_WATCH_FOUND;
		}
	}
#endif // INSIGHTS_DEBUG_WATCH

#if INSIGHTS_VALIDATE_ALLOC_EVENTS
	FAllocationItem* AllocationPtr = LiveAllocs[RootHeap]->FindRef(Address);
#else
	FAllocationItem* AllocationPtr = nullptr;
#endif

	if (!AllocationPtr)
	{
		AllocationPtr = LiveAllocs[RootHeap]->AddNew(Address);
		FAllocationItem& Allocation = *AllocationPtr;

		uint32 MetadataId = MetadataProvider.InvalidMetadataId;
		{
			FProviderEditScopeLock _(MetadataProvider);
			MetadataId = MetadataProvider.PinAndGetId(CurrentSystemThreadId);
#if INSIGHTS_DEBUG_METADATA
			if (MetadataId != FMetadataProvider::InvalidMetadataId &&
				!TagTracker.HasTagFromPtrScope(CurrentSystemThreadId, CurrentTracker))
			{
				const uint32 MetadataStackSize = MetadataProvider.GetMetadataStackSize(CurrentSystemThreadId, MetadataId);
				check(MetadataStackSize > 0);
				uint16 MetaType;
				const void* MetaData;
				uint32 MetaDataSize;
				MetadataProvider.GetMetadata(CurrentSystemThreadId, MetadataId, MetadataStackSize - 1, MetaType, MetaData, MetaDataSize);
				if (MetaType == 0) // "MemTagId"
				{
					TagIdType MetaMemTag = *(TagIdType*)MetaData;
					ensure(MetaMemTag == Tag);
				}
			}
#endif
		}

		INSIGHTS_SLOW_CHECK(Allocation.Address == Address);
		INSIGHTS_SLOW_CHECK(InAlignment < 256);
		Allocation.SizeAndAlignment = FAllocationItem::PackSizeAndAlignment(InSize, static_cast<uint8>(InAlignment));
		Allocation.StartEventIndex = EventIndex[RootHeap];
		Allocation.EndEventIndex = (uint32)-1;
		Allocation.StartTime = Time;
		Allocation.EndTime = std::numeric_limits<double>::infinity();
		Allocation.ThreadId = CurrentSystemThreadId;
		Allocation.CallstackId = CallstackId;
		Allocation.FreeCallstackId = 0; // no callstack yet
		Allocation.MetadataId = MetadataId;
		Allocation.Tag = Tag;
		Allocation.RootHeap = static_cast<uint8>(RootHeap);
		Allocation.Flags = EMemoryTraceHeapAllocationFlags::None;

		UpdateHistogramByAllocSize(InSize);

		// Update stats for the current timeline sample.
		TotalAllocatedMemory += InSize;
		++TotalLiveAllocations;
		SampleMaxTotalAllocatedMemory = FMath::Max(SampleMaxTotalAllocatedMemory, TotalAllocatedMemory);
		SampleMaxLiveAllocations = FMath::Max(SampleMaxLiveAllocations, TotalLiveAllocations);
		++SampleAllocEvents;
	}
#if INSIGHTS_VALIDATE_ALLOC_EVENTS
	else
	{
		++AllocErrors;
		if (AllocErrors <= MaxLogMessagesPerErrorType)
		{
			UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] Invalid ALLOC event (Address=0x%llX, Size=%llu, Tag=%u, RootHeap=%u, Time=%f)!"), Address, InSize, Tag, RootHeap, Time);
		}
	}
#endif

	++AllocCount;
	if (EventIndex[RootHeap] != ~0u)
	{
		++EventIndex[RootHeap];
	}
	else
	{
		UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] Too many events!"));
		bInitialized = false; // ignore further events
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::EditFree(double Time, uint32 CallstackId, uint64 Address, HeapId RootHeap)
{
	Lock.WriteAccessCheck();

	if (!bInitialized)
	{
		return;
	}

	if (Address == 0)
	{
		UE_LOG(LogTraceServices, Warning, TEXT("[MemAlloc] Free for address 0 : RootHeap=%u, Time=%f, CallstackId=%u"), RootHeap, Time, CallstackId);
		return;
	}

	check(RootHeap < MaxRootHeaps);
	if (SbTree[RootHeap] == nullptr)
	{
		UE_LOG(LogTraceServices, Warning, TEXT("[MemAlloc] Invalid root heap id (%u). This heap has not yet been registered."), RootHeap);
		return;
	}

	SbTree[RootHeap]->SetTimeForEvent(EventIndex[RootHeap], Time);

	AdvanceTimelines(Time);

#if INSIGHTS_DEBUG_WATCH
	for (int32 AddrIndex = 0; AddrIndex < UE_ARRAY_COUNT(GWatchAddresses); ++AddrIndex)
	{
		if (GWatchAddresses[AddrIndex] == Address)
		{
			UE_LOG(LogTraceServices, Warning, TEXT("[MemAlloc][%u] Free 0x%llX : RootHeap=%u, Time=%f, CallstackId=%u"), CurrentTraceThreadId, Address, RootHeap, Time, CallstackId);
			INSIGHTS_DEBUG_WATCH_FOUND;
		}
	}
#endif // INSIGHTS_DEBUG_WATCH

	FAllocationItem* AllocationPtr = LiveAllocs[RootHeap]->Remove(Address); // we take ownership of AllocationPtr
	if (!AllocationPtr)
	{
		// Try to free a heap allocation.
		AllocationPtr = LiveAllocs[RootHeap]->RemoveHeap(Address); // we take ownership of AllocationPtr
		INSIGHTS_SLOW_CHECK(!AllocationPtr || AllocationPtr->IsHeap());
	}
	else
	{
		INSIGHTS_SLOW_CHECK(!AllocationPtr->IsHeap());
	}
	if (AllocationPtr)
	{
		check(EventIndex[RootHeap] > AllocationPtr->StartEventIndex);
		AllocationPtr->EndEventIndex = EventIndex[RootHeap];
		AllocationPtr->EndTime = Time;

		AllocationPtr->FreeCallstackId = CallstackId;

		const uint64 OldSize = AllocationPtr->GetSize();

		SbTree[RootHeap]->AddAlloc(AllocationPtr); // SbTree takes ownership of AllocationPtr

		uint32 EventDistance = AllocationPtr->EndEventIndex - AllocationPtr->StartEventIndex;
		UpdateHistogramByEventDistance(EventDistance);

		// Update stats for the current timeline sample. (Heap allocations are already excluded)
		if (!AllocationPtr->IsHeap())
		{
			TotalAllocatedMemory -= OldSize;
			--TotalLiveAllocations;
			SampleMinTotalAllocatedMemory = FMath::Min(SampleMinTotalAllocatedMemory, TotalAllocatedMemory);
			SampleMinLiveAllocations = FMath::Min(SampleMinLiveAllocations, TotalLiveAllocations);
		}
		++SampleFreeEvents;
	}
	else
	{
		++FreeErrors;
		if (FreeErrors <= MaxLogMessagesPerErrorType)
		{
			UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] Invalid FREE event (Address=0x%llX, RootHeap=%u, Time=%f)!"), Address, RootHeap, Time);
		}
	}

	++FreeCount;
	if (EventIndex[RootHeap] != ~0u)
	{
		++EventIndex[RootHeap];
	}
	else
	{
		UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] Too many events!"));
		bInitialized = false; // ignore further events
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::EditUnmarkAllocationAsHeap(double Time, uint64 Address, HeapId Heap)
{
	Lock.WriteAccessCheck();

#if INSIGHTS_DEBUG_WATCH
	for (int32 AddrIndex = 0; AddrIndex < UE_ARRAY_COUNT(GWatchAddresses); ++AddrIndex)
	{
		if (GWatchAddresses[AddrIndex] == Address)
		{
			UE_LOG(LogTraceServices, Warning, TEXT("[MemAlloc][%u] HeapUnmarkAlloc 0x%llX : Heap=%u, Time=%f"), CurrentTraceThreadId, Address, Heap, Time);
			INSIGHTS_DEBUG_WATCH_FOUND;
		}
	}
#endif // INSIGHTS_DEBUG_WATCH

	HeapId RootHeap = FindRootHeap(Heap);

	// Remove the heap allocation from the Live allocs.
	FAllocationItem* Alloc = LiveAllocs[RootHeap]->RemoveHeap(Address); // we take ownership of Alloc
	if (Alloc)
	{
		const uint64 Size = Alloc->GetSize();
		const uint32 Alignment = Alloc->GetAlignment();
		const uint32 CallstackId = Alloc->CallstackId;
		const uint32 FreeCallstackId = 0; // unknown

		// Re-add this allocation to the Live allocs.
		LiveAllocs[RootHeap]->Add(Alloc); // the Live allocs takes ownership of Alloc

		// We cannot just unmark the allocation as heap, there is no timestamp support, instead fake a "free"
		// event and an "alloc" event. Make sure the new allocation retains the tag from the original.
		CurrentTracker = 1;
		EditPushTagFromPtr(CurrentSystemThreadId, CurrentTracker, Address);
		EditFree(Time, FreeCallstackId, Address, RootHeap);
		EditAlloc(Time, CallstackId, Address, Size, Alignment, RootHeap);
		EditPopTagFromPtr(CurrentSystemThreadId, CurrentTracker);
		CurrentTracker = 0;
	}
	else
	{
		++HeapErrors;
		if (HeapErrors <= MaxLogMessagesPerErrorType)
		{
			UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] HeapUnmarkAlloc: Could not find address 0x%llX (Heap=%u, Time=%f)!"), Address, Heap, Time);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::EditMarkAllocationAsHeap(double Time, uint64 Address, HeapId Heap, EMemoryTraceHeapAllocationFlags Flags)
{
	Lock.WriteAccessCheck();

#if INSIGHTS_DEBUG_WATCH
	for (int32 AddrIndex = 0; AddrIndex < UE_ARRAY_COUNT(GWatchAddresses); ++AddrIndex)
	{
		if (GWatchAddresses[AddrIndex] == Address)
		{
			UE_LOG(LogTraceServices, Warning, TEXT("[MemAlloc][%u] HeapMarkAlloc 0x%llX : Heap=%u, Flags=%u, Time=%f"), CurrentTraceThreadId, Address, Heap, uint32(Flags), Time);
			INSIGHTS_DEBUG_WATCH_FOUND;
		}
	}
#endif // INSIGHTS_DEBUG_WATCH

	HeapId RootHeap = FindRootHeap(Heap);

	// Remove the allocation from the Live allocs.
	FAllocationItem* Alloc = LiveAllocs[RootHeap]->Remove(Address);
	if (Alloc)
	{
		// Mark allocation as a "heap" allocation.
		check(EnumHasAnyFlags(Flags, EMemoryTraceHeapAllocationFlags::Heap));
		Alloc->Flags = Flags;
		Alloc->RootHeap = static_cast<uint8>(RootHeap);

		// Re-add it to the Live allocs as a heap allocation.
		LiveAllocs[RootHeap]->AddHeap(Alloc);

		// Update stats. Remove this allocation from the total.
		TotalAllocatedMemory -= Alloc->GetSize();
		--TotalLiveAllocations;
		SampleMinTotalAllocatedMemory = FMath::Min(SampleMinTotalAllocatedMemory, TotalAllocatedMemory);
		SampleMinLiveAllocations = FMath::Min(SampleMinLiveAllocations, TotalLiveAllocations);
	}
	else
	{
		++HeapErrors;
		if (HeapErrors <= MaxLogMessagesPerErrorType)
		{
			UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] HeapMarkAlloc: Could not find address 0x%llX (Heap=%u, Flags=%u, Time=%f)!"), Address, Heap, uint32(Flags), Time);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::UpdateHistogramByAllocSize(uint64 Size)
{
	if (Size > MaxAllocSize)
	{
		MaxAllocSize = Size;
	}

	// HistogramIndex : Value Range
	// 0 : [0]
	// 1 : [1]
	// 2 : [2 .. 3]
	// 3 : [4 .. 7]
	// ...
	// i : [2^(i-1) .. 2^i-1], i > 0
	// ...
	// 64 : [2^63 .. 2^64-1]
	uint32 HistogramIndexPow2 = 64 - static_cast<uint32>(FMath::CountLeadingZeros64(Size));
	++AllocSizeHistogramPow2[HistogramIndexPow2];
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::UpdateHistogramByEventDistance(uint32 EventDistance)
{
	if (EventDistance > MaxEventDistance)
	{
		MaxEventDistance = EventDistance;
	}

	// HistogramIndex : Value Range
	// 0 : [0]
	// 1 : [1]
	// 2 : [2 .. 3]
	// 3 : [4 .. 7]
	// ...
	// i : [2^(i-1) .. 2^i-1], i > 0
	// ...
	// 32 : [2^31 .. 2^32-1]
	uint32 HistogramIndexPow2 = 32 - FMath::CountLeadingZeros(EventDistance);
	++EventDistanceHistogramPow2[HistogramIndexPow2];
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::AdvanceTimelines(double Time)
{
	// If enough time has passed (since the current sample is started)...
	if (Time - SampleStartTimestamp > DefaultTimelineSampleGranularity)
	{
		// Add the current sample to the timelines.
		Timeline.EmplaceBack(SampleStartTimestamp);
		MinTotalAllocatedMemoryTimeline.EmplaceBack(SampleMinTotalAllocatedMemory);
		MaxTotalAllocatedMemoryTimeline.EmplaceBack(SampleMaxTotalAllocatedMemory);
		MinLiveAllocationsTimeline.EmplaceBack(SampleMinLiveAllocations);
		MaxLiveAllocationsTimeline.EmplaceBack(SampleMaxLiveAllocations);
		AllocEventsTimeline.EmplaceBack(SampleAllocEvents);
		FreeEventsTimeline.EmplaceBack(SampleFreeEvents);

		// Start a new sample.
		SampleStartTimestamp = Time;
		SampleMinTotalAllocatedMemory = TotalAllocatedMemory;
		SampleMaxTotalAllocatedMemory = TotalAllocatedMemory;
		SampleMinLiveAllocations = TotalLiveAllocations;
		SampleMaxLiveAllocations = TotalLiveAllocations;
		SampleAllocEvents = 0;
		SampleFreeEvents = 0;

		// If the previous sample is well distanced in time...
		if (Time - SampleEndTimestamp > DefaultTimelineSampleGranularity)
		{
			// Add an intermediate "flat region" sample.
			Timeline.EmplaceBack(SampleEndTimestamp);
			MinTotalAllocatedMemoryTimeline.EmplaceBack(TotalAllocatedMemory);
			MaxTotalAllocatedMemoryTimeline.EmplaceBack(TotalAllocatedMemory);
			MinLiveAllocationsTimeline.EmplaceBack(TotalLiveAllocations);
			MaxLiveAllocationsTimeline.EmplaceBack(TotalLiveAllocations);
			AllocEventsTimeline.EmplaceBack(0);
			FreeEventsTimeline.EmplaceBack(0);
		}
	}

	SampleEndTimestamp = Time;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

HeapId FAllocationsProvider::FindRootHeap(HeapId Heap) const
{
	const FHeapSpec* RootHeap = &HeapSpecs[Heap];
	while (RootHeap->Parent != nullptr)
	{
		RootHeap = RootHeap->Parent;
	}
	return RootHeap->Id;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::EditPushTag(uint32 ThreadId, uint8 Tracker, TagIdType Tag)
{
	EditAccessCheck();

	TagTracker.PushTag(ThreadId, Tracker, Tag);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::EditPopTag(uint32 ThreadId, uint8 Tracker)
{
	EditAccessCheck();

	TagTracker.PopTag(ThreadId, Tracker);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::EditPushTagFromPtr(uint32 ThreadId, uint8 Tracker, uint64 Ptr)
{
	EditAccessCheck();

	// Currently only system root heap is affected by reallocs, so limit search.
	FLiveAllocCollection* Allocs = LiveAllocs[EMemoryTraceRootHeap::SystemMemory];
	FAllocationItem* Alloc = Allocs ? Allocs->FindRef(Ptr) : nullptr;
	const TagIdType Tag = Alloc ? Alloc->Tag : 0; // If ptr is not found use "Untagged"
	TagTracker.PushTagFromPtr(ThreadId, Tracker, Tag);

	if (!Alloc)
	{
		++MiscErrors;
		if (MiscErrors <= MaxLogMessagesPerErrorType)
		{
			UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] Invalid Ptr for MemoryScopePtr event (Ptr=0x%llX)!"), Ptr);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::EditPopTagFromPtr(uint32 ThreadId, uint8 Tracker)
{
	EditAccessCheck();

	TagTracker.PopTagFromPtr(ThreadId, Tracker);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::EditOnAnalysisCompleted(double Time)
{
	Lock.WriteAccessCheck();

	if (!bInitialized)
	{
		return;
	}

#if 0
	const bool bResetTimelineAtEnd = false;

	// Add all live allocs to SbTree (with infinite end time).
	uint64 LiveAllocsTotalSize = 0;
	LiveAllocs.Enumerate([](const FAllocationItem& Alloc)
	{
		FAllocationItem* AllocationPtr = const_cast<FAllocationItem*>(&Alloc);

		LiveAllocsTotalSize += AllocationPtr->GetSize();

		// Assign same event index to all live allocs at the end of the session.
		AllocationPtr->EndEventIndex = EventIndex;

		SbTree->AddAlloc(AllocationPtr);

		uint32 EventDistance = AllocationPtr->EndEventIndex - AllocationPtr->StartEventIndex;
		UpdateHistogramByEventDistance(EventDistance);
	});
	//TODO: LiveAllocs.RemoveAll();
	check(TotalAllocatedMemory == LiveAllocsTotalSize);

	if (bResetTimelineAtEnd)
	{
		AdvanceTimelines(Time + 10 * DefaultTimelineSampleGranularity);

		const uint32 LiveAllocsTotalCount = TotalLiveAllocations;
		LiveAllocs.Empty();

		// Update stats for the last timeline sample (reset to zero).
		TotalAllocatedMemory = 0;
		SampleMinTotalAllocatedMemory = 0;
		SampleMinLiveAllocations = 0;
		SampleFreeEvents += LiveAllocsTotalCount;
	}
#endif

	// Flush the last cached timeline sample.
	AdvanceTimelines(std::numeric_limits<double>::infinity());

#if 0
	DebugPrint();
#endif

	for (const FSbTree* Tree : SbTree)
	{
		if (Tree)
		{
			Tree->Validate();
		}
	}

	//TODO: shrink live allocs buffers

	if (AllocErrors > 0)
	{
		UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] ALLOC event errors: %u"), AllocErrors);
	}
	if (FreeErrors > 0)
	{
		UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] FREE event errors: %u"), FreeErrors);
	}
	if (HeapErrors > 0)
	{
		UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] HEAP event errors: %u"), HeapErrors);
	}
	if (MiscErrors > 0)
	{
		UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] Other errors: %u"), MiscErrors);
	}
	if (TagTracker.GetNumErrors() > 0)
	{
		UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] TagTracker errors: %u"), TagTracker.GetNumErrors());
	}

	uint64 TotalEventCount = 0;
	for (uint32 RootHeap = 0; RootHeap < MaxRootHeaps; ++RootHeap)
	{
		TotalEventCount += EventIndex[RootHeap];
	}
	UE_LOG(LogTraceServices, Log, TEXT("[MemAlloc] Analysis Completed (%llu events, %llu allocs, %llu frees)"), TotalEventCount, AllocCount, FreeCount);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::EnumerateRootHeaps(TFunctionRef<void(HeapId Id, const FHeapSpec&)> Callback) const
{
	for (const FHeapSpec& Spec : HeapSpecs)
	{
		if (Spec.Parent == nullptr && Spec.Name != nullptr)
		{
			Callback(Spec.Id, Spec);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::GetTimelineIndexRange(double StartTime, double EndTime, int32& StartIndex, int32& EndIndex) const
{
	using PageType = const TPagedArrayPage<double>;

	PageType* PageData = Timeline.GetPages();
	if (PageData)
	{
		const int32 NumPoints = static_cast<int32>(Timeline.Num());

		const int32 NumPages = static_cast<int32>(Timeline.NumPages());
		TArrayView<PageType, int32> Pages(PageData, NumPages);

		const int32 StartPageIndex = Algo::UpperBoundBy(Pages, StartTime, [](PageType& Page) { return Page.Items[0]; }) - 1;
		if (StartPageIndex < 0)
		{
			StartIndex = -1;
		}
		else
		{
			PageType& Page = PageData[StartPageIndex];
			TArrayView<double> PageValues(Page.Items, static_cast<int32>(Page.Count));
			const int32 Index = Algo::UpperBound(PageValues, StartTime) - 1;
			check(Index >= 0);
			StartIndex = StartPageIndex * static_cast<int32>(Timeline.GetPageSize()) + Index;
			check(Index < NumPoints);
		}

		const int32 EndPageIndex = Algo::UpperBoundBy(Pages, EndTime, [](PageType& Page) { return Page.Items[0]; }) - 1;
		if (EndPageIndex < 0)
		{
			EndIndex = -1;
		}
		else
		{
			PageType& Page = PageData[EndPageIndex];
			TArrayView<double> PageValues(Page.Items, static_cast<int32>(Page.Count));
			const int32 Index = Algo::UpperBound(PageValues, EndTime) - 1;
			check(Index >= 0);
			EndIndex = EndPageIndex * static_cast<int32>(Timeline.GetPageSize()) + Index;
			check(Index < NumPoints);
		}
	}
	else
	{
		StartIndex = -1;
		EndIndex = -1;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::EnumerateMinTotalAllocatedMemoryTimeline(int32 StartIndex, int32 EndIndex, TFunctionRef<void(double Time, double Duration, uint64 Value)> Callback) const
{
	const int32 NumPoints = static_cast<int32>(Timeline.Num());
	StartIndex = FMath::Max(StartIndex, 0);
	EndIndex = FMath::Min(EndIndex + 1, NumPoints); // make it exclusive
	if (StartIndex < EndIndex)
	{
		auto TimeIt = Timeline.GetIteratorFromItem(StartIndex);
		auto ValueIt = MinTotalAllocatedMemoryTimeline.GetIteratorFromItem(StartIndex);
		double PrevTime = *TimeIt;
		uint64 PrevValue = *ValueIt;
		++TimeIt;
		++ValueIt;
		for (int32 Index = StartIndex + 1; Index < EndIndex; ++Index, ++TimeIt, ++ValueIt)
		{
			const double Time = *TimeIt;
			Callback(PrevTime, Time - PrevTime, PrevValue);
			PrevTime = *TimeIt;
			PrevValue = *ValueIt;
		}
		if (EndIndex < NumPoints)
		{
			const double Time = *TimeIt;
			Callback(PrevTime, Time - PrevTime, PrevValue);
		}
		else
		{
			Callback(PrevTime, std::numeric_limits<double>::infinity(), PrevValue);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::EnumerateMaxTotalAllocatedMemoryTimeline(int32 StartIndex, int32 EndIndex, TFunctionRef<void(double Time, double Duration, uint64 Value)> Callback) const
{
	const int32 NumPoints = static_cast<int32>(Timeline.Num());
	StartIndex = FMath::Max(StartIndex, 0);
	EndIndex = FMath::Min(EndIndex + 1, NumPoints); // make it exclusive
	if (StartIndex < EndIndex)
	{
		auto TimeIt = Timeline.GetIteratorFromItem(StartIndex);
		auto ValueIt = MaxTotalAllocatedMemoryTimeline.GetIteratorFromItem(StartIndex);
		double PrevTime = *TimeIt;
		uint64 PrevValue = *ValueIt;
		++TimeIt;
		++ValueIt;
		for (int32 Index = StartIndex + 1; Index < EndIndex; ++Index, ++TimeIt, ++ValueIt)
		{
			const double Time = *TimeIt;
			Callback(PrevTime, Time - PrevTime, PrevValue);
			PrevTime = *TimeIt;
			PrevValue = *ValueIt;
		}
		if (EndIndex < NumPoints)
		{
			const double Time = *TimeIt;
			Callback(PrevTime, Time - PrevTime, PrevValue);
		}
		else
		{
			Callback(PrevTime, std::numeric_limits<double>::infinity(), PrevValue);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::EnumerateMinLiveAllocationsTimeline(int32 StartIndex, int32 EndIndex, TFunctionRef<void(double Time, double Duration, uint32 Value)> Callback) const
{
	const int32 NumPoints = static_cast<int32>(Timeline.Num());
	StartIndex = FMath::Max(StartIndex, 0);
	EndIndex = FMath::Min(EndIndex + 1, NumPoints); // make it exclusive
	if (StartIndex < EndIndex)
	{
		auto TimeIt = Timeline.GetIteratorFromItem(StartIndex);
		auto ValueIt = MinLiveAllocationsTimeline.GetIteratorFromItem(StartIndex);
		double PrevTime = *TimeIt;
		uint32 PrevValue = *ValueIt;
		++TimeIt;
		++ValueIt;
		for (int32 Index = StartIndex + 1; Index < EndIndex; ++Index, ++TimeIt, ++ValueIt)
		{
			const double Time = *TimeIt;
			Callback(PrevTime, Time - PrevTime, PrevValue);
			PrevTime = *TimeIt;
			PrevValue = *ValueIt;
		}
		if (EndIndex < NumPoints)
		{
			const double Time = *TimeIt;
			Callback(PrevTime, Time - PrevTime, PrevValue);
		}
		else
		{
			Callback(PrevTime, std::numeric_limits<double>::infinity(), PrevValue);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::EnumerateMaxLiveAllocationsTimeline(int32 StartIndex, int32 EndIndex, TFunctionRef<void(double Time, double Duration, uint32 Value)> Callback) const
{
	const int32 NumPoints = static_cast<int32>(Timeline.Num());
	StartIndex = FMath::Max(StartIndex, 0);
	EndIndex = FMath::Min(EndIndex + 1, NumPoints); // make it exclusive
	if (StartIndex < EndIndex)
	{
		auto TimeIt = Timeline.GetIteratorFromItem(StartIndex);
		auto ValueIt = MaxLiveAllocationsTimeline.GetIteratorFromItem(StartIndex);
		double PrevTime = *TimeIt;
		uint32 PrevValue = *ValueIt;
		++TimeIt;
		++ValueIt;
		for (int32 Index = StartIndex + 1; Index < EndIndex; ++Index, ++TimeIt, ++ValueIt)
		{
			const double Time = *TimeIt;
			Callback(PrevTime, Time - PrevTime, PrevValue);
			PrevTime = *TimeIt;
			PrevValue = *ValueIt;
		}
		if (EndIndex < NumPoints)
		{
			const double Time = *TimeIt;
			Callback(PrevTime, Time - PrevTime, PrevValue);
		}
		else
		{
			Callback(PrevTime, std::numeric_limits<double>::infinity(), PrevValue);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::EnumerateAllocEventsTimeline(int32 StartIndex, int32 EndIndex, TFunctionRef<void(double Time, double Duration, uint32 Value)> Callback) const
{
	const int32 NumPoints = static_cast<int32>(Timeline.Num());
	StartIndex = FMath::Max(StartIndex, 0);
	EndIndex = FMath::Min(EndIndex + 1, NumPoints); // make it exclusive
	if (StartIndex < EndIndex)
	{
		auto TimeIt = Timeline.GetIteratorFromItem(StartIndex);
		auto ValueIt = AllocEventsTimeline.GetIteratorFromItem(StartIndex);
		double PrevTime = *TimeIt;
		uint32 PrevValue = *ValueIt;
		++TimeIt;
		++ValueIt;
		for (int32 Index = StartIndex + 1; Index < EndIndex; ++Index, ++TimeIt, ++ValueIt)
		{
			const double Time = *TimeIt;
			Callback(PrevTime, Time - PrevTime, PrevValue);
			PrevTime = *TimeIt;
			PrevValue = *ValueIt;
		}
		if (EndIndex < NumPoints)
		{
			const double Time = *TimeIt;
			Callback(PrevTime, Time - PrevTime, PrevValue);
		}
		else
		{
			Callback(PrevTime, std::numeric_limits<double>::infinity(), PrevValue);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::EnumerateFreeEventsTimeline(int32 StartIndex, int32 EndIndex, TFunctionRef<void(double Time, double Duration, uint32 Value)> Callback) const
{
	const int32 NumPoints = static_cast<int32>(Timeline.Num());
	StartIndex = FMath::Max(StartIndex, 0);
	EndIndex = FMath::Min(EndIndex + 1, NumPoints); // make it exclusive
	if (StartIndex < EndIndex)
	{
		auto TimeIt = Timeline.GetIteratorFromItem(StartIndex);
		auto ValueIt = FreeEventsTimeline.GetIteratorFromItem(StartIndex);
		double PrevTime = *TimeIt;
		uint32 PrevValue = *ValueIt;
		++TimeIt;
		++ValueIt;
		for (int32 Index = StartIndex + 1; Index < EndIndex; ++Index, ++TimeIt, ++ValueIt)
		{
			const double Time = *TimeIt;
			Callback(PrevTime, Time - PrevTime, PrevValue);
			PrevTime = *TimeIt;
			PrevValue = *ValueIt;
		}
		if (EndIndex < NumPoints)
		{
			const double Time = *TimeIt;
			Callback(PrevTime, Time - PrevTime, PrevValue);
		}
		else
		{
			Callback(PrevTime, std::numeric_limits<double>::infinity(), PrevValue);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::EnumerateTags(TFunctionRef<void(const TCHAR*, const TCHAR*, TagIdType, TagIdType)> Callback) const
{
	TagTracker.EnumerateTags(Callback);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::DebugPrint() const
{
	for (const FSbTree* Tree : SbTree)
	{
		if (Tree)
		{
			Tree->DebugPrint();
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

IAllocationsProvider::FQueryHandle FAllocationsProvider::StartQuery(const IAllocationsProvider::FQueryParams& Params) const
{
	auto* Inner = new FAllocationsQuery(*this, Params);
	return IAllocationsProvider::FQueryHandle(Inner);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::CancelQuery(FQueryHandle Query) const
{
	auto* Inner = (FAllocationsQuery*)Query;
	return Inner->Cancel();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const IAllocationsProvider::FQueryStatus FAllocationsProvider::PollQuery(FQueryHandle Query) const
{
	auto* Inner = (FAllocationsQuery*)Query;
	return Inner->Poll();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::EnumerateLiveAllocs(TFunctionRef<void(const FAllocationItem& Alloc)> Callback) const
{
	ReadAccessCheck();
	for (const FLiveAllocCollection* Allocs : LiveAllocs)
	{
		if (Allocs)
		{
			Allocs->Enumerate(Callback);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

uint32 FAllocationsProvider::GetNumLiveAllocs() const
{
	ReadAccessCheck();
	return TotalLiveAllocations;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FName GetAllocationsProviderName()
{
	static FName Name(TEXT("AllocationsProvider"));
	return Name;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const IAllocationsProvider* ReadAllocationsProvider(const IAnalysisSession& Session)
{
	return Session.ReadProvider<IAllocationsProvider>(GetAllocationsProviderName());
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace TraceServices

#undef INSIGHTS_SLOW_CHECK
#undef INSIGHTS_DEBUG_WATCH
#undef INSIGHTS_DEBUG_WATCH_FOUND
#undef INSIGHTS_LLA_RESERVE
#undef INSIGHTS_USE_SHORT_LIVING_ALLOCS
#undef INSIGHTS_SLA_USE_ADDRESS_MAP
#undef INSIGHTS_USE_LAST_ALLOC
#undef INSIGHTS_VALIDATE_ALLOC_EVENTS
#undef INSIGHTS_DEBUG_METADATA
