// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "MetalRHIPrivate.h"
#include "device.hpp"
#include "buffer.hpp"
#include "Containers/LockFreeList.h"
#include "ResourcePool.h"

struct FMetalPooledBufferArgs
{
    FMetalPooledBufferArgs() : Device(nil), Size(0), Flags(BUF_None), Storage(mtlpp::StorageMode::Shared), CpuCacheMode(mtlpp::CpuCacheMode::DefaultCache) {}
	
    FMetalPooledBufferArgs(mtlpp::Device InDevice, uint32 InSize, EBufferUsageFlags InFlags, mtlpp::StorageMode InStorage, mtlpp::CpuCacheMode InCpuCacheMode = mtlpp::CpuCacheMode::DefaultCache)
	: Device(InDevice)
	, Size(InSize)
    , Flags(InFlags)
	, Storage(InStorage)
	, CpuCacheMode(InCpuCacheMode)
	{
	}
	
	mtlpp::Device Device;
	uint32 Size;
	EBufferUsageFlags Flags;
	mtlpp::StorageMode Storage;
	mtlpp::CpuCacheMode CpuCacheMode;
};

class FMetalSubBufferHeap
{
    friend class FMetalResourceHeap;
    
public:
	FMetalSubBufferHeap(NSUInteger Size, NSUInteger Alignment, mtlpp::ResourceOptions, FCriticalSection& PoolMutex);
	~FMetalSubBufferHeap();
	
	ns::String   GetLabel() const;
    mtlpp::Device       GetDevice() const;
    mtlpp::StorageMode  GetStorageMode() const;
    mtlpp::CpuCacheMode GetCpuCacheMode() const;
    NSUInteger     GetSize() const;
    NSUInteger     GetUsedSize() const;
	NSUInteger	 MaxAvailableSize() const;
	int64     NumCurrentAllocations() const;
    bool     CanAllocateSize(NSUInteger Size) const;

    void SetLabel(const ns::String& label);
	
    FMetalBuffer NewBuffer(NSUInteger length);
    mtlpp::PurgeableState SetPurgeableState(mtlpp::PurgeableState state);
	void FreeRange(ns::Range const& Range);

    void SetOwner(ns::Range const& Range, FMetalRHIBuffer* Owner, bool bIsSwap);

private:
    struct Allocation
    {
        ns::Range Range;
        mtlpp::Buffer::Type Resource;
        FMetalRHIBuffer* Owner;
    };
    
	FCriticalSection& PoolMutex;
	int64 volatile OutstandingAllocs;
	NSUInteger MinAlign;
	NSUInteger UsedSize;
	mtlpp::Buffer ParentBuffer;
	mutable mtlpp::Heap ParentHeap;
	TArray<ns::Range> FreeRanges;
    TArray<Allocation> AllocRanges;
};

class FMetalSubBufferLinear
{
public:
	FMetalSubBufferLinear(NSUInteger Size, NSUInteger Alignment, mtlpp::ResourceOptions, FCriticalSection& PoolMutex);
	~FMetalSubBufferLinear();
	
	ns::String   GetLabel() const;
	mtlpp::Device       GetDevice() const;
	mtlpp::StorageMode  GetStorageMode() const;
	mtlpp::CpuCacheMode GetCpuCacheMode() const;
	NSUInteger     GetSize() const;
	NSUInteger     GetUsedSize() const;
	bool	 CanAllocateSize(NSUInteger Size) const;

	void SetLabel(const ns::String& label);
	
	FMetalBuffer NewBuffer(NSUInteger length);
	mtlpp::PurgeableState SetPurgeableState(mtlpp::PurgeableState state);
	void FreeRange(ns::Range const& Range);
	
private:
	FCriticalSection& PoolMutex;
	NSUInteger MinAlign;
	NSUInteger WriteHead;
	NSUInteger UsedSize;
	NSUInteger FreedSize;
	mtlpp::Buffer ParentBuffer;
};

class FMetalSubBufferMagazine
{
public:
	FMetalSubBufferMagazine(NSUInteger Size, NSUInteger ChunkSize, mtlpp::ResourceOptions);
	~FMetalSubBufferMagazine();
	
