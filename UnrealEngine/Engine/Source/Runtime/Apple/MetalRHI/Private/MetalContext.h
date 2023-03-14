// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include "MetalViewport.h"
#include "MetalCommandEncoder.h"
#include "MetalCommandQueue.h"
#include "MetalCommandList.h"
#include "MetalRenderPass.h"
#include "MetalBuffer.h"
#include "MetalCaptureManager.h"
#include "MetalFrameAllocator.h"
#if PLATFORM_IOS
#include "IOS/IOSView.h"
#endif
#include "Containers/LockFreeList.h"
#include "device.hpp"

#define NUM_SAFE_FRAMES 4

class FMetalRHICommandContext;
class FMetalPipelineStateCacheManager;
class FMetalQueryBufferPool;
class FMetalRHIBuffer;

class FMetalContext
{
public:
	FMetalContext(mtlpp::Device InDevice, FMetalCommandQueue& Queue, bool const bIsImmediate);
	virtual ~FMetalContext();
	
	mtlpp::Device& GetDevice();
	FMetalCommandQueue& GetCommandQueue();
	FMetalCommandList& GetCommandList();
	mtlpp::CommandBuffer const& GetCurrentCommandBuffer() const;
	mtlpp::CommandBuffer& METALRHI_API GetCurrentCommandBuffer();
	FMetalStateCache& GetCurrentState() { return StateCache; }
	FMetalRenderPass& GetCurrentRenderPass() { return RenderPass; }
	
	void InsertCommandBufferFence(FMetalCommandBufferFence& Fence, mtlpp::CommandBufferHandler Handler = nil);

	/**
	 * Do anything necessary to prepare for any kind of draw call 
	 * @param PrimitiveType The UnrealEngine primitive type for the draw call, needed to compile the correct render pipeline.
	 * @returns True if the preparation completed and the draw call can be encoded, false to skip.
	 */
	bool PrepareToDraw(uint32 PrimitiveType);
	
	/**
	 * Set the color, depth and stencil render targets, and then make the new command buffer/encoder
	 */
	void SetRenderPassInfo(const FRHIRenderPassInfo& RenderTargetsInfo, bool const bRestart = false);
	
	/**
	 * Allocate from a dynamic ring buffer - by default align to the allowed alignment for offset field when setting buffers
	 */
	FMetalBuffer AllocateFromRingBuffer(uint32 Size, uint32 Alignment=0);

	TSharedRef<FMetalQueryBufferPool, ESPMode::ThreadSafe> GetQueryBufferPool()
	{
		return QueryBuffer.ToSharedRef();
	}

    void SubmitCommandsHint(uint32 const bFlags = EMetalSubmitFlagsCreateCommandBuffer);
	void SubmitCommandBufferAndWait();
	void ResetRenderCommandEncoder();
	
