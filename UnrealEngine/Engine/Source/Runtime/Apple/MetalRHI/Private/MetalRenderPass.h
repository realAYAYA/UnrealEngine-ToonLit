// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetalRHIPrivate.h"
#include "MetalCommandEncoder.h"
#include "MetalState.h"
#include "MetalFence.h"

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
	void SetDispatchType(mtlpp::DispatchType Type);
	
    void Begin(FMetalFence* Fence, bool const bParallelBegin = false);
	
	void Wait(FMetalFence* Fence);

	void Update(FMetalFence* Fence);
	
    void BeginParallelRenderPass(mtlpp::RenderPassDescriptor RenderPass, uint32 NumParallelContextsInPass);

    void BeginRenderPass(mtlpp::RenderPassDescriptor RenderPass);

    void RestartRenderPass(mtlpp::RenderPassDescriptor RenderPass);
    
    void DrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances);
    
    void DrawPrimitiveIndirect(uint32 PrimitiveType, FMetalVertexBuffer* VertexBuffer, uint32 ArgumentOffset);
    
    void DrawIndexedPrimitive(FMetalBuffer const& IndexBuffer, uint32 IndexStride, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 FirstInstance,
                         uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances);
    
    void DrawIndexedIndirect(FMetalIndexBuffer* IndexBufferRHI, uint32 PrimitiveType, FMetalStructuredBuffer* VertexBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances);
    
    void DrawIndexedPrimitiveIndirect(uint32 PrimitiveType,FMetalIndexBuffer* IndexBufferRHI,FMetalVertexBuffer* VertexBufferRHI,uint32 ArgumentOffset);
	
    void Dispatch(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ);
    
    void DispatchIndirect(FMetalVertexBuffer* ArgumentBufferRHI, uint32 ArgumentOffset);
    
    TRefCountPtr<FMetalFence> const& EndRenderPass(void);
    
    void CopyFromTextureToBuffer(FMetalTexture const& Texture, uint32 sourceSlice, uint32 sourceLevel, mtlpp::Origin sourceOrigin, mtlpp::Size sourceSize, FMetalBuffer const& toBuffer, uint32 destinationOffset, uint32 destinationBytesPerRow, uint32 destinationBytesPerImage, mtlpp::BlitOption options);
    
    void CopyFromBufferToTexture(FMetalBuffer const& Buffer, uint32 sourceOffset, uint32 sourceBytesPerRow, uint32 sourceBytesPerImage, mtlpp::Size sourceSize, FMetalTexture const& toTexture, uint32 destinationSlice, uint32 destinationLevel, mtlpp::Origin destinationOrigin, mtlpp::BlitOption options);
    
    void CopyFromTextureToTexture(FMetalTexture const& Texture, uint32 sourceSlice, uint32 sourceLevel, mtlpp::Origin sourceOrigin, mtlpp::Size sourceSize, FMetalTexture const& toTexture, uint32 destinationSlice, uint32 destinationLevel, mtlpp::Origin destinationOrigin);
	
	void CopyFromBufferToBuffer(FMetalBuffer const& SourceBuffer, NSUInteger SourceOffset, FMetalBuffer const& DestinationBuffer, NSUInteger DestinationOffset, NSUInteger Size);
	
	void PresentTexture(FMetalTexture const& Texture, uint32 sourceSlice, uint32 sourceLevel, mtlpp::Origin sourceOrigin, mtlpp::Size sourceSize, FMetalTexture const& toTexture, uint32 destinationSlice, uint32 destinationLevel, mtlpp::Origin destinationOrigin);
    
    void SynchronizeTexture(FMetalTexture const& Texture, uint32 Slice, uint32 Level);
    
	void SynchroniseResource(mtlpp::Resource const& Resource);
    
	void FillBuffer(FMetalBuffer const& Buffer, ns::Range Range, uint8 Value);
	
	bool AsyncCopyFromBufferToTexture(FMetalBuffer const& Buffer, uint32 sourceOffset, uint32 sourceBytesPerRow, uint32 sourceBytesPerImage, mtlpp::Size sourceSize, FMetalTexture const& toTexture, uint32 destinationSlice, uint32 destinationLevel, mtlpp::Origin destinationOrigin, mtlpp::BlitOption options);
	
	bool AsyncCopyFromTextureToTexture(FMetalTexture const& Texture, uint32 sourceSlice, uint32 sourceLevel, mtlpp::Origin sourceOrigin, mtlpp::Size sourceSize, FMetalTexture const& toTexture, uint32 destinationSlice, uint32 destinationLevel, mtlpp::Origin destinationOrigin);
	
	bool CanAsyncCopyToBuffer(FMetalBuffer const& DestinationBuffer);
	
	void AsyncCopyFromBufferToBuffer(FMetalBuffer const& SourceBuffer, NSUInteger SourceOffset, FMetalBuffer const& DestinationBuffer, NSUInteger DestinationOffset, NSUInteger Size);
	
	FMetalBuffer AllocateTemporyBufferForCopy(FMetalBuffer const& DestinationBuffer, NSUInteger Size, NSUInteger Align);
	
	void AsyncGenerateMipmapsForTexture(FMetalTexture const& Texture);
	
    TRefCountPtr<FMetalFence> const& Submit(EMetalSubmitFlags SubmissionFlags);
    
    TRefCountPtr<FMetalFence> const& End(void);
	
	void InsertCommandBufferFence(FMetalCommandBufferFence& Fence, mtlpp::CommandBufferHandler Handler);
	
	void AddCompletionHandler(mtlpp::CommandBufferHandler Handler);
	
	void AddAsyncCommandBufferHandlers(mtlpp::CommandBufferHandler Scheduled, mtlpp::CommandBufferHandler Completion);
	
	void TransitionResources(mtlpp::Resource const& Resource);