	ns::String   GetLabel() const;
    mtlpp::Device       GetDevice() const;
    mtlpp::StorageMode  GetStorageMode() const;
    mtlpp::CpuCacheMode GetCpuCacheMode() const;
    NSUInteger     GetSize() const;
    NSUInteger     GetUsedSize() const;
	NSUInteger	 GetFreeSize() const;
	int64     NumCurrentAllocations() const;
    bool     CanAllocateSize(NSUInteger Size) const;

    void SetLabel(const ns::String& label);
	void FreeRange(ns::Range const& Range);

    FMetalBuffer NewBuffer();
    mtlpp::PurgeableState SetPurgeableState(mtlpp::PurgeableState state);

private:
	NSUInteger MinAlign;
    NSUInteger BlockSize;
	int64 volatile OutstandingAllocs;
	int64 volatile UsedSize;
	mtlpp::Buffer ParentBuffer;
	mutable mtlpp::Heap ParentHeap;
	TArray<int8> Blocks;
};

struct FMetalRingBufferRef
{
	FMetalRingBufferRef(FMetalBuffer Buf);
	~FMetalRingBufferRef();
	
	void SetLastRead(uint64 Read) { FPlatformAtomics::InterlockedExchange((int64*)&LastRead, Read); }
	
	FMetalBuffer Buffer;
	uint64 LastRead;
};

class FMetalResourceHeap;

class FMetalSubBufferRing
{
public:
	FMetalSubBufferRing(NSUInteger Size, NSUInteger Alignment, mtlpp::ResourceOptions Options);
	~FMetalSubBufferRing();
	
	mtlpp::Device       GetDevice() const;
	mtlpp::StorageMode  GetStorageMode() const;
	mtlpp::CpuCacheMode GetCpuCacheMode() const;
	NSUInteger     GetSize() const;
	
	FMetalBuffer NewBuffer(NSUInteger Size, uint32 Alignment);
	
	/** Tries to shrink the ring-buffer back toward its initial size, but not smaller. */
	void Shrink();
	
	/** Submits all outstanding writes to the GPU, coalescing the updates into a single contiguous range. */
	void Submit();
	
	/** Commits a completion handler to the cmd-buffer to release the processed range */
	void Commit(mtlpp::CommandBuffer& CmdBuffer);
	
private:
	NSUInteger FrameSize[10];
	NSUInteger LastFrameChange;
	NSUInteger InitialSize;
	NSUInteger MinAlign;
	NSUInteger CommitHead;
	NSUInteger SubmitHead;
	NSUInteger WriteHead;
	NSUInteger BufferSize;
	mtlpp::ResourceOptions Options;
	mtlpp::StorageMode Storage;
	TSharedPtr<FMetalRingBufferRef, ESPMode::ThreadSafe> Buffer;
	TArray<ns::Range> AllocatedRanges;
};

class FMetalBufferPoolPolicyData
{
	enum BucketSizes
	{
		// These sizes are required for ring-buffers and esp. Managed Memory which is a Mac-only feature
		BucketSize256,
		BucketSize512,
		BucketSize1k,
		BucketSize2k,
		BucketSize4k,
		BucketSize8k,
		BucketSize16k,
		BucketSize32k,
		BucketSize64k,
		BucketSize128k,
		BucketSize256k,
		BucketSize512k,
		BucketSize1Mb,
		BucketSize2Mb,
		BucketSize4Mb,
		// These sizes are the ones typically used by buffer allocations
		BucketSize8Mb,
		BucketSize12Mb,
		BucketSize16Mb,
		BucketSize24Mb,
		BucketSize32Mb,
		NumBucketSizes
	};
public:
	/** Buffers are created with a simple byte size */
	typedef FMetalPooledBufferArgs CreationArguments;
	enum
	{
		NumResourceStorageModes = 4, /* Corresponds to MTLStorageMode types: managed, shared, private, memoryless */
		NumSafeFrames = 1, /** Number of frames to leave buffers before reclaiming/reusing */
		NumPoolBucketSizes = NumBucketSizes, /** Number of pool bucket sizes */
		NumPoolBuckets = NumPoolBucketSizes * NumResourceStorageModes, /** Number of pool bucket sizes - all entries must use consistent ResourceOptions, so the total number of pool buckets is the number of pool bucket sizes x the number of resource storage modes */
		NumToDrainPerFrame = 65536, /** Max. number of resources to cull in a single frame */
		CullAfterFramesNum = 30 /** Resources are culled if unused for more frames than this */
	};
	
	/** Get the pool bucket index from the size
	 * @param Size the number of bytes for the resource
	 * @returns The bucket index.
	 */
	uint32 GetPoolBucketIndex(CreationArguments Args);
	
	/** Get the pool bucket size from the index
	 * @param Bucket the bucket index
	 * @returns The bucket size.
	 */
	uint32 GetPoolBucketSize(uint32 Bucket);
	
	/** Creates the resource
	 * @param Args The buffer size in bytes.
	 * @returns A suitably sized buffer or NULL on failure.
	 */
	FMetalBuffer CreateResource(CreationArguments Args);
	
	/** Gets the arguments used to create resource
	 * @param Resource The buffer to get data for.
	 * @returns The arguments used to create the buffer.
	 */
	CreationArguments GetCreationArguments(FMetalBuffer const& Resource);
	
	/** Frees the resource
	 * @param Resource The buffer to prepare for release from the pool permanently.
	 */
	void FreeResource(FMetalBuffer& Resource);
	
private:
	/** The bucket sizes */
	static uint32 BucketSizes[NumPoolBucketSizes];
};

/** A pool for metal buffers with consistent usage, bucketed for efficiency. */
class FMetalBufferPool : public TResourcePool<FMetalBuffer, FMetalBufferPoolPolicyData, FMetalBufferPoolPolicyData::CreationArguments>
{
public:
	/** Destructor */
	virtual ~FMetalBufferPool();
};

class FMetalTexturePool
{
	enum
	{
        PurgeAfterNumFrames = 2, /* Textures must be reused fairly rapidly but after this number of frames we reclaim the memory, even though the object persists */
		CullAfterNumFrames = 3, /* Textures must be reused fairly rapidly or we bin them as they are much larger than buffers */
	};
public:
	struct Descriptor
	{
		friend uint32 GetTypeHash(Descriptor const& Other)
		{
			uint32 Hash = GetTypeHash((uint64)Other.textureType);
			Hash = HashCombine(Hash, GetTypeHash((uint64)Other.pixelFormat));
			Hash = HashCombine(Hash, GetTypeHash((uint64)Other.usage));
			Hash = HashCombine(Hash, GetTypeHash((uint64)Other.width));
			Hash = HashCombine(Hash, GetTypeHash((uint64)Other.height));
			Hash = HashCombine(Hash, GetTypeHash((uint64)Other.depth));
			Hash = HashCombine(Hash, GetTypeHash((uint64)Other.mipmapLevelCount));
			Hash = HashCombine(Hash, GetTypeHash((uint64)Other.sampleCount));
			Hash = HashCombine(Hash, GetTypeHash((uint64)Other.arrayLength));
			Hash = HashCombine(Hash, GetTypeHash((uint64)Other.resourceOptions));
			return Hash;
		}
		
		bool operator<(Descriptor const& Other) const
		{
			if (this != &Other)
			{
				return (textureType < Other.textureType ||
						pixelFormat < Other.pixelFormat ||
						width < Other.width ||
						height < Other.height ||
						depth < Other.depth ||
						mipmapLevelCount < Other.mipmapLevelCount ||
						sampleCount < Other.sampleCount ||
						arrayLength < Other.arrayLength ||
						resourceOptions < Other.resourceOptions ||
						usage < Other.usage);
			}
			return false;
		}
		
		bool operator==(Descriptor const& Other) const
		{
			if (this != &Other)
			{
				return (textureType == Other.textureType &&
				pixelFormat == Other.pixelFormat &&
				width == Other.width &&
				height == Other.height &&
				depth == Other.depth &&
				mipmapLevelCount == Other.mipmapLevelCount &&
				sampleCount == Other.sampleCount &&
				arrayLength == Other.arrayLength &&
				resourceOptions == Other.resourceOptions &&
				usage == Other.usage);
			}
			return true;
		}
		
		NSUInteger textureType;
		NSUInteger pixelFormat;
		NSUInteger width;
		NSUInteger height;
		NSUInteger depth;
		NSUInteger mipmapLevelCount;
		NSUInteger sampleCount;
		NSUInteger arrayLength;
		NSUInteger resourceOptions;
		NSUInteger usage;
		NSUInteger freedFrame;
	};
	
	FMetalTexturePool(FCriticalSection& PoolMutex);
	~FMetalTexturePool();
	
	FMetalTexture CreateTexture(mtlpp::Device Device, mtlpp::TextureDescriptor Desc);
	void ReleaseTexture(FMetalTexture& Texture);
	