	void DrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances);
	
	void DrawPrimitiveIndirect(uint32 PrimitiveType, FMetalVertexBuffer* VertexBuffer, uint32 ArgumentOffset);
	
	void DrawIndexedPrimitive(FMetalBuffer const& IndexBuffer, uint32 IndexStride, mtlpp::IndexType IndexType, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 FirstInstance,
							  uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances);
	
	void DrawIndexedIndirect(FMetalIndexBuffer* IndexBufferRHI, uint32 PrimitiveType, FMetalStructuredBuffer* VertexBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances);
	
	void DrawIndexedPrimitiveIndirect(uint32 PrimitiveType,FMetalIndexBuffer* IndexBufferRHI,FMetalVertexBuffer* VertexBufferRHI,uint32 ArgumentOffset);
	
	void DrawPatches(uint32 PrimitiveType, FMetalBuffer const& IndexBuffer, uint32 IndexBufferStride, int32 BaseVertexIndex, uint32 FirstInstance, uint32 StartIndex,
					 uint32 NumPrimitives, uint32 NumInstances);
	
	void CopyFromTextureToBuffer(FMetalTexture const& Texture, uint32 sourceSlice, uint32 sourceLevel, mtlpp::Origin sourceOrigin, mtlpp::Size sourceSize, FMetalBuffer const& toBuffer, uint32 destinationOffset, uint32 destinationBytesPerRow, uint32 destinationBytesPerImage, mtlpp::BlitOption options);
	
	void CopyFromBufferToTexture(FMetalBuffer const& Buffer, uint32 sourceOffset, uint32 sourceBytesPerRow, uint32 sourceBytesPerImage, mtlpp::Size sourceSize, FMetalTexture const& toTexture, uint32 destinationSlice, uint32 destinationLevel, mtlpp::Origin destinationOrigin, mtlpp::BlitOption options);
	
	void CopyFromTextureToTexture(FMetalTexture const& Texture, uint32 sourceSlice, uint32 sourceLevel, mtlpp::Origin sourceOrigin, mtlpp::Size sourceSize, FMetalTexture const& toTexture, uint32 destinationSlice, uint32 destinationLevel, mtlpp::Origin destinationOrigin);
	
	void CopyFromBufferToBuffer(FMetalBuffer const& SourceBuffer, NSUInteger SourceOffset, FMetalBuffer const& DestinationBuffer, NSUInteger DestinationOffset, NSUInteger Size);
	
    bool AsyncCopyFromBufferToTexture(FMetalBuffer const& Buffer, uint32 sourceOffset, uint32 sourceBytesPerRow, uint32 sourceBytesPerImage, mtlpp::Size sourceSize, FMetalTexture const& toTexture, uint32 destinationSlice, uint32 destinationLevel, mtlpp::Origin destinationOrigin, mtlpp::BlitOption options);
    
    bool AsyncCopyFromTextureToTexture(FMetalTexture const& Texture, uint32 sourceSlice, uint32 sourceLevel, mtlpp::Origin sourceOrigin, mtlpp::Size sourceSize, FMetalTexture const& toTexture, uint32 destinationSlice, uint32 destinationLevel, mtlpp::Origin destinationOrigin);
	
	bool CanAsyncCopyToBuffer(FMetalBuffer const& DestinationBuffer);
	
    void AsyncCopyFromBufferToBuffer(FMetalBuffer const& SourceBuffer, NSUInteger SourceOffset, FMetalBuffer const& DestinationBuffer, NSUInteger DestinationOffset, NSUInteger Size);
	
    void AsyncGenerateMipmapsForTexture(FMetalTexture const& Texture);
    
	void SubmitAsyncCommands(mtlpp::CommandBufferHandler ScheduledHandler, mtlpp::CommandBufferHandler CompletionHandler, bool const bWait);
	
	void SynchronizeTexture(FMetalTexture const& Texture, uint32 Slice, uint32 Level);
	
	void SynchroniseResource(mtlpp::Resource const& Resource);
	
	void FillBuffer(FMetalBuffer const& Buffer, ns::Range Range, uint8 Value);

	void Dispatch(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ);
	void DispatchIndirect(FMetalVertexBuffer* ArgumentBuffer, uint32 ArgumentOffset);

	void StartTiming(class FMetalEventNode* EventNode);
	void EndTiming(class FMetalEventNode* EventNode);

#if ENABLE_METAL_GPUPROFILE
	static void MakeCurrent(FMetalContext* Context);
	static FMetalContext* GetCurrentContext();
#endif
	
	void SetParallelPassFences(FMetalFence* Start, FMetalFence* End);
	TRefCountPtr<FMetalFence> const& GetParallelPassStartFence(void) const;
	TRefCountPtr<FMetalFence> const& GetParallelPassEndFence(void) const;
	
	void InitFrame(bool const bImmediateContext, uint32 Index, uint32 Num);
	void FinishFrame(bool const bImmediateContext);

	// Track Write->Read transitions for TBDR Fragment->Verex fencing
	void TransitionResource(FRHIUnorderedAccessView* InResource);
	void TransitionResource(FRHITexture* InResource);

	template<typename T>
	void TransitionRHIResource(T* InResource);

