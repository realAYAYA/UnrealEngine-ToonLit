// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <Metal/Metal.h>
#include "MetalBuffer.h"
#include "MetalDebugCommandEncoder.h"
#include "MetalBlitCommandEncoder.h"
#include "MetalComputeCommandEncoder.h"
#include "MetalRenderCommandEncoder.h"
#include "MetalFence.h"
#include "MetalProfiler.h"
#include "command_buffer.hpp"

class FMetalCommandList;
class FMetalCommandQueue;
class FMetalGraphicsPipelineState;
struct FMetalCommandBufferFence;

/**
 * Enumeration for submission hints to avoid unclear bool values.
 */
enum EMetalSubmitFlags
{
	/** No submission flags. */
	EMetalSubmitFlagsNone = 0,
	/** Create the next command buffer. */
	EMetalSubmitFlagsCreateCommandBuffer = 1 << 0,
	/** Wait on the submitted command buffer. */
	EMetalSubmitFlagsWaitOnCommandBuffer = 1 << 1,
	/** Break a single logical command-buffer into parts to keep the GPU active. */
	EMetalSubmitFlagsBreakCommandBuffer = 1 << 2,
	/** Submit the prologue command-buffer only, leave the current command-buffer active.  */
	EMetalSubmitFlagsAsyncCommandBuffer = 1 << 3,
	/** Submit and reset all the cached encoder state.  */
	EMetalSubmitFlagsResetState = 1 << 4,
	/** Force submission even if the command-buffer is empty. */
	EMetalSubmitFlagsForce = 1 << 5,
	/** Indicates this is the final command buffer in a frame. */
	EMetalSubmitFlagsLastCommandBuffer = 1 << 6,
};

@class FMetalCommandBufferDebug;

struct FMetalCommandData
{
	enum class Type
	{
		DrawPrimitive,
		DrawPrimitiveIndexed,
		DrawPrimitivePatch,
		DrawPrimitiveIndirect,
		DrawPrimitiveIndexedIndirect,
		Dispatch,
		DispatchIndirect,
		Num,
	};
	struct DispatchIndirectArgs
	{
		id ArgumentBuffer;
		NSUInteger ArgumentOffset;
	};
	Type CommandType;
	union
	{
		mtlpp::DrawPrimitivesIndirectArguments Draw;
		mtlpp::DrawIndexedPrimitivesIndirectArguments DrawIndexed;
		mtlpp::DrawPatchIndirectArguments DrawPatch;
		MTLDispatchThreadgroupsIndirectArguments Dispatch;
		DispatchIndirectArgs DispatchIndirect;
	};
	FString ToString() const;
};

struct FMetalCommandDebug
{
    uint32 CmdBufIndex;
	uint32 Encoder;
	uint32 Index;
	FMetalGraphicsPipelineState* PSO;
	FMetalComputeShader* ComputeShader;
	FMetalCommandData Data;
};

class FMetalCommandBufferMarkers : public ns::Object<FMetalCommandBufferDebug*, ns::CallingConvention::ObjectiveC>
{
public:
	FMetalCommandBufferMarkers(void);
	FMetalCommandBufferMarkers(mtlpp::CommandBuffer& CmdBuf);
	FMetalCommandBufferMarkers(FMetalCommandBufferDebug* CmdBuf);
	
	void AllocateContexts(uint32 NumContexts);
	uint32 AddCommand(uint32 CmdBufIndex, uint32 Encoder, uint32 ContextIndex, FMetalBuffer& DebugBuffer, FMetalGraphicsPipelineState* PSO, FMetalComputeShader* ComputeShader, FMetalCommandData& Data);
	TArray<FMetalCommandDebug>* GetCommands(uint32 Context);
	ns::AutoReleased<FMetalBuffer> GetDebugBuffer(uint32 ContextIndex);
	uint32 NumContexts() const;
	uint32 GetIndex() const;
	
