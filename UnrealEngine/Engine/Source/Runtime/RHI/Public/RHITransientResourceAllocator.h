// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHI.h"
#include "RHIResources.h"

class FRHICommandListBase;
class FRHICommandListImmediate;
class FRHITransientHeap;
class FRHITransientPagePool;

struct FRHITransientPageSpan
{
	// Offset of the span in the page pool in pages. 
	uint16 Offset = 0;

	// Number of pages in the span.
	uint16 Count = 0;
};

/** Represents an allocation from a transient page pool. */
struct FRHITransientPagePoolAllocation
{
	bool IsValid() const { return Pool != nullptr; }

	// The transient page pool which made the allocation.
	FRHITransientPagePool* Pool = nullptr;

	// A unique hash identifying this allocation to the allocator implementation.
	uint64 Hash = 0;

	// The index identifying the allocation to the page pool.
	uint16 SpanIndex = 0;

	// Offsets into the array of spans for the allocator implementation.
	uint16 SpanOffsetMin = 0;
	uint16 SpanOffsetMax = 0;
};

/** Represents a full set of page allocations from multiple page pools. */
struct FRHITransientPageAllocation
{
	// The list of allocations by pool.
	TArray<FRHITransientPagePoolAllocation> PoolAllocations;

	// The full list of spans indexed by each allocation.
	TArray<FRHITransientPageSpan> Spans;
};

/** Represents an allocation from the transient heap. */
struct FRHITransientHeapAllocation
{
	bool IsValid() const { return Size != 0; }

	// Transient heap which made the allocation.
	FRHITransientHeap* Heap = nullptr;

	// Size of the allocation made from the allocator (aligned).
	uint64 Size = 0;

	// Offset in the transient heap; front of the heap starts at 0.
	uint64 Offset = 0;

	// Number of bytes of padding were added to the offset.
	uint32 AlignmentPad = 0;
};

enum class ERHITransientResourceType : uint8
{
	Texture,
	Buffer
};

enum class ERHITransientAllocationType : uint8
{
	Heap,
	Page
};

class FRHITransientResource
{
public:
	static const uint32 kInvalidPassIndex = TNumericLimits<uint32>::Max();

	FRHITransientResource(
		FRHIResource* InResource,
		uint64 InGpuVirtualAddress,
		uint64 InHash,
		uint64 InSize,
		ERHITransientAllocationType InAllocationType,
		ERHITransientResourceType InResourceType)
		: Resource(InResource)
		, GpuVirtualAddress(InGpuVirtualAddress)
		, Hash(InHash)
		, Size(InSize)
		, AllocationType(InAllocationType)
		, ResourceType(InResourceType)
	{}

	virtual ~FRHITransientResource() = default;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	//! Internal Allocator API

	virtual void Acquire(FRHICommandListBase& RHICmdList, const TCHAR* InName, uint32 InAcquirePassIndex, uint64 InAllocatorCycle)
	{
		Name = InName;
		AcquirePasses = TInterval<uint32>(0, InAcquirePassIndex);
		DiscardPasses = TInterval<uint32>(kInvalidPassIndex, kInvalidPassIndex);
		AcquireCycle = InAllocatorCycle;
		AcquireCount++;
		AliasingOverlaps.Reset();
	}

	void Discard(uint32 InDiscardPassIndex)
	{
		DiscardPasses.Min = InDiscardPassIndex;
	}

	void AddAliasingOverlap(FRHITransientResource* InResource)
	{
		AliasingOverlaps.Emplace(InResource->GetRHI(), InResource->IsTexture() ? FRHITransientAliasingOverlap::EType::Texture : FRHITransientAliasingOverlap::EType::Buffer);

		check(InResource->DiscardPasses.Min != kInvalidPassIndex);

		InResource->DiscardPasses.Max = FMath::Min(InResource->DiscardPasses.Max,             AcquirePasses.Max);
		            AcquirePasses.Min = FMath::Max(            AcquirePasses.Min, InResource->DiscardPasses.Min);
	}

	FRHITransientHeapAllocation& GetHeapAllocation()
	{
		check(AllocationType == ERHITransientAllocationType::Heap);
		return HeapAllocation;
	}

	const FRHITransientHeapAllocation& GetHeapAllocation() const
	{
		check(AllocationType == ERHITransientAllocationType::Heap);
		return HeapAllocation;
	}

	FRHITransientPageAllocation& GetPageAllocation()
	{
		check(IsPageAllocated());
		return PageAllocation;
	}

