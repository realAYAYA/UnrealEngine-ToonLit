// Copyright Epic Games, Inc. All Rights Reserved.

#include "RHICoreTransientResourceAllocator.h"
#include "HAL/IConsoleManager.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/LowLevelMemStats.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "RHICommandList.h"

static int32 GRHITransientAllocatorMinimumHeapSize = 128;
static FAutoConsoleVariableRef CVarRHITransientAllocatorMinimumHeapSize(
	TEXT("RHI.TransientAllocator.MinimumHeapSize"),
	GRHITransientAllocatorMinimumHeapSize,
	TEXT("Minimum size of an RHI transient heap in MB. Heaps will default to this size and grow to the maximum based on the first allocation (Default 128)."),
	ECVF_ReadOnly);

static int32 GRHITransientAllocatorBufferCacheSize = 64;
static FAutoConsoleVariableRef CVarRHITransientAllocatorBufferCacheSize(
	TEXT("RHI.TransientAllocator.BufferCacheSize"),
	GRHITransientAllocatorBufferCacheSize,
	TEXT("The maximum number of RHI buffers to cache on each heap before garbage collecting."),
	ECVF_ReadOnly);

static int32 GRHITransientAllocatorTextureCacheSize = 64;
static FAutoConsoleVariableRef CVarRHITransientAllocatorTextureCacheSize(
	TEXT("RHI.TransientAllocator.TextureCacheSize"),
	GRHITransientAllocatorTextureCacheSize,
	TEXT("The maximum number of RHI textures to cache on each heap before garbage collecting."),
	ECVF_ReadOnly);

static int32 GRHITransientAllocatorGarbageCollectLatency = 16;
static FAutoConsoleVariableRef CVarRHITransientAllocatorGarbageCollectLatency(
	TEXT("RHI.TransientAllocator.GarbageCollectLatency"),
	GRHITransientAllocatorGarbageCollectLatency,
	TEXT("Amount of update cycles before memory is reclaimed."),
	ECVF_ReadOnly);

TRACE_DECLARE_INT_COUNTER(TransientResourceCreateCount, TEXT("TransientAllocator/ResourceCreateCount"));

TRACE_DECLARE_INT_COUNTER(TransientTextureCreateCount, TEXT("TransientAllocator/TextureCreateCount"));
TRACE_DECLARE_INT_COUNTER(TransientTextureCount, TEXT("TransientAllocator/TextureCount"));
TRACE_DECLARE_INT_COUNTER(TransientTextureCacheSize, TEXT("TransientAllocator/TextureCacheSize"));
TRACE_DECLARE_FLOAT_COUNTER(TransientTextureCacheHitPercentage, TEXT("TransientAllocator/TextureCacheHitPercentage"));

TRACE_DECLARE_INT_COUNTER(TransientBufferCreateCount, TEXT("TransientAllocator/BufferCreateCount"));
TRACE_DECLARE_INT_COUNTER(TransientBufferCount, TEXT("TransientAllocator/BufferCount"));
TRACE_DECLARE_INT_COUNTER(TransientBufferCacheSize, TEXT("TransientAllocator/BufferCacheSize"));
TRACE_DECLARE_FLOAT_COUNTER(TransientBufferCacheHitPercentage, TEXT("TransientAllocator/BufferCacheHitPercentage"));

TRACE_DECLARE_INT_COUNTER(TransientPageMapCount, TEXT("TransientAllocator/PageMapCount"));
TRACE_DECLARE_INT_COUNTER(TransientPageAllocateCount, TEXT("TransientAllocator/PageAllocateCount"));
TRACE_DECLARE_INT_COUNTER(TransientPageSpanCount, TEXT("TransientAllocator/PageSpanCount"));

TRACE_DECLARE_INT_COUNTER(TransientMemoryRangeCount, TEXT("TransientAllocator/MemoryRangeCount"));

TRACE_DECLARE_MEMORY_COUNTER(TransientMemoryUsed, TEXT("TransientAllocator/MemoryUsed"));
TRACE_DECLARE_MEMORY_COUNTER(TransientMemoryRequested, TEXT("TransientAllocator/MemoryRequested"));

DECLARE_STATS_GROUP(TEXT("RHI: Transient Memory"), STATGROUP_RHITransientMemory, STATCAT_Advanced);

DECLARE_MEMORY_STAT(TEXT("Memory Used"), STAT_RHITransientMemoryUsed, STATGROUP_RHITransientMemory);
DECLARE_MEMORY_STAT(TEXT("Memory Aliased"), STAT_RHITransientMemoryAliased, STATGROUP_RHITransientMemory);
DECLARE_MEMORY_STAT(TEXT("Memory Requested"), STAT_RHITransientMemoryRequested, STATGROUP_RHITransientMemory);
DECLARE_MEMORY_STAT(TEXT("Buffer Memory Requested"), STAT_RHITransientBufferMemoryRequested, STATGROUP_RHITransientMemory);
DECLARE_MEMORY_STAT(TEXT("Texture Memory Requested"), STAT_RHITransientTextureMemoryRequested, STATGROUP_RHITransientMemory);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Resources"), STAT_RHITransientResources, STATGROUP_RHITransientMemory);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Textures"), STAT_RHITransientTextures, STATGROUP_RHITransientMemory);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Buffers"), STAT_RHITransientBuffers, STATGROUP_RHITransientMemory);

DECLARE_LLM_MEMORY_STAT(TEXT("RHI Transient Resources"), STAT_RHITransientResourcesLLM, STATGROUP_LLMFULL);
RHICORE_API LLM_DEFINE_TAG(RHITransientResources, NAME_None, NAME_None, GET_STATFNAME(STAT_RHITransientResourcesLLM), GET_STATFNAME(STAT_EngineSummaryLLM));

//////////////////////////////////////////////////////////////////////////

