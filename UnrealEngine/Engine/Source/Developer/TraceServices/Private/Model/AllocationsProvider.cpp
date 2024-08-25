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

// Detects cases where an address is allocated multiple times (i.e. missing Free or MarkAllocAsHeap events).
// Note: This slows down analysis significantly, so it is disabled by default.
#define INSIGHTS_VALIDATE_ALLOC_EVENTS 0

// Automatically free the previous alloc when detecting addresses allocated multiple times (INSIGHTS_VALIDATE_ALLOC_EVENTS).
#define INSIGHTS_DOUBLE_ALLOC_FREE_PREVIOUS 1

// Automatically add a fake alloc when Free event detects a missing alloc.
#define INSIGHTS_VALIDATE_FREE_EVENTS 1

// Automatically add a fake alloc when MarkAllocAsHeap event detects a missing alloc.
#define INSIGHTS_VALIDATE_HEAP_MARK_ALLOC_EVENTS 1

// Automatically add a fake heap (or mark an existing alloc as heap) when UnmarkAllocAsHeap detects a missing alloc.
#define INSIGHTS_VALIDATE_HEAP_UNMARK_ALLOC_EVENTS 1

#define INSIGHTS_DEBUG_METADATA 0

// Generates warnings for alloc/free events with nullptr, for the System root heap.
// Note: Allocating nullptr with size != 0 will always generate warnings.
#define INSIGHTS_WARNINGS_FOR_NULLPTR_ALLOCS 0

////////////////////////////////////////////////////////////////////////////////////////////////////

// Debug functionality to discard some of the Alloc/Free/MarkAllocAsHeap/UnmarkAllocAsHeap events.
#define INSIGHTS_FILTER_EVENTS_ENABLED 0

#if INSIGHTS_FILTER_EVENTS_ENABLED
namespace TraceServices
{
bool FAllocationsProvider::ShouldIgnoreEvent(uint32 ThreadId, double Time, uint64 Address, HeapId RootHeapId, uint32 CallstackId)
{
	//if (RootHeapId != EMemoryTraceRootHeap::VideoMemory)
	//{
	//	return true;
	//}
	//if (Address != 0x0ull)
	//{
	//	return true;
	//}
	//if (CallstackId != 0)
	//{
	//	return true;
	//}
	//if (Time > 10.0) // stop analysis after specified time
	//{
	//	EditOnAnalysisCompleted(Time);
	//	bInitialized = false;
	//	return true;
	//}
	return false;
}
} // namespace TraceServices
#define INSIGHTS_FILTER_EVENT(ThreadId, Time, Address, RootHeapId, CallstackId) { if (ShouldIgnoreEvent(ThreadId, Time, Address, RootHeapId, CallstackId)) return; }
#else // INSIGHTS_FILTER_EVENTS_ENABLED
#define INSIGHTS_FILTER_EVENT(ThreadId, Time, Address, RootHeapId, CallstackId)
#endif // INSIGHTS_FILTER_EVENTS_ENABLED

////////////////////////////////////////////////////////////////////////////////////////////////////

#define INSIGHTS_DEBUG_WATCH 0

#if INSIGHTS_DEBUG_WATCH
namespace TraceServices
{
	static uint64 GWatchAddresses[] =
	{
		// add here addresses to watch
		//0x0ull,
	};
}
#endif //INSIGHTS_DEBUG_WATCH

// Action to be executed when a match is found.
#define INSIGHTS_DEBUG_WATCH_FOUND // just log the API call
//#define INSIGHTS_DEBUG_WATCH_FOUND return // ignore events
//#define INSIGHTS_DEBUG_WATCH_FOUND UE_DEBUG_BREAK()

////////////////////////////////////////////////////////////////////////////////////////////////////

#define INSIGHTS_LOGF(Format, ...) { UE_LOG(LogTraceServices, Log, TEXT("[MemAlloc]") Format, __VA_ARGS__); }
//#define INSIGHTS_LOGF(Format, ...) { FPlatformMisc::LowLevelOutputDebugStringf(TEXT("[MemAlloc]") Format TEXT("\n"), __VA_ARGS__); }

#if 0 || INSIGHTS_DEBUG_WATCH
#define INSIGHTS_API_LOGF(ThreadId, Time, Address, Format, ...) \
	INSIGHTS_LOGF(TEXT("[API][%3u][%f] ") Format, ThreadId, Time, __VA_ARGS__);
#else
#define INSIGHTS_API_LOGF(ThreadId, Time, Address, Format, ...)
#endif

#if 0 || INSIGHTS_DEBUG_WATCH
#define INSIGHTS_INDIRECT_API_LOGF(ApiName, ThreadId, Time, Address) \
	INSIGHTS_LOGF(TEXT("[API] --> ") ApiName TEXT(" 0x%llX"), Address);
#else
#define INSIGHTS_INDIRECT_API_LOGF(ApiName, ThreadId, Time, Address)
#endif

#if INSIGHTS_DEBUG_WATCH
	#define INSIGHTS_WATCH_API_LOGF(ThreadId, Time, Address, Format, ...) \
	{\
		if (IsAddressWatched(Address))\
		{\
			INSIGHTS_DEBUG_WATCH_FOUND;\
			INSIGHTS_API_LOGF(ThreadId, Time, Address, Format, __VA_ARGS__);\
		}\
	}
	#define INSIGHTS_WATCH_INDIRECT_API_LOGF(ApiName, ThreadId, Time, Address) \
	{\
		if (IsAddressWatched(Address))\
		{\
			INSIGHTS_DEBUG_WATCH_FOUND;\
			INSIGHTS_INDIRECT_API_LOGF(ApiName, ThreadId, Time, Address);\
		}\
	}
#else
	#define INSIGHTS_WATCH_API_LOGF(ThreadId, Time, Address, Format, ...) INSIGHTS_API_LOGF(ThreadId, Time, Address, Format, __VA_ARGS__)
	#define INSIGHTS_WATCH_INDIRECT_API_LOGF(ApiName, ThreadId, Time, Address) INSIGHTS_INDIRECT_API_LOGF(ApiName, ThreadId, Time, Address)
