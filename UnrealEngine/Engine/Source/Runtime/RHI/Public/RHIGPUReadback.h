// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
  RHIGPUReadback.h: classes for managing fences and staging buffers for
  asynchronous GPU memory updates and readbacks with minimal stalls and no
  RHI thread flushes
=============================================================================*/

#pragma once

#include "RHI.h"
#include "DynamicRHI.h"
#include "RHICommandList.h"

/**
 * FRHIGPUMemoryReadback: Represents a memory readback request scheduled with CopyToStagingBuffer
 * Wraps a staging buffer with a FRHIGPUFence for synchronization.
 */
class FRHIGPUMemoryReadback
{
public:

	FRHIGPUMemoryReadback(FName RequestName)
	{
		Fence = RHICreateGPUFence(RequestName);
		LastLockGPUIndex = 0;
	}

	virtual ~FRHIGPUMemoryReadback() {}

	/** Indicates if the data is in place and ready to be read. */
	FORCEINLINE bool IsReady()
	{
		return !Fence || (Fence->NumPendingWriteCommands.GetValue() == 0 && Fence->Poll());
	}

	/** Indicates if the data is in place and ready to be read on a subset of GPUs. */
	FORCEINLINE bool IsReady(FRHIGPUMask GPUMask)
	{
		return !Fence || Fence->Poll(GPUMask);
	}

	/**
	 * Copy the current state of the resource to the readback data.
	 * @param RHICmdList The command list to enqueue the copy request on.
	 * @param SourceBuffer The buffer holding the source data.
	 * @param NumBytes The number of bytes to copy. If 0, this will copy the entire buffer.
	 */
	virtual void EnqueueCopy(FRHICommandList& RHICmdList, FRHIBuffer* SourceBuffer, uint32 NumBytes = 0)
	{
		unimplemented();
	}

	virtual void EnqueueCopy(FRHICommandList& RHICmdList, FRHITexture* SourceTexture, const FIntVector& SourcePosition, uint32 SourceSlice, const FIntVector& Size)
	{
		unimplemented();
	}

	void EnqueueCopy(FRHICommandList& RHICmdList, FRHITexture* SourceTexture, FResolveRect Rect = FResolveRect())
	{
		FIntVector SourcePosition, Size;

		if (Rect.IsValid())
		{
			SourcePosition = FIntVector(Rect.X1, Rect.Y1, 0);
			Size = FIntVector(Rect.X2 - Rect.X1, Rect.Y2 - Rect.Y1, 1);
		}
		else
		{
			SourcePosition = FIntVector::ZeroValue;
			Size = FIntVector::ZeroValue;
		}
		
		EnqueueCopy(RHICmdList, SourceTexture, SourcePosition, 0, Size);
	}

	/**
	 * Returns the CPU accessible pointer that backs this staging buffer.
	 * @param NumBytes The maximum number of bytes the host will read from this pointer.
	 * @returns A CPU accessible pointer to the backing buffer.
	 */
	virtual void* Lock(uint32 NumBytes) = 0;

	/**
	 * Signals that the host is finished reading from the backing buffer.
	 */
	virtual void Unlock() = 0;

	FORCEINLINE const FRHIGPUMask& GetLastCopyGPUMask() const { return LastCopyGPUMask; }

	FName GetName() const { return Fence->GetFName(); }

protected:

	FGPUFenceRHIRef Fence;
	FRHIGPUMask LastCopyGPUMask;

	// We need to separately track which GPU buffer was last locked.  It's possible for a new copy operation to
	// be enqueued (writing to LastCopyGPUMask) while the buffer is technically locked, with the unlock and enqueued
	// copy on the GPU itself happening later, during pass execution in FRDGBuilder::Execute (for example, this
	// happens with Nanite streaming).  It's not unsafe, because the operations are occurring in order on both the
	// render thread and later pass Execute, but our locking logic needs to handle that scenario.
	uint32 LastLockGPUIndex;
};

/** Buffer readback implementation. */
class FRHIGPUBufferReadback final : public FRHIGPUMemoryReadback
{
public:

	RHI_API FRHIGPUBufferReadback(FName RequestName);
	 
	RHI_API void EnqueueCopy(FRHICommandList& RHICmdList, FRHIBuffer* SourceBuffer, uint32 NumBytes = 0) override;
	RHI_API void* Lock(uint32 NumBytes) override;
	RHI_API void Unlock() override;
	RHI_API uint64 GetGPUSizeBytes() const;

private:

	// RHI staging buffers are single GPU -- need to be branched when using multiple GPUs
#if WITH_MGPU
	FStagingBufferRHIRef DestinationStagingBuffers[MAX_NUM_GPUS];
#else
	FStagingBufferRHIRef DestinationStagingBuffers[1];
#endif
};


/** Texture readback implementation. */
class FRHIGPUTextureReadback final : public FRHIGPUMemoryReadback
{
public:
	RHI_API FRHIGPUTextureReadback(FName RequestName);

	using FRHIGPUMemoryReadback::EnqueueCopy;

	RHI_API void EnqueueCopy(FRHICommandList& RHICmdList, FRHITexture* SourceTexture, const FIntVector& SourcePosition, uint32 SourceSlice, const FIntVector& Size) override;

	UE_DEPRECATED(5.0, "Use FRHIGPUTextureReadback::Lock( int32& OutRowPitchInPixels) instead.")
	RHI_API void* Lock(uint32 NumBytes) override;

	RHI_API void* Lock(int32& OutRowPitchInPixels, int32* OutBufferHeight = nullptr);
	RHI_API void Unlock() override;

	UE_DEPRECATED(5.0, "Use FRHIGPUTextureReadback::Lock( int32& OutRowPitchInPixels) instead.")
	RHI_API void LockTexture(FRHICommandListImmediate& RHICmdList, void*& OutBufferPtr, int32& OutRowPitchInPixels);

	RHI_API uint64 GetGPUSizeBytes() const;

#if WITH_MGPU
	FTextureRHIRef DestinationStagingTextures[MAX_NUM_GPUS];
#else
	FTextureRHIRef DestinationStagingTextures[1];
#endif
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_3
#include "CoreMinimal.h"
#endif
