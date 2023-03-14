// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetadataProvider.h"

#include "Common/Utils.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace TraceServices
{

////////////////////////////////////////////////////////////////////////////////////////////////////
// FMetadataProviderLock
////////////////////////////////////////////////////////////////////////////////////////////////////

thread_local FMetadataProviderLock* GThreadCurrentMetadataProviderLock;
thread_local int32 GThreadCurrentReadMetadataProviderLockCount;
thread_local int32 GThreadCurrentWriteMetadataProviderLockCount;

void FMetadataProviderLock::ReadAccessCheck() const
{
	checkf(GThreadCurrentMetadataProviderLock == this && (GThreadCurrentReadMetadataProviderLockCount > 0 || GThreadCurrentWriteMetadataProviderLockCount > 0),
		TEXT("Trying to READ from metadata provider outside of a READ scope"));
}

void FMetadataProviderLock::WriteAccessCheck() const
{
	checkf(GThreadCurrentMetadataProviderLock == this && GThreadCurrentWriteMetadataProviderLockCount > 0,
		TEXT("Trying to WRITE to metadata provider outside of an EDIT/WRITE scope"));
}

void FMetadataProviderLock::BeginRead()
{
	check(!GThreadCurrentMetadataProviderLock || GThreadCurrentMetadataProviderLock == this);
	checkf(GThreadCurrentWriteMetadataProviderLockCount == 0, TEXT("Trying to lock metadata provider for READ while holding EDIT/WRITE access"));
	if (GThreadCurrentReadMetadataProviderLockCount++ == 0)
	{
		GThreadCurrentMetadataProviderLock = this;
		RWLock.ReadLock();
	}
}

void FMetadataProviderLock::EndRead()
{
	check(GThreadCurrentReadMetadataProviderLockCount > 0);
	if (--GThreadCurrentReadMetadataProviderLockCount == 0)
	{
		RWLock.ReadUnlock();
		GThreadCurrentMetadataProviderLock = nullptr;
	}
}

void FMetadataProviderLock::BeginWrite()
{
	check(!GThreadCurrentMetadataProviderLock || GThreadCurrentMetadataProviderLock == this);
	checkf(GThreadCurrentReadMetadataProviderLockCount == 0, TEXT("Trying to lock metadata provider for EDIT/WRITE while holding READ access"));
	if (GThreadCurrentWriteMetadataProviderLockCount++ == 0)
	{
		GThreadCurrentMetadataProviderLock = this;
		RWLock.WriteLock();
	}
}

void FMetadataProviderLock::EndWrite()
{
	check(GThreadCurrentWriteMetadataProviderLockCount > 0);
	if (--GThreadCurrentWriteMetadataProviderLockCount == 0)
	{
		RWLock.WriteUnlock();
		GThreadCurrentMetadataProviderLock = nullptr;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// FMetadataProvider
////////////////////////////////////////////////////////////////////////////////////////////////////

FMetadataProvider::FMetadataProvider(IAnalysisSession& InSession)
	: Session(InSession)
	, RegisteredTypes(Session.GetLinearAllocator(), 16)
	, MetadataStore(Session.GetLinearAllocator(), 1024)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FMetadataProvider::~FMetadataProvider()
{
	for (const auto& KV : Threads)
	{
		for (const FMetadataStackEntry& StackEntry : KV.Value->CurrentStack)
		{
			if (StackEntry.StoreIndex == InvalidMetadataStoreIndex &&
				StackEntry.StoreEntry.Size > MaxInlinedMetadataSize)
			{
				FMemory::Free(StackEntry.StoreEntry.Ptr);
				check(AllocationCount > 0);
				--AllocationCount;
				check(TotalAllocatedMemory >= StackEntry.StoreEntry.Size);
				TotalAllocatedMemory -= StackEntry.StoreEntry.Size;
			}
		}
		delete KV.Value;
	}

	if (MetadataStore.Num() > 0)
	{
		for (const FMetadataStoreEntry& StoreEntry : MetadataStore)
		{
			if (StoreEntry.Size > MaxInlinedMetadataSize)
			{
				FMemory::Free(StoreEntry.Ptr);
				check(AllocationCount > 0);
				--AllocationCount;
				check(TotalAllocatedMemory >= StoreEntry.Size);
				TotalAllocatedMemory -= StoreEntry.Size;
			}
		}
	}

	check(AllocationCount == 0);
	check(TotalAllocatedMemory == 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

uint16 FMetadataProvider::RegisterMetadataType(const TCHAR* Name, const FMetadataSchema& Schema)
{
	Lock.WriteAccessCheck();

	check(RegisteredTypes.Num() <= MaxMetadataTypeId);
	const uint16 Type = static_cast<uint16>(RegisteredTypes.Num());
	RegisteredTypes.EmplaceBack(Schema);
	RegisteredTypesMap.Add(Name, Type);

	return Type;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

uint16 FMetadataProvider::GetRegisteredMetadataType(FName Name) const
{
	Lock.ReadAccessCheck();

	const uint16* TypePtr = RegisteredTypesMap.Find(Name);
	return TypePtr ? *TypePtr : InvalidMetadataType;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FName FMetadataProvider::GetRegisteredMetadataName(uint16 Type) const
{
	Lock.ReadAccessCheck();

	const FName* Name = RegisteredTypesMap.FindKey(Type);
	return Name ? *Name : FName();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const FMetadataSchema* FMetadataProvider::GetRegisteredMetadataSchema(uint16 Type) const
{
	Lock.ReadAccessCheck();

	const int32 Index = (int32)Type;
	return Index < RegisteredTypes.Num() ? &RegisteredTypes[Index] : nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FMetadataProvider::FMetadataThread::FMetadataThread(uint32 InThreadId, ILinearAllocator& InAllocator)
	: ThreadId(InThreadId)
	, Metadata(InAllocator, 1024)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FMetadataProvider::FMetadataThread& FMetadataProvider::GetOrAddThread(uint32 ThreadId)
{
	FMetadataThread** MetadataThreadPtr = Threads.Find(ThreadId);
	if (MetadataThreadPtr)
	{
		return **MetadataThreadPtr;
	}
	FMetadataThread* MetadataThread = new FMetadataThread(ThreadId, Session.GetLinearAllocator());
	Threads.Add(ThreadId, MetadataThread);
	return *MetadataThread;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FMetadataProvider::FMetadataThread* FMetadataProvider::GetThread(uint32 ThreadId)
{
	FMetadataThread** MetadataThreadPtr = Threads.Find(ThreadId);
	return (MetadataThreadPtr) ? *MetadataThreadPtr : nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const FMetadataProvider::FMetadataThread* FMetadataProvider::GetThread(uint32 ThreadId) const
{
	FMetadataThread* const* MetadataThreadPtr = Threads.Find(ThreadId);
	return (MetadataThreadPtr) ? *MetadataThreadPtr : nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMetadataProvider::PushScopedMetadata(uint32 ThreadId, uint16 Type, void* Data, uint32 Size)
{
	Lock.WriteAccessCheck();

	check(Type < RegisteredTypes.Num());
	check(Data != nullptr && Size > 0);

	if (Size > MaxMetadataSize)
	{
		UE_LOG(LogTraceServices, Error, TEXT("[Meta] Cannot push metadata (thread %u, type %u, size %u). Data size is too large."), ThreadId, Type, Size);
		return;
	}

	FMetadataThread& MetadataThread = GetOrAddThread(ThreadId);

	if (MetadataThread.CurrentStack.Num() == MaxMetadataStackSize)
	{
		UE_LOG(LogTraceServices, Error, TEXT("[Meta] Cannot push metadata (thread %u, type %u). Stack size is too large."), ThreadId, Type);
		return;
	}

	FMetadataStackEntry StackEntry;
	if (Size > MaxInlinedMetadataSize)
	{
		StackEntry.StoreEntry.Ptr = FMemory::Malloc(Size);
		++AllocationEventCount;
		++AllocationCount;
		TotalAllocatedMemory += Size;
		FMemory::Memcpy(StackEntry.StoreEntry.Ptr, Data, Size);
	}
	else
	{
		FMemory::Memcpy(StackEntry.StoreEntry.Value, Data, Size);
	}
	StackEntry.StoreEntry.Size = static_cast<uint16>(Size);
	StackEntry.StoreEntry.Type = Type;
	StackEntry.StoreIndex = InvalidMetadataStoreIndex;
	StackEntry.PinnedId = InvalidMetadataId;

	MetadataThread.CurrentStack.Push(StackEntry);
	++EventCount;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMetadataProvider::PopScopedMetadata(uint32 ThreadId, uint16 Type)
{
	Lock.WriteAccessCheck();

	FMetadataThread& MetadataThread = GetOrAddThread(ThreadId);

	if (MetadataThread.CurrentStack.Num() == 0)
	{
		UE_LOG(LogTraceServices, Error, TEXT("[Meta] Cannot pop metadata (thread %u, type %u). Stack is empty."), ThreadId, uint32(Type));
		return;
	}

	const FMetadataStackEntry& Top = MetadataThread.CurrentStack.Top();

	if (Top.StoreEntry.Type != Type)
	{
		UE_LOG(LogTraceServices, Error, TEXT("[Meta] Cannot pop metadata (thread %u, type %u). Type mismatch."), ThreadId, uint32(Type));
		return;
	}

	if (Top.StoreIndex == InvalidMetadataStoreIndex &&
		Top.StoreEntry.Size > MaxInlinedMetadataSize)
	{
		FMemory::Free(Top.StoreEntry.Ptr);
		check(AllocationCount > 0);
		--AllocationCount;
		check(TotalAllocatedMemory >= Top.StoreEntry.Size);
		TotalAllocatedMemory -= Top.StoreEntry.Size;
	}
	MetadataThread.CurrentStack.Pop();
	++EventCount;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

uint32 FMetadataProvider::PinAndGetId(uint32 ThreadId)
{
	Lock.WriteAccessCheck();

	FMetadataThread* MetadataThread = GetThread(ThreadId);

	if (!MetadataThread || MetadataThread->CurrentStack.Num() == 0)
	{
		return InvalidMetadataId;
	}

	TArray<FMetadataStackEntry>& CurrentStack = MetadataThread->CurrentStack;
	TPagedArray<FMetadataEntry>& Metadata = MetadataThread->Metadata;

	const uint32 LastPinnedId = CurrentStack.Top().PinnedId;
	if (LastPinnedId != InvalidMetadataId)
	{
		return LastPinnedId;
	}

	const int32 StackSize = CurrentStack.Num();

	// Store all entries of the current stack.
	for (int32 StackIndex = StackSize - 1; StackIndex >= 0; --StackIndex)
	{
		FMetadataStackEntry& StackEntry = CurrentStack[StackIndex];
		if (StackEntry.PinnedId != InvalidMetadataId)
		{
			break;
		}
		if (StackEntry.StoreIndex == InvalidMetadataStoreIndex)
		{
			StackEntry.StoreIndex = static_cast<uint32>(MetadataStore.Num());
			MetadataStore.EmplaceBack(StackEntry.StoreEntry);
		}
	}

	int32 FirstUnpinnedStackIndex = StackSize - 1;
	for (int32 StackIndex = StackSize - 2; StackIndex >= 0; --StackIndex)
	{
		const FMetadataStackEntry& StackEntry = CurrentStack[StackIndex];
		if (StackEntry.PinnedId != InvalidMetadataId)
		{
			break;
		}
		FirstUnpinnedStackIndex = StackIndex;
	}
	if (FirstUnpinnedStackIndex > 0)
	{
		const FMetadataStackEntry& LastPinnedStackEntry = CurrentStack[FirstUnpinnedStackIndex - 1];
		if (LastPinnedStackEntry.PinnedId == Metadata.Num() - 1)
		{
			// Reuse the partial pinned metadata stack.
			// Only pin additional metadata entries.
			for (int32 StackIndex = FirstUnpinnedStackIndex; StackIndex < StackSize; ++StackIndex)
			{
				FMetadataStackEntry& StackEntry = CurrentStack[StackIndex];
				StackEntry.PinnedId = static_cast<uint32>(Metadata.Num());
				FMetadataEntry& Entry = Metadata.PushBack();
				Entry.StoreIndex = StackEntry.StoreIndex;
				Entry.Type = StackEntry.StoreEntry.Type;
				Entry.StackSize = static_cast<uint16>(StackIndex + 1);
			}

			return CurrentStack.Top().PinnedId;
		}
	}

	// Pin all metadata entries in the current stack.
	for (int32 StackIndex = StackSize - 1; StackIndex >= 0; --StackIndex)
	{
		FMetadataStackEntry& StackEntry = CurrentStack[StackIndex];
		StackEntry.PinnedId = static_cast<uint32>(Metadata.Num());
		FMetadataEntry& Entry = Metadata.PushBack();
		Entry.StoreIndex = StackEntry.StoreIndex;
		Entry.Type = StackEntry.StoreEntry.Type;
		Entry.StackSize = static_cast<uint16>(StackIndex + 1);
	}

	return CurrentStack.Top().PinnedId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

uint32 FMetadataProvider::GetMetadataStackSize(uint32 InThreadId, uint32 InMetadataId) const
{
	Lock.ReadAccessCheck();

	const FMetadataThread* MetadataThread = GetThread(InThreadId);

	if (!MetadataThread || InMetadataId >= MetadataThread->Metadata.Num())
	{
		UE_LOG(LogTraceServices, Warning, TEXT("[Meta] Invalid metadata id (thread %u, id %u)."), InThreadId, InMetadataId);
		return 0;
	}

	auto Iterator = MetadataThread->Metadata.GetIteratorFromItem(InMetadataId);
	const FMetadataEntry* Entry = Iterator.GetCurrentItem();
	return Entry->StackSize;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FMetadataProvider::GetMetadata(uint32 InThreadId, uint32 InMetadataId, uint32 InStackDepth, uint16& OutType, const void*& OutData, uint32& OutSize) const
{
	Lock.ReadAccessCheck();

	const FMetadataThread* MetadataThread = GetThread(InThreadId);

	if (!MetadataThread || InMetadataId >= MetadataThread->Metadata.Num())
	{
		UE_LOG(LogTraceServices, Warning, TEXT("[Meta] Invalid metadata id (thread %u, id %u)."), InThreadId, InMetadataId);
		OutType = InvalidMetadataType;
		OutData = nullptr;
		OutSize = 0;
		return false;
	}

	auto Iterator = MetadataThread->Metadata.GetIteratorFromItem(InMetadataId);
	const FMetadataEntry* Entry = Iterator.GetCurrentItem();
	if (InStackDepth >= Entry->StackSize)
	{
		UE_LOG(LogTraceServices, Warning, TEXT("[Meta] Invalid metadata stack detph (thread %u, id %u, depth %u)."), InThreadId, InMetadataId, InStackDepth);
		OutType = InvalidMetadataType;
		OutData = nullptr;
		OutSize = 0;
		return false;
	}
	while (Entry->StackSize != InStackDepth + 1)
	{
		Entry = Iterator.PrevItem();
		check(Entry != nullptr);
	}

	check(Entry->StoreIndex < MetadataStore.Num());
	const FMetadataStoreEntry& StoreEntry = *MetadataStore.GetIteratorFromItem(Entry->StoreIndex);
	OutType = StoreEntry.Type;
	OutData = (StoreEntry.Size > MaxInlinedMetadataSize) ? StoreEntry.Ptr : StoreEntry.Value;
	OutSize = StoreEntry.Size;
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMetadataProvider::EnumerateMetadata(uint32 InThreadId, uint32 InMetadataId, TFunctionRef<bool(uint32 StackDepth, uint16 Type, const void* Data, uint32 Size)> Callback) const
{
	Lock.ReadAccessCheck();

	const FMetadataThread* MetadataThread = GetThread(InThreadId);

	if (!MetadataThread || InMetadataId >= MetadataThread->Metadata.Num())
	{
		UE_LOG(LogTraceServices, Warning, TEXT("[Meta] Invalid metadata id (thread %u, id %u)."), InThreadId, InMetadataId);
		return;
	}

	auto Iterator = MetadataThread->Metadata.GetIteratorFromItem(InMetadataId);
	const FMetadataEntry* Entry = Iterator.GetCurrentItem();
	while (true)
	{
		check(Entry->StoreIndex < MetadataStore.Num());
		const FMetadataStoreEntry& StoreEntry = *MetadataStore.GetIteratorFromItem(Entry->StoreIndex);
		const void* Data = (StoreEntry.Size > MaxInlinedMetadataSize) ? StoreEntry.Ptr : StoreEntry.Value;
		if (!Callback(Entry->StackSize - 1, StoreEntry.Type, Data, StoreEntry.Size))
		{
			break;
		}
		if (Entry->StackSize == 1) // last one
		{
			break;
		}
		Entry = Iterator.PrevItem();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FName GetMetadataProviderName()
{
	static FName Name(TEXT("MetadataProvider"));
	return Name;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const IMetadataProvider* ReadMetadataProvider(const IAnalysisSession& Session)
{
	return Session.ReadProvider<IMetadataProvider>(GetMetadataProviderName());
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace TraceServices