protected:
	/** The underlying Metal device */
	mtlpp::Device Device;
	
	/** The wrapper around the device command-queue for creating & committing command buffers to */
	FMetalCommandQueue& CommandQueue;
	
	/** The wrapper around commabd buffers for ensuring correct parallel execution order */
	FMetalCommandList CommandList;
	
	/** The cache of all tracked & accessible state. */
	FMetalStateCache StateCache;
	
	/** The render pass handler that actually encodes our commands. */
	FMetalRenderPass RenderPass;
	
	/** A sempahore used to ensure that wait for previous frames to complete if more are in flight than we permit */
	dispatch_semaphore_t CommandBufferSemaphore;
	
	/** A pool of buffers for writing visibility query results. */
	TSharedPtr<FMetalQueryBufferPool, ESPMode::ThreadSafe> QueryBuffer;
	
	/** Initial fence to wait on for parallel contexts */
	TRefCountPtr<FMetalFence> StartFence;
	
	/** Fence to update at the end for parallel contexts */
	TRefCountPtr<FMetalFence> EndFence;
	
#if ENABLE_METAL_GPUPROFILE
	/** the slot to store a per-thread context ref */
	static uint32 CurrentContextTLSSlot;
#endif
	
	/** Total number of parallel contexts that constitute the current pass. */
	int32 NumParallelContextsInPass;
	
	/** Whether the validation layer is enabled */
	bool bValidationEnabled;
};

template<typename T>
void FMetalContext::TransitionRHIResource(T* InResource)
{
	auto Resource = ResourceCast(InResource);
	if (Resource->GetCurrentBufferOrNil())
	{
		RenderPass.TransitionResources(Resource->GetCurrentBuffer());
	}
}

class FMetalDeviceContext : public FMetalContext
{
public:
	static FMetalDeviceContext* CreateDeviceContext();
	virtual ~FMetalDeviceContext();
	
	void Init(void);
	
	inline bool SupportsFeature(EMetalFeatures InFeature) { return CommandQueue.SupportsFeature(InFeature); }
	
	inline FMetalResourceHeap& GetResourceHeap(void) { return Heap; }
	
	FMetalTexture CreateTexture(FMetalSurface* Surface, mtlpp::TextureDescriptor Descriptor);
	FMetalBuffer CreatePooledBuffer(FMetalPooledBufferArgs const& Args);
	void ReleaseBuffer(FMetalBuffer& Buf);
	void ReleaseObject(id Object);
	void ReleaseTexture(FMetalSurface* Surface, FMetalTexture& Texture);
	void ReleaseTexture(FMetalTexture& Texture);
	void ReleaseFence(FMetalFence* Fence);
	
	void BeginFrame();
	void FlushFreeList(bool const bFlushFences = true);
	void ClearFreeList();
	void DrainHeap();
	void EndFrame();
	
	/** RHIBeginScene helper */
	void BeginScene();
	/** RHIEndScene helper */
	void EndScene();
	
	void BeginDrawingViewport(FMetalViewport* Viewport);
	void EndDrawingViewport(FMetalViewport* Viewport, bool bPresent, bool bLockToVsync);
	
	/** Take a parallel FMetalContext from the free-list or allocate a new one if required */
	FMetalRHICommandContext* AcquireContext(int32 NewIndex, int32 NewNum);
	
	/** Release a parallel FMetalContext back into the free-list */
	void ReleaseContext(FMetalRHICommandContext* Context);
	
	/** Returns the number of concurrent contexts encoding commands, including the device context. */
	uint32 GetNumActiveContexts(void) const;

	void BeginParallelRenderCommandEncoding(uint32 Num);
	void SetParallelRenderPassDescriptor(FRHIRenderPassInfo const& TargetInfo);
	mtlpp::RenderCommandEncoder GetParallelRenderCommandEncoder(uint32 Index, mtlpp::ParallelRenderCommandEncoder& ParallelEncoder, mtlpp::CommandBuffer& CommandBuffer);
	void EndParallelRenderCommandEncoding(void);
	
	/** Get the index of the bound Metal device in the global list of rendering devices. */
	uint32 GetDeviceIndex(void) const;
	
	FMetalFrameAllocator* GetTransferAllocator()
	{
		return TransferBufferAllocator;
	}
    
    FMetalFrameAllocator* GetUniformAllocator()
    {
        return UniformBufferAllocator;
    }
    