#pragma mark - Public Debug Support -
	
    /*
     * Inserts a debug compute encoder into the command buffer. This is how we generate a timestamp when no encoder exists.
     */
    void InsertDebugEncoder();
	
	/*
	 * Inserts a debug string into the command buffer.  This does not change any API behavior, but can be useful when debugging.
	 * @param string The name of the signpost. 
	 */
	void InsertDebugSignpost(ns::String const& String);
	
	/*
	 * Push a new named string onto a stack of string labels.
	 * @param string The name of the debug group. 
	 */
	void PushDebugGroup(ns::String const& String);
	
	/* Pop the latest named string off of the stack. */
	void PopDebugGroup(void);
    
#pragma mark - Public Accessors -
	
	/*
	 * Get the current internal command buffer.
	 * @returns The current command buffer.
	 */
	mtlpp::CommandBuffer const& GetCurrentCommandBuffer(void) const;
	mtlpp::CommandBuffer& GetCurrentCommandBuffer(void);
	
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
	
	/*
	 * Whether the render-pass is within a parallel rendering pass.
	 * @returns True if and only if within a parallel rendering pass, otherwise false.
	 */
	bool IsWithinParallelPass(void);

	/*
	 * Get a child render command-encoder and the parent parallel command-encoder when within a parallel pass.
	 * @returns A valid render command encoder or nil.
	 */
    mtlpp::RenderCommandEncoder GetParallelRenderCommandEncoder(uint32 Index, mtlpp::ParallelRenderCommandEncoder& ParallelEncoder);
	
	void InsertTextureBarrier();
	
private:
    void ConditionalSwitchToRender(void);
    void ConditionalSwitchToCompute(void);
	void ConditionalSwitchToBlit(void);
	void ConditionalSwitchToAsyncBlit(void);
	void ConditionalSwitchToAsyncCompute(void);
	
    void PrepareToRender(uint32 PrimType);
    void PrepareToDispatch(void);
	void PrepareToAsyncDispatch(void);

    void CommitRenderResourceTables(void);
    void CommitDispatchResourceTables(void);
	void CommitAsyncDispatchResourceTables(void);
    
    void ConditionalSubmit();
	
	uint32 GetEncoderIndex(void) const;
	uint32 GetCommandBufferIndex(void) const;
	
	void InsertDebugDraw(FMetalCommandData& Data);
	void InsertDebugDispatch(FMetalCommandData& Data);

private:
	FMetalCommandList& CmdList;
    FMetalStateCache& State;
    
    // Which of the buffers/textures/sampler slots are bound
    // The state cache is responsible for ensuring we bind the correct 
	FMetalTextureMask BoundTextures[EMetalShaderStages::Num];
    uint32 BoundBuffers[EMetalShaderStages::Num];
    uint16 BoundSamplers[EMetalShaderStages::Num];
    
    FMetalCommandEncoder CurrentEncoder;
    FMetalCommandEncoder PrologueEncoder;
	
	// To ensure that buffer uploads aren't overwritten before they are used track what is in flight
	// Disjoint ranges *are* permitted!
	TMap<id<MTLBuffer>, TArray<NSRange>> OutstandingBufferUploads;

	// Fences for the current command encoder chain
	TRefCountPtr<FMetalFence> PassStartFence;
	TRefCountPtr<FMetalFence> CurrentEncoderFence;
	TRefCountPtr<FMetalFence> ParallelPassEndFence;

	// Fences for the prologue command encoder chain
	TRefCountPtr<FMetalFence> PrologueStartEncoderFence;
	TRefCountPtr<FMetalFence> PrologueEncoderFence;
    
    mtlpp::RenderPassDescriptor RenderPassDesc;
    
	mtlpp::DispatchType ComputeDispatchType;
    uint32 NumOutstandingOps;
    bool bWithinRenderPass;
};