	static FMetalCommandBufferMarkers Get(mtlpp::CommandBuffer const& CmdBuf);
	
	static char const* kTableAssociationKey;
};

/**
 * EMetalCommandEncoderType:
 *   EMetalCommandEncoderCurrent: The primary encoder that is used for draw calls & dispatches
 *   EMetalCommandEncoderPrologue: A secondary encoder that is used for blits & dispatches that setup resources & state for the current encoder.
 */
enum EMetalCommandEncoderType
{
	EMetalCommandEncoderCurrent,
	EMetalCommandEncoderPrologue
};

/**
 * FMetalCommandEncoder:
 *	Wraps the details of switching between different command encoders on the command-buffer, allowing for restoration of the render encoder if needed.
 * 	UnrealEngine expects the API to serialise commands in-order, but Metal expects applications to work with command-buffers directly so we need to implement 
 *	the RHI semantics by switching between encoder types. This class hides the ugly details. Eventually it might be possible to move some of the operations
 *	into pre- & post- command-buffers so that we avoid encoder switches but that will take changes to the RHI and high-level code too, so it won't happen soon.
 */
class FMetalCommandEncoder
{
public:
#pragma mark - Public C++ Boilerplate -

	/** Default constructor */
	FMetalCommandEncoder(FMetalCommandList& CmdList, EMetalCommandEncoderType Type);
	
	/** Destructor */
	~FMetalCommandEncoder(void);
	
	/** Reset cached state for reuse */
	void Reset(void);
	
	/** Reset cached state for reuse while in rendering */
	void ResetLive(void);
	
#pragma mark - Public Command Buffer Mutators -

	/**
	 * Start encoding to CommandBuffer. It is an error to call this with any outstanding command encoders or current command buffer.
	 * Instead call EndEncoding & CommitCommandBuffer before calling this.
	 */
	void StartCommandBuffer(void);
	
	/**
	 * Commit the existing command buffer if there is one & optionally waiting for completion, if there isn't a current command buffer this is a no-op.
	 * @param Flags Flags to control commit behaviour.
 	 */
	void CommitCommandBuffer(uint32 const Flags);

#pragma mark - Public Command Buffer Accessors -
	
	/** @returns the current command buffer */
	mtlpp::CommandBuffer& GetCommandBuffer() { return CommandBuffer; }

	/** @returns the current command buffer */
	mtlpp::CommandBuffer const& GetCommandBuffer() const { return CommandBuffer; }
	
	/** @returns the monotonically incremented command buffer index */
	uint32 GetCommandBufferIndex() const { return CmdBufIndex; }

#pragma mark - Public Command Encoder Accessors -
	
	/** @returns True if and only if there is an active parallel render command encoder, otherwise false. */
	bool IsParallelRenderCommandEncoderActive(void) const;
	
	/** @returns True if and only if there is an active render command encoder, otherwise false. */
	bool IsRenderCommandEncoderActive(void) const;
	
	/** @returns True if and only if there is an active compute command encoder, otherwise false. */
	bool IsComputeCommandEncoderActive(void) const;
	
	/** @returns True if and only if there is an active blit command encoder, otherwise false. */
	bool IsBlitCommandEncoderActive(void) const;
	
	/**
	 * True iff the command-encoder submits immediately to the command-queue, false if it performs any buffering.
	 * @returns True iff the command-list submits immediately to the command-queue, false if it performs any buffering.
	 */
	bool IsImmediate(void) const;
	
	/**
	 * True iff the command-encoder encodes only to a child of a parallel render command encoder, false if it is standalone.
	 * @returns True iff the command-encoder encodes only to a child of a parallel render command encoder, false if it is standalone.
	 */
	bool IsParallel(void) const;

	/** @returns True if and only if there is valid render pass descriptor set on the encoder, otherwise false. */
	bool IsRenderPassDescriptorValid(void) const;
	
	/** @returns The current render pass descriptor. */
	mtlpp::RenderPassDescriptor const& GetRenderPassDescriptor(void) const;
	