void FRHITransientMemoryStats::Submit(uint64 UsedSize)
{
	const int32 CreateResourceCount = Textures.CreateCount + Buffers.CreateCount;
	const int64 MemoryUsed = UsedSize;
	const int64 MemoryRequested = AliasedSize;
	const float ToMB = 1.0f / (1024.0f * 1024.0f);

	TRACE_COUNTER_SET(TransientResourceCreateCount, CreateResourceCount);
	TRACE_COUNTER_SET(TransientTextureCreateCount, Textures.CreateCount);
	TRACE_COUNTER_SET(TransientBufferCreateCount, Buffers.CreateCount);
	TRACE_COUNTER_SET(TransientMemoryUsed, MemoryUsed);
	TRACE_COUNTER_SET(TransientMemoryRequested, MemoryRequested);

	CSV_CUSTOM_STAT_GLOBAL(TransientResourceCreateCount, CreateResourceCount, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT_GLOBAL(TransientMemoryUsedMB, static_cast<float>(MemoryUsed * ToMB) , ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT_GLOBAL(TransientMemoryAliasedMB, static_cast<float>(MemoryRequested * ToMB), ECsvCustomStatOp::Set);

	SET_MEMORY_STAT(STAT_RHITransientMemoryUsed, UsedSize);
	SET_MEMORY_STAT(STAT_RHITransientMemoryAliased, AliasedSize);
	SET_MEMORY_STAT(STAT_RHITransientMemoryRequested, Textures.AllocatedSize + Buffers.AllocatedSize);
	SET_MEMORY_STAT(STAT_RHITransientBufferMemoryRequested, Buffers.AllocatedSize);
	SET_MEMORY_STAT(STAT_RHITransientTextureMemoryRequested, Textures.AllocatedSize);

	SET_DWORD_STAT(STAT_RHITransientTextures, Textures.AllocationCount);
	SET_DWORD_STAT(STAT_RHITransientBuffers, Buffers.AllocationCount);
	SET_DWORD_STAT(STAT_RHITransientResources, Textures.AllocationCount + Buffers.AllocationCount);

	Reset();
}

//////////////////////////////////////////////////////////////////////////

void FRHITransientResourceOverlapTracker::Track(FRHITransientResource* TransientResource, uint32 PageOffsetMin, uint32 PageOffsetMax)
{
	check(TransientResource);

	FResourceRange ResourceRangeNew;
	ResourceRangeNew.ResourceIndex = Resources.Emplace(TransientResource);
	ResourceRangeNew.PageOffsetMin = PageOffsetMin;
	ResourceRangeNew.PageOffsetMax = PageOffsetMax;

	int32 InsertIndex = Algo::LowerBound(ResourceRanges, ResourceRangeNew, [](const FResourceRange& Lhs, const FResourceRange& Rhs)
	{
		return Lhs.PageOffsetMax <= Rhs.PageOffsetMin;
	});

	for (int32 Index = InsertIndex; Index < ResourceRanges.Num(); ++Index)
	{
		FResourceRange& ResourceRangeOld = ResourceRanges[Index];

		// If the old range starts later in the heap and doesn't overlap, the sort invariant guarantees no future range will overlap.
		if (ResourceRangeOld.PageOffsetMin >= ResourceRangeNew.PageOffsetMax)
		{
			break;
		}

		TransientResource->AddAliasingOverlap(Resources[ResourceRangeOld.ResourceIndex]);

		// Complete overlap.
		if (ResourceRangeOld.PageOffsetMin >= ResourceRangeNew.PageOffsetMin && ResourceRangeOld.PageOffsetMax <= ResourceRangeNew.PageOffsetMax)
		{
			ResourceRanges.RemoveAt(Index, 1, EAllowShrinking::No);
			Index--;
		}
		// Partial overlap, can manifest as three cases:
		else
		{
			// 1) New:    ********
			//            |||        ->
			//    Old: ======             ===********
			if (ResourceRangeOld.PageOffsetMin < ResourceRangeNew.PageOffsetMin)
			{
				FResourceRange ResourceRangeOldCopy = ResourceRangeOld;
				ResourceRangeOld.PageOffsetMax = ResourceRangeNew.PageOffsetMin;

				InsertIndex++;
				// 3) New:    ********
				//            ||||||||      ->
				//    Old: ==============        ===********===
				if (ResourceRangeOldCopy.PageOffsetMax > ResourceRangeNew.PageOffsetMax)
				{
					ResourceRangeOldCopy.PageOffsetMin = ResourceRangeNew.PageOffsetMax;

					// Lower bound has been resized already; add an upper bound.
					FResourceRange ResourceRangeOldUpper = ResourceRangeOldCopy;
					ResourceRanges.Insert(ResourceRangeOldUpper, Index + 1);

					break;
				}
			}
			else
			{
				// 2) New:    ********
				//                |||      ->
				//    Old:        ======         ********===
				ResourceRangeOld.PageOffsetMin = ResourceRangeNew.PageOffsetMax;

				break;
			}
		}
	}

	ResourceRanges.Insert(ResourceRangeNew, InsertIndex);
}

//////////////////////////////////////////////////////////////////////////

void FRHITransientResourceOverlapTracker::Reset()
{
	TArray<FResourceRange> AcquiredRanges;
	TArray<FRHITransientResource*> AcquiredResources;
	AcquiredRanges.Reserve(ResourceRanges.Num());
	AcquiredResources.Reserve(Resources.Num());

	for (FResourceRange Range : ResourceRanges)
	{
		FRHITransientResource* Resource = Resources[Range.ResourceIndex];

		if (Resource->IsAcquired())
		{
			Range.ResourceIndex = AcquiredResources.Num();
			AcquiredRanges.Emplace(Range);
			AcquiredResources.Emplace(Resource);
		}
	}

	Swap(ResourceRanges, AcquiredRanges);
	Swap(Resources, AcquiredResources);
}

//////////////////////////////////////////////////////////////////////////
// Transient Resource Heap Allocator
//////////////////////////////////////////////////////////////////////////

FRHITransientHeapAllocator::FRHITransientHeapAllocator(uint64 InCapacity, uint32 InAlignment)
	: Capacity(InCapacity)
	, AlignmentMin(InAlignment)
{
	HeadHandle = CreateRange();
	InsertRange(HeadHandle, 0, Capacity);
}

FRHITransientHeapAllocation FRHITransientHeapAllocator::Allocate(uint64 Size, uint32 Alignment)
{
	check(Size > 0);

	if (Alignment < AlignmentMin)
	{
		Alignment = AlignmentMin;
	}

	FFindResult FindResult = FindFreeRange(Size, Alignment);

	if (FindResult.FoundHandle == InvalidRangeHandle)
	{
		return {};
	}

	FRange& FoundRange = Ranges[FindResult.FoundHandle];

	const uint64 AlignedSize = FoundRange.Size - FindResult.LeftoverSize;
	const uint64 AlignmentPad = AlignedSize - Size;
	const uint64 AlignedOffset = FoundRange.Offset + AlignmentPad;
	const uint64 AllocationEnd = AlignedOffset + Size;

	// Adjust the range if there is space left over.
	if (FindResult.LeftoverSize)
	{
		FoundRange.Offset = AllocationEnd;
		FoundRange.Size = FindResult.LeftoverSize;
	}
	// Otherwise, remove it.
	else
	{
		RemoveRange(FindResult.PreviousHandle, FindResult.FoundHandle);
	}

	AllocationCount++;
	UsedSize += AlignedSize;
	AlignmentWaste += AlignmentPad;

	FRHITransientHeapAllocation Allocation;
	Allocation.Size = Size;
	Allocation.Offset = AlignedOffset;
	Allocation.AlignmentPad = AlignmentPad;

	Validate();

	return Allocation;
}

void FRHITransientHeapAllocator::Deallocate(FRHITransientHeapAllocation Allocation)
{
	check(Allocation.Size > 0 && Allocation.Size <= UsedSize);

	// Reconstruct the original range offset by subtracting the alignment pad, and expand the size accordingly.
	const uint64 RangeToFreeOffset = Allocation.Offset - Allocation.AlignmentPad;
	const uint64 RangeToFreeSize = Allocation.Size + Allocation.AlignmentPad;
	const uint64 RangeToFreeEnd = RangeToFreeOffset + RangeToFreeSize;

	FRangeHandle PreviousHandle = HeadHandle;
	FRangeHandle NextHandle = InvalidRangeHandle;
	FRangeHandle Handle = GetFirstFreeRangeHandle();

	while (Handle != InvalidRangeHandle)
	{
		const FRange& Range = Ranges[Handle];

		// Find the first free range after the one being freed.
		if (RangeToFreeOffset < Range.Offset)
		{
			NextHandle = Handle;
			break;
		}

		PreviousHandle = Handle;
		Handle = Range.NextFreeHandle;
	}

	uint64 MergedFreeRangeStart = RangeToFreeOffset;
	uint64 MergedFreeRangeEnd = RangeToFreeEnd;
	bool bMergedPrevious = false;
	bool bMergedNext = false;

	if (PreviousHandle != HeadHandle)
	{
		FRange& PreviousRange = Ranges[PreviousHandle];

		// Attempt to merge the previous range with the range being freed.
		if (PreviousRange.GetEnd() == RangeToFreeOffset)
		{
			PreviousRange.Size += RangeToFreeSize;
			MergedFreeRangeStart = PreviousRange.Offset;
			MergedFreeRangeEnd = PreviousRange.GetEnd();
			bMergedPrevious = true;
		}
	}

	if (NextHandle != InvalidRangeHandle)
	{
		FRange& NextRange = Ranges[NextHandle];

		// Attempt to merge the next range with the range being freed.
		if (RangeToFreeEnd == NextRange.Offset)
		{
			NextRange.Size += RangeToFreeSize;
			NextRange.Offset = RangeToFreeOffset;
			MergedFreeRangeStart = FMath::Min(MergedFreeRangeStart, RangeToFreeOffset);
			MergedFreeRangeEnd = NextRange.GetEnd();
			bMergedNext = true;
		}
	}

	// With both previous and next ranges merged with the freed range, they now overlap. Remove next and expand previous to cover all three.
	if (bMergedPrevious && bMergedNext)
	{
		FRange& PreviousRange = Ranges[PreviousHandle];
		FRange& NextRange = Ranges[NextHandle];

		PreviousRange.Size = MergedFreeRangeEnd - MergedFreeRangeStart;
		RemoveRange(PreviousHandle, NextHandle);
	}
	// If neither previous or next were merged, insert a new range between them.
	else if (!bMergedPrevious && !bMergedNext)
	{
		InsertRange(PreviousHandle, RangeToFreeOffset, RangeToFreeSize);
	}

	UsedSize -= RangeToFreeSize;
	AlignmentWaste -= Allocation.AlignmentPad;
	AllocationCount--;

	Validate();
}

FRHITransientHeapAllocator::FFindResult FRHITransientHeapAllocator::FindFreeRange(uint64 Size, uint32 Alignment)
{
	FFindResult FindResult;
	FindResult.PreviousHandle = HeadHandle;

	FRangeHandle Handle = GetFirstFreeRangeHandle();
	while (Handle != InvalidRangeHandle)
	{
		FRange& Range = Ranges[Handle];

		// Due to alignment we may have to shift the offset and expand the size accordingly.
		const uint64 AlignmentPad = Align(GpuVirtualAddress + Range.Offset, Alignment) - GpuVirtualAddress - Range.Offset;
		const uint64 RequiredSize = Size + AlignmentPad;

		if (RequiredSize <= Range.Size)
		{
			FindResult.FoundHandle = Handle;
			FindResult.LeftoverSize = Range.Size - RequiredSize;
			return FindResult;
		}

		FindResult.PreviousHandle = Handle;
		Handle = Range.NextFreeHandle;
	}

	return {};
}

void FRHITransientHeapAllocator::Validate()
{
#if UE_BUILD_DEBUG
	uint64 DerivedFreeSize = 0;

	FRangeHandle PreviousHandle = HeadHandle;
	FRangeHandle NextHandle = InvalidRangeHandle;
	FRangeHandle Handle = GetFirstFreeRangeHandle();

	while (Handle != InvalidRangeHandle)
	{
		const FRange& Range = Ranges[Handle];
		DerivedFreeSize += Range.Size;

		if (PreviousHandle != HeadHandle)
		{
			const FRange& PreviousRange = Ranges[PreviousHandle];

			// Checks that the ranges are sorted.
			check(PreviousRange.Offset + PreviousRange.Size < Range.Offset);
		}

		PreviousHandle = Handle;
		Handle = Range.NextFreeHandle;
	}

	check(Capacity == DerivedFreeSize + UsedSize);
#endif
}

//////////////////////////////////////////////////////////////////////////

FRHITransientTexture* FRHITransientHeap::CreateTexture(
	const FRHITextureCreateInfo& CreateInfo,
	const TCHAR* DebugName,
	uint32 PassIndex,
	uint64 CurrentAllocatorCycle,
	uint64 TextureSize,
	uint32 TextureAlignment,
	FCreateTextureFunction CreateTextureFunction)
{
	FRHITransientHeapAllocation Allocation = Allocator.Allocate(TextureSize, TextureAlignment);
	Allocation.Heap = this;

	if (!Allocation.IsValid())
	{
		return nullptr;
	}

	FRHITransientTexture* Texture = Textures.Acquire(ComputeHash(CreateInfo, Allocation.Offset), [&](uint64 Hash)
	{
		Stats.Textures.CreateCount++;
		return CreateTextureFunction(FResourceInitializer(Allocation, Hash));
	});

	check(Texture);
	Texture->Acquire(FRHICommandListImmediate::Get(), DebugName, PassIndex, CurrentAllocatorCycle);
	AllocateMemoryInternal(Texture, DebugName, PassIndex, CurrentAllocatorCycle, Allocation);
	Stats.AllocateTexture(Allocation.Size);
	return Texture;
}

void FRHITransientHeap::DeallocateMemory(FRHITransientTexture* Texture, uint32 PassIndex)
{
	DeallocateMemoryInternal(Texture, PassIndex);
	Stats.DeallocateTexture(Texture->GetSize());
}

FRHITransientBuffer* FRHITransientHeap::CreateBuffer(
	const FRHIBufferCreateInfo& CreateInfo,
	const TCHAR* DebugName,
	uint32 PassIndex,
	uint64 CurrentAllocatorCycle,
	uint64 BufferSize,
	uint32 BufferAlignment,
	FCreateBufferFunction CreateBufferFunction)
{
	FRHITransientHeapAllocation Allocation = Allocator.Allocate(BufferSize, BufferAlignment);
	Allocation.Heap = this;

	if (!Allocation.IsValid())
	{
		return nullptr;
	}

	FRHITransientBuffer* Buffer = Buffers.Acquire(ComputeHash(CreateInfo, Allocation.Offset), [&](uint64 Hash)
	{
		Stats.Buffers.CreateCount++;
		return CreateBufferFunction(FResourceInitializer(Allocation, Hash));
	});

	check(Buffer);
	Buffer->Acquire(FRHICommandListImmediate::Get(), DebugName, PassIndex, CurrentAllocatorCycle);
	AllocateMemoryInternal(Buffer, DebugName, PassIndex, CurrentAllocatorCycle, Allocation);
	Stats.AllocateBuffer(Allocation.Size);
	return Buffer;
}

void FRHITransientHeap::DeallocateMemory(FRHITransientBuffer* Buffer, uint32 PassIndex)
{
	DeallocateMemoryInternal(Buffer, PassIndex);
	Stats.DeallocateBuffer(Buffer->GetSize());
}

void FRHITransientHeap::AllocateMemoryInternal(FRHITransientResource* Resource, const TCHAR* Name, uint32 PassIndex, uint64 CurrentAllocatorCycle, const FRHITransientHeapAllocation& Allocation)
{
	Resource->GetHeapAllocation() = Allocation;

	check(Allocation.Offset % Initializer.Alignment == 0);
		const uint64 AlignedSize   = Align(Allocation.Size, Initializer.Alignment);
		const uint32 PageOffsetMin = Allocation.Offset >> AlignmentLog2;
		const uint32 PageOffsetMax = (Allocation.Offset + AlignedSize) >> AlignmentLog2;
		OverlapTracker.Track(Resource, PageOffsetMin, PageOffsetMax);

	CommitSize = FMath::Max(CommitSize, Allocation.Offset + Allocation.Size);
}

void FRHITransientHeap::DeallocateMemoryInternal(FRHITransientResource* Resource, uint32 PassIndex)
{
	Resource->Discard(PassIndex);

	const FRHITransientHeapAllocation Allocation = Resource->GetHeapAllocation();
	check(Allocation.Heap == this);

	Allocator.Deallocate(Allocation);
}

void FRHITransientHeap::Flush(uint64 AllocatorCycle, FRHITransientMemoryStats& OutMemoryStats, FRHITransientAllocationStats* OutAllocationStats)
{
	const bool bHasDeallocations = Stats.HasDeallocations();
	OutMemoryStats.Accumulate(Stats);
	Stats.Reset();

	if (OutAllocationStats)
	{
		const auto AddResourceToStats = [](FRHITransientAllocationStats& AllocationStats, FRHITransientResource* Resource)
		{
			const FRHITransientHeapAllocation& HeapAllocation = Resource->GetHeapAllocation();

			FRHITransientAllocationStats::FAllocation Allocation;
			Allocation.OffsetMin = HeapAllocation.Offset;
			Allocation.OffsetMax = HeapAllocation.Offset + HeapAllocation.Size;
			Allocation.MemoryRangeIndex = AllocationStats.MemoryRanges.Num();

			AllocationStats.Resources.Emplace(Resource, FRHITransientAllocationStats::FAllocationArray{ Allocation });
		};

		OutAllocationStats->Resources.Reserve(OutAllocationStats->Resources.Num() + Textures.GetAllocatedCount() + Buffers.GetAllocatedCount());

		for (FRHITransientTexture* Texture : Textures.GetAllocated())
		{
			AddResourceToStats(*OutAllocationStats, Texture);
		}

		for (FRHITransientBuffer* Buffer : Buffers.GetAllocated())
		{
			AddResourceToStats(*OutAllocationStats, Buffer);
		}

		FRHITransientAllocationStats::FMemoryRange MemoryRange;
		MemoryRange.Capacity = GetCapacity();
		MemoryRange.CommitSize = GetCommitSize();
		OutAllocationStats->MemoryRanges.Add(MemoryRange);
	}

	if (bHasDeallocations)
	{
		CommitSize = 0;

		{
			Textures.Forfeit(GFrameCounterRenderThread);

			for (FRHITransientTexture* Texture : Textures.GetAllocated())
			{
				const FRHITransientHeapAllocation& Allocation = Texture->GetHeapAllocation();
				CommitSize = FMath::Max(CommitSize, Allocation.Offset + Allocation.Size);
			}
		}

		{
			Buffers.Forfeit(GFrameCounterRenderThread);

			for (FRHITransientBuffer* Buffer : Buffers.GetAllocated())
			{
				const FRHITransientHeapAllocation& Allocation = Buffer->GetHeapAllocation();
				CommitSize = FMath::Max(CommitSize, Allocation.Offset + Allocation.Size);
			}
		}

			OverlapTracker.Reset();
		}
}

//////////////////////////////////////////////////////////////////////////

FRHITransientHeapCache::FInitializer FRHITransientHeapCache::FInitializer::CreateDefault()
{
	FInitializer Initializer;
	Initializer.MinimumHeapSize = GRHITransientAllocatorMinimumHeapSize * 1024 * 1024;
	Initializer.HeapAlignment = 64 * 1024;
	Initializer.BufferCacheSize = GRHITransientAllocatorBufferCacheSize;
	Initializer.TextureCacheSize = GRHITransientAllocatorTextureCacheSize;
	Initializer.GarbageCollectLatency = GRHITransientAllocatorGarbageCollectLatency;
	return Initializer;
}

FRHITransientHeapCache::~FRHITransientHeapCache()
{
	for (FRHITransientHeap* Heap : LiveList)
	{
		delete Heap;
	}
	LiveList.Empty();
	FreeList.Empty();
}

FRHITransientHeap* FRHITransientHeapCache::Acquire(uint64 FirstAllocationSize, ERHITransientHeapFlags FirstAllocationHeapFlags)
{
	FScopeLock Lock(&CriticalSection);

	for (int32 HeapIndex = FreeList.Num() - 1; HeapIndex >= 0; --HeapIndex)
	{
		FRHITransientHeap* Heap = FreeList[HeapIndex];

		if (Heap->IsAllocationSupported(FirstAllocationSize, FirstAllocationHeapFlags))
		{
			FreeList.RemoveAt(HeapIndex);
			return Heap;
		}
	}

	FRHITransientHeap::FInitializer HeapInitializer;
	HeapInitializer.Size = GetHeapSize(FirstAllocationSize);
	HeapInitializer.Alignment = Initializer.HeapAlignment;
	HeapInitializer.Flags = (Initializer.bSupportsAllHeapFlags ? ERHITransientHeapFlags::AllowAll : FirstAllocationHeapFlags);
	HeapInitializer.TextureCacheSize = Initializer.TextureCacheSize;
	HeapInitializer.BufferCacheSize = Initializer.BufferCacheSize;

	LLM_SCOPE_BYTAG(RHITransientResources);
	FRHITransientHeap* Heap = CreateHeap(HeapInitializer);
	check(Heap);

	TotalMemoryCapacity += HeapInitializer.Size;
	LiveList.Emplace(Heap);
	return Heap;
}

void FRHITransientHeapCache::Forfeit(TConstArrayView<FRHITransientHeap*> InForfeitedHeaps)
{
	FScopeLock Lock(&CriticalSection);

	LiveList.Reserve(InForfeitedHeaps.Num());
	for (int32 HeapIndex = InForfeitedHeaps.Num() - 1; HeapIndex >= 0; --HeapIndex)
	{
		FRHITransientHeap* Heap = InForfeitedHeaps[HeapIndex];
		check(Heap->IsEmpty());
		Heap->LastUsedGarbageCollectCycle = GarbageCollectCycle;
		FreeList.Add(Heap);
	}
}

void FRHITransientHeapCache::GarbageCollect()
{
	FScopeLock Lock(&CriticalSection);

	for (int32 HeapIndex = 0; HeapIndex < FreeList.Num(); ++HeapIndex)
	{
		FRHITransientHeap* Heap = FreeList[HeapIndex];

		if (Heap->GetLastUsedGarbageCollectCycle() + Initializer.GarbageCollectLatency <= GarbageCollectCycle)
		{
			TotalMemoryCapacity -= Heap->GetCapacity();
			FreeList.RemoveAt(HeapIndex);
			LiveList.Remove(Heap);
			HeapIndex--;

			delete Heap;
		}
	}

	TRACE_COUNTER_SET(TransientMemoryRangeCount, LiveList.Num());

	Stats.Submit(TotalMemoryCapacity);

	GarbageCollectCycle++;
}

//////////////////////////////////////////////////////////////////////////

FRHITransientTexture* FRHITransientResourceHeapAllocator::CreateTextureInternal(
	const FRHITextureCreateInfo& CreateInfo,
	const TCHAR* DebugName,
	uint32 PassIndex,
	uint64 TextureSize,
	uint32 TextureAlignment,
	FRHITransientHeap::FCreateTextureFunction CreateTextureFunction)
{
	const ERHITransientHeapFlags TextureHeapFlags =
		EnumHasAnyFlags(CreateInfo.Flags, TexCreate_RenderTargetable | TexCreate_ResolveTargetable | TexCreate_DepthStencilTargetable | TexCreate_DepthStencilResolveTarget)
		? ERHITransientHeapFlags::AllowRenderTargets
		: ERHITransientHeapFlags::AllowTextures;

	FRHITransientTexture* Texture = nullptr;

	for (FRHITransientHeap* Heap : Heaps)
	{
		if (!Heap->IsAllocationSupported(TextureSize, TextureHeapFlags))
		{
			continue;
		}

		Texture = Heap->CreateTexture(CreateInfo, DebugName, PassIndex, CurrentCycle, TextureSize, TextureAlignment, CreateTextureFunction);

		if (Texture)
		{
			break;
		}
	}

	if (!Texture)
	{
		FRHITransientHeap* Heap = HeapCache.Acquire(TextureSize, TextureHeapFlags);
		Heaps.Emplace(Heap);

		Texture = Heap->CreateTexture(CreateInfo, DebugName, PassIndex, CurrentCycle, TextureSize, TextureAlignment, CreateTextureFunction);
	}

	check(Texture);
	IF_RHICORE_TRANSIENT_ALLOCATOR_DEBUG(ActiveResources.Emplace(Texture));
	return Texture;
}

FRHITransientBuffer* FRHITransientResourceHeapAllocator::CreateBufferInternal(
	const FRHIBufferCreateInfo& CreateInfo,
	const TCHAR* DebugName,
	uint32 PassIndex,
	uint32 BufferSize,
	uint32 BufferAlignment,
	FRHITransientHeap::FCreateBufferFunction CreateBufferFunction)
{
	FRHITransientBuffer* Buffer = nullptr;

	for (FRHITransientHeap* Heap : Heaps)
	{
		if (!Heap->IsAllocationSupported(BufferSize, ERHITransientHeapFlags::AllowBuffers))
		{
			continue;
		}

		Buffer = Heap->CreateBuffer(CreateInfo, DebugName, PassIndex, CurrentCycle, BufferSize, BufferAlignment, CreateBufferFunction);

		if (Buffer)
		{
			break;
		}
	}

	if (!Buffer)
	{
		FRHITransientHeap* Heap = HeapCache.Acquire(BufferSize, ERHITransientHeapFlags::AllowBuffers);
		Heaps.Emplace(Heap);

		Buffer = Heap->CreateBuffer(CreateInfo, DebugName, PassIndex, CurrentCycle, BufferSize, BufferAlignment, CreateBufferFunction);
	}

	check(Buffer);
	IF_RHICORE_TRANSIENT_ALLOCATOR_DEBUG(ActiveResources.Emplace(Buffer));
	return Buffer;
}

void FRHITransientResourceHeapAllocator::DeallocateMemory(FRHITransientTexture* Texture, uint32 PassIndex)
{
	check(Texture);

	FRHITransientHeap* Heap = Texture->GetHeapAllocation().Heap;

	check(Heap);
	check(Heaps.Contains(Heap));

	Heap->DeallocateMemory(Texture, PassIndex);
	DeallocationCount++;

	IF_RHICORE_TRANSIENT_ALLOCATOR_DEBUG(ActiveResources.Remove(Texture));
}

void FRHITransientResourceHeapAllocator::DeallocateMemory(FRHITransientBuffer* Buffer, uint32 PassIndex)
{
	check(Buffer);

	FRHITransientHeap* Heap = Buffer->GetHeapAllocation().Heap;

	check(Heap);
	check(Heaps.Contains(Heap));

	Heap->DeallocateMemory(Buffer, PassIndex);
	DeallocationCount++;

	IF_RHICORE_TRANSIENT_ALLOCATOR_DEBUG(ActiveResources.Remove(Buffer));
}

void FRHITransientResourceHeapAllocator::Flush(FRHICommandListImmediate& RHICmdList, FRHITransientAllocationStats* OutAllocationStats)
{
	FRHITransientMemoryStats Stats;

	uint32 NumBuffers = 0;
	uint32 NumTextures = 0;

	for (FRHITransientHeap* Heap : Heaps)
	{
		Heap->Flush(CurrentCycle, Stats, OutAllocationStats);

		NumBuffers += Heap->Buffers.GetSize();
		NumTextures += Heap->Textures.GetSize();
	}

	TRACE_COUNTER_SET(TransientBufferCacheSize, NumBuffers);
	TRACE_COUNTER_SET(TransientTextureCacheSize, NumTextures);

	if (DeallocationCount > 0)
	{
		// This could be done more efficiently, but the number of heaps is small and the goal is to keep the list stable
		// so that heaps are acquired in the same order each frame, because the resource caches are tied to heaps.
		TArray<FRHITransientHeap*, FConcurrentLinearArrayAllocator> EmptyHeaps;
		TArray<FRHITransientHeap*, FConcurrentLinearArrayAllocator> ActiveHeaps;
		EmptyHeaps.Reserve(Heaps.Num());
		ActiveHeaps.Reserve(Heaps.Num());

		for (FRHITransientHeap* Heap : Heaps)
		{
			if (Heap->IsEmpty())
			{
				EmptyHeaps.Emplace(Heap);
			}
			else
			{
				ActiveHeaps.Emplace(Heap);
			}
		}

		HeapCache.Forfeit(EmptyHeaps);
		Heaps = ActiveHeaps;
		DeallocationCount = 0;
	}

	RHICmdList.EnqueueLambda([&HeapCache = HeapCache, Stats](FRHICommandListImmediate&)
	{
		HeapCache.Stats.Accumulate(Stats);
	});

	CurrentCycle++;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Transient Resource Page Allocator
///////////////////////////////////////////////////////////////////////////////////////////////////

void FRHITransientPageSpanAllocator::Init()
{
	check(MaxSpanCount == MaxPageCount + 2);

	PageToSpanStart.AddDefaulted(MaxPageCount + 1);
	PageToSpanEnd.AddDefaulted(MaxPageCount + 1);

	PageSpans.AddDefaulted(MaxSpanCount);
	UnusedSpanList.AddDefaulted(MaxSpanCount);

	Reset();
}

void FRHITransientPageSpanAllocator::Reset()
{
	FreePageCount = MaxPageCount;
	AllocationCount = 0;

	// Initialize the unused span index pool with MaxSpanCount entries
	for (uint32 Index = 0; Index < MaxSpanCount; Index++)
	{
		UnusedSpanList[Index] = MaxSpanCount - 1 - Index;
	}
	UnusedSpanListCount = MaxSpanCount;

	// Allocate the head and tail spans (dummy spans), and a span between them covering the entire range
	uint32 HeadSpanIndex = AllocSpan();
	uint32 TailSpanIndex = AllocSpan();
	check(HeadSpanIndex == FreeSpanListHeadIndex);
	check(TailSpanIndex == FreeSpanListTailIndex);

	if (MaxPageCount > 0)
	{
		uint32 FirstFreeNodeIndex = AllocSpan();

		// Allocate head and tail nodes (0 and 1)
		for (uint32 Index = 0; Index < 2; Index++)
		{
			PageSpans[Index].Offset = 0;
			PageSpans[Index].Count = 0;
			PageSpans[Index].PrevSpanIndex = InvalidIndex;
			PageSpans[Index].NextSpanIndex = InvalidIndex;
			PageSpans[Index].bAllocated = false;
		}
		PageSpans[HeadSpanIndex].NextSpanIndex = FirstFreeNodeIndex;
		PageSpans[TailSpanIndex].PrevSpanIndex = FirstFreeNodeIndex;

		// First Node
		PageSpans[FirstFreeNodeIndex].Offset = 0;
		PageSpans[FirstFreeNodeIndex].Count = MaxPageCount;
		PageSpans[FirstFreeNodeIndex].PrevSpanIndex = HeadSpanIndex;
		PageSpans[FirstFreeNodeIndex].NextSpanIndex = TailSpanIndex;
		PageSpans[FirstFreeNodeIndex].bAllocated = false;

		// Initialize the page->span mapping
		for (uint32 Index = 0; Index < MaxPageCount + 1; Index++)
		{
			PageToSpanStart[Index] = InvalidIndex;
			PageToSpanEnd[Index] = InvalidIndex;
		}
		PageToSpanStart[0] = FirstFreeNodeIndex;
		PageToSpanEnd[MaxPageCount] = FirstFreeNodeIndex;
	}
	else
	{
		PageSpans[HeadSpanIndex].NextSpanIndex = TailSpanIndex;
		PageSpans[TailSpanIndex].PrevSpanIndex = HeadSpanIndex;
	}
}

bool FRHITransientPageSpanAllocator::Allocate(uint32 PageCount, uint32& OutNumPagesAllocated, uint32& OutSpanIndex)
{
	OutNumPagesAllocated = 0;

	if (FreePageCount < PageCount)
	{
		// If we're allowing partial allocs and we run out of pages, allocate all the remaining pages
		PageCount = FreePageCount;
	}

	if (PageCount > FreePageCount || PageCount == 0)
	{
		return false;
	}
	OutNumPagesAllocated = PageCount;

	// Allocate spans from the free list head
	uint32 NumPagesToFind = PageCount;
	uint32 FoundPages = 0;
	FPageSpan& HeadSpan = PageSpans[FreeSpanListHeadIndex];
	uint32 StartSpanIndex = HeadSpan.NextSpanIndex;
	uint32 SpanIndex = StartSpanIndex;
	while (SpanIndex != FreeSpanListTailIndex && SpanIndex != InvalidIndex)
	{
		FPageSpan& Span = PageSpans[SpanIndex];
		if (NumPagesToFind <= Span.Count)
		{
			// Span is too big, so split it
			if (Span.Count > NumPagesToFind)
			{
				SplitSpan(SpanIndex, NumPagesToFind);
			}
			check(NumPagesToFind == Span.Count);

			// Move the head to point to the next free span
			if (HeadSpan.NextSpanIndex != InvalidIndex)
			{
				PageSpans[HeadSpan.NextSpanIndex].PrevSpanIndex = InvalidIndex;
			}
			HeadSpan.NextSpanIndex = Span.NextSpanIndex;
			if (Span.NextSpanIndex != InvalidIndex)
			{
				PageSpans[Span.NextSpanIndex].PrevSpanIndex = FreeSpanListHeadIndex;
			}
			Span.NextSpanIndex = InvalidIndex;
		}
		Span.bAllocated = true;
		NumPagesToFind -= Span.Count;
		SpanIndex = Span.NextSpanIndex;
	}
	check(NumPagesToFind == 0);
	FreePageCount -= PageCount;
#if UE_BUILD_DEBUG
	Validate();
#endif
	AllocationCount++;
	OutSpanIndex = StartSpanIndex;
	return true;
}

void FRHITransientPageSpanAllocator::Deallocate(uint32 SpanIndex)
{
	if (SpanIndex == InvalidIndex)
	{
		return;
	}
	check(AllocationCount > 0);
	// Find the right span with which to merge this
	while (SpanIndex != InvalidIndex)
	{
		FPageSpan& FreedSpan = PageSpans[SpanIndex];
		check(FreedSpan.bAllocated);
		FreePageCount += FreedSpan.Count;
		uint32 NextSpanIndex = FreedSpan.NextSpanIndex;
		FreedSpan.bAllocated = false;
		if (!MergeFreeSpanIfPossible(SpanIndex))
		{
			// If we can't merge this span, just unlink and add it to the head (or tail)
			Unlink(SpanIndex);

			InsertAfter(FreeSpanListHeadIndex, SpanIndex);
		}
		SpanIndex = NextSpanIndex;
	}
	AllocationCount--;

#if UE_BUILD_DEBUG
	Validate();
#endif
}

void FRHITransientPageSpanAllocator::SplitSpan(uint32 InSpanIndex, uint32 InPageCount)
{
	FPageSpan& Span = PageSpans[InSpanIndex];
	check(InPageCount <= Span.Count);
	if (InPageCount < Span.Count)
	{
		uint32 NewSpanIndex = AllocSpan();
		FPageSpan& NewSpan = PageSpans[NewSpanIndex];
		NewSpan.NextSpanIndex = Span.NextSpanIndex;
		NewSpan.PrevSpanIndex = InSpanIndex;
		NewSpan.Count = Span.Count - InPageCount;
		NewSpan.Offset = Span.Offset + InPageCount;
		NewSpan.bAllocated = Span.bAllocated;
		Span.Count = InPageCount;
		Span.NextSpanIndex = NewSpanIndex;
		if (NewSpan.NextSpanIndex != InvalidIndex)
		{
			PageSpans[NewSpan.NextSpanIndex].PrevSpanIndex = NewSpanIndex;
		}

		// Update the PageToSpan mappings
		PageToSpanEnd[NewSpan.Offset] = InSpanIndex;
		PageToSpanStart[NewSpan.Offset] = NewSpanIndex;
		PageToSpanEnd[NewSpan.Offset + NewSpan.Count] = NewSpanIndex;
	}
}

void FRHITransientPageSpanAllocator::MergeSpans(uint32 SpanIndex0, uint32 SpanIndex1, const bool bKeepSpan1)
{
	FPageSpan& Span0 = PageSpans[SpanIndex0];
	FPageSpan& Span1 = PageSpans[SpanIndex1];
	check(Span0.Offset + Span0.Count == Span1.Offset);
	check(Span0.bAllocated == Span1.bAllocated);
	check(Span0.NextSpanIndex == SpanIndex1);
	check(Span1.PrevSpanIndex == SpanIndex0);

	uint32 SpanIndexToKeep = bKeepSpan1 ? SpanIndex1 : SpanIndex0;
	uint32 SpanIndexToRemove = bKeepSpan1 ? SpanIndex0 : SpanIndex1;

	// Update the PageToSpan mappings
	PageToSpanStart[Span0.Offset] = SpanIndexToKeep;
	PageToSpanStart[Span1.Offset] = InvalidIndex;
	PageToSpanEnd[Span0.Offset + Span0.Count] = InvalidIndex; // Should match Span1.Offset
	PageToSpanEnd[Span1.Offset + Span1.Count] = SpanIndexToKeep;
	if (bKeepSpan1)
	{
		Span1.Offset = Span0.Offset;
		Span1.Count += Span0.Count;
	}
	else
	{
		Span0.Count += Span1.Count;
	}

	Unlink(SpanIndexToRemove);
	ReleaseSpan(SpanIndexToRemove);
}

// Inserts a span after an existing span. The span to insert must be unlinked
void FRHITransientPageSpanAllocator::InsertAfter(uint32 InsertPosition, uint32 InsertSpanIndex)
{
	check(InsertPosition != InvalidIndex);
	check(InsertSpanIndex != InvalidIndex);
	FPageSpan& SpanAtPos = PageSpans[InsertPosition];
	FPageSpan& SpanToInsert = PageSpans[InsertSpanIndex];
	check(!SpanToInsert.IsLinked());

	// Connect Span0's next node with the inserted node
	SpanToInsert.NextSpanIndex = SpanAtPos.NextSpanIndex;
	if (SpanAtPos.NextSpanIndex != InvalidIndex)
	{
		PageSpans[SpanAtPos.NextSpanIndex].PrevSpanIndex = InsertSpanIndex;
	}
	// Connect the two nodes
	SpanAtPos.NextSpanIndex = InsertSpanIndex;
	SpanToInsert.PrevSpanIndex = InsertPosition;
}

// Inserts a span after an existing span. The span to insert must be unlinked
void FRHITransientPageSpanAllocator::InsertBefore(uint32 InsertPosition, uint32 InsertSpanIndex)
{
	check(InsertPosition != InvalidIndex && InsertPosition != 0); // Can't insert before the head
	check(InsertSpanIndex != InvalidIndex);
	FPageSpan& SpanAtPos = PageSpans[InsertPosition];
	FPageSpan& SpanToInsert = PageSpans[InsertSpanIndex];
	check(!SpanToInsert.IsLinked());

	// Connect Span0's prev node with the inserted node
	SpanToInsert.PrevSpanIndex = SpanAtPos.PrevSpanIndex;
	if (SpanAtPos.PrevSpanIndex != InvalidIndex)
	{
		PageSpans[SpanAtPos.PrevSpanIndex].NextSpanIndex = InsertSpanIndex;
	}

	// Connect the two nodes
	SpanAtPos.PrevSpanIndex = InsertSpanIndex;
	SpanToInsert.NextSpanIndex = InsertPosition;
}

void FRHITransientPageSpanAllocator::Unlink(uint32 SpanIndex)
{
	FPageSpan& Span = PageSpans[SpanIndex];
	check(SpanIndex != FreeSpanListHeadIndex);
	if (Span.PrevSpanIndex != InvalidIndex)
	{
		PageSpans[Span.PrevSpanIndex].NextSpanIndex = Span.NextSpanIndex;
	}
	if (Span.NextSpanIndex != InvalidIndex)
	{
		PageSpans[Span.NextSpanIndex].PrevSpanIndex = Span.PrevSpanIndex;
	}
	Span.PrevSpanIndex = InvalidIndex;
	Span.NextSpanIndex = InvalidIndex;
}

uint32 FRHITransientPageSpanAllocator::GetAllocationPageCount(uint32 SpanIndex) const
{
	check(SpanIndex != InvalidIndex && SpanIndex < MaxSpanCount);
	check(PageSpans[SpanIndex].bAllocated);

	uint32 Count = 0;
	do
	{
		Count += PageSpans[SpanIndex].Count;
		SpanIndex = PageSpans[SpanIndex].NextSpanIndex;

	} while (SpanIndex != InvalidIndex);

	return Count;
}

bool FRHITransientPageSpanAllocator::MergeFreeSpanIfPossible(uint32 SpanIndex)
{
	FPageSpan& Span = PageSpans[SpanIndex];
	check(!Span.bAllocated);
	bool bMerged = false;

	// Can we merge this span with an existing one to the left?
	uint32 AdjSpanIndexPrev = PageToSpanEnd[Span.Offset];
	if (AdjSpanIndexPrev != InvalidIndex && !PageSpans[AdjSpanIndexPrev].bAllocated)
	{
		Unlink(SpanIndex);
		InsertAfter(AdjSpanIndexPrev, SpanIndex);
		MergeSpans(AdjSpanIndexPrev, SpanIndex, true);
		bMerged = true;
	}

	// Can we merge this span with an existing free one to the right?
	uint32 AdjSpanIndexNext = PageToSpanStart[Span.Offset + Span.Count];
	if (AdjSpanIndexNext != InvalidIndex && !PageSpans[AdjSpanIndexNext].bAllocated)
	{
		Unlink(SpanIndex);
		InsertBefore(AdjSpanIndexNext, SpanIndex);
		MergeSpans(SpanIndex, AdjSpanIndexNext, false);
		bMerged = true;
	}

	return bMerged;
}

void FRHITransientPageSpanAllocator::Validate()
{
#if UE_BUILD_DEBUG
	// Check the mappings are valid
	for (uint32 Index = 0; Index < MaxPageCount; Index++)
	{
		check(PageToSpanStart[Index] == InvalidIndex || PageSpans[PageToSpanStart[Index]].Offset == Index);
		check(PageToSpanEnd[Index] == InvalidIndex || PageSpans[PageToSpanEnd[Index]].Offset + PageSpans[PageToSpanEnd[Index]].Count == Index);
	}

	// Count free pages
	uint32 FreeCount = 0;
	uint32 PrevIndex = FreeSpanListHeadIndex;
	for (uint32 Index = GetFirstSpanIndex(); PageSpans.IsValidIndex(Index); Index = PageSpans[Index].NextSpanIndex)
	{
		FPageSpan& Span = PageSpans[Index];
		check(Span.PrevSpanIndex == PrevIndex);
		PrevIndex = Index;
		FreeCount += Span.Count;
	}
	check(FreeCount <= MaxPageCount);
	check(FreeCount == FreePageCount);
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void FRHITransientPagePool::Allocate(FAllocationContext& Context)
{
	uint32 SpanIndex = 0;
	uint32 PagesAllocated = 0;

	uint32 PagesRemaining = Context.MaxAllocationPage > 0 ? FMath::Min(Context.PagesRemaining, Context.MaxAllocationPage) : Context.PagesRemaining;
	if (Allocator.Allocate(PagesRemaining, PagesAllocated, SpanIndex))
	{
		const uint64 DestinationGpuVirtualAddress = Context.GpuVirtualAddress + Context.PagesAllocated * Initializer.PageSize;
		const uint32 PageSpanOffsetMin = PageSpans.Num();

		Allocator.GetSpanArray(SpanIndex, PageSpans);

		const uint32 PageSpanOffsetMax = PageSpans.Num();
		const uint32 PageSpanCount     = PageSpanOffsetMax - PageSpanOffsetMin;

		const  int32 AllocationIndex   = Context.Allocations.Num();
		const uint64 AllocationHash    = CityHash64WithSeed((const char*)&PageSpans[PageSpanOffsetMin], PageSpanCount * sizeof(FRHITransientPageSpan), DestinationGpuVirtualAddress);

			for (uint32 Index = PageSpanOffsetMin; Index < PageSpanOffsetMax; ++Index)
			{
				const FRHITransientPageSpan Span = PageSpans[Index];
				OverlapTracker.Track(&Context.Resource, Span.Offset, Span.Offset + Span.Count);
			}

		FRHITransientPagePoolAllocation Allocation;
		Allocation.Pool = this;
		Allocation.Hash = AllocationHash;
		Allocation.SpanOffsetMin = Context.Spans.Num();
		Allocation.SpanOffsetMax = Context.Spans.Num() + PageSpanCount;
		Allocation.SpanIndex = SpanIndex;
		Context.Allocations.Emplace(Allocation);
		Context.Spans.Append(&PageSpans[PageSpanOffsetMin], PageSpanCount);

		bool bMapPages = true;

		if (AllocationIndex < Context.AllocationsBefore.Num())
		{
			const FRHITransientPagePoolAllocation& AllocationBefore = Context.AllocationsBefore[AllocationIndex];

			if (AllocationBefore.Hash == AllocationHash && AllocationBefore.Pool == this)
			{
				Context.AllocationMatchingCount++;
				bMapPages = false;
			}
		}

		if (bMapPages)
		{
			PageMapRequests.Emplace(DestinationGpuVirtualAddress, GpuVirtualAddress, Initializer.PageCount, PageSpanOffsetMin, PageSpanCount);
			Context.PagesMapped += PagesAllocated;
		}

		check(Context.PagesRemaining >= PagesAllocated);
		Context.PagesRemaining -= PagesAllocated;
		Context.PagesAllocated += PagesAllocated;
		Context.PageSpansAllocated += PageSpanCount;
		Context.AllocationCount++;
	}
}

void FRHITransientPagePool::Flush(FRHICommandListImmediate& RHICmdList)
{
	if (!PageMapRequests.IsEmpty())
	{
		PageMapRequestCountMax = FMath::Max<uint32>(PageMapRequests.Num(), PageMapRequestCountMax);
		PageSpanCountMax       = FMath::Max<uint32>(PageSpans.Num(),       PageSpanCountMax);

		Flush(RHICmdList, MoveTemp(PageMapRequests), MoveTemp(PageSpans));

		PageMapRequests.Reserve(PageMapRequestCountMax);
		PageSpans.Reserve(PageSpanCountMax);
	}

		OverlapTracker.Reset();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

FRHITransientPagePoolCache::FInitializer FRHITransientPagePoolCache::FInitializer::CreateDefault()
{
	FInitializer Initializer;
	Initializer.BufferCacheSize = GRHITransientAllocatorBufferCacheSize;
	Initializer.TextureCacheSize = GRHITransientAllocatorTextureCacheSize;
	Initializer.GarbageCollectLatency = GRHITransientAllocatorGarbageCollectLatency;
	return Initializer;
}

FRHITransientPagePoolCache::~FRHITransientPagePoolCache()
{
	delete FastPagePool;
	FastPagePool = nullptr;

	for (FRHITransientPagePool* PagePool : LiveList)
	{
		delete PagePool;
	}
	LiveList.Empty();
	FreeList.Empty();
}

FRHITransientPagePool* FRHITransientPagePoolCache::Acquire()
{
	FScopeLock Lock(&CriticalSection);

	if (!FreeList.IsEmpty())
	{
		return FreeList.Pop();
	}

	LLM_SCOPE_BYTAG(RHITransientResources);

	FRHITransientPagePool::FInitializer PagePoolInitializer;
	PagePoolInitializer.PageSize = Initializer.PageSize;

	if (LiveList.IsEmpty() && Initializer.PoolSizeFirst > Initializer.PoolSize)
	{
		PagePoolInitializer.PageCount = Initializer.PoolSizeFirst / Initializer.PageSize;
	}
	else
	{
		PagePoolInitializer.PageCount = Initializer.PoolSize / Initializer.PageSize;
	}

	FRHITransientPagePool* PagePool = CreatePagePool(PagePoolInitializer);
	check(PagePool);

	TotalMemoryCapacity += PagePool->GetCapacity();
	LiveList.Emplace(PagePool);

	return PagePool;
}

FRHITransientPagePool* FRHITransientPagePoolCache::GetFastPagePool()
{
	if (!FastPagePool)
	{
		LLM_SCOPE_BYTAG(RHITransientResources);
		FastPagePool = CreateFastPagePool();

		if (FastPagePool)
		{
			TotalMemoryCapacity += FastPagePool->GetCapacity();
			return FastPagePool;
		}
	}

	return nullptr;
}

void FRHITransientPagePoolCache::Forfeit(TConstArrayView<FRHITransientPagePool*> InForfeitedPagePools)
{
	FScopeLock Lock(&CriticalSection);

	// These are iterated in reverse so they are acquired in the same order. 
	for (int32 Index = InForfeitedPagePools.Num() - 1; Index >= 0; --Index)
	{
		FRHITransientPagePool* PagePool = InForfeitedPagePools[Index];
		check(PagePool->IsEmpty());
		PagePool->LastUsedGarbageCollectCycle = GarbageCollectCycle;
		FreeList.Add(PagePool);
	}
}

void FRHITransientPagePoolCache::GarbageCollect()
{
	SCOPED_NAMED_EVENT_TEXT("TransientPagePoolCache::GarbageCollect", FColor::Magenta);
	TArray<FRHITransientPagePool*, TInlineAllocator<16>> PoolsToDelete;

	{
		FScopeLock Lock(&CriticalSection);

		for (int32 PagePoolIndex = 0; PagePoolIndex < FreeList.Num(); ++PagePoolIndex)
		{
			FRHITransientPagePool* PagePool = FreeList[PagePoolIndex];

			if (PagePool->GetLastUsedGarbageCollectCycle() + Initializer.GarbageCollectLatency <= GarbageCollectCycle)
			{
				TotalMemoryCapacity -= PagePool->GetCapacity();
				FreeList.RemoveAt(PagePoolIndex);
				LiveList.Remove(PagePool);
				PagePoolIndex--;

				PoolsToDelete.Emplace(PagePool);

			#if 1 // Only delete one per frame. Deletion can be quite expensive.
				break;
			#endif
			}
		}

		TRACE_COUNTER_SET(TransientMemoryRangeCount, LiveList.Num());
	}

	Stats.Submit(TotalMemoryCapacity);

	GarbageCollectCycle++;

	for (FRHITransientPagePool* PagePool : PoolsToDelete)
	{
		delete PagePool;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

FRHITransientTexture* FRHITransientResourcePageAllocator::CreateTexture(
	const FRHITextureCreateInfo& CreateInfo,
	const TCHAR* DebugName,
	uint32 PassIndex)
{
	FRHITransientTexture* Texture = Textures.Acquire(ComputeHash(CreateInfo), [&](uint64 Hash)
	{
		Stats.Textures.CreateCount++;
		return CreateTextureInternal(CreateInfo, DebugName, Hash);
	});

	const bool bFastPool = EnumHasAnyFlags(CreateInfo.Flags, ETextureCreateFlags::FastVRAM) || EnumHasAnyFlags(CreateInfo.Flags, ETextureCreateFlags::FastVRAMPartialAlloc);
	const float FastPoolPercentageRequested = bFastPool ? CreateInfo.FastVRAMPercentage / 255.f : 0.f;

	check(Texture);
	Texture->Acquire(FRHICommandListImmediate::Get(), DebugName, PassIndex, CurrentCycle);
	AllocateMemoryInternal(Texture, DebugName, PassIndex, bFastPool, FastPoolPercentageRequested);
	Stats.AllocateTexture(Texture->GetSize());
	IF_RHICORE_TRANSIENT_ALLOCATOR_DEBUG(ActiveResources.Emplace(Texture));
	return Texture;
}

FRHITransientBuffer* FRHITransientResourcePageAllocator::CreateBuffer(
	const FRHIBufferCreateInfo& CreateInfo,
	const TCHAR* DebugName,
	uint32 PassIndex)
{
	FRHITransientBuffer* Buffer = Buffers.Acquire(ComputeHash(CreateInfo), [&](uint64 Hash)
	{
		Stats.Buffers.CreateCount++;
		return CreateBufferInternal(CreateInfo, DebugName, Hash);
	});

	check(Buffer);
	Buffer->Acquire(FRHICommandListImmediate::Get(), DebugName, PassIndex, CurrentCycle);
	AllocateMemoryInternal(Buffer, DebugName, PassIndex, EnumHasAnyFlags(CreateInfo.Usage, EBufferUsageFlags::FastVRAM), false);
	Stats.AllocateBuffer(Buffer->GetSize());
	IF_RHICORE_TRANSIENT_ALLOCATOR_DEBUG(ActiveResources.Emplace(Buffer));
	return Buffer;
}

void FRHITransientResourcePageAllocator::AllocateMemoryInternal(FRHITransientResource* Resource, const TCHAR* DebugName, uint32 PassIndex, bool bFastPoolRequested, float FastPoolPercentageRequested)
{
	FRHITransientPagePool::FAllocationContext AllocationContext(*Resource, PageSize);

	if (bFastPoolRequested && FastPagePool)
	{
		// If a partial allocation is requested, compute the max. number of page which should be allocated in fast memory
		AllocationContext.MaxAllocationPage = FastPoolPercentageRequested > 0 ? FMath::CeilToInt(AllocationContext.PagesRemaining * FastPoolPercentageRequested) : AllocationContext.MaxAllocationPage;
		FastPagePool->Allocate(AllocationContext);
		AllocationContext.MaxAllocationPage = 0;
	}

	if (!AllocationContext.IsComplete())
	{
		for (FRHITransientPagePool* PagePool : PagePools)
		{
			PagePool->Allocate(AllocationContext);

			if (AllocationContext.IsComplete())
			{
				break;
			}
		}
	}

	while (!AllocationContext.IsComplete())
	{
		FRHITransientPagePool* PagePool = PagePoolCache.Acquire();
		PagePool->Allocate(AllocationContext);
		PagePools.Emplace(PagePool);
	}

	PageMapCount      += AllocationContext.PagesMapped;
	PageAllocateCount += AllocationContext.PagesAllocated;
	PageSpanCount     += AllocationContext.PageSpansAllocated;
}

void FRHITransientResourcePageAllocator::DeallocateMemoryInternal(FRHITransientResource* Resource, uint32 PassIndex)
{
	Resource->Discard(PassIndex);

	for (const FRHITransientPagePoolAllocation& Allocation : Resource->GetPageAllocation().PoolAllocations)
	{
		Allocation.Pool->Deallocate(Allocation.SpanIndex);
	}
}

void FRHITransientResourcePageAllocator::DeallocateMemory(FRHITransientTexture* Texture, uint32 PassIndex)
{
	DeallocateMemoryInternal(Texture, PassIndex);
	Stats.DeallocateTexture(Texture->GetSize());
}

void FRHITransientResourcePageAllocator::DeallocateMemory(FRHITransientBuffer* Buffer, uint32 PassIndex)
{
	DeallocateMemoryInternal(Buffer, PassIndex);
	Stats.DeallocateBuffer(Buffer->GetSize());
}

void FRHITransientResourcePageAllocator::Flush(FRHICommandListImmediate& RHICmdList, FRHITransientAllocationStats* OutAllocationStats)
{
	if (OutAllocationStats)
	{
		TMap<FRHITransientPagePool*, int32> PagePoolToMemoryRangeIndex;
		PagePoolToMemoryRangeIndex.Reserve(PagePools.Num() + !!FastPagePool);

		const auto AddMemoryRange = [&](FRHITransientPagePool* PagePool, FRHITransientAllocationStats::EMemoryRangeFlags Flags)
		{
			PagePoolToMemoryRangeIndex.Emplace(PagePool, OutAllocationStats->MemoryRanges.Num());

			FRHITransientAllocationStats::FMemoryRange MemoryRange;
			MemoryRange.Capacity = PagePool->GetCapacity();
			MemoryRange.CommitSize = PagePool->GetCapacity();
			MemoryRange.Flags = Flags;
			OutAllocationStats->MemoryRanges.Emplace(MemoryRange);
		};

		if (FastPagePool)
		{
			AddMemoryRange(FastPagePool, FRHITransientAllocationStats::EMemoryRangeFlags::FastVRAM);
		}

		for (FRHITransientPagePool* PagePool : PagePools)
		{
			AddMemoryRange(PagePool, FRHITransientAllocationStats::EMemoryRangeFlags::None);
		}

		for (const FRHITransientTexture* Texture : Textures.GetAllocated())
		{
			FRHITransientAllocationStats::FAllocationArray& Allocations = OutAllocationStats->Resources.Emplace(Texture);

			EnumeratePageSpans(Texture, [&](FRHITransientPagePool* PagePool, FRHITransientPageSpan PageSpan)
			{
				FRHITransientAllocationStats::FAllocation Allocation;
				Allocation.OffsetMin = PageSize *  PageSpan.Offset;
				Allocation.OffsetMax = PageSize * (PageSpan.Offset + PageSpan.Count);
				Allocation.MemoryRangeIndex = PagePoolToMemoryRangeIndex[PagePool];
				Allocations.Emplace(Allocation);
			});
		}

		for (const FRHITransientBuffer* Buffer : Buffers.GetAllocated())
		{
			FRHITransientAllocationStats::FAllocationArray& Allocations = OutAllocationStats->Resources.Emplace(Buffer);

			EnumeratePageSpans(Buffer, [&](FRHITransientPagePool* PagePool, FRHITransientPageSpan PageSpan)
			{
				FRHITransientAllocationStats::FAllocation Allocation;
				Allocation.OffsetMin = PageSize *  PageSpan.Offset;
				Allocation.OffsetMax = PageSize * (PageSpan.Offset + PageSpan.Count);
				Allocation.MemoryRangeIndex = PagePoolToMemoryRangeIndex[PagePool];
				Allocations.Emplace(Allocation);
			});
		}
	}

	{
		TRACE_COUNTER_SET(TransientPageMapCount, PageMapCount);
		TRACE_COUNTER_SET(TransientPageAllocateCount, PageAllocateCount);
		TRACE_COUNTER_SET(TransientPageSpanCount, PageSpanCount);
		PageMapCount = 0;
		PageAllocateCount = 0;
		PageSpanCount = 0;
	}

	{
		TRACE_COUNTER_SET(TransientTextureCount, Textures.GetAllocatedCount());
		TRACE_COUNTER_SET(TransientTextureCacheHitPercentage, Textures.GetHitPercentage());

		Textures.Forfeit(GFrameCounterRenderThread, [this](FRHITransientTexture* Texture) { ReleaseTextureInternal(Texture); });

		TRACE_COUNTER_SET(TransientTextureCacheSize, Textures.GetSize());
	}

	{
		TRACE_COUNTER_SET(TransientBufferCount, Buffers.GetAllocatedCount());
		TRACE_COUNTER_SET(TransientBufferCacheHitPercentage, Buffers.GetHitPercentage());

		Buffers.Forfeit(GFrameCounterRenderThread, [this](FRHITransientBuffer* Buffer) { ReleaseBufferInternal(Buffer); });

		TRACE_COUNTER_SET(TransientBufferCacheSize, Buffers.GetSize());
	}

	if (FastPagePool)
	{
		FastPagePool->Flush(RHICmdList);
	}

	for (FRHITransientPagePool* PagePool : PagePools)
	{
		PagePool->Flush(RHICmdList);
	}

	if (Stats.HasDeallocations())
	{
		const int32 FirstForfeitIndex = Algo::Partition(PagePools.GetData(), PagePools.Num(), [](const FRHITransientPagePool* PagePool) { return !PagePool->IsEmpty(); });
		PagePoolCache.Forfeit(MakeArrayView(PagePools.GetData() + FirstForfeitIndex, PagePools.Num() - FirstForfeitIndex));
		PagePools.SetNum(FirstForfeitIndex, EAllowShrinking::No);
	}

	RHICmdList.EnqueueLambda([&PagePoolCache = PagePoolCache, Stats = Stats](FRHICommandListImmediate&)
	{
		PagePoolCache.Stats.Accumulate(Stats);
	});

	Stats.Reset();

	CurrentCycle++;
}
