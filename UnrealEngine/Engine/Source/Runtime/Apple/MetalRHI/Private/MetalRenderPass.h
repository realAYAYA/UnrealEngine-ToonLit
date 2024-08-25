// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetalRHIPrivate.h"
#include "MetalState.h"
#include "MetalFence.h"
#include "MetalBuffer.h"
#include "MetalCommandEncoder.h"

class FMetalCommandList;
class FMetalCommandQueue;

class FMetalRenderPass
{
public:
#pragma mark - Public C++ Boilerplate -

	/** Default constructor */
	FMetalRenderPass(FMetalCommandList& CmdList, FMetalStateCache& StateCache);
	
	/** Destructor */
	~FMetalRenderPass(void);
	
#pragma mark -
	void SetDispatchType(MTL::DispatchType Type);
	
    void Begin();

    void BeginRenderPass(MTL::RenderPassDescriptor* RenderPass);

    void RestartRenderPass(MTL::RenderPassDescriptor* RenderPass);
    
    void DrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances);
    
    void DrawPrimitiveIndirect(uint32 PrimitiveType, FMetalRHIBuffer* VertexBuffer, uint32 ArgumentOffset);
    
    void DrawIndexedPrimitive(FMetalBufferPtr IndexBuffer, uint32 IndexStride, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 FirstInstance,
                         uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances);
    
    void DrawIndexedIndirect(FMetalRHIBuffer* IndexBufferRHI, uint32 PrimitiveType, FMetalRHIBuffer* VertexBufferRHI, int32 DrawArgumentsIndex);
    
    void DrawIndexedPrimitiveIndirect(uint32 PrimitiveType,FMetalRHIBuffer* IndexBufferRHI,FMetalRHIBuffer* VertexBufferRHI,uint32 ArgumentOffset);
	
    void Dispatch(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ);
    
    void DispatchIndirect(FMetalRHIBuffer* ArgumentBufferRHI, uint32 ArgumentOffset);
     
#if PLATFORM_SUPPORTS_MESH_SHADERS
    void DispatchMeshShader(uint32 PrimitiveType, uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ);
    void DispatchIndirectMeshShader(uint32 PrimitiveType, FMetalRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset);
#endif
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
    void DispatchMeshShaderGSEmulation(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances);
#endif

    TRefCountPtr<FMetalFence> const& EndRenderPass(void);
    
    void CopyFromTextureToBuffer(MTL::Texture* Texture, uint32 sourceSlice, uint32 sourceLevel, MTL::Origin sourceOrigin, MTL::Size sourceSize, FMetalBufferPtr toBuffer, uint32 destinationOffset, uint32 destinationBytesPerRow, uint32 destinationBytesPerImage, MTL::BlitOption options);
    
    void CopyFromBufferToTexture(FMetalBufferPtr Buffer, uint32 sourceOffset, uint32 sourceBytesPerRow, uint32 sourceBytesPerImage, MTL::Size sourceSize, MTL::Texture* toTexture, uint32 destinationSlice, uint32 destinationLevel, MTL::Origin destinationOrigin, MTL::BlitOption options);
    
    void CopyFromTextureToTexture(MTL::Texture* Texture, uint32 sourceSlice, uint32 sourceLevel, MTL::Origin sourceOrigin, MTL::Size sourceSize, MTL::Texture* toTexture, uint32 destinationSlice, uint32 destinationLevel, MTL::Origin destinationOrigin);
	
	void CopyFromBufferToBuffer(FMetalBufferPtr SourceBuffer, NS::UInteger SourceOffset, FMetalBufferPtr DestinationBuffer, NS::UInteger DestinationOffset, NS::UInteger Size);
	
	void PresentTexture(MTL::Texture* Texture, uint32 sourceSlice, uint32 sourceLevel, MTL::Origin sourceOrigin, MTL::Size sourceSize, MTL::Texture* toTexture, uint32 destinationSlice, uint32 destinationLevel, MTL::Origin destinationOrigin);
    
#if PLATFORM_VISIONOS
	void EncodePresentImmersive(cp_drawable_t Drawable, cp_frame_t Frame);