	const FRHITransientPageAllocation& GetPageAllocation() const
	{
		check(IsPageAllocated());
		return PageAllocation;
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////

	// Returns the underlying RHI resource.
	FRHIResource* GetRHI() const { return Resource; }

	// Returns the gpu virtual address of the transient resource.
	uint64 GetGpuVirtualAddress() const { return GpuVirtualAddress; }

	// Returns the name assigned to the transient resource at allocation time.
	const TCHAR* GetName() const { return Name; }

	// Returns the hash used to uniquely identify this resource if cached.
	uint64 GetHash() const { return Hash; }

	// Returns the required size in bytes of the resource.
	uint64 GetSize() const { return Size; }

	// Returns the last allocator cycle this resource was acquired.
	uint64 GetAcquireCycle() const { return AcquireCycle; }

	// Returns the number of times Acquire has been called.
	uint32 GetAcquireCount() const { return AcquireCount; }

	// Returns the list of aliasing overlaps used when transitioning the resource.
	TConstArrayView<FRHITransientAliasingOverlap> GetAliasingOverlaps() const { return AliasingOverlaps; }

	// Returns the pass index which may end acquiring this resource.
	TInterval<uint32> GetAcquirePasses() const { return AcquirePasses; }

	// Returns the pass index which discarded this resource.
	TInterval<uint32> GetDiscardPasses() const { return DiscardPasses; }

	// Returns whether this resource is still in an acquired state.
	bool IsAcquired() const { return DiscardPasses.Min == kInvalidPassIndex; }

	ERHITransientResourceType GetResourceType() const { return ResourceType; }

	bool IsTexture() const { return ResourceType == ERHITransientResourceType::Texture; }
	bool IsBuffer()  const { return ResourceType == ERHITransientResourceType::Buffer; }

	ERHITransientAllocationType GetAllocationType() const { return AllocationType; }

	bool IsHeapAllocated() const { return AllocationType == ERHITransientAllocationType::Heap; }
	bool IsPageAllocated() const { return AllocationType == ERHITransientAllocationType::Page; }

private:
	// Underlying RHI resource.
	TRefCountPtr<FRHIResource> Resource;

	// The Gpu virtual address of the RHI resource.
	uint64 GpuVirtualAddress = 0;

	// The hash used to uniquely identify this resource if cached.
	uint64 Hash;

	// Size of the resource in bytes.
	uint64 Size;

	// Alignment of the resource in bytes.
	uint32 Alignment;

	// Tracks the number of times Acquire has been called.
	uint32 AcquireCount = 0;

	// Cycle count used to deduce age of the resource.
	uint64 AcquireCycle = 0;

	// Debug name of the resource. Updated with each allocation.
	const TCHAR* Name = nullptr;

	FRHITransientHeapAllocation HeapAllocation;
	FRHITransientPageAllocation PageAllocation;

	// List of aliasing resources overlapping with this one.
	TArray<FRHITransientAliasingOverlap> AliasingOverlaps;

	// Start -> End split pass index intervals for acquire / discard operations.
	TInterval<uint32> AcquirePasses = TInterval<uint32>(0, 0);
	TInterval<uint32> DiscardPasses = TInterval<uint32>(0, 0);

	ERHITransientAllocationType AllocationType;
	ERHITransientResourceType ResourceType;
};

class FRHITransientTexture final : public FRHITransientResource
{
public:
	FRHITransientTexture(
		FRHITexture* InTexture,
		uint64 InGpuVirtualAddress,
		uint64 InHash,
		uint64 InSize,
		ERHITransientAllocationType InAllocationType,
		const FRHITextureCreateInfo& InCreateInfo)
		: FRHITransientResource(InTexture, InGpuVirtualAddress, InHash, InSize, InAllocationType, ERHITransientResourceType::Texture)
		, CreateInfo(InCreateInfo)
	{}

	//////////////////////////////////////////////////////////////////////////////////////////////////
	//! Internal Allocator API
	RHI_API void Acquire(FRHICommandListBase& RHICmdList, const TCHAR* InName, uint32 InAcquirePassIndex, uint64 InInitCycle) override;
	//////////////////////////////////////////////////////////////////////////////////////////////////

	// Returns the underlying RHI texture.
	FRHITexture* GetRHI() const { return static_cast<FRHITexture*>(FRHITransientResource::GetRHI()); }

	// Returns the create info struct used when creating this texture.
	const FRHITextureCreateInfo& GetCreateInfo() const { return CreateInfo; }

	// Finds a UAV matching the descriptor in the cache or creates a new one and updates the cache.
	FRHIUnorderedAccessView* GetOrCreateUAV(FRHICommandListBase& RHICmdList, const FRHITextureUAVCreateInfo& InCreateInfo) { return ViewCache.GetOrCreateUAV(RHICmdList, GetRHI(), InCreateInfo); }

	// Finds a SRV matching the descriptor in the cache or creates a new one and updates the cache.
	FRHIShaderResourceView* GetOrCreateSRV(FRHICommandListBase& RHICmdList, const FRHITextureSRVCreateInfo& InCreateInfo) { return ViewCache.GetOrCreateSRV(RHICmdList, GetRHI(), InCreateInfo); }