    uint32 GetFrameNumberRHIThread()
    {
        return FrameNumberRHIThread;
    }
	
	void NewLock(FMetalRHIBuffer* Buffer, FMetalFrameAllocator::AllocationEntry& Allocation);
	FMetalFrameAllocator::AllocationEntry FetchAndRemoveLock(FMetalRHIBuffer* Buffer);
	
#if METAL_DEBUG_OPTIONS
    void AddActiveBuffer(FMetalBuffer const& Buffer);
    void RemoveActiveBuffer(FMetalBuffer const& Buffer);
	bool ValidateIsInactiveBuffer(FMetalBuffer const& Buffer);
	void ScribbleBuffer(FMetalBuffer& Buffer);
#endif
	
private:
	FMetalDeviceContext(mtlpp::Device MetalDevice, uint32 DeviceIndex, FMetalCommandQueue* Queue);
	
private:
	/** The index into the GPU device list for the selected Metal device */
	uint32 DeviceIndex;
	
	/** Dynamic memory heap */
	FMetalResourceHeap Heap;
	
	/** GPU Frame Capture Manager */
	FMetalCaptureManager CaptureManager;
	
	/** Free lists for releasing objects only once it is safe to do so */
	TSet<FMetalBuffer> UsedBuffers;
	TSet<FMetalTexture> UsedTextures;
	TSet<FMetalFence*> UsedFences;
	TLockFreePointerListLIFO<FMetalFence> FenceFreeList;
	TSet<id> ObjectFreeList;
	struct FMetalDelayedFreeList
	{
		bool IsComplete() const;
		TArray<mtlpp::CommandBufferFence> Fences;
		TSet<FMetalBuffer> UsedBuffers;
		TSet<FMetalTexture> UsedTextures;
		TSet<FMetalFence*> FenceFreeList;
		TSet<id> ObjectFreeList;
#if METAL_DEBUG_OPTIONS
		int32 DeferCount;
#endif
	};
	TArray<FMetalDelayedFreeList*> DelayedFreeLists;
	
//	TSet<FMetalUniformBuffer*> UniformBuffers;
    FMetalFrameAllocator* UniformBufferAllocator;
	FMetalFrameAllocator* TransferBufferAllocator;
	
	TMap<FMetalRHIBuffer*, FMetalFrameAllocator::AllocationEntry> OutstandingLocks;
	
#if METAL_DEBUG_OPTIONS
	/** The list of fences for the current frame */
	TArray<FMetalFence*> FrameFences;
    
    FCriticalSection ActiveBuffersMutex;
    
    /** These are the active buffers that cannot be CPU modified */
    TMap<id<MTLBuffer>, TArray<NSRange>> ActiveBuffers;
#endif
	
	/** Free-list of contexts for parallel encoding */
	TLockFreePointerListLIFO<FMetalRHICommandContext> ParallelContexts;
	
	/** Fences for parallel execution */
	TArray<TRefCountPtr<FMetalFence>> ParallelFences;
	
	/** Critical section for FreeList */
	FCriticalSection FreeListMutex;
	
	/** Event for coordinating pausing of render thread to keep inline with the ios display link. */
	FEvent* FrameReadyEvent;
	
	/** Internal frame counter, incremented on each call to RHIBeginScene. */
	uint32 SceneFrameCounter;
	
	/** Internal frame counter, used to ensure that we only drain the buffer pool one after each frame within RHIEndFrame. */
	uint32 FrameCounter;
	
	/** Bitfield of supported Metal features with varying availability depending on OS/device */
	uint32 Features;
	
	/** Count of concurrent contexts encoding commands. */
	int32 ActiveContexts;
	
	/** Count of concurrent parallel contexts encoding commands. */
	int32 ActiveParallelContexts;
	
	/** Whether we presented this frame - only used to track when to introduce debug markers */
	bool bPresented;
	
	/** PSO cache manager */
	FMetalPipelineStateCacheManager* PSOManager;

    /** Thread index owned by the RHI Thread. Monotonically increases every call to EndFrame() */
    uint32 FrameNumberRHIThread;
};