#endif
	
    void SynchronizeTexture(MTL::Texture* Texture, uint32 Slice, uint32 Level);
    
	void SynchroniseResource(MTL::Resource* Resource);
    
	void FillBuffer(MTL::Buffer* Buffer, NS::Range Range, uint8 Value);
    
    bool CanAsyncCopyToBuffer(FMetalBufferPtr DestinationBuffer);
    
	bool AsyncCopyFromBufferToTexture(FMetalBufferPtr Buffer, uint32 sourceOffset, uint32 sourceBytesPerRow, uint32 sourceBytesPerImage, MTL::Size sourceSize, MTL::Texture*, uint32 destinationSlice, uint32 destinationLevel, MTL::Origin destinationOrigin, MTL::BlitOption options);
	
	bool AsyncCopyFromTextureToTexture(MTL::Texture* Texture, uint32 sourceSlice, uint32 sourceLevel, MTL::Origin sourceOrigin, MTL::Size sourceSize, MTL::Texture* toTexture, uint32 destinationSlice, uint32 destinationLevel, MTL::Origin destinationOrigin);
	
	void AsyncCopyFromBufferToBuffer(FMetalBufferPtr SourceBuffer, NS::UInteger SourceOffset, FMetalBufferPtr DestinationBuffer, NS::UInteger DestinationOffset, NS::UInteger Size);
	
	FMetalBufferPtr AllocateTemporyBufferForCopy(FMetalBufferPtr DestinationBuffer, NS::UInteger Size, NS::UInteger Align);
	
	void AsyncGenerateMipmapsForTexture(MTL::Texture* Texture);
	
    TRefCountPtr<FMetalFence> const& Submit(EMetalSubmitFlags SubmissionFlags);
    
    TRefCountPtr<FMetalFence> const& End(void);
	
	void InsertCommandBufferFence(TSharedPtr<FMetalCommandBufferFence, ESPMode::ThreadSafe>& Fence, FMetalCommandBufferCompletionHandler Handler);
	
	void AddCompletionHandler(FMetalCommandBufferCompletionHandler Handler);
	
	void AddAsyncCommandBufferHandlers(MTL::HandlerFunction Scheduled, MTL::HandlerFunction Completion);
	
	void TransitionResources(MTL::Resource* Resource);

#pragma mark - Public Debug Support -
	
    /*
     * Inserts a debug compute encoder into the command buffer. This is how we generate a timestamp when no encoder exists.
     */
    void InsertDebugEncoder();
	
	/*
	 * Inserts a debug string into the command buffer.  This does not change any API behavior, but can be useful when debugging.
	 * @param string The name of the signpost. 
	 */
	void InsertDebugSignpost(NS::String* String);
	
	/*
	 * Push a new named string onto a stack of string labels.
	 * @param string The name of the debug group. 
	 */
	void PushDebugGroup(NS::String* String);
	
	/* Pop the latest named string off of the stack. */
	void PopDebugGroup(void);
    
#pragma mark - Public Accessors -
	
	/*
	 * Get the current internal command buffer.
	 * @returns The current command buffer.
	 */
    FMetalCommandBuffer* GetCurrentCommandBuffer(void);
	
    /*
     * Get the internal current command-encoder.
     * @returns The current command encoder.
     */
	inline FMetalCommandEncoder& GetCurrentCommandEncoder(void) { return CurrentEncoder; }
	
	/*
	 * Get the internal ring-buffer used for temporary allocations.
	 * @returns The temporary allocation buffer for the command-pass.
	 */
	FMetalSubBufferRing& GetRingBuffer(void);
	
	/*
	 * Attempts to shrink the ring-buffers so we don't keep very large allocations when we don't need them.
	 */
	void ShrinkRingBuffers(void);
	
	void InsertTextureBarrier();

	inline bool IsWithinRenderPass() const { return bWithinRenderPass; }

#if METAL_RHI_RAYTRACING
	// TODO: Crappy workaround for inline raytracing support.
	inline void SetRayTracingInstanceBufferSRV(TRefCountPtr<FMetalShaderResourceView>& SRV) { InstanceBufferSRV = SRV; }

	TRefCountPtr<FMetalShaderResourceView> InstanceBufferSRV;
#endif
	
private:
    void ConditionalSwitchToRender(void);
    void ConditionalSwitchToCompute(void);
	void ConditionalSwitchToBlit(void);
	
    void PrepareToRender(uint32 PrimType);
    void PrepareToDispatch(void);

    void CommitRenderResourceTables(void);
    void CommitDispatchResourceTables(void);
    
    void ConditionalSubmit();
	
	uint32 GetEncoderIndex(void) const;
	uint32 GetCommandBufferIndex(void) const;

private:
	FMetalCommandList& CmdList;
    FMetalStateCache& State;
    
    // Which of the buffers/textures/sampler slots are bound
    // The state cache is responsible for ensuring we bind the correct 
	FMetalTextureMask BoundTextures[EMetalShaderStages::Num];
    uint32 BoundBuffers[EMetalShaderStages::Num];
    uint16 BoundSamplers[EMetalShaderStages::Num];
    
    FMetalCommandEncoder CurrentEncoder;
	
	// To ensure that buffer uploads aren't overwritten before they are used track what is in flight
	// Disjoint ranges *are* permitted!
	TMap<MTLBufferPtr, TArray<NS::Range>> OutstandingBufferUploads;

	// Fences for the current command encoder chain
	TRefCountPtr<FMetalFence> CurrentEncoderFence;
    
    MTL::RenderPassDescriptor* RenderPassDesc;
    
	MTL::DispatchType ComputeDispatchType;
    uint32 NumOutstandingOps;
    bool bWithinRenderPass;
	
#if PLATFORM_VISIONOS	
	cp_frame_t CompositorServicesFrame;
#endif
};
