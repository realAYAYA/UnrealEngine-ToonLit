// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "D3D12RHIPrivate.h"
#include "Experimental/Containers/SherwoodHashTable.h"

class FD3D12DynamicRHI;
struct FD3D12DefaultViews;
class FD3D12DescriptorCache;
struct FD3D12VertexBufferCache;
struct FD3D12IndexBufferCache;
struct FD3D12ConstantBufferCache;
struct FD3D12ShaderResourceViewCache;
struct FD3D12UnorderedAccessViewCache;
struct FD3D12SamplerStateCache;

// #dxr_todo UE-72158: FD3D12Device::GlobalViewHeap/GlobalSamplerHeap should be used instead of ad-hoc heaps here.
// Unfortunately, this requires a major refactor of how global heaps work.
// FD3D12CommandContext-s should not get static chunks of the global heap, but instead should dynamically allocate
// chunks on as-needed basis and release them when possible.
// This would allow calling code to sub-allocate heap blocks from the same global heap.
class FD3D12ExplicitDescriptorHeapCache : FD3D12DeviceChild
{
public:

	UE_NONCOPYABLE(FD3D12ExplicitDescriptorHeapCache)

	struct Entry
	{
		ID3D12DescriptorHeap* Heap = nullptr;
		uint64 FenceValue = 0;
		uint32 NumDescriptors = 0;
		D3D12_DESCRIPTOR_HEAP_TYPE Type = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
	};

	FD3D12ExplicitDescriptorHeapCache(FD3D12Device* Device)
	: FD3D12DeviceChild(Device)
	{
	}

	~FD3D12ExplicitDescriptorHeapCache();

	void ReleaseHeap(Entry& Entry);

	Entry AllocateHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32 NumDescriptors);

	void ReleaseStaleEntries(uint32 MaxAge, uint64 CompletedFenceValue);

	void Flush();

	FCriticalSection CriticalSection;
	TArray<Entry> Entries;
	uint32 AllocatedEntries = 0;
};

struct FD3D12ExplicitDescriptorHeap : public FD3D12DeviceChild
{
	UE_NONCOPYABLE(FD3D12ExplicitDescriptorHeap)

	FD3D12ExplicitDescriptorHeap(FD3D12Device* Device)
	: FD3D12DeviceChild(Device)
	{
	}

	~FD3D12ExplicitDescriptorHeap();

	void Init(uint32 InMaxNumDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE InType);

	// Returns descriptor heap base index or -1 if allocation is not possible.
	// Thread-safe (uses atomic linear allocation).
	int32 Allocate(uint32 InNumDescriptors);

	void CopyDescriptors(int32 BaseIndex, const D3D12_CPU_DESCRIPTOR_HANDLE* InDescriptors, uint32 InNumDescriptors);

	bool CompareDescriptors(int32 BaseIndex, const D3D12_CPU_DESCRIPTOR_HANDLE* InDescriptors, uint32 InNumDescriptors);

	D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorCPU(uint32 Index) const;

	D3D12_GPU_DESCRIPTOR_HANDLE GetDescriptorGPU(uint32 Index) const;

	void UpdateSyncPoint();

	D3D12_DESCRIPTOR_HEAP_TYPE Type = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
	ID3D12DescriptorHeap* D3D12Heap = nullptr;
	uint32 MaxNumDescriptors = 0;

	int32 NumAllocatedDescriptors = 0;

	// Marks the valid range of the heap when exhaustive sampler deduplication is enabled. Not used otherwise.
	int32 NumWrittenSamplerDescriptors = 0;

	uint32 DescriptorSize = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE CPUBase = {};
	D3D12_GPU_DESCRIPTOR_HANDLE GPUBase = {};

	FD3D12ExplicitDescriptorHeapCache::Entry HeapCacheEntry;

	TArray<D3D12_CPU_DESCRIPTOR_HANDLE> Descriptors;

	bool bExhaustiveSamplerDeduplication = false;
};

class FD3D12ExplicitDescriptorCache : public FD3D12DeviceChild
{
public:

	UE_NONCOPYABLE(FD3D12ExplicitDescriptorCache)

	FD3D12ExplicitDescriptorCache(FD3D12Device* Device, uint32 MaxWorkerCount)
	: FD3D12DeviceChild(Device)
	, ViewHeap(Device)
	, SamplerHeap(Device)
	{
		check(MaxWorkerCount > 0u);
		WorkerData.SetNum(MaxWorkerCount);
	}

	void Init(uint32 NumViewDescriptors, uint32 NumSamplerDescriptors, ERHIBindlessConfiguration BindlessConfig);

	void UpdateSyncPoint();

	void SetDescriptorHeaps(FD3D12CommandContext& CommandContext);

	// Returns descriptor heap base index for this descriptor table allocation or -1 if allocation failed.
	int32 Allocate(const D3D12_CPU_DESCRIPTOR_HANDLE* Descriptors, uint32 NumDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32 WorkerIndex);

	// Returns descriptor heap base index for this descriptor table allocation (checking for duplicates and reusing existing tables) or -1 if allocation failed.
	int32 AllocateDeduplicated(const uint32* DescriptorVersions, const D3D12_CPU_DESCRIPTOR_HANDLE* Descriptors, uint32 NumDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32 WorkerIndex);

	FD3D12ExplicitDescriptorHeap ViewHeap;
	FD3D12ExplicitDescriptorHeap SamplerHeap;

#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
	bool bBindlessViews = false;
	bool bBindlessSamplers = false;
#endif

	template<typename KeyType>
	struct TIdentityHash
	{
		static FORCEINLINE bool Matches(KeyType A, KeyType B)
		{
			return A == B;
		}
		static FORCEINLINE uint32 GetKeyHash(KeyType Key)
		{
			return (uint32)Key;
		}
	};

	using TDescriptorHashMap = Experimental::TSherwoodMap<uint64, int32, TIdentityHash<uint64>>;

	struct alignas(PLATFORM_CACHE_LINE_SIZE) FWorkerThreadData
	{
		TDescriptorHashMap ViewDescriptorTableCache;
		TDescriptorHashMap SamplerDescriptorTableCache;
	};

	TArray<FWorkerThreadData> WorkerData;
};