// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "MetalViewport.h"
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

#define NUM_SAFE_FRAMES 4

class FMetalRHICommandContext;
class FMetalPipelineStateCacheManager;
class FMetalQueryBufferPool;
class FMetalRHIBuffer;
class FMetalBindlessDescriptorManager;

class FMetalContext
{
public:
	FMetalContext(MTL::Device* InDevice, FMetalCommandQueue& Queue);
	virtual ~FMetalContext();
	
	MTL::Device* GetDevice();
	FMetalCommandQueue& GetCommandQueue();
	FMetalCommandList& GetCommandList();
	FMetalCommandBuffer* GetCurrentCommandBuffer();
	FMetalStateCache& GetCurrentState() { return StateCache; }
	FMetalRenderPass& GetCurrentRenderPass() { return RenderPass; }
	
	void InsertCommandBufferFence(TSharedPtr<FMetalCommandBufferFence, ESPMode::ThreadSafe>& Fence, FMetalCommandBufferCompletionHandler Handler);

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
    void EndRenderPass();
	
	/**
	 * Allocate from a dynamic ring buffer - by default align to the allowed alignment for offset field when setting buffers
	 */
	FMetalBufferPtr AllocateFromRingBuffer(uint32 Size, uint32 Alignment=0);

	TSharedRef<FMetalQueryBufferPool, ESPMode::ThreadSafe> GetQueryBufferPool()
	{
		return QueryBuffer.ToSharedRef();
	}

    void SubmitCommandsHint(uint32 const bFlags = EMetalSubmitFlagsCreateCommandBuffer);
	void SubmitCommandBufferAndWait();
	void ResetRenderCommandEncoder();
	
	void DrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances);
	
	void DrawPrimitiveIndirect(uint32 PrimitiveType, FMetalRHIBuffer* VertexBuffer, uint32 ArgumentOffset);
	
	void DrawIndexedPrimitive(FMetalBufferPtr IndexBuffer, uint32 IndexStride, MTL::IndexType IndexType, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 FirstInstance,
							  uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances);
	
	void DrawIndexedIndirect(FMetalRHIBuffer* IndexBufferRHI, uint32 PrimitiveType, FMetalRHIBuffer* VertexBufferRHI, int32 DrawArgumentsIndex);
	
	void DrawIndexedPrimitiveIndirect(uint32 PrimitiveType,FMetalRHIBuffer* IndexBufferRHI,FMetalRHIBuffer* VertexBufferRHI,uint32 ArgumentOffset);
	
	void CopyFromTextureToBuffer(MTL::Texture* Texture, uint32 sourceSlice, uint32 sourceLevel, MTL::Origin sourceOrigin, MTL::Size sourceSize, FMetalBufferPtr ToBuffer, uint32 destinationOffset, uint32 destinationBytesPerRow, uint32 destinationBytesPerImage, MTL::BlitOption options);
	
	void CopyFromBufferToTexture(FMetalBufferPtr Buffer, uint32 sourceOffset, uint32 sourceBytesPerRow, uint32 sourceBytesPerImage, MTL::Size sourceSize, MTL::Texture* ToTexture, uint32 destinationSlice, uint32 destinationLevel, MTL::Origin destinationOrigin, MTL::BlitOption options);
	
	void CopyFromTextureToTexture(MTL::Texture* Texture, uint32 sourceSlice, uint32 sourceLevel, MTL::Origin sourceOrigin, MTL::Size sourceSize, MTL::Texture* ToTexture, uint32 destinationSlice, uint32 destinationLevel, MTL::Origin destinationOrigin);
	
	void CopyFromBufferToBuffer(FMetalBufferPtr SourceBuffer, NS::UInteger SourceOffset, FMetalBufferPtr DestinationBuffer, NS::UInteger DestinationOffset, NS::UInteger Size);
	
    bool AsyncCopyFromBufferToTexture(FMetalBufferPtr Buffer, uint32 sourceOffset, uint32 sourceBytesPerRow, uint32 sourceBytesPerImage, MTL::Size sourceSize, MTL::Texture* toTexture, uint32 destinationSlice, uint32 destinationLevel, MTL::Origin destinationOrigin, MTL::BlitOption options);
    
    bool AsyncCopyFromTextureToTexture(MTL::Texture* Texture, uint32 sourceSlice, uint32 sourceLevel, MTL::Origin sourceOrigin, MTL::Size sourceSize, MTL::Texture* toTexture, uint32 destinationSlice, uint32 destinationLevel, MTL::Origin destinationOrigin);
	
    void AsyncCopyFromBufferToBuffer(FMetalBufferPtr SourceBuffer, NS::UInteger SourceOffset, FMetalBufferPtr DestinationBuffer, NS::UInteger DestinationOffset, NS::UInteger Size);
    
    bool CanAsyncCopyToBuffer(FMetalBufferPtr DestinationBuffer);
	
    void AsyncGenerateMipmapsForTexture(MTL::Texture* Texture);
    
	void SubmitAsyncCommands(MTL::HandlerFunction ScheduledHandler, MTL::HandlerFunction CompletionHandler, bool const bWait);
	
	void SynchronizeTexture(MTL::Texture* Texture, uint32 Slice, uint32 Level);
	
	void SynchroniseResource(MTL::Resource* Resource);
	
	void FillBuffer(MTL::Buffer* Buffer, NS::Range Range, uint8 Value);

	void Dispatch(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ);
	void DispatchIndirect(FMetalRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset);