	/** @returns The active render command encoder or nil if there isn't one. */
	mtlpp::ParallelRenderCommandEncoder& GetParallelRenderCommandEncoder(void);
	
	/** @returns The child render command encoder of the current parallel render encoder for Index. */
	mtlpp::RenderCommandEncoder& GetChildRenderCommandEncoder(uint32 Index);

	/** @returns The active render command encoder or nil if there isn't one. */
	mtlpp::RenderCommandEncoder& GetRenderCommandEncoder(void);
	
	/** @returns The active compute command encoder or nil if there isn't one. */
	mtlpp::ComputeCommandEncoder& GetComputeCommandEncoder(void);
	
	/** @returns The active blit command encoder or nil if there isn't one. */
	mtlpp::BlitCommandEncoder& GetBlitCommandEncoder(void);
	
	/** @returns The MTLFence for the current encoder or nil if there isn't one. */
	TRefCountPtr<FMetalFence> const& GetEncoderFence(void) const;
	
	/** @returns The number of encoded passes in the command buffer. */
	uint32 NumEncodedPasses(void) const { return EncoderNum; }
	
#pragma mark - Public Command Encoder Mutators -

	/**
 	 * Begins encoding rendering commands into the current command buffer. No other encoder may be active & the mtlpp::RenderPassDescriptor must previously have been set.
	 * @param NumChildren The number of child render-encoders to create. 
	 */
	void BeginParallelRenderCommandEncoding(uint32 NumChildren);

	/**
 	 * Begins encoding rendering commands into the current command buffer. No other encoder may be active & the mtlpp::RenderPassDescriptor must previously have been set.
	 */
	void BeginRenderCommandEncoding(void);
	
	/** Begins encoding compute commands into the current command buffer. No other encoder may be active. */
	void BeginComputeCommandEncoding(mtlpp::DispatchType Type = mtlpp::DispatchType::Serial);
	
	/** Begins encoding blit commands into the current command buffer. No other encoder may be active. */
	void BeginBlitCommandEncoding(void);
	
	/** Declare that all command generation from this encoder is complete, and detach from the MTLCommandBuffer if there is an encoder active or does nothing if there isn't. */
	TRefCountPtr<FMetalFence> EndEncoding(void);
	
	/** Initialises a fence for the current command-buffer optionally adding a command-buffer completion handler to the command-buffer */
	void InsertCommandBufferFence(FMetalCommandBufferFence& Fence, mtlpp::CommandBufferHandler Handler);
	
	/** Adds a command-buffer completion handler to the command-buffer */
	void AddCompletionHandler(mtlpp::CommandBufferHandler Handler);
	
	/** Update the event to capture all GPU work so far enqueued by this encoder. */
	void UpdateFence(FMetalFence* Fence);
	
	/** Prevent further GPU work until the event is reached. */
	void WaitForFence(FMetalFence* Fence);
	
	/**
	 * Prevent further GPU work until the event is reached and then kick the fence again so future encoders may wait on it.
	 * Unlike the functions above this is not guarded and the caller must ensure proper matching waits & updates.
	 */
	void WaitAndUpdateFence(FMetalFence* Fence);

#pragma mark - Public Debug Support -
	
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
	
	/** Returns the object that records debug command markers into the command-buffer. */
	FMetalCommandBufferMarkers& GetMarkers(void);
	
#if ENABLE_METAL_GPUPROFILE
	/* Get the command-buffer stats object. */
	FMetalCommandBufferStats* GetCommandBufferStats(void);
#endif

#if MTLPP_CONFIG_VALIDATE && METAL_DEBUG_OPTIONS
	/** @returns The active render command encoder or nil if there isn't one. */
	FMetalCommandBufferDebugging& GetCommandBufferDebugging(void) { return CommandBufferDebug; } 
	
