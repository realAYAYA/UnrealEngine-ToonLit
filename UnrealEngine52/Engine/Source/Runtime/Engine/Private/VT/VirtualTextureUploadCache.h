// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "RenderResource.h"
#include "PixelFormat.h"

class FRHITexture;
class FRHIBuffer;

/** Opaque handle for referencing an upload tile returned by FVirtualTextureUploadCache::PrepareTileForUpload(). */
struct FVTUploadTileHandle
{
	explicit FVTUploadTileHandle(uint32 InIndex = ~0u) 
		: Index(InIndex) 
	{}

	inline bool IsValid() const { return Index != ~0u; }

	uint32 Index;
};

/** 
 * Memory buffer for uploading virtual texture data. 
 * This is a simple view of the buffer memory for a single tile intended for use by the streaming systems.
 */
struct FVTUploadTileBuffer
{
	void* Memory = nullptr;
	uint32 MemorySize = 0u;
	uint32 Stride = 0u;
};

/** 
 * Extended definition of the memory buffer for uploading virtual texture 
 * This extended view of the buffer is used internally by FVirtualTextureUploadCache.
 */
struct FVTUploadTileBufferExt
{
	TRefCountPtr<FRHIBuffer> RHIBuffer;
	void* BufferMemory = nullptr;
	uint32 BufferOffset = 0u;
	uint32 Stride = 0u;
};

/** 
 * Handles allocation of staging buffer memory. 
 */
class FVTUploadTileAllocator
{
public:
	/** Allocate a tile. Sometimes does an allocation of the backing CPU/GPU block of memory. */
	uint32 Allocate(FRHICommandList& RHICmdList, EPixelFormat InFormat, uint32 InTileSize);
	/** Free a tile. Sometimes does a free of the backing CPU/GPU block of memory. */
	void Free(FRHICommandList& RHICmdList, uint32 InHandle);

	/** Get upload buffer description from handle. */
	FVTUploadTileBuffer GetBufferFromHandle(uint32 InHandle) const;
	/** Get upload buffer extended description from handle. */
	FVTUploadTileBufferExt GetBufferFromHandleExt(uint32 InHandle) const;
	
	/** Get allocated memory in bytes. */
	uint32 TotalAllocatedBytes() const { return NumAllocatedBytes; }

private:
	/** Handle for an allocated tile used by the allocation system. */
	union FHandle
	{
		uint32 PackedValue = 0u;
		struct
		{
			uint32 FormatIndex : 8;
			uint32 StagingBufferIndex : 8;
			uint32 TileIndex : 16;
		};
	};

	/**
	 * Backing memory for buffers used by the streaming/transcoding to write texture data.
	 * The memory is split into equal sized tiles for multiple upload tasks.
	 * Backing memory can be either CPU heap memory or a locked GPU memory buffer depending on the platform.
	 */
	struct FStagingBuffer
	{
		~FStagingBuffer();

		void Init(FRHICommandList& RHICmdList, uint32 InBufferStrideBytes, uint32 InTileSizeBytes);
		void Release(FRHICommandList* RHICmdList);

		/** GPU buffer if used on platform. */
		TRefCountPtr<FRHIBuffer> RHIBuffer;
		/** Memory pointer to locked GPU buffer if used on platform, or to allocated CPU heap memory if not. */
		void* Memory = nullptr;
		uint32 TileSize = 0u;
		uint32 TileSizeAligned = 0u;
		uint32 NumTiles = 0u;
		/** List of tile indices that haven't been allocated. */
		TArray<uint16> TileFreeList;
	};

	/** Container for multiple staging buffers. */
	struct FSharedFormatBuffers
	{
		TArray<FStagingBuffer> StagingBuffers;
	};

	/**
	 * Description of values that affect staging buffer creation.
	 * Multiple virtual texture pools may map onto the same description and so can share staging buffer memory.
	 */
	struct FSharedFormatDesc
	{
		uint32 BlockBytes = 0u;
		uint32 Stride = 0u;
		uint32 MemorySize = 0u;
	};

	/** Array of all discovered format descriptions. */
	TArray<FSharedFormatDesc> FormatDescs;
	/** Array of staging buffers. Kept in sync with associated formats from FormatDescs. */
	TArray<FSharedFormatBuffers> FormatBuffers;
	/** Allocated memory counter. */
	uint32 NumAllocatedBytes = 0;
};

/** 
 * Finalizer implementation for uploading virtual textures.
 * Handles management of upload buffers and copying streamed data to the GPU physical texture.
 */
class FVirtualTextureUploadCache : public IVirtualTextureFinalizer, public FRenderResource
{
public:
	//~ Begin FRenderResource Interface.
	virtual void ReleaseRHI() override;
	//~ End FRenderResource Interface.

	/** Get a staging upload buffer for streaming texture data into. */
	FVTUploadTileHandle PrepareTileForUpload(FRHICommandList& RHICmdList, FVTUploadTileBuffer& OutBuffer, EPixelFormat InFormat, uint32 InTileSize);
	/** 
	 * Mark streamed upload data ready for upload to to the physical virtual texture.
	 * Depending on the platform the upload might happen here, or be deferred to the Finalize() call.
	 */
	void SubmitTile(FRHICommandList& RHICmdList, const FVTUploadTileHandle& InHandle, FRHITexture2D* InDestTexture, int InDestX, int InDestY, int InSkipBorderSize);
	/** Cancel a tile that was already in flight. */
	void CancelTile(FRHICommandList& RHICmdList, const FVTUploadTileHandle& InHandle);

	//~ Begin IVirtualTextureFinalizer Interface.
	virtual void Finalize(FRDGBuilder& GraphBuilder) override;
	//~ End IVirtualTextureFinalizer Interface.

	/** Call on a tick to recycle submitted staging buffers. */
	void UpdateFreeList(FRHICommandList& RHICmdList, bool bForceFreeAll = false);

	/** Returns true if underlying allocator within the budget set by r.VT.MaxUploadMemory.  */
	uint32 IsInMemoryBudget() const;

private:
	int32 GetOrCreatePoolIndex(EPixelFormat InFormat, uint32 InTileSize);

	/** Description of a single allocated tile. Carries mutable state as tile moves from uploading to submitting to pending delete. */
	struct FTileEntry
	{
		uint32 PoolIndex = 0;
		uint32 TileHandle = 0u;
		uint32 FrameSubmitted = 0u;
		FRHITexture2D* RHISubmitTexture = nullptr;
		int32 SubmitDestX = 0;
		int32 SubmitDestY = 0;
		int32 SubmitSkipBorderSize = 0;
	};

	/** Staging texture used for tile upload. Only used on platforms that don't have faster upload methods. */
	struct FStagingTexture
	{
		TRefCountPtr<FRHITexture2D> RHITexture;
		uint32 WidthInTiles = 0u;
		uint32 BatchCapacity = 0u;
		bool bIsCPUWritable;
	};

	/** State for a single pool. A pool covers all virtual textures of the same format and tile size. */
	struct FPoolEntry
	{
		EPixelFormat Format = PF_Unknown;
		uint32 TileSize = 0u;

		static const uint32 NUM_STAGING_TEXTURES = 3u;
		FStagingTexture StagingTexture[NUM_STAGING_TEXTURES];
		uint32 BatchTextureIndex = 0u;

		TArray<FTileEntry> PendingSubmit;
	};

	TArray<FPoolEntry> Pools;
	FVTUploadTileAllocator TileAllocator;
	TSparseArray<FTileEntry> PendingUpload;
	TSparseArray<FTileEntry> PendingRelease;
	TArray<FRHITexture*> UpdatedTextures;
};