#endif // INSIGHTS_DEBUG_WATCH

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace TraceServices
{

constexpr uint32 MaxLogMessagesPerWarningType = 100;
constexpr uint32 MaxLogMessagesPerErrorType = 100;

#if INSIGHTS_DEBUG_WATCH
static bool IsAddressWatched(uint64 InAddress)
{
	for (int32 AddrIndex = 0; AddrIndex < UE_ARRAY_COUNT(GWatchAddresses); ++AddrIndex)
	{
		if (GWatchAddresses[AddrIndex] == InAddress)
		{
			return true;
		}
	}
	return false;
}
#endif // INSIGHTS_DEBUG_WATCH

thread_local FProviderLock::FThreadLocalState GAllocationsProviderLockState;

static const TCHAR* GDefaultHeapName = TEXT("Unknown");

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
				for (const auto& EntryPair : TagMap)
				{
					const uint32 Id = EntryPair.Get<0>();
					const FTagEntry Entry = EntryPair.Get<1>();
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

		const TCHAR* TagDisplayName = Session.StoreString(DisplayName);
		const TCHAR* TagFullPath = Session.StoreString(FullName.ToString());
		const FTagEntry& Entry = TagMap.Emplace(InTag, FTagEntry{ TagDisplayName, TagFullPath, InParentTag });

		// Check if this new tag has been referenced before by a child tag
		for (const TTuple<TagIdType,FString>& Pending : PendingTags)
		{
			const TagIdType ReferencingId = Pending.Get<0>();
			const FString& Name = Pending.Get<1>();
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
			UE_LOG(LogTraceServices, Verbose, TEXT("[MemAlloc] Added Tag '%s' ('%s') with id %u (ParentTag=%u)."), Entry.Display, Entry.FullPath, InTag, InParentTag);
		}
	}
	else
	{
		++NumErrors;
		if (NumErrors <= MaxLogMessagesPerErrorType)
		{
			UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] Tag with id %u (ParentTag=%u, Display='%s') already added!"), InTag, InParentTag, InDisplay);
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

uint32 IAllocationsProvider::FAllocation::GetAllocThreadId() const
{
	const auto* Inner = (const FAllocationItem*)this;
	return Inner->AllocThreadId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

uint32 IAllocationsProvider::FAllocation::GetFreeThreadId() const
{
	const auto* Inner = (const FAllocationItem*)this;
	return Inner->FreeThreadId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

uint32 IAllocationsProvider::FAllocation::GetAllocCallstackId() const
{
	const auto* Inner = (const FAllocationItem*)this;
	return Inner->AllocCallstackId;
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

FAllocationItem* FShortLivingAllocs::FindRef(uint64 Address) const
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

FAllocationItem* FShortLivingAllocs::FindRange(const uint64 Address) const
{
	//TODO: Can we do better than iterating all the nodes?
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

void FShortLivingAllocs::Enumerate(TFunctionRef<void(const FAllocationItem& Alloc)> Callback) const
{
	FNode* Node = LastAddedAllocNode;
	while (Node != nullptr)
	{
		Callback(*Node->Alloc);
		Node = Node->Prev;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FShortLivingAllocs::Enumerate(uint64 StartAddress, uint64 EndAddress, TFunctionRef<void(const FAllocationItem& Alloc)> Callback) const
{
	FNode* Node = LastAddedAllocNode;
	while (Node != nullptr)
	{
		const FAllocationItem& Alloc = *Node->Alloc;
		if (Alloc.Address >= StartAddress && Alloc.Address < EndAddress)
		{
			Callback(Alloc);
		}
		Node = Node->Prev;
	}
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
	if (Node->Prev)
	{
		Node->Prev->Next = Node->Next;
	}
	else
	{
		INSIGHTS_SLOW_CHECK(Node == OldestAllocNode);
		OldestAllocNode = Node->Next;
	}
	if (Node->Next)
	{
		Node->Next->Prev = Node->Prev;
	}
	else
	{
		INSIGHTS_SLOW_CHECK(Node == LastAddedAllocNode)
		LastAddedAllocNode = Node->Prev;
	}

	// Add the removed node to the "unused" list.
	Node->Next = FirstUnusedNode;
	FirstUnusedNode = Node;

	return Node->Alloc;
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

FAllocationItem* FHeapAllocs::FindRef(uint64 Address) const
{
	const FList* ListPtr = AddressMap.Find(Address);
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

void FHeapAllocs::Enumerate(uint64 StartAddress, uint64 EndAddress, TFunctionRef<void(const FAllocationItem& Alloc)> Callback) const
{
	for (const auto& KV : AddressMap)
	{
		if (KV.Key >= StartAddress && KV.Key < EndAddress)
		{
			// Iterate in same order as added.
			for (const FNode* Node = KV.Value.First; Node != nullptr; Node = Node->Next)
			{
				//check(Node->Alloc->Address == KV.Key);
				Callback(*Node->Alloc);
			}
		}
	}
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

FAllocationItem* FLiveAllocCollection::FindRef(uint64 Address) const
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

FAllocationItem* FLiveAllocCollection::FindHeapRef(uint64 Address) const
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

FAllocationItem* FLiveAllocCollection::FindByAddressRange(uint64 Address) const
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

	FAllocationItem* FoundLongLivingAlloc = LongLivingAllocs.FindRange(Address);
	if (FoundLongLivingAlloc)
	{
		INSIGHTS_SLOW_CHECK(FoundLongLivingAlloc->IsContained(Address));
		return FoundLongLivingAlloc;
	}

	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FAllocationItem* FLiveAllocCollection::FindHeapByAddressRange(uint64 Address) const
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

	LongLivingAllocs.Enumerate(Callback);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FLiveAllocCollection::Enumerate(uint64 StartAddress, uint64 EndAddress, TFunctionRef<void(const FAllocationItem& Alloc)> Callback) const
{
	HeapAllocs.Enumerate(StartAddress, EndAddress, Callback);

#if INSIGHTS_USE_LAST_ALLOC
	if (LastAlloc && LastAlloc->Address >= StartAddress && LastAlloc->Address < EndAddress)
	{
		Callback(*LastAlloc);
	}
#endif

#if INSIGHTS_USE_SHORT_LIVING_ALLOCS
	ShortLivingAllocs.Enumerate(StartAddress, EndAddress, Callback);
#endif

	LongLivingAllocs.Enumerate(StartAddress, EndAddress, Callback);
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

	LongLivingAllocs.Add(Alloc);
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

	FAllocationItem* RemovedLongLivingAlloc = LongLivingAllocs.Remove(Address);
	if (RemovedLongLivingAlloc)
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
	// Initial number of heap spec locations. Actual number of heap specs is unlimited (uint32).
	HeapSpecs.AddDefaulted(FMath::Max(1024u, MaxRootHeaps));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FAllocationsProvider::~FAllocationsProvider()
{
	// Delete root heaps.
	for (uint32 RootHeapIdx = 0; RootHeapIdx < MaxRootHeaps; ++RootHeapIdx)
	{
		FRootHeap* RootHeap = RootHeaps[RootHeapIdx];
		if (RootHeap)
		{
			RootHeap->HeapSpec = nullptr;

			if (RootHeap->SbTree)
			{
				delete RootHeap->SbTree;
				RootHeap->SbTree = nullptr;
			}
			if (RootHeap->LiveAllocs)
			{
				delete RootHeap->LiveAllocs;
				RootHeap->LiveAllocs = nullptr;
			}

			delete RootHeap;
			RootHeaps[RootHeapIdx] = nullptr;
		}
	}

	// Delete all heap specs.
	for (FHeapSpec* HeapSpec : HeapSpecs)
	{
		if (HeapSpec)
		{
			delete HeapSpec;
		}
	}
	HeapSpecs.Reset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::EditInit(double InTime, uint8 InMinAlignment)
{
	EditAccessCheck();

	if (bInitialized)
	{
		// error: already initialized
		return;
	}

	INSIGHTS_API_LOGF(0u, InTime, 0ull, TEXT("Init : MinAlignment=%u)"), uint32(InMinAlignment));

	InitTime = InTime;
	MinAlignment = InMinAlignment;

	// Create system root heap structures for backwards compatibility (before heap description events)
	AddHeapSpec(EMemoryTraceRootHeap::SystemMemory, 0, TEXT("System memory"), EMemoryTraceHeapFlags::Root);

	bInitialized = true;

	AdvanceTimelines(InTime);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::EditAlloc(uint32 ThreadId, double Time, uint32 CallstackId, uint64 Address, uint64 Size, uint32 Alignment, HeapId RootHeapId)
{
	EditAccessCheck();

	if (!bInitialized)
	{
		return;
	}

	INSIGHTS_WATCH_API_LOGF(ThreadId, Time, Address, TEXT("Alloc 0x%llX : Size=%llu Alignment=%u RootHeap=%u CallstackId=%u"), Address, Size, Alignment, RootHeapId, CallstackId);
	INSIGHTS_FILTER_EVENT(ThreadId, Time, Address, RootHeapId, CallstackId);

	if (Address == 0 && RootHeapId == EMemoryTraceRootHeap::SystemMemory)
	{
#if !INSIGHTS_WARNINGS_FOR_NULLPTR_ALLOCS
		if (Size == 0)
		{
			return;
		}
#endif // INSIGHTS_WARNINGS_FOR_NULLPTR_ALLOCS
		++AllocWarnings;
		if (AllocWarnings <= MaxLogMessagesPerWarningType)
		{
			UE_LOG(LogTraceServices, Warning, TEXT("[MemAlloc] Alloc at address 0 : Size=%llu, RootHeap=%u, Time=%f, CallstackId=%u"), Size, RootHeapId, Time, CallstackId);
		}
		return;
	}

	if (!IsValidRootHeap(RootHeapId))
	{
		++AllocWarnings;
		if (AllocWarnings <= MaxLogMessagesPerWarningType)
		{
			UE_LOG(LogTraceServices, Warning, TEXT("[MemAlloc] Alloc with invalid root heap id (%u)."), RootHeapId);
		}
		return;
	}

	FRootHeap& RootHeap = *RootHeaps[RootHeapId];

	RootHeap.SbTree->SetTimeForEvent(RootHeap.EventIndex, Time);

	AdvanceTimelines(Time);

	const uint8 Tracker = 0;
	const TagIdType Tag = TagTracker.GetCurrentTag(ThreadId, Tracker);

#if INSIGHTS_VALIDATE_ALLOC_EVENTS
	FAllocationItem* ExistingAllocationPtr = RootHeap.LiveAllocs->FindRef(Address);
	if (ExistingAllocationPtr)
	{
#if INSIGHTS_DOUBLE_ALLOC_FREE_PREVIOUS

		++AllocErrors;
		if (AllocErrors <= MaxLogMessagesPerErrorType)
		{
			UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] Invalid ALLOC event (Address=0x%llX, Size=%llu, Tag=%u, RootHeap=%u, Time=%f, CallstackId=%u)! The previous alloc will be freed."), Address, Size, Tag, RootHeapId, Time, CallstackId);
		}

		// Free the previous allocation.
		INSIGHTS_WATCH_INDIRECT_API_LOGF(TEXT("Free"), ThreadId, Time, Address);
		constexpr uint32 FreeCallstackId = 0; // no callstack
		EditFree(ThreadId, Time, FreeCallstackId, Address, RootHeapId);
		RootHeap.SbTree->SetTimeForEvent(RootHeap.EventIndex, Time); // for the case where the next event is first event in a new SbTree column after changing the heap alloc

#else // INSIGHTS_DOUBLE_ALLOC_FREE_PREVIOUS

		++AllocErrors;
		if (AllocErrors <= MaxLogMessagesPerErrorType)
		{
			UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] Invalid ALLOC event (Address=0x%llX, Size=%llu, Tag=%u, RootHeap=%u, Time=%f, CallstackId=%u)!"), Address, Size, Tag, RootHeapId, Time, CallstackId);
		}

#endif // INSIGHTS_DOUBLE_ALLOC_FREE_PREVIOUS
	}
#endif // INSIGHTS_VALIDATE_ALLOC_EVENTS

	{
		FAllocationItem* AllocationPtr = RootHeap.LiveAllocs->AddNew(Address);
		FAllocationItem& Allocation = *AllocationPtr;

		uint32 MetadataId = MetadataProvider.InvalidMetadataId;
		{
			FProviderEditScopeLock _(MetadataProvider);
			MetadataId = MetadataProvider.PinAndGetId(ThreadId);
#if INSIGHTS_DEBUG_METADATA
			if (MetadataId != FMetadataProvider::InvalidMetadataId &&
				!TagTracker.HasTagFromPtrScope(ThreadId, Tracker))
			{
				const uint32 MetadataStackSize = MetadataProvider.GetMetadataStackSize(ThreadId, MetadataId);
				check(MetadataStackSize > 0);
				uint16 MetaType;
				const void* MetaData;
				uint32 MetaDataSize;
				MetadataProvider.GetMetadata(ThreadId, MetadataId, MetadataStackSize - 1, MetaType, MetaData, MetaDataSize);
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
		Allocation.SizeAndAlignment = FAllocationItem::PackSizeAndAlignment(Size, static_cast<uint8>(Alignment));
		Allocation.StartEventIndex = RootHeap.EventIndex;
		Allocation.EndEventIndex = (uint32)-1;
		Allocation.StartTime = Time;
		Allocation.EndTime = std::numeric_limits<double>::infinity();
		Allocation.AllocThreadId = static_cast<uint16>(ThreadId);
		check(uint32(Allocation.AllocThreadId) == ThreadId);
		Allocation.FreeThreadId = 0;
		Allocation.AllocCallstackId = CallstackId;
		Allocation.FreeCallstackId = 0; // no callstack yet
		Allocation.MetadataId = MetadataId;
		Allocation.Tag = Tag;
		Allocation.RootHeap = static_cast<uint8>(RootHeapId);
		Allocation.Flags = EMemoryTraceHeapAllocationFlags::None;

		RootHeap.UpdateHistogramByAllocSize(Size);

		// Update stats for the current timeline sample.
		TotalAllocatedMemory += Size;
		++TotalLiveAllocations;
		SampleMaxTotalAllocatedMemory = FMath::Max(SampleMaxTotalAllocatedMemory, TotalAllocatedMemory);
		SampleMaxLiveAllocations = FMath::Max(SampleMaxLiveAllocations, TotalLiveAllocations);
		++SampleAllocEvents;
	}

	++AllocCount;
	if (RootHeap.EventIndex != ~0u)
	{
		RootHeap.EventIndex++;
	}
	else
	{
		UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] Too many events!"));
		bInitialized = false; // ignore further events
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::EditFree(uint32 ThreadId, double Time, uint32 CallstackId, uint64 Address, HeapId RootHeapId)
{
	EditAccessCheck();

	if (!bInitialized)
	{
		return;
	}

	INSIGHTS_WATCH_API_LOGF(ThreadId, Time, Address, TEXT("Free 0x%llX : RootHeap=%u CallstackId=%u"), Address, RootHeapId, CallstackId);
	INSIGHTS_FILTER_EVENT(ThreadId, Time, Address, RootHeapId, CallstackId);

	if (Address == 0 && RootHeapId == EMemoryTraceRootHeap::SystemMemory)
	{
#if INSIGHTS_WARNINGS_FOR_NULLPTR_ALLOCS
		++FreeWarnings;
		if (FreeWarnings <= MaxLogMessagesPerWarningType)
		{
			UE_LOG(LogTraceServices, Warning, TEXT("[MemAlloc] Free for address 0 : RootHeap=%u, Time=%f, CallstackId=%u"), RootHeapId, Time, CallstackId);
		}
#endif // INSIGHTS_WARNINGS_FOR_NULLPTR_ALLOCS
		return;
	}

	if (!IsValidRootHeap(RootHeapId))
	{
		++FreeWarnings;
		if (FreeWarnings <= MaxLogMessagesPerWarningType)
		{
			UE_LOG(LogTraceServices, Warning, TEXT("[MemAlloc] Free with invalid root heap id (%u)."), RootHeapId);
		}
		return;
	}

	FRootHeap& RootHeap = *RootHeaps[RootHeapId];

	RootHeap.SbTree->SetTimeForEvent(RootHeap.EventIndex, Time);

	AdvanceTimelines(Time);

	FAllocationItem* AllocationPtr = RootHeap.LiveAllocs->Remove(Address); // we take ownership of AllocationPtr
	if (!AllocationPtr)
	{
		// Try to free a heap allocation.
		AllocationPtr = RootHeap.LiveAllocs->FindHeapRef(Address);
		if (AllocationPtr)
		{
			INSIGHTS_SLOW_CHECK(AllocationPtr->IsHeap());
			HeapId Heap = AllocationPtr->RootHeap;
			INSIGHTS_WATCH_INDIRECT_API_LOGF(TEXT("UnmarkAllocationAsHeap"), ThreadId, Time, Address);
			EditUnmarkAllocationAsHeap(ThreadId, Time, CallstackId, Address, Heap);
			RootHeap.SbTree->SetTimeForEvent(RootHeap.EventIndex, Time); // for the case where the next event is first event in a new SbTree column after changing the heap alloc
			AllocationPtr = RootHeap.LiveAllocs->Remove(Address); // we take ownership of AllocationPtr
		}
	}
	else
	{
		INSIGHTS_SLOW_CHECK(!AllocationPtr->IsHeap());
	}

#if INSIGHTS_VALIDATE_FREE_EVENTS
	if (!AllocationPtr)
	{
		++FreeErrors;
		if (FreeErrors <= MaxLogMessagesPerErrorType)
		{
			UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] Invalid FREE event (Address=0x%llX, RootHeap=%u, Time=%f, CallstackId=%u)! A fake alloc will be created with size 0."), Address, RootHeapId, Time, CallstackId);
		}
		// Fake the missing alloc.
		constexpr uint64 FakeAllocSize = 0;
		constexpr uint32 FakeAllocAlignment = 0;
		INSIGHTS_WATCH_INDIRECT_API_LOGF(TEXT("Alloc"), ThreadId, Time, Address);
		EditAlloc(ThreadId, Time, CallstackId, Address, FakeAllocSize, FakeAllocAlignment, RootHeapId);
		RootHeap.SbTree->SetTimeForEvent(RootHeap.EventIndex, Time); // for the case where the next event is first event in a new SbTree column after adding the fake alloc
		AllocationPtr = RootHeap.LiveAllocs->Remove(Address); // we take ownership of AllocationPtr
	}
#endif // INSIGHTS_VALIDATE_FREE_EVENTS

	if (AllocationPtr)
	{
		check(RootHeap.EventIndex > AllocationPtr->StartEventIndex);
		AllocationPtr->EndEventIndex = RootHeap.EventIndex;
		AllocationPtr->EndTime = Time;

		AllocationPtr->FreeThreadId = static_cast<uint16>(ThreadId);
		check(uint32(AllocationPtr->FreeThreadId) == ThreadId);

		AllocationPtr->FreeCallstackId = CallstackId;

		const uint64 OldSize = AllocationPtr->GetSize();

		RootHeap.SbTree->AddAlloc(AllocationPtr); // SbTree takes ownership of AllocationPtr

		const uint32 EventDistance = AllocationPtr->EndEventIndex - AllocationPtr->StartEventIndex;
		RootHeap.UpdateHistogramByEventDistance(EventDistance);

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
			UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] Invalid FREE event (Address=0x%llX, RootHeap=%u, Time=%f, CallstackId=%u)!"), Address, RootHeapId, Time, CallstackId);
		}
	}

	++FreeCount;
	if (RootHeap.EventIndex != ~0u)
	{
		RootHeap.EventIndex++;
	}
	else
	{
		UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] Too many events!"));
		bInitialized = false; // ignore further events
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::EditHeapSpec(HeapId Id, HeapId ParentId, const FStringView& Name, EMemoryTraceHeapFlags Flags)
{
	EditAccessCheck();

	INSIGHTS_API_LOGF(0u, 0.0, 0ull, TEXT("HeapSpec : Id=%u ParentId=%u Name=\"%.*s\" Flags=0x%X"), Id, ParentId, Name.Len(), Name.GetData(), uint32(Flags));

	AddHeapSpec(Id, ParentId, Name, Flags);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

IAllocationsProvider::FHeapSpec& FAllocationsProvider::GetOrCreateHeapSpec(HeapId Id)
{
	if (Id < (uint32)HeapSpecs.Num())
	{
		if (HeapSpecs[Id] != nullptr)
		{
			return *HeapSpecs[Id];
		}
	}
	else
	{
		HeapSpecs.AddDefaulted(Id - HeapSpecs.Num() + 1);
	}

	FHeapSpec* HeapSpec = new FHeapSpec();

	HeapSpec->Id = Id;
	HeapSpec->Parent = nullptr;
	HeapSpec->Name = GDefaultHeapName;
	HeapSpec->Flags = EMemoryTraceHeapFlags::None;

	HeapSpecs[Id] = HeapSpec;
	return *HeapSpec;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FAllocationsProvider::FRootHeap& FAllocationsProvider::FindParentRootHeapUnchecked(HeapId Id) const
{
	const FHeapSpec* HeapSpec = HeapSpecs[Id];
	while (HeapSpec->Parent != nullptr)
	{
		HeapSpec = HeapSpec->Parent;
	}
	return *RootHeaps[HeapSpec->Id];
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::CreateRootHeap(HeapId Id)
{
	FRootHeap* RootHeapPtr = new FRootHeap();

	RootHeapPtr->HeapSpec = &GetOrCreateHeapSpec(Id);

	constexpr uint32 ColumnShift = 17; // 1<<17 = 128K
	RootHeapPtr->SbTree = new FSbTree(Session.GetLinearAllocator(), ColumnShift);

	RootHeapPtr->LiveAllocs = new FLiveAllocCollection();

	RootHeaps[Id] = RootHeapPtr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::AddHeapSpec(HeapId Id, HeapId ParentId, const FStringView& Name, EMemoryTraceHeapFlags Flags)
{
	const TCHAR* HeapName = Session.StoreString(Name);

	if (Id < MaxRootHeaps)
	{
		FHeapSpec* HeapSpec = HeapSpecs[Id];
		FRootHeap* RootHeapPtr = RootHeaps[Id];

		if (RootHeapPtr != nullptr)
		{
			check(HeapSpec != nullptr)
			check(HeapSpec->Id == Id);

			check(RootHeapPtr->HeapSpec == HeapSpec);
			check(RootHeapPtr->SbTree != nullptr);
			check(RootHeapPtr->LiveAllocs != nullptr);

			// "System" root heap is already created. See class constructor.
			if (Id != 0)
			{
				++HeapWarnings;
				if (HeapWarnings <= MaxLogMessagesPerWarningType)
				{
					UE_LOG(LogTraceServices, Warning, TEXT("[MemAlloc] Root heap %u has changed from (\"%s\", flags=%u) to (\"%s\", flags=%u)."),
						Id,
						HeapSpec->Name,
						int(HeapSpec->Flags),
						HeapName,
						int(Flags));
				}
			}
		}
		else
		{
			check(HeapSpec == nullptr);

			CreateRootHeap(Id);

			HeapSpec = HeapSpecs[Id];
			check(HeapSpec != nullptr);
		}

		HeapSpec->Name = HeapName;
		HeapSpec->Flags = Flags;
	}
	else
	{
		FHeapSpec& ParentHeapSpec = GetOrCreateHeapSpec(ParentId);
		if (ParentHeapSpec.Name == GDefaultHeapName)
		{
			if (ParentId < MaxRootHeaps && RootHeaps[ParentId] == nullptr)
			{
				CreateRootHeap(ParentId);
			}

			++HeapWarnings;
			if (HeapWarnings <= MaxLogMessagesPerWarningType)
			{
				UE_LOG(LogTraceServices, Warning, TEXT("[MemAlloc] Heap %u (\"%s\") is used before its parent heap %u was announced."), Id, HeapName, ParentId);
			}
		}

		if (IsValidHeap(Id))
		{
			FHeapSpec& PreviousHeap = GetHeapSpecUnchecked(Id);
			if (PreviousHeap.Name != GDefaultHeapName)
			{
				++HeapWarnings;
				if (HeapWarnings <= MaxLogMessagesPerWarningType)
				{
					UE_LOG(LogTraceServices, Warning, TEXT("[MemAlloc] Heap %u has changed from (\"%s\", parent=\"%s\", flags=%u) to (\"%s\", parent=\"%s\", flags=%u)."),
						Id,
						PreviousHeap.Name,
						PreviousHeap.Parent && PreviousHeap.Parent->Name ? PreviousHeap.Parent->Name : TEXT(""),
						int(PreviousHeap.Flags),
						HeapName,
						ParentHeapSpec.Name ? ParentHeapSpec.Name : TEXT(""),
						int(Flags));
				}
			}
		}

		FHeapSpec& HeapSpec = GetOrCreateHeapSpec(Id);
		HeapSpec.Parent = &ParentHeapSpec;
		HeapSpec.Name = HeapName;
		HeapSpec.Flags = Flags;

		ParentHeapSpec.Children.Add(&HeapSpec);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::EditMarkAllocationAsHeap(uint32 ThreadId, double Time, uint32 CallstackId, uint64 Address, HeapId Heap, EMemoryTraceHeapAllocationFlags Flags)
{
	EditAccessCheck();

	if (!bInitialized)
	{
		return;
	}

	INSIGHTS_WATCH_API_LOGF(ThreadId, Time, Address, TEXT("MarkAllocAsHeap 0x%llX : Heap=%u Flags=0x%X CallstackId=%u"), Address, Heap, uint32(Flags), CallstackId);

	if (!IsValidHeap(Heap))
	{
		++HeapErrors;
		if (HeapErrors <= MaxLogMessagesPerErrorType)
		{
			UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] HeapMarkAlloc: Heap %u is not valid (Address=0x%llX, Time=%f, CallstackId=%u)!"), Heap, Address, Time, CallstackId);
		}
		return;
	}

	const FHeapSpec& HeapSpec = GetHeapSpecUnchecked(Heap);
	const FRootHeap& RootHeap = FindParentRootHeapUnchecked(Heap);

	INSIGHTS_FILTER_EVENT(ThreadId, Time, Address, RootHeap.HeapSpec->Id, CallstackId);

#if 0 // TODO
	if (Heap == RootHeap.HeapSpec->Id)
	{
		++HeapWarnings;
		if (HeapWarnings <= MaxLogMessagesPerWarningType)
		{
			UE_LOG(LogTraceServices, Warning, TEXT("[MemAlloc] HeapMarkAlloc: Heap %u is a root heap (Address=0x%llX, Time=%f, CallstackId=%u)!"), Heap, Address, Time, CallstackId);
		}
	}
#endif

	// Remove the allocation from the Live allocs.
	FAllocationItem* Alloc = RootHeap.LiveAllocs->Remove(Address); // we take ownership of Alloc

#if INSIGHTS_VALIDATE_HEAP_MARK_ALLOC_EVENTS
	if (!Alloc)
	{
		++HeapErrors;
		if (HeapErrors <= MaxLogMessagesPerErrorType)
		{
			UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] HeapMarkAlloc: Could not find alloc with address 0x%llX (Heap=%u \"%s\", Flags=%u, Time=%f, CallstackId=%u)! A fake alloc will be created with size 0."), Address, Heap, HeapSpec.Name, uint32(Flags), Time, CallstackId);
		}

		// Fake the missing alloc.
		constexpr uint64 FakeAllocSize = 0;
		constexpr uint32 FakeAllocAlignment = 0;
		INSIGHTS_WATCH_INDIRECT_API_LOGF(TEXT("Alloc"), ThreadId, Time, Address);
		EditAlloc(ThreadId, Time, CallstackId, Address, FakeAllocSize, FakeAllocAlignment, RootHeap.HeapSpec->Id);
		RootHeap.SbTree->SetTimeForEvent(RootHeap.EventIndex, Time); // for the case where the next event is first event in a new SbTree column after adding the fake alloc
		Alloc = RootHeap.LiveAllocs->Remove(Address); // we take ownership of Alloc
	}
#endif // INSIGHTS_VALIDATE_HEAP_MARK_ALLOC_EVENTS

	if (Alloc)
	{
		check(Address == Alloc->Address);

		if (EnumHasAnyFlags(Alloc->Flags, EMemoryTraceHeapAllocationFlags::Heap))
		{
			++HeapErrors;
			if (HeapErrors <= MaxLogMessagesPerErrorType)
			{
				UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] HeapMarkAlloc: Alloc 0x%llX is already marked as heap (Heap=%u, Flags=%u, Time=%f, CallstackId=%u)!"), Address, Heap, uint32(Flags), Time, CallstackId);
			}
		}

		// Mark allocation as a "heap" allocation.
		ensure(EnumHasAnyFlags(Flags, EMemoryTraceHeapAllocationFlags::Heap));
		Alloc->Flags = Flags | EMemoryTraceHeapAllocationFlags::Heap;
		Alloc->RootHeap = static_cast<uint8>(Heap);
		if (CallstackId != 0)
		{
			Alloc->AllocCallstackId = CallstackId;
		}

		// Re-add it to the Live allocs as a heap allocation.
		RootHeap.LiveAllocs->AddHeap(Alloc); // the Live allocs takes ownership of Alloc

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
			UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] HeapMarkAlloc: Could not find alloc with address 0x%llX (Heap=%u \"%s\", Flags=%u, Time=%f, CallstackId=%u)!"), Address, Heap, HeapSpec.Name, uint32(Flags), Time, CallstackId);
		}
	}

	++HeapCount;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::EditUnmarkAllocationAsHeap(uint32 ThreadId, double Time, uint32 CallstackId, uint64 Address, HeapId Heap)
{
	EditAccessCheck();

	if (!bInitialized)
	{
		return;
	}

	INSIGHTS_WATCH_API_LOGF(ThreadId, Time, Address, TEXT("UnmarkAllocAsHeap 0x%llX : Heap=%u CallstackId=%u"), Address, Heap, CallstackId);

	if (!IsValidHeap(Heap))
	{
		++HeapErrors;
		if (HeapErrors <= MaxLogMessagesPerErrorType)
		{
			UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] HeapUnmarkAlloc: Heap %u is not valid (Address=0x%llX, Time=%f, CallstackId=%u)!"), Heap, Address, Time, CallstackId);
		}
		return;
	}

	const FHeapSpec& HeapSpec = GetHeapSpecUnchecked(Heap);
	const FRootHeap& RootHeap = FindParentRootHeapUnchecked(Heap);

	INSIGHTS_FILTER_EVENT(ThreadId, Time, Address, RootHeap.HeapSpec->Id, CallstackId);

#if 0 // TODO
	if (Heap == RootHeap.HeapSpec->Id)
	{
		++HeapWarnings;
		if (HeapWarnings <= MaxLogMessagesPerWarningType)
		{
			UE_LOG(LogTraceServices, Warning, TEXT("[MemAlloc] HeapUnmarkAlloc: Heap %u is a root heap (Address=0x%llX, Time=%f, CallstackId=%u)!"), Heap, Address, Time, CallstackId);
		}
	}
#endif

	// Remove the heap allocation from the Live allocs.
	FAllocationItem* Alloc = RootHeap.LiveAllocs->RemoveHeap(Address); // we take ownership of Alloc

#if INSIGHTS_VALIDATE_HEAP_UNMARK_ALLOC_EVENTS
	if (!Alloc)
	{
		// Remove the allocation from the Live allocs.
		Alloc = RootHeap.LiveAllocs->Remove(Address); // we take ownership of Alloc

		if (!Alloc)
		{
			++HeapErrors;
			if (HeapErrors <= MaxLogMessagesPerErrorType)
			{
				UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] HeapUnmarkAlloc: Could not find heap with address 0x%llX (Heap=%u \"%s\", Time=%f, CallstackId=%u)! A fake heap alloc will be created with size 0."), Address, Heap, HeapSpec.Name, Time, CallstackId);
			}

			// Fake the missing alloc.
			constexpr uint64 FakeAllocSize = 0;
			constexpr uint32 FakeAllocAlignment = 0;
			INSIGHTS_WATCH_INDIRECT_API_LOGF(TEXT("Alloc"), ThreadId, Time, Address);
			EditAlloc(ThreadId, Time, CallstackId, Address, FakeAllocSize, FakeAllocAlignment, RootHeap.HeapSpec->Id);
			RootHeap.SbTree->SetTimeForEvent(RootHeap.EventIndex, Time); // for the case where the next event is first event in a new SbTree column after adding the fake alloc
			Alloc = RootHeap.LiveAllocs->Remove(Address); // we take ownership of Alloc
			check(Alloc != nullptr);
		}
		else
		{
			++HeapErrors;
			if (HeapErrors <= MaxLogMessagesPerErrorType)
			{
				UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] HeapUnmarkAlloc: Could not find heap with address 0x%llX (Heap=%u \"%s\", Time=%f, CallstackId=%u)! An alloc with this address exists. It will be marked as heap."), Address, Heap, HeapSpec.Name, Time, CallstackId);
			}
		}

		check(RootHeap.LiveAllocs->FindRef(Address) == nullptr);

		// Mark allocation as a "heap" allocation.
		Alloc->Flags = Alloc->Flags | EMemoryTraceHeapAllocationFlags::Heap;
		Alloc->RootHeap = static_cast<uint8>(Heap);
		Alloc->AllocCallstackId = CallstackId;
	}
#endif // INSIGHTS_VALIDATE_HEAP_UNMARK_ALLOC_EVENTS

	if (Alloc)
	{
		check(Address == Alloc->Address);

		if (!EnumHasAnyFlags(Alloc->Flags, EMemoryTraceHeapAllocationFlags::Heap))
		{
			++HeapErrors;
			if (HeapErrors <= MaxLogMessagesPerErrorType)
			{
				UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] HeapUnmarkAlloc: Alloc 0x%llX is not marked as heap (Heap=%u, Time=%f, CallstackId=%u)!"), Address, Heap, Time, CallstackId);
			}
		}

		//constexpr bool bSearchLiveAllocs = false; // never
		const bool bSearchLiveAllocs = EnumHasAnyFlags(HeapSpec.Flags, EMemoryTraceHeapFlags::NeverFrees); // only for heaps with NeverFrees flag
		//constexpr bool bSearchLiveAllocs = true; // for all heaps

		if (bSearchLiveAllocs)
		{
			// Find all live allocs in this heap.
			uint64 AllocatedSizeInHeap = 0;
			struct FAllocInHeap
			{
				uint64 Address;
				uint64 Size;
			};
			TArray<FAllocInHeap> AllocsInHeap;
			const uint64 HeapSize = Alloc->GetSize();
			const uint64 EndAddress = Alloc->Address + HeapSize;
			RootHeap.LiveAllocs->Enumerate(Address, EndAddress, [&AllocatedSizeInHeap, &AllocsInHeap](const FAllocationItem& ChildAlloc)
			{
				const uint64 ChildAllocSize = ChildAlloc.GetSize();
				AllocatedSizeInHeap += ChildAllocSize;
				AllocsInHeap.Add({ ChildAlloc.Address, ChildAllocSize });
			});

			if (AllocsInHeap.Num() > 0)
			{
				if (!EnumHasAnyFlags(HeapSpec.Flags, EMemoryTraceHeapFlags::NeverFrees))
				{
					++HeapWarnings;
					if (HeapWarnings <= MaxLogMessagesPerWarningType)
					{
						// For heaps that do not have NeverFrees flag, we report live allocs as leaks.
						UE_LOG(LogTraceServices, Warning, TEXT("[MemAlloc] HeapUnmarkAlloc: %d memory leaks (%llu bytes) detected for heap %u (\"%s\", Address=0x%llX, Size=%llu, Time=%f, CallstackId=%u)"),
							AllocsInHeap.Num(), AllocatedSizeInHeap, Heap, HeapSpec.Name, Address, HeapSize, Time, CallstackId);
						UE_LOG(LogTraceServices, Warning, TEXT("[MemAlloc] Top memory leaks:"));
						AllocsInHeap.Sort([this](const FAllocInHeap& A, const FAllocInHeap& B) -> bool { return A.Size > B.Size; });
						int NumAllocsInTop = 10; // only show first 10 allocs
						for (const FAllocInHeap& AllocInHeap : AllocsInHeap)
						{
							UE_LOG(LogTraceServices, Warning, TEXT("[MemAlloc]     alloc 0x%llX (%u bytes)"), AllocInHeap.Address, AllocInHeap.Size);
							if (--NumAllocsInTop <= 0)
							{
								break;
							}
						}
					}
				}
#if 0 // debug
				else
				{
					UE_LOG(LogTraceServices, Log, TEXT("[MemAlloc] HeapUnmarkAlloc: %d memory allocs (%llu bytes) freed for heap %u (\"%s\", Address=0x%llX, Size=%llu, Time=%f, CallstackId=%u)"),
						AllocsInHeap.Num(), AllocatedSizeInHeap, Heap, HeapSpec.Name, Address, HeapSize, Time, CallstackId);
				}
#endif

				// Free automatically all allocs in this heap.
				for (const FAllocInHeap& AllocInHeap : AllocsInHeap)
				{
					INSIGHTS_WATCH_INDIRECT_API_LOGF(TEXT("Free"), ThreadId, Time, AllocInHeap.Address);
					EditFree(ThreadId, Time, CallstackId, AllocInHeap.Address, RootHeap.HeapSpec->Id);
				}
			}
		}

		const uint64 Size = Alloc->GetSize();
		const uint32 Alignment = Alloc->GetAlignment();
		const uint32 AllocCallstackId = Alloc->AllocCallstackId;

		// Re-add this allocation to the Live allocs.
		RootHeap.LiveAllocs->Add(Alloc); // the Live allocs takes ownership of Alloc

		// We cannot just unmark the allocation as heap, there is no timestamp support, instead fake a "free"
		// event and an "alloc" event. Make sure the new allocation retains the tag from the original.
		const uint8 Tracker = 1;
		TagTracker.PushTagFromPtr(ThreadId, Tracker, Alloc->Tag);
		INSIGHTS_WATCH_INDIRECT_API_LOGF(TEXT("Free"), ThreadId, Time, Address);
		EditFree(ThreadId, Time, CallstackId, Address, RootHeap.HeapSpec->Id);
		INSIGHTS_WATCH_INDIRECT_API_LOGF(TEXT("Alloc"), ThreadId, Time, Address);
		EditAlloc(ThreadId, Time, AllocCallstackId, Address, Size, Alignment, RootHeap.HeapSpec->Id);
		TagTracker.PopTagFromPtr(ThreadId, Tracker);
	}
	else
	{
		++HeapErrors;
		if (HeapErrors <= MaxLogMessagesPerErrorType)
		{
			UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] HeapUnmarkAlloc: Could not find heap with address 0x%llX (Heap=%u \"%s\", Time=%f, CallstackId=%u)!"), Address, Heap, HeapSpec.Name, Time, CallstackId);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::FRootHeap::UpdateHistogramByAllocSize(uint64 Size)
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

void FAllocationsProvider::FRootHeap::UpdateHistogramByEventDistance(uint32 EventDistance)
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

void FAllocationsProvider::EditPushTagFromPtr(uint32 ThreadId, uint8 Tracker, uint64 Ptr, HeapId RootHeapId)
{
	EditAccessCheck();

	if (!bInitialized)
	{
		// No errors if the "memallocs" channel was not enabled.
		TagTracker.PushTagFromPtr(ThreadId, Tracker, 0);
		return;
	}

	const FAllocationItem* Alloc = nullptr;

	if (IsValidRootHeap(RootHeapId))
	{
		const FRootHeap& RootHeap = GetRootHeapUnchecked(RootHeapId);
		Alloc = RootHeap.LiveAllocs->FindRef(Ptr);
	}
	else
	{
		++MiscErrors;
		if (MiscErrors <= MaxLogMessagesPerErrorType)
		{
			UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] Invalid root heap (%u) for MemoryScopePtr or ReallocFree event!"), RootHeapId);
		}
	}

	const TagIdType Tag = Alloc ? Alloc->Tag : 0; // If ptr is not found use "Untagged"
	TagTracker.PushTagFromPtr(ThreadId, Tracker, Tag);

	INSIGHTS_FILTER_EVENT(ThreadId, 0.0, Ptr, RootHeapId, 0);

	if (!Alloc)
	{
		++MiscErrors;
		if (MiscErrors <= MaxLogMessagesPerErrorType)
		{
			UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] Invalid address (0x%llX) for MemoryScopePtr or ReallocFree event!"), Ptr);
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
	EditAccessCheck();

	if (!bInitialized)
	{
		if (TagTracker.GetNumErrors() > 0)
		{
			UE_LOG(LogTraceServices, Error, TEXT("[MemAlloc] TagTracker errors: %u"), TagTracker.GetNumErrors());
		}
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

	for (uint32 RootHeapIdx = 0; RootHeapIdx < MaxRootHeaps; ++RootHeapIdx)
	{
		FRootHeap* RootHeap = RootHeaps[RootHeapIdx];
		if (RootHeap)
		{
			RootHeap->SbTree->Validate();
		}
	}

	//TODO: shrink live allocs buffers

	if (AllocWarnings > 0 || FreeWarnings > 0 || HeapWarnings > 0 || MiscWarnings > 0)
	{
		UE_LOG(LogTraceServices, Warning, TEXT("[MemAlloc] %u warnings (%u ALLOC + %u FREE + %u HEAP + %u other)"),
			AllocWarnings + FreeWarnings + HeapWarnings + MiscWarnings,
			AllocWarnings, FreeWarnings, HeapWarnings, MiscWarnings);
	}

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
	for (uint32 RootHeapIdx = 0; RootHeapIdx < MaxRootHeaps; ++RootHeapIdx)
	{
		FRootHeap* RootHeap = RootHeaps[RootHeapIdx];
		if (RootHeap)
		{
			TotalEventCount += RootHeap->EventIndex;
		}
	}
	uint32 NumHeapSpecs = 0;
	for (FHeapSpec* HeapSpec : HeapSpecs)
	{
		if (HeapSpec)
		{
			++NumHeapSpecs;
		}
	}
	UE_LOG(LogTraceServices, Log, TEXT("[MemAlloc] Analysis completed (%llu events, %llu allocs, %llu frees, %llu heaps, %d heap specs)."),
		TotalEventCount, AllocCount, FreeCount, HeapCount, NumHeapSpecs);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::EnumerateRootHeaps(TFunctionRef<void(HeapId Id, const FHeapSpec&)> Callback) const
{
	ReadAccessCheck();

	for (uint32 RootHeapIdx = 0; RootHeapIdx < MaxRootHeaps; ++RootHeapIdx)
	{
		FRootHeap* RootHeap = RootHeaps[RootHeapIdx];
		if (RootHeap)
		{
			check(RootHeap->HeapSpec != nullptr);
			const FHeapSpec& HeapSpec = *RootHeap->HeapSpec;
			check(HeapSpec.Parent == nullptr);
			if (HeapSpec.Name != nullptr)
			{
				Callback(HeapSpec.Id, HeapSpec);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::EnumerateHeaps(TFunctionRef<void(HeapId Id, const FHeapSpec&)> Callback) const
{
	ReadAccessCheck();

	const uint32 NumHeapSpecs = HeapSpecs.Num();
	for (uint32 HeapSpecIdx = 0; HeapSpecIdx < NumHeapSpecs; ++HeapSpecIdx)
	{
		const FHeapSpec* HeapSpecPtr = HeapSpecs[HeapSpecIdx];
		if (HeapSpecPtr)
		{
			const FHeapSpec& HeapSpec = *HeapSpecPtr;
			if (HeapSpec.Name != nullptr)
			{
				Callback(HeapSpec.Id, HeapSpec);
			}
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
	ReadAccessCheck();

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
	ReadAccessCheck();

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
	ReadAccessCheck();

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
	ReadAccessCheck();

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
	ReadAccessCheck();

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
	ReadAccessCheck();

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
	ReadAccessCheck();

	TagTracker.EnumerateTags(Callback);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FAllocationsProvider::DebugPrint() const
{
	ReadAccessCheck();

	for (uint32 RootHeapIdx = 0; RootHeapIdx < MaxRootHeaps; ++RootHeapIdx)
	{
		FRootHeap* RootHeap = RootHeaps[RootHeapIdx];
		if (RootHeap)
		{
			RootHeap->SbTree->DebugPrint();
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

	for (uint32 RootHeapIdx = 0; RootHeapIdx < MaxRootHeaps; ++RootHeapIdx)
	{
		FRootHeap* RootHeap = RootHeaps[RootHeapIdx];
		if (RootHeap)
		{
			RootHeap->LiveAllocs->Enumerate(Callback);
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
	static const FName Name("AllocationsProvider");
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
#undef INSIGHTS_LLA_RESERVE
#undef INSIGHTS_USE_SHORT_LIVING_ALLOCS
#undef INSIGHTS_SLA_USE_ADDRESS_MAP
#undef INSIGHTS_USE_LAST_ALLOC
#undef INSIGHTS_VALIDATE_ALLOC_EVENTS
#undef INSIGHTS_DOUBLE_ALLOC_FREE_PREVIOUS
#undef INSIGHTS_VALIDATE_FREE_EVENTS
#undef INSIGHTS_VALIDATE_HEAP_MARK_ALLOC_EVENTS
#undef INSIGHTS_VALIDATE_HEAP_UNMARK_ALLOC_EVENTS
#undef INSIGHTS_DEBUG_METADATA
#undef INSIGHTS_WARNINGS_FOR_NULLPTR_ALLOCS
#undef INSIGHTS_FILTER_EVENTS_ENABLED
#undef INSIGHTS_FILTER_EVENT
#undef INSIGHTS_DEBUG_WATCH
#undef INSIGHTS_DEBUG_WATCH_FOUND
#undef INSIGHTS_LOGF
#undef INSIGHTS_API_LOGF
#undef INSIGHTS_INDIRECT_API_LOGF
#undef INSIGHTS_WATCH_API_LOGF
#undef INSIGHTS_WATCH_INDIRECT_API_LOGF