	/** @returns The active render command encoder or nil if there isn't one. */
	FMetalRenderCommandEncoderDebugging& GetRenderCommandEncoderDebugging(void) { return RenderEncoderDebug; } 
	
	/** @returns The active compute command encoder or nil if there isn't one. */
	FMetalComputeCommandEncoderDebugging& GetComputeCommandEncoderDebugging(void) { return ComputeEncoderDebug; }
	
	/** @returns The active blit command encoder or nil if there isn't one. */
	FMetalBlitCommandEncoderDebugging& GetBlitCommandEncoderDebugging(void) { return BlitEncoderDebug; }
	
	/** @returns The active blit command encoder or nil if there isn't one. */
	FMetalParallelRenderCommandEncoderDebugging& GetParallelRenderCommandEncoderDebugging(void) { return ParallelEncoderDebug; }
#endif
	
#pragma mark - Public Render State Mutators -

	/**
	 * Set the render pass descriptor - no encoder may be active when this function is called.
	 * @param RenderPass The render pass descriptor to set. May be nil.
	 */
	void SetRenderPassDescriptor(mtlpp::RenderPassDescriptor RenderPass);
	
	/**
	 * Set the render pass store actions, call after SetRenderPassDescriptor but before EndEncoding.
	 * @param ColorStore The store actions for color targets.
	 * @param DepthStore The store actions for the depth buffer - use mtlpp::StoreAction::Unknown if no depth-buffer bound.
	 * @param StencilStore The store actions for the stencil buffer - use mtlpp::StoreAction::Unknown if no stencil-buffer bound.
	 */
	void SetRenderPassStoreActions(mtlpp::StoreAction const* const ColorStore, mtlpp::StoreAction const DepthStore, mtlpp::StoreAction const StencilStore);
	
	/*
	 * Sets the current render pipeline state object.
	 * @param PipelineState The pipeline state to set. Must not be nil.
	 */
	void SetRenderPipelineState(FMetalShaderPipeline* const PipelineState);
	
	/*
	 * Set the viewport, which is used to transform vertexes from normalized device coordinates to window coordinates.  Fragments that lie outside of the viewport are clipped, and optionally clamped for fragments outside of znear/zfar.
	 * @param Viewport The array of viewport dimensions to use.
	 * @param NumActive The number of active viewport dimensions to use.
	 */
	void SetViewport(mtlpp::Viewport const Viewport[], uint32 NumActive);
	
	/*
	 * The winding order of front-facing primitives.
	 * @param FrontFacingWinding The front face winding.
	 */
	void SetFrontFacingWinding(mtlpp::Winding const FrontFacingWinding);
	
	/*
	 * Controls if primitives are culled when front facing, back facing, or not culled at all.
	 * @param CullMode The cull mode.
	 */
	void SetCullMode(mtlpp::CullMode const CullMode);
	
	/*
	 * Depth Bias.
	 * @param DepthBias The depth-bias value.
	 * @param SlopeScale The slope-scale to apply.
	 * @param Clamp The value to clamp to.
	 */
	void SetDepthBias(float const DepthBias, float const SlopeScale, float const Clamp);
	
	/*
	 * Specifies a rectangle for a fragment scissor test.  All fragments outside of this rectangle are discarded.
	 * @param Rect The array of scissor rect dimensions.
	 * @param NumActive The number of active scissor rect dimensions.
	 */
	void SetScissorRect(mtlpp::ScissorRect const Rect[], uint32 NumActive);
	
	/*
	 * Set how to rasterize triangle and triangle strip primitives.
	 * @param FillMode The fill mode.
	 */
	void SetTriangleFillMode(mtlpp::TriangleFillMode const FillMode);

	/*
	 * Set wether to clip or clamp triangles based on depth.
	 * @param FillMode The fill mode.
	 */
	void SetDepthClipMode(mtlpp::DepthClipMode DepthClipMode);
	