	void Drain(bool const bForce);

private:
	FCriticalSection& PoolMutex;
	TMap<Descriptor, FMetalTexture> Pool;
};

class FMetalResourceHeap
{
	enum MagazineSize
	{
		Size16,
		Size32,
		Size64,
		Size128,
		Size256,
		Size512,
		Size1024,
		Size2048,
		Size4096,
        Size8192,
		NumMagazineSizes
	};
	
	enum HeapSize
	{
		Size1Mb,
		Size2Mb,
		NumHeapSizes
	};

	enum TextureHeapSize
	{
		Size4Mb,
		Size8Mb,
		Size16Mb,
		Size32Mb,
		Size64Mb,
		Size128Mb,
		Size256Mb,
		NumTextureHeapSizes,
		MinTexturesPerHeap = 4,
		MaxTextureSize = Size64Mb,
	};
	
	enum AllocTypes
	{
		AllocShared,
		AllocPrivate,
		NumAllocTypes = 2
	};
	
	enum EMetalHeapTextureUsage
	{
		/** Regular texture resource */
		EMetalHeapTextureUsageResource = 0,
		/** Render target or UAV that can be aliased */
		EMetalHeapTextureUsageRenderTarget = 1,
		/** Number of texture usage types */
		EMetalHeapTextureUsageNum = 2
	};
    
    enum UsageTypes
    {
        UsageStatic,
        UsageDynamic,
        NumUsageTypes = 2
    };
    
public:
	FMetalResourceHeap(void);
	~FMetalResourceHeap();
	
	void Init(FMetalCommandQueue& Queue);
	
    FMetalBuffer CreateBuffer(uint32 Size, uint32 Alignment, EBufferUsageFlags Flags, mtlpp::ResourceOptions Options, bool bForceUnique = false);
	FMetalTexture CreateTexture(mtlpp::TextureDescriptor Desc, FMetalSurface* Surface);
	
	void ReleaseBuffer(FMetalBuffer& Buffer);
	void ReleaseTexture(FMetalSurface* Surface, FMetalTexture& Texture);
	
	void Compact(class FMetalRenderPass* Pass, bool const bForce);
	
private:
	uint32 GetMagazineIndex(uint32 Size);
	uint32 GetHeapIndex(uint32 Size);
	TextureHeapSize TextureSizeToIndex(uint32 Size);
	
	mtlpp::Heap GetTextureHeap(mtlpp::TextureDescriptor Desc, mtlpp::SizeAndAlign Size);
	
private:
	static uint32 MagazineSizes[NumMagazineSizes];
	static uint32 HeapSizes[NumHeapSizes];
	static uint32 MagazineAllocSizes[NumMagazineSizes];
	static uint32 HeapAllocSizes[NumHeapSizes];
	static uint32 HeapTextureHeapSizes[NumTextureHeapSizes];

	FCriticalSection Mutex;
	FMetalCommandQueue* Queue;
	
	/** Small allocations (<= 4KB) are made from magazine allocators that use sub-ranges of a buffer */
    TArray<FMetalSubBufferMagazine*> SmallBuffers[NumUsageTypes][NumAllocTypes][NumMagazineSizes];

	/** Typical allocations (4KB - 4MB) are made from heap allocators that use sub-ranges of a buffer */
	/** There are two alignment categories for heaps - 16b for Vertes/Index data and 256b for constant data (macOS-only) */
    TArray<FMetalSubBufferHeap*> BufferHeaps[NumUsageTypes][NumAllocTypes][NumHeapSizes];
	
	/** Larger buffers (up-to 32MB) that are subject to bucketing & pooling rather than sub-allocation */
	FMetalBufferPool Buffers[NumAllocTypes];
#if PLATFORM_MAC // All managed buffers are bucketed & pooled rather than sub-allocated to avoid memory consistency complexities
	FMetalBufferPool ManagedBuffers;
	TArray<FMetalSubBufferLinear*> ManagedSubHeaps;
#endif
	/** Anything else is just allocated directly from the device! */
	
	/** We can reuse texture allocations as well, to minimize their performance impact */
	FMetalTexturePool TexturePool;
	FMetalTexturePool TargetPool;
	
	TArray<mtlpp::Heap> TextureHeaps[EMetalHeapTextureUsageNum][NumTextureHeapSizes];
};