#if PLATFORM_SUPPORTS_MESH_SHADERS
    void DispatchMeshShader(uint32 PrimitiveType, uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ);
    void DispatchIndirectMeshShader(uint32 PrimitiveType, FMetalRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset);
#endif

	void StartTiming(class FMetalEventNode* EventNode);
	void EndTiming(class FMetalEventNode* EventNode);

#if ENABLE_METAL_GPUPROFILE
	static void MakeCurrent(FMetalContext* Context);
	static FMetalContext* GetCurrentContext();
#endif
	
	void InitFrame();
	void FinishFrame(bool const bImmediateContext);

	// Track Write->Read transitions for TBDR Fragment->Verex fencing
	void TransitionResource(FRHIUnorderedAccessView* InResource);
	void TransitionResource(FRHITexture* InResource);

    void TransitionRHIResource(FRHIBuffer* InResource);

protected:
	/** The underlying Metal device */
	MTL::Device* Device;
	
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
	
#if ENABLE_METAL_GPUPROFILE
	/** the slot to store a per-thread context ref */
	static uint32 CurrentContextTLSSlot;
#endif
	
	/** Whether the validation layer is enabled */
	bool bValidationEnabled;
};

inline void FMetalContext::TransitionRHIResource(FRHIBuffer* InResource)
{
    auto Resource = ResourceCast(InResource);
    if (Resource->GetCurrentBufferOrNil())
    {
        RenderPass.TransitionResources(Resource->GetCurrentBuffer()->GetMTLBuffer().get());
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
	
	MTLTexturePtr CreateTexture(FMetalSurface* Surface, MTL::TextureDescriptor* Descriptor);
	FMetalBufferPtr CreatePooledBuffer(FMetalPooledBufferArgs const& Args);
	void ReleaseBuffer(FMetalBufferPtr Buf);
	void ReleaseObject(NS::Object* Obj);
	void ReleaseTexture(FMetalSurface* Surface, MTLTexturePtr Texture);
	void ReleaseTexture(MTLTexturePtr Texture);
	void ReleaseFence(FMetalFence* Fence);
    void ReleaseFunction(TFunction<void()>);
	
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
	
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
    FMetalBindlessDescriptorManager* GetBindlessDescriptorManager()
    {
        return BindlessDescriptorManager;
    }
#endif

	void NewLock(FMetalRHIBuffer* Buffer, FMetalFrameAllocator::AllocationEntry& Allocation);
	FMetalFrameAllocator::AllocationEntry FetchAndRemoveLock(FMetalRHIBuffer* Buffer);
	
#if METAL_DEBUG_OPTIONS
    void AddActiveBuffer(MTL::Buffer* Buffer, const NS::Range& Range);
    void RemoveActiveBuffer(MTL::Buffer* Buffer, const NS::Range& Range);
	bool ValidateIsInactiveBuffer(MTL::Buffer* Buffer, const NS::Range& Range);
	void ScribbleBuffer(MTL::Buffer* Buffer, const NS::Range& Range);
#endif
	
private:
	FMetalDeviceContext(MTL::Device* MetalDevice, uint32 DeviceIndex, FMetalCommandQueue* Queue);
	
private:
	/** The index into the GPU device list for the selected Metal device */
	uint32 DeviceIndex;
	
	/** Dynamic memory heap */
	FMetalResourceHeap Heap;
	
	/** GPU Frame Capture Manager */
	FMetalCaptureManager CaptureManager;
	
	/** Free lists for releasing objects only once it is safe to do so */
	TSet<FMetalBufferPtr> UsedBuffers;
	TSet<MTLTexturePtr> UsedTextures;
	TSet<FMetalFence*> UsedFences;
    TSet<FMetalBufferData*> UsedBufferDatas;
	TLockFreePointerListLIFO<FMetalFence> FenceFreeList;
    TArray<TFunction<void()>> FunctionFreeList;
	TSet<NS::Object*> ObjectFreeList;
	struct FMetalDelayedFreeList
	{
		bool IsComplete() const;
		TArray<TSharedPtr<FMetalCommandBufferFence, ESPMode::ThreadSafe>> Fences;
		TSet<FMetalBufferPtr> UsedBuffers;
		TSet<MTLTexturePtr> UsedTextures;
		TSet<FMetalFence*> FenceFreeList;
		TSet<NS::Object*> ObjectFreeList;
        TArray<TFunction<void()>> FunctionFreeList;
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
    TMap<MTL::Buffer*, TArray<NS::Range>> ActiveBuffers;
#endif
	
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
	
	/** Whether we presented this frame - only used to track when to introduce debug markers */
	bool bPresented;
	
	/** PSO cache manager */
	FMetalPipelineStateCacheManager* PSOManager;

    /** Thread index owned by the RHI Thread. Monotonically increases every call to EndFrame() */
    uint32 FrameNumberRHIThread;

#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
    /** Bindless Descriptor Heaps manager. */
    FMetalBindlessDescriptorManager* BindlessDescriptorManager;
#endif

#if METAL_RHI_RAYTRACING
	FMetalRayTracingCompactionRequestHandler* RayTracingCompactionRequestHandler;

	void InitializeRayTracing();
	void CleanUpRayTracing();

public:
	void UpdateRayTracing();

	inline FMetalRayTracingCompactionRequestHandler* GetRayTracingCompactionRequestHandler() const { return RayTracingCompactionRequestHandler; }
#endif // METAL_RHI_RAYTRACING
};