	/*
	 * Set the constant blend color used across all blending on all render targets
	 * @param Red The value for the red channel in 0-1.
	 * @param Green The value for the green channel in 0-1.
	 * @param Blue The value for the blue channel in 0-1.
	 * @param Alpha The value for the alpha channel in 0-1.
	 */
	void SetBlendColor(float const Red, float const Green, float const Blue, float const Alpha);
	
	/*
	 * Set the DepthStencil state object.
	 * @param DepthStencilState The depth-stencil state, must not be nil.
	 */
	void SetDepthStencilState(mtlpp::DepthStencilState const& DepthStencilState);
	
	/*
	 * Set the stencil reference value for both the back and front stencil buffers.
	 * @param ReferenceValue The stencil ref value to use.
	 */
	void SetStencilReferenceValue(uint32 const ReferenceValue);
	
	/*
	 * Monitor if samples pass the depth and stencil tests.
	 * @param Mode Controls if the counter is disabled or moniters passing samples.
	 * @param Offset The offset relative to the occlusion query buffer provided when the command encoder was created.  offset must be a multiple of 8.
	 */
	void SetVisibilityResultMode(mtlpp::VisibilityResultMode const Mode, NSUInteger const Offset);
	
#pragma mark - Public Shader Resource Mutators -
	
	/*
	 * Set a global buffer for the specified shader frequency at the given bind point index.
	 * @param FunctionType The shader function to modify.
	 * @param Buffer The buffer to bind or nil to clear.
	 * @param Offset The offset in the buffer or 0 when Buffer is nil.
	 * @param Length The data length - caller is responsible for accounting for non-zero Offset.
	 * @param Index The index to modify.
	 * @param Usage The resource usage mask.
	 * @param Format The Pixel format to reinterpret the resource as.
	 */
	void SetShaderBuffer(mtlpp::FunctionType const FunctionType, FMetalBuffer const& Buffer, NSUInteger const Offset, NSUInteger const Length, NSUInteger const Index, mtlpp::ResourceUsage const Usage, EPixelFormat const Format = PF_Unknown, NSUInteger const ElementRowPitch = 0);
	
	/*
	 * Set an FMetalBufferData to the specified shader frequency at the given bind point index.
	 * @param FunctionType The shader function to modify.
	 * @param Data The data to bind or nullptr to clear.
	 * @param Offset The offset in the buffer or 0 when Buffer is nil.
	 * @param Index The index to modify.
	 * @param Format The pixel format to reinterpret the resource as.
	 */
	void SetShaderData(mtlpp::FunctionType const FunctionType, FMetalBufferData* Data, NSUInteger const Offset, NSUInteger const Index, EPixelFormat const Format = PF_Unknown, NSUInteger const ElementRowPitch = 0);
	
	/*
	 * Set bytes to the specified shader frequency at the given bind point index.
	 * @param FunctionType The shader function to modify.
	 * @param Bytes The data to bind or nullptr to clear.
	 * @param Length The length of the buffer or 0 when Bytes is nil.
	 * @param Index The index to modify.
	 */
	void SetShaderBytes(mtlpp::FunctionType const FunctionType, uint8 const* Bytes, NSUInteger const Length, NSUInteger const Index);
	
	/*
	 * Set a global texture for the specified shader frequency at the given bind point index.
	 * @param FunctionType The shader function to modify.
	 * @param Texture The texture to bind or nil to clear.
	 * @param Index The index to modify.
	 * @param Usage The resource usage mask.
	 */
	void SetShaderTexture(mtlpp::FunctionType const FunctionType, FMetalTexture const& Texture, NSUInteger const Index, mtlpp::ResourceUsage const Usage);
	
	/*
	 * Set a global sampler for the specified shader frequency at the given bind point index.
	 * @param FunctionType The shader function to modify.
	 * @param Sampler The sampler state to bind or nil to clear.
	 * @param Index The index to modify.
	 */
	void SetShaderSamplerState(mtlpp::FunctionType const FunctionType, mtlpp::SamplerState const& Sampler, NSUInteger const Index);
	