	// The create info describing the texture.
	const FRHITextureCreateInfo CreateInfo;

	// The persistent view cache containing all views created for this texture.
	FRHITextureViewCache ViewCache;
};

class FRHITransientBuffer final : public FRHITransientResource
{
public:
	FRHITransientBuffer(
		FRHIBuffer* InBuffer,
		uint64 InGpuVirtualAddress,
		uint64 InHash,
		uint64 InSize,
		ERHITransientAllocationType InAllocationType,
		const FRHIBufferCreateInfo& InCreateInfo)
		: FRHITransientResource(InBuffer, InGpuVirtualAddress, InHash, InSize, InAllocationType, ERHITransientResourceType::Buffer)
		, CreateInfo(InCreateInfo)
	{}

	//////////////////////////////////////////////////////////////////////////////////////////////////
	//! Internal Allocator API
	RHI_API void Acquire(FRHICommandListBase& RHICmdList, const TCHAR* InName, uint32 InAcquirePassIndex, uint64 InInitCycle) override;
	//////////////////////////////////////////////////////////////////////////////////////////////////

	// Returns the underlying RHI buffer.
	FRHIBuffer* GetRHI() const { return static_cast<FRHIBuffer*>(FRHITransientResource::GetRHI()); }

	// Returns the create info used when creating this buffer.
	const FRHIBufferCreateInfo& GetCreateInfo() const { return CreateInfo; }

	// Finds a UAV matching the descriptor in the cache or creates a new one and updates the cache.
	FRHIUnorderedAccessView* GetOrCreateUAV(FRHICommandListBase& RHICmdList, const FRHIBufferUAVCreateInfo& InCreateInfo) { return ViewCache.GetOrCreateUAV(RHICmdList, GetRHI(), InCreateInfo); }

	// Finds a SRV matching the descriptor in the cache or creates a new one and updates the cache.
	FRHIShaderResourceView* GetOrCreateSRV(FRHICommandListBase& RHICmdList, const FRHIBufferSRVCreateInfo& InCreateInfo) { return ViewCache.GetOrCreateSRV(RHICmdList, GetRHI(), InCreateInfo); }

	// The create info describing the texture.
	const FRHIBufferCreateInfo CreateInfo;

	// The persistent view cache containing all views created for this buffer.
	FRHIBufferViewCache ViewCache;
};

class FRHITransientAllocationStats
{
public:
	struct FAllocation
	{
		uint64 OffsetMin = 0;
		uint64 OffsetMax = 0;
		uint32 MemoryRangeIndex = 0;
	};

	using FAllocationArray = TArray<FAllocation, TInlineAllocator<2>>;

	enum class EMemoryRangeFlags
	{
		None = 0,

		// The memory range references platform specific fast RAM.
		FastVRAM = 1 << 0
	};

	struct FMemoryRange
	{
		// Number of bytes available for use in the memory range.
		uint64 Capacity = 0;

		// Number of bytes allocated for use in the memory range.
		uint64 CommitSize = 0;

		// Flags specified for this memory range.
		EMemoryRangeFlags Flags = EMemoryRangeFlags::None;
	};

	TArray<FMemoryRange> MemoryRanges;
	TMap<const FRHITransientResource*, FAllocationArray> Resources;
};

ENUM_CLASS_FLAGS(FRHITransientAllocationStats::EMemoryRangeFlags);

class IRHITransientResourceAllocator
{
public:
	virtual ~IRHITransientResourceAllocator() = default;

	// Supports transient allocations of given resource type
	virtual bool SupportsResourceType(ERHITransientResourceType InType) const = 0;

	// Allocates a new transient resource with memory backed by the transient allocator.
	virtual FRHITransientTexture* CreateTexture(const FRHITextureCreateInfo& InCreateInfo, const TCHAR* InDebugName, uint32 InPassIndex) = 0;
	virtual FRHITransientBuffer* CreateBuffer(const FRHIBufferCreateInfo& InCreateInfo, const TCHAR* InDebugName, uint32 InPassIndex) = 0;

	// Deallocates the underlying memory for use by a future resource creation call.
	virtual void DeallocateMemory(FRHITransientTexture* InTexture, uint32 InPassIndex) = 0;
	virtual void DeallocateMemory(FRHITransientBuffer* InBuffer, uint32 InPassIndex) = 0;

	// Flushes any pending allocations prior to rendering. Optionally emits stats if OutStats is valid.
	virtual void Flush(FRHICommandListImmediate& RHICmdList, FRHITransientAllocationStats* OutStats = nullptr) = 0;

	// Releases this instance of the transient allocator. Invalidates any outstanding transient resources.
	virtual void Release(FRHICommandListImmediate& RHICmdList) { delete this; }
};