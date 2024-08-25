// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualTextureUploadCache.h"

#include "RenderGraphBuilder.h"
#include "RenderUtils.h"
#include "VirtualTextureChunkManager.h"
#include "Stats/StatsTrace.h"

// Allow uploading CPU buffer directly to GPU texture
// This is slow under D3D11
// Should be pretty decent on D3D12X...UpdateTexture does make an extra copy of the data, but Lock/Unlock texture also buffers an extra copy of texture on this platform
// Might also be worth enabling this path on PC D3D12, need to measure
#if !defined(ALLOW_UPDATE_TEXTURE)
	#define ALLOW_UPDATE_TEXTURE 0
#endif

DECLARE_MEMORY_STAT_POOL(TEXT("Total GPU Upload Memory"), STAT_TotalGPUUploadSize, STATGROUP_VirtualTextureMemory, FPlatformMemory::MCR_GPU);
DECLARE_MEMORY_STAT(TEXT("Total CPU Upload Memory"), STAT_TotalCPUUploadSize, STATGROUP_VirtualTextureMemory);

static TAutoConsoleVariable<int32> CVarVTUploadMemoryPageSize(
	TEXT("r.VT.UploadMemoryPageSize"),
	4,
	TEXT("Size in MB for a single page of virtual texture upload memory."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarVTMaxUploadMemory(
	TEXT("r.VT.MaxUploadMemory"),
	64,
	TEXT("Maximum amount of upload memory to allocate in MB before throttling virtual texture streaming requests.\n")
	TEXT("We never throttle high priority requests so allocation can peak above this value."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMaxUploadRequests(
	TEXT("r.VT.MaxUploadRequests"),
	2000,
	TEXT("Maximum number of virtual texture tile upload requests that can be in flight."),
	ECVF_RenderThreadSafe);


uint32 FVTUploadTileAllocator::Allocate(FRHICommandList& RHICmdList, EPixelFormat InFormat, uint32 InTileSize)
{
	// Find matching FormatBuffer.
	const FPixelFormatInfo& FormatInfo = GPixelFormats[InFormat];
	const uint32 TileWidthInBlocks = FMath::DivideAndRoundUp(InTileSize, (uint32)FormatInfo.BlockSizeX);
	const uint32 TileHeightInBlocks = FMath::DivideAndRoundUp(InTileSize, (uint32)FormatInfo.BlockSizeY);

	FSharedFormatDesc Desc;
	Desc.BlockBytes = FormatInfo.BlockBytes;
	Desc.Stride = TileWidthInBlocks * FormatInfo.BlockBytes;
	Desc.MemorySize = TileWidthInBlocks * TileHeightInBlocks * FormatInfo.BlockBytes;

	int32 FormatIndex = 0;
	for (; FormatIndex < FormatDescs.Num(); ++FormatIndex)
	{
		if (Desc.BlockBytes == FormatDescs[FormatIndex].BlockBytes && Desc.Stride == FormatDescs[FormatIndex].Stride && Desc.MemorySize == FormatDescs[FormatIndex].MemorySize)
		{
			break;
		}
	}

	if (FormatIndex == FormatDescs.Num())
	{
		// Add newly found format.
		FormatDescs.Add(Desc);
		FormatBuffers.AddDefaulted();
	}

	FSharedFormatBuffers& FormatBuffer = FormatBuffers[FormatIndex];

	// Find available staging buffer.
	int32 StagingBufferIndex = 0;
	for (; StagingBufferIndex < FormatBuffer.StagingBuffers.Num(); ++StagingBufferIndex)
	{
		if (FormatBuffer.StagingBuffers[StagingBufferIndex].Memory == nullptr)
		{
			// Staging buffer was released so we can re-init and use it.
			break;
		}

		if (FormatBuffer.StagingBuffers[StagingBufferIndex].TileFreeList.Num())
		{
			// Staging buffer has free tiles available.
			break;
		}
	}

	if (StagingBufferIndex == FormatBuffer.StagingBuffers.Num())
	{
		// Current staging buffers are full. Need to allocate a new staging buffer.
		FormatBuffer.StagingBuffers.AddDefaulted();
	}

	FStagingBuffer& StagingBuffer = FormatBuffer.StagingBuffers[StagingBufferIndex];
	if (StagingBuffer.Memory == nullptr)
	{
		// Staging buffer needs underlying buffer allocating.
		StagingBuffer.Init(RHICmdList, Desc.BlockBytes, Desc.MemorySize);
		NumAllocatedBytes += StagingBuffer.TileSizeAligned * StagingBuffer.NumTiles;
	}

	// Pop a free tile and return handle.
	int32 TileIndex = StagingBuffer.TileFreeList.Pop(EAllowShrinking::No);

	FHandle Handle;
	Handle.FormatIndex = FormatIndex;
	Handle.StagingBufferIndex = StagingBufferIndex;
	Handle.TileIndex = TileIndex;
	return Handle.PackedValue;
}

void FVTUploadTileAllocator::Free(FRHICommandList& RHICmdList, uint32 InHandle)
{
	FHandle Handle;
	Handle.PackedValue = InHandle;

	// Push tile back onto free list.
	FStagingBuffer& StagingBuffer = FormatBuffers[Handle.FormatIndex].StagingBuffers[Handle.StagingBufferIndex];
	StagingBuffer.TileFreeList.Push(Handle.TileIndex);

	if (StagingBuffer.NumTiles == StagingBuffer.TileFreeList.Num())
	{
		// All tiles are free, so release the underlying memory.
		check(NumAllocatedBytes >= StagingBuffer.TileSizeAligned * StagingBuffer.NumTiles);
		NumAllocatedBytes -= StagingBuffer.TileSizeAligned * StagingBuffer.NumTiles;

		StagingBuffer.Release(&RHICmdList);
	}
}

FVTUploadTileBuffer FVTUploadTileAllocator::GetBufferFromHandle(uint32 InHandle) const
{
	FHandle Handle;
	Handle.PackedValue = InHandle;

	FSharedFormatDesc const& FormatDesc = FormatDescs[Handle.FormatIndex];
	FStagingBuffer const& StagingBuffer = FormatBuffers[Handle.FormatIndex].StagingBuffers[Handle.StagingBufferIndex];

	FVTUploadTileBuffer Buffer;
	Buffer.Memory = (uint8*)StagingBuffer.Memory + StagingBuffer.TileSizeAligned * Handle.TileIndex;
	Buffer.MemorySize = StagingBuffer.TileSize;
	Buffer.Stride = FormatDesc.Stride;
	return Buffer;
}

FVTUploadTileBufferExt FVTUploadTileAllocator::GetBufferFromHandleExt(uint32 InHandle) const
{
	FHandle Handle;
	Handle.PackedValue = InHandle;

	FSharedFormatDesc const& FormatDesc = FormatDescs[Handle.FormatIndex];
	FStagingBuffer const& StagingBuffer = FormatBuffers[Handle.FormatIndex].StagingBuffers[Handle.StagingBufferIndex];

	FVTUploadTileBufferExt Buffer;
	Buffer.RHIBuffer = StagingBuffer.RHIBuffer;
	Buffer.BufferMemory = StagingBuffer.Memory;
	Buffer.BufferOffset = StagingBuffer.TileSizeAligned * Handle.TileIndex;
	Buffer.Stride = FormatDesc.Stride;
	return Buffer;
}

FVTUploadTileAllocator::FStagingBuffer::~FStagingBuffer()
{
	Release(nullptr);
}

void FVTUploadTileAllocator::FStagingBuffer::Init(FRHICommandList& RHICmdList, uint32 InBufferStrideBytes, uint32 InTileSizeBytes)
{
	TileSize = InTileSizeBytes;
	TileSizeAligned = Align(InTileSizeBytes, 128u);

	const uint32 RequestedBufferSize = CVarVTUploadMemoryPageSize.GetValueOnRenderThread() * 1024 * 1024;
	NumTiles = FMath::DivideAndRoundUp(RequestedBufferSize, TileSizeAligned);
	const uint32 BufferSize = TileSizeAligned * NumTiles;

	check(TileFreeList.Num() == 0);
	TileFreeList.AddUninitialized(NumTiles);
	for (uint32 Index = 0; Index < NumTiles; ++Index)
	{
		TileFreeList[Index] = NumTiles - Index - 1;
	}

	if (GRHISupportsDirectGPUMemoryLock && GRHISupportsUpdateFromBufferTexture)
	{
		// Allocate staging buffer directly in GPU memory.
		FRHIResourceCreateInfo CreateInfo(TEXT("StagingBuffer"));
		RHIBuffer = RHICmdList.CreateBuffer(BufferSize, BUF_ShaderResource | BUF_Static | BUF_KeepCPUAccessible | BUF_StructuredBuffer, InBufferStrideBytes, ERHIAccess::SRVMask, CreateInfo);

		// Here we bypass 'normal' RHI operations in order to get a persistent pointer to GPU memory, on supported platforms
		// This should be encapsulated into a proper RHI method at some point
		Memory = RHICmdList.LockBuffer(RHIBuffer, 0u, BufferSize, RLM_WriteOnly_NoOverwrite);

		INC_MEMORY_STAT_BY(STAT_TotalGPUUploadSize, BufferSize);
	}
	else
	{
		// Allocate staging buffer in CPU memory.
		Memory = FMemory::Malloc(BufferSize, 128u);

		INC_MEMORY_STAT_BY(STAT_TotalCPUUploadSize, BufferSize);
	}
}

void FVTUploadTileAllocator::FStagingBuffer::Release(FRHICommandList* RHICmdList)
{
	const uint32 BufferSize = TileSizeAligned * NumTiles;

	if (RHIBuffer.IsValid())
	{
		check(RHICmdList);

		// Unmap and release the GPU buffer if present.
		RHICmdList->UnlockBuffer(RHIBuffer);
		RHIBuffer.SafeRelease();
		// In this case 'Memory' was the mapped pointer, so release it.
		Memory = nullptr;

		DEC_MEMORY_STAT_BY(STAT_TotalGPUUploadSize, BufferSize);
	}

	if (Memory != nullptr)
	{
		// If we still have 'Memory' pointer, it is CPU, release it now.
		FMemory::Free(Memory);
		Memory = nullptr;

		DEC_MEMORY_STAT_BY(STAT_TotalCPUUploadSize, BufferSize);
	}

	TileSize = 0u;
	NumTiles = 0u;
	TileFreeList.Reset();
}

int32 FVirtualTextureUploadCache::GetOrCreatePoolIndex(EPixelFormat InFormat, uint32 InTileSize)
{
	for (int32 i = 0; i < Pools.Num(); ++i)
	{
		const FPoolEntry& Entry = Pools[i];
		if (Entry.Format == InFormat && Entry.TileSize == InTileSize)
		{
			return i;
		}
	}

	const int32 PoolIndex = Pools.AddDefaulted();
	FPoolEntry& Entry = Pools[PoolIndex];
	Entry.Format = InFormat;
	Entry.TileSize = InTileSize;

	return PoolIndex;
}

void FVirtualTextureUploadCache::Finalize(FRDGBuilder& GraphBuilder)
{
	FRHICommandListImmediate& RHICmdList = GraphBuilder.RHICmdList;
	SCOPED_DRAW_EVENT(RHICmdList, FVirtualTextureUploadCache_Finalize);
	SCOPE_CYCLE_COUNTER(STAT_VTP_FlushUpload)

	// Multi-GPU support:
	SCOPED_GPU_MASK(RHICmdList, FRHIGPUMask::All());

	for (int PoolIndex = 0; PoolIndex < Pools.Num(); ++PoolIndex)
	{
		FPoolEntry& PoolEntry = Pools[PoolIndex];
		const uint32 BatchCount = PoolEntry.PendingSubmit.Num();
		if (BatchCount == 0)
		{
			continue;
		}

		const FPixelFormatInfo& FormatInfo = GPixelFormats[PoolEntry.Format];
		const uint32 TileSize = PoolEntry.TileSize;
		const uint32 BlockBytes = FormatInfo.BlockBytes;
		const uint32 TileWidthInBlocks = FMath::DivideAndRoundUp(TileSize, (uint32)FormatInfo.BlockSizeX);
		const uint32 TileHeightInBlocks = FMath::DivideAndRoundUp(TileSize, (uint32)FormatInfo.BlockSizeY);

		const uint32 TextureIndex = PoolEntry.BatchTextureIndex;
		PoolEntry.BatchTextureIndex = (PoolEntry.BatchTextureIndex + 1u) % FPoolEntry::NUM_STAGING_TEXTURES;
		FStagingTexture& StagingTexture = PoolEntry.StagingTexture[TextureIndex];

		// On some platforms the staging texture create/lock behavior will depend on whether we are running with RHI threading
		const bool bIsCpuWritable = !IsRunningRHIInSeparateThread();

		if (BatchCount > StagingTexture.BatchCapacity || BatchCount * 2 <= StagingTexture.BatchCapacity || bIsCpuWritable != StagingTexture.bIsCPUWritable)
		{
			// Staging texture is vertical stacked in widths of multiples of 4
			// Smaller widths mean smaller stride which is more efficient for copying
			// Round up to 4 to reduce likely wasted memory from width not aligning to whatever GPU prefers
			const uint32 MaxTextureDimension = GetMax2DTextureDimension();
			const uint32 MaxSizeInTiles = FMath::DivideAndRoundDown(MaxTextureDimension, TileSize);
			const uint32 MaxCapacity = MaxSizeInTiles * MaxSizeInTiles;
			check(BatchCount <= MaxCapacity);
			const uint32 WidthInTiles = FMath::DivideAndRoundUp(FMath::DivideAndRoundUp(BatchCount, MaxSizeInTiles), 4u) * 4;
			check(WidthInTiles > 0u);
			const uint32 HeightInTiles = FMath::DivideAndRoundUp(BatchCount, WidthInTiles);
			check(HeightInTiles > 0u);

			if (StagingTexture.RHITexture)
			{
				DEC_MEMORY_STAT_BY(STAT_TotalGPUUploadSize, CalcTextureSize(StagingTexture.RHITexture->GetSizeX(), StagingTexture.RHITexture->GetSizeY(), PoolEntry.Format, 1u));
			}

			FRHITextureCreateDesc Desc =
				FRHITextureCreateDesc::Create2D(TEXT("FVirtualTextureUploadCache_StagingTexture"), TileSize * WidthInTiles, TileSize * HeightInTiles, PoolEntry.Format);

			if (bIsCpuWritable)
			{
				Desc.AddFlags(ETextureCreateFlags::CPUWritable);
			}

			StagingTexture.RHITexture = RHICreateTexture(Desc);

			StagingTexture.WidthInTiles = WidthInTiles;
			StagingTexture.BatchCapacity = WidthInTiles * HeightInTiles;
			StagingTexture.bIsCPUWritable = bIsCpuWritable;
			INC_MEMORY_STAT_BY(STAT_TotalGPUUploadSize, CalcTextureSize(TileSize * WidthInTiles, TileSize * HeightInTiles, PoolEntry.Format, 1u));
		}

		uint32 BatchStride = 0u;
		void* BatchMemory = RHICmdList.LockTexture2D(StagingTexture.RHITexture, 0, RLM_WriteOnly, BatchStride, false, false);

		// Copy all tiles to the staging texture
		for (uint32 Index = 0u; Index < BatchCount; ++Index)
		{
			const FTileEntry& Entry = PoolEntry.PendingSubmit[Index];
			const uint32_t SrcTileX = Index % StagingTexture.WidthInTiles;
			const uint32_t SrcTileY = Index / StagingTexture.WidthInTiles;

			const FVTUploadTileBufferExt UploadBuffer = TileAllocator.GetBufferFromHandleExt(Entry.TileHandle);

			uint8* BatchDst = (uint8*)BatchMemory + TileHeightInBlocks * SrcTileY * BatchStride + TileWidthInBlocks * SrcTileX * BlockBytes;
			for (uint32 y = 0u; y < TileHeightInBlocks; ++y)
			{
				FMemory::Memcpy(
					BatchDst + y * BatchStride,
					(uint8*)UploadBuffer.BufferMemory + UploadBuffer.BufferOffset + y * UploadBuffer.Stride,
					TileWidthInBlocks * BlockBytes);
			}

			// Can release upload buffer.
			TileAllocator.Free(RHICmdList, Entry.TileHandle);
		}

		RHICmdList.UnlockTexture2D(StagingTexture.RHITexture, 0u, false, false);
		RHICmdList.Transition(FRHITransitionInfo(StagingTexture.RHITexture, ERHIAccess::SRVMask, ERHIAccess::CopySrc));

		// Upload each tile from staging texture to physical texture
		for (uint32 Index = 0u; Index < BatchCount; ++Index)
		{
			const FTileEntry& Entry = PoolEntry.PendingSubmit[Index];
			const uint32_t SrcTileX = Index % StagingTexture.WidthInTiles;
			const uint32_t SrcTileY = Index / StagingTexture.WidthInTiles;

			const uint32 SkipBorderSize = Entry.SubmitSkipBorderSize;
			const uint32 SubmitTileSize = TileSize - SkipBorderSize * 2;
			const FIntVector SourceBoxStart(SrcTileX * TileSize + SkipBorderSize, SrcTileY * TileSize + SkipBorderSize, 0);
			const FIntVector DestinationBoxStart(Entry.SubmitDestX * SubmitTileSize, Entry.SubmitDestY * SubmitTileSize, 0);

			if (!UpdatedTextures.Contains(Entry.RHISubmitTexture))
			{
				RHICmdList.Transition(FRHITransitionInfo(Entry.RHISubmitTexture, ERHIAccess::Unknown, ERHIAccess::CopyDest));
				UpdatedTextures.Add(Entry.RHISubmitTexture);
			}

			FRHICopyTextureInfo CopyInfo;
			CopyInfo.Size = FIntVector(SubmitTileSize, SubmitTileSize, 1);
			CopyInfo.SourcePosition = SourceBoxStart;
			CopyInfo.DestPosition = DestinationBoxStart;
			RHICmdList.CopyTexture(StagingTexture.RHITexture, Entry.RHISubmitTexture, CopyInfo);
		}

		RHICmdList.Transition(FRHITransitionInfo(StagingTexture.RHITexture, ERHIAccess::CopySrc, ERHIAccess::SRVMask));

		PoolEntry.PendingSubmit.Reset();
	}

	// Transition all updates textures back to SRV
	TArray<FRHITransitionInfo, SceneRenderingAllocator> SRVTransitions;
	SRVTransitions.Reserve(UpdatedTextures.Num());
	for (int32 Index = 0; Index < UpdatedTextures.Num(); ++Index)
	{
		SRVTransitions.Add(FRHITransitionInfo(UpdatedTextures[Index], ERHIAccess::CopyDest, ERHIAccess::SRVMask));
	}
	RHICmdList.Transition(SRVTransitions);
	UpdatedTextures.Reset();
}

void FVirtualTextureUploadCache::ReleaseRHI()
{
	check(IsInRenderingThread());
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

	// Complete/Cancel all work will release allocated staging buffers.
	UpdateFreeList(RHICmdList, true);
	for (TSparseArray<FTileEntry>::TIterator It(PendingUpload); It; ++It)
	{
		CancelTile(RHICmdList, FVTUploadTileHandle(It.GetIndex()));
	}
	// Release staging textures.
	Pools.Empty();
}

FVTUploadTileHandle FVirtualTextureUploadCache::PrepareTileForUpload(FRHICommandList& RHICmdList, FVTUploadTileBuffer& OutBuffer, EPixelFormat InFormat, uint32 InTileSize)
{
	SCOPE_CYCLE_COUNTER(STAT_VTP_StageTile)

	uint32 TileHandle = TileAllocator.Allocate(RHICmdList, InFormat, InTileSize);
	OutBuffer = TileAllocator.GetBufferFromHandle(TileHandle);

	const int32 PoolIndex = GetOrCreatePoolIndex(InFormat, InTileSize);
	const FPoolEntry& PoolEntry = Pools[PoolIndex];
	
	FTileEntry Tile;
	Tile.PoolIndex = PoolIndex;
	Tile.TileHandle = TileHandle;

	uint32 Index = PendingUpload.Emplace(Tile);
	return FVTUploadTileHandle(Index);
}

void FVirtualTextureUploadCache::SubmitTile(FRHICommandList& RHICmdList, const FVTUploadTileHandle& InHandle, FRHITexture2D* InDestTexture, int InDestX, int InDestY, int InSkipBorderSize)
{
	checkSlow(IsInParallelRenderingThread());

	check(PendingUpload.IsValidIndex(InHandle.Index));
	FTileEntry& Entry = PendingUpload[InHandle.Index];
	Entry.FrameSubmitted = GFrameNumberRenderThread;

	FPoolEntry& PoolEntry = Pools[Entry.PoolIndex];
	const uint32 TileSize = PoolEntry.TileSize - InSkipBorderSize * 2;

	const FVTUploadTileBufferExt UploadBuffer = TileAllocator.GetBufferFromHandleExt(Entry.TileHandle);
	if (UploadBuffer.RHIBuffer != nullptr)
	{
		if (!UpdatedTextures.Contains(InDestTexture))
		{
			RHICmdList.Transition(FRHITransitionInfo(InDestTexture, ERHIAccess::Unknown, ERHIAccess::CopyDest));
			UpdatedTextures.Add(InDestTexture);
		}

		check(GRHISupportsUpdateFromBufferTexture);
		const FUpdateTextureRegion2D UpdateRegion(InDestX * TileSize, InDestY * TileSize, InSkipBorderSize, InSkipBorderSize, TileSize, TileSize);
		RHICmdList.UpdateFromBufferTexture2D(InDestTexture, 0u, UpdateRegion, UploadBuffer.Stride, UploadBuffer.RHIBuffer, UploadBuffer.BufferOffset);

		// Move to pending list, so we won't re-use this buffer until the GPU has finished the copy.
		// We're using persist mapped buffer here, so this is the only synchronization method in place...without this delay we'd get corrupt textures.
		PendingRelease.Emplace(Entry);
	}
	else if(ALLOW_UPDATE_TEXTURE)
	{
		if (!UpdatedTextures.Contains(InDestTexture))
		{
			RHICmdList.Transition(FRHITransitionInfo(InDestTexture, ERHIAccess::Unknown, ERHIAccess::CopyDest));
			UpdatedTextures.Add(InDestTexture);
		}

		const FUpdateTextureRegion2D UpdateRegion(InDestX * TileSize, InDestY * TileSize, InSkipBorderSize, InSkipBorderSize, TileSize, TileSize);
		RHICmdList.UpdateTexture2D(InDestTexture, 0u, UpdateRegion, UploadBuffer.Stride, (uint8*)UploadBuffer.BufferMemory + UploadBuffer.BufferOffset);

		// UpdateTexture2D makes internal copy of data, no need to wait before re-using tile
		TileAllocator.Free(RHICmdList, Entry.TileHandle);
	}
	else
	{
		// Move to list of batched updates for the current pool
		Entry.RHISubmitTexture = InDestTexture;
		Entry.SubmitDestX = InDestX;
		Entry.SubmitDestY = InDestY;
		Entry.SubmitSkipBorderSize = InSkipBorderSize;
		PoolEntry.PendingSubmit.Emplace(Entry);
	}

	// Remove from pending uploads.
	PendingUpload.RemoveAt(InHandle.Index);
}

void FVirtualTextureUploadCache::CancelTile(FRHICommandList& RHICmdList, const FVTUploadTileHandle& InHandle)
{
	check(PendingUpload.IsValidIndex(InHandle.Index));
	FTileEntry& Entry = PendingUpload[InHandle.Index];
	TileAllocator.Free(RHICmdList, Entry.TileHandle);
	PendingUpload.RemoveAt(InHandle.Index);
}

void FVirtualTextureUploadCache::UpdateFreeList(FRHICommandList& RHICmdList, bool bForceFreeAll)
{
	const uint32 CurrentFrame = GFrameNumberRenderThread;

	// Iterate tiles pending release and free them if they are old enough.
	for (TSparseArray<FTileEntry>::TIterator It(PendingRelease); It; ++It)
	{
		check(CurrentFrame >= It->FrameSubmitted);
		const uint32 FramesSinceSubmitted = CurrentFrame - It->FrameSubmitted;
		if (FramesSinceSubmitted < 2u)
		{
			break;
		}

		TileAllocator.Free(RHICmdList, It->TileHandle);
		It.RemoveCurrent();
	}
}

uint32 FVirtualTextureUploadCache::IsInMemoryBudget() const
{
	return 
		PendingUpload.Num() + PendingRelease.Num() <= CVarMaxUploadRequests.GetValueOnRenderThread() &&
		TileAllocator.TotalAllocatedBytes() <= (uint32)CVarVTMaxUploadMemory.GetValueOnRenderThread() * 1024u * 1024u;
}