	/*
	 * Set the shader side-table data for FunctionType at Index.
	 * @param FunctionType The shader function to modify.
	 * @param Index The index to bind data to.
	 */
	void SetShaderSideTable(mtlpp::FunctionType const FunctionType, NSUInteger const Index);
	
    /*
     * Inform the driver that an indirect argument resource will be used and what the usage mode will be.
     * @param Texture The texture that will be used as an indirect argument.
     * @param Usage The usage mode for the texture.
     */
	void UseIndirectArgumentResource(FMetalTexture const& Texture, mtlpp::ResourceUsage const Usage);
    
    /*
     * Inform the driver that an indirect argument resource will be used and what the usage mode will be.
     * @param Buffer The buffer that will be used as an indirect argument.
     * @param Usage The usage mode for the texture.
     */
	void UseIndirectArgumentResource(FMetalBuffer const& Buffer, mtlpp::ResourceUsage const Usage);

	/*
	 * Transition resource so that we can barrier fragment->vertex stages.
	 * @param Resource The resource we are going to make readable having been written.
	 */
	void TransitionResources(mtlpp::Resource const& Resource);
	
#pragma mark - Public Compute State Mutators -
	
	/*
	 * Set the compute pipeline state that will be used.
	 * @param State The state to set - must not be nil.
	 */
	void SetComputePipelineState(FMetalShaderPipeline* State);

#pragma mark - Public Ring-Buffer Accessor -
	
	/*
	 * Get the internal ring-buffer used for temporary allocations.
	 * @returns The temporary allocation buffer for this command-encoder.
	 */
	FMetalSubBufferRing& GetRingBuffer(void);
	
#pragma mark - Public Resource query Access -

	/*
	 * Returns True if the Resource has been bound to a command encoder, otherwise false.  History will be cleared after a commit operation
	 * @returns True if the Resource has been bound to a command encoder, otherwise false.
	 */
	bool HasTextureBindingHistory(FMetalTexture const& Texture) const;
	bool HasBufferBindingHistory(FMetalBuffer const& Buffer) const;
	
private:
#pragma mark - Private Functions -
	/*
	 * Set the offset for the buffer bound on the specified shader frequency at the given bind point index.
	 * @param FunctionType The shader function to modify.
	 * @param Offset The offset in the buffer or 0 when Buffer is nil.
	 * @param Length The data length - caller is responsible for accounting for non-zero Offset.
	 * @param Index The index to modify.
	 */
	void SetShaderBufferOffset(mtlpp::FunctionType const FunctionType, NSUInteger const Offset, NSUInteger const Length, NSUInteger const Index);
	
	void SetShaderBufferInternal(mtlpp::FunctionType Function, uint32 Index);
	
	void FenceResource(mtlpp::Texture const& Resource);
	void FenceResource(mtlpp::Buffer const& Resource);

	void UseResource(mtlpp::Resource const& Resource, mtlpp::ResourceUsage const Usage);
	
#pragma mark - Private Type Declarations -
private:
    /** A structure of arrays for the current buffer binding settings. */
    struct FMetalBufferBindings
    {
		/** Side-table wrapper object to allow us to use Set*Bytes. */
		FMetalBufferData* SideTable;
        /** The bound buffers or nil. */
		ns::AutoReleased<FMetalBuffer> Buffers[ML_MaxBuffers];
        /** The bound buffers or nil. */
        FMetalBufferData* Bytes[ML_MaxBuffers];
        /** The bound buffer offsets or 0. */
        NSUInteger Offsets[ML_MaxBuffers];
		/** The usage mask for the bound resource or 0 */
		mtlpp::ResourceUsage Usage[ML_MaxBuffers];
		/** The bound buffer constants */
		struct FSizeConstants
		{
			union
			{
				uint32 	Length;
				uint32 	Swizzle;
			};
			uint32 Format;
			uint32 ElementRowPitch;
		};
		FSizeConstants Lengths[ML_MaxBuffers + ML_MaxTextures];
        /** A bitmask for which buffers were bound by the application where a bit value of 1 is bound and 0 is unbound. */
        uint32 Bound;
        
public:
        void SetBufferMetaData(NSUInteger Index, NSUInteger Length, NSUInteger Format, NSUInteger ElementRowPitch)
        {
			Lengths[Index].Length = Length;
			Lengths[Index].Format = Format;
			Lengths[Index].ElementRowPitch = ElementRowPitch;
        }
        void SetTextureSwizzle(NSUInteger Index, uint8 (&Swizzle)[4])
        {
			FMemory::Memcpy(&Lengths[ML_MaxBuffers + Index].Swizzle, Swizzle, sizeof(Swizzle));
			Lengths[ML_MaxBuffers + Index].Format = 0;
			Lengths[ML_MaxBuffers + Index].ElementRowPitch = 0;
        }
	};
	
#pragma mark - Private Member Variables -
	FMetalCommandList& CommandList;

    // Cache Queue feature
    bool bSupportsMetalFeaturesSetBytes;
    
	FMetalBufferBindings ShaderBuffers[int(mtlpp::FunctionType::Kernel)+1];
	
	mtlpp::StoreAction ColorStoreActions[MaxSimultaneousRenderTargets];
	mtlpp::StoreAction DepthStoreAction;
	mtlpp::StoreAction StencilStoreAction;
	
	FMetalSubBufferRing RingBuffer;
	
	mtlpp::RenderPassDescriptor RenderPassDesc;
	
	mtlpp::CommandBuffer CommandBuffer;
	mtlpp::ParallelRenderCommandEncoder ParallelRenderCommandEncoder;
	mtlpp::RenderCommandEncoder RenderCommandEncoder;
	mtlpp::ComputeCommandEncoder ComputeCommandEncoder;
	mtlpp::BlitCommandEncoder BlitCommandEncoder;
	TArray<mtlpp::RenderCommandEncoder> ChildRenderCommandEncoders;
	FMetalCommandBufferMarkers CommandBufferMarkers;
	
	METAL_DEBUG_ONLY(FMetalCommandBufferDebugging CommandBufferDebug);
	METAL_DEBUG_ONLY(FMetalRenderCommandEncoderDebugging RenderEncoderDebug);
	METAL_DEBUG_ONLY(FMetalComputeCommandEncoderDebugging ComputeEncoderDebug);
	METAL_DEBUG_ONLY(FMetalBlitCommandEncoderDebugging BlitEncoderDebug);
	METAL_DEBUG_ONLY(FMetalParallelRenderCommandEncoderDebugging ParallelEncoderDebug);
	
	TRefCountPtr<FMetalFence> EncoderFence;
#if ENABLE_METAL_GPUPROFILE
	FMetalCommandBufferStats* CommandBufferStats;
#endif
#if METAL_DEBUG_OPTIONS
	uint8 WaitCount;
	uint8 UpdateCount;
#endif

	TArray<ns::Object<mtlpp::CommandBufferHandler>> CompletionHandlers;
	NSMutableArray* DebugGroups;
    
    TSet<ns::AutoReleased<FMetalBuffer>> ActiveBuffers;
    
	TSet<ns::AutoReleased<FMetalBuffer>> BufferBindingHistory;
	TSet<ns::AutoReleased<FMetalTexture>> TextureBindingHistory;
	
	TMap<mtlpp::Resource::Type, mtlpp::ResourceUsage> ResourceUsage;
	
	TSet<mtlpp::Resource::Type> TransitionedResources;
	TSet<mtlpp::Resource::Type> FenceResources;
	
	TSet<TRefCountPtr<FMetalFence>> FragmentFences;
	
	mtlpp::RenderStages FenceStage;
	uint32 EncoderNum;
	uint32 CmdBufIndex;
	EMetalCommandEncoderType Type;
};
