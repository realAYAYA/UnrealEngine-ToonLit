// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12CommandContext.h: D3D12 Command Context Interfaces
=============================================================================*/

#pragma once
#define AFR_ENGINE_CHANGES_PRESENT WITH_MGPU

// TODO: Because the upper engine is yet to implement these interfaces we can't 'override' something that doesn't exist.
//       Remove when upper engine is ready
#if AFR_ENGINE_CHANGES_PRESENT
#define AFR_API_OVERRIDE override
#else
#define AFR_API_OVERRIDE
#endif

#include "D3D12RHIPrivate.h"
#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
#include <delayimp.h>

#if USE_PIX
	#include "pix3.h"
#endif
#include "Windows/HideWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_END

#include "RHICoreShader.h"
#include "RHICore.h"

struct FRayTracingShaderBindings;
class FD3D12Device;

struct FD3D12DeferredDeleteObject
{
	enum class EType
	{
		RHIObject,
		D3DObject,
		D3DHeap,
		BindlessDescriptor,
		CPUAllocation
	} Type;

	union
	{
		FD3D12Resource* RHIObject;
		FD3D12Heap* D3DHeap;
		ID3D12Object* D3DObject;

		struct
		{
			FRHIDescriptorHandle Handle;
			FD3D12Device* Device;
		} BindlessDescriptor;

		void* CPUAllocation;
	};

	explicit FD3D12DeferredDeleteObject(FD3D12Resource* RHIObject)
		: Type(EType::RHIObject)
		, RHIObject(RHIObject)
	{}

	explicit FD3D12DeferredDeleteObject(FD3D12Heap* D3DHeap)
		: Type(EType::D3DHeap)
		, D3DHeap(D3DHeap)
	{}

	explicit FD3D12DeferredDeleteObject(ID3D12Object* D3DObject)
		: Type(EType::D3DObject)
		, D3DObject(D3DObject)
	{}

	explicit FD3D12DeferredDeleteObject(FRHIDescriptorHandle Handle, FD3D12Device* Device)
		: Type(EType::BindlessDescriptor)
		, BindlessDescriptor({ Handle, Device })
	{}

	explicit FD3D12DeferredDeleteObject(void* Ptr, EType Type)
		: Type(Type)
		, CPUAllocation(Ptr)
	{
		check(Type == EType::CPUAllocation);
	}
};

enum class ED3D12Units
{
	Raw,
	Microseconds
};

enum class ED3D12FlushFlags
{
	None = 0,

	// Block the calling thread until the submission thread has dispatched all work.
	WaitForSubmission = 1,

	// Both the calling thread until the GPU has signaled completion of all dispatched work.
	WaitForCompletion = 2
};
ENUM_CLASS_FLAGS(ED3D12FlushFlags)

//
// Base class that manages the recording of FD3D12FinalizedCommands instances.
// Manages the logic for creating and recycling command lists and allocators.
//
class FD3D12ContextCommon
{
	friend class FScopedResourceBarrier;

protected:
	FD3D12ContextCommon(FD3D12Device* Device, ED3D12QueueType QueueType, bool bIsDefaultContext);

public:
	virtual ~FD3D12ContextCommon() = default;

protected:
	virtual void OpenCommandList();
	virtual void CloseCommandList();

public:
	enum class EClearStateMode
	{
		TransientOnly,
		All
	};

	virtual void ClearState(EClearStateMode ClearStateMode = EClearStateMode::All) {}

	// Inserts a command to signal the specified sync point
	void SignalSyncPoint(FD3D12SyncPoint* SyncPoint);

	// Inserts a command that blocks the GPU queue until the specified sync point is signaled.
	void WaitSyncPoint(FD3D12SyncPoint* SyncPoint);

	// Inserts a command that signals the specified D3D12 fence object.
	void SignalManualFence(ID3D12Fence* Fence, uint64 Value);

	// Inserts a command that waits the specified D3D12 fence object.
	void WaitManualFence(ID3D12Fence* Fence, uint64 Value);

	// Inserts a timestamp query command. "Target" specifies the optional 
	// location the result will be written to by the interrupt handler thread.
	FD3D12QueryLocation InsertTimestamp(ED3D12Units Units, uint64* Target);

	// Allocates a query of the specified type, returning its location.
	FD3D12QueryLocation AllocateQuery(ED3D12QueryType Type, uint64* Target);

	// Complete recording of the current command list set, and appends the resulting
	// payloads to the given array. Resets the context so new commands can be recorded.
	void Finalize(TArray<FD3D12Payload*>& OutPayloads);

	// The owner device of this context
	FD3D12Device* const Device;

	// The type of command lists this context records.
	ED3D12QueueType const QueueType;
	bool IsAsyncComputeContext() const { return QueueType == ED3D12QueueType::Async; }

	// True for the immediate context (@todo remove this)
	bool const bIsDefaultContext;
	bool IsDefaultContext() const { return bIsDefaultContext; }

	bool IsOpen() const { return CommandList != nullptr; }

	// Returns unique identity that can be used to distinguish between command lists even after they were recycled.
	uint64 GetCommandListID() { return GetCommandList().State.CommandListID; }

	FD3D12SyncPoint* GetContextSyncPoint()
	{
		if (!ContextSyncPoint)
		{
			ContextSyncPoint = FD3D12SyncPoint::Create(ED3D12SyncPointType::GPUAndCPU);
			BatchedSyncPoints.ToSignal.Add(ContextSyncPoint);
		}

		return ContextSyncPoint;
	}

	// Sync points which are waited at the start / signaled at the end 
	// of the whole batch of command lists this context recorded.
	struct
	{
		TArray<FD3D12SyncPointRef> ToWait;
		TArray<FD3D12SyncPointRef> ToSignal;
	} BatchedSyncPoints;

private:
	// Allocators to manage query heaps
	FD3D12QueryAllocator TimestampQueries, OcclusionQueries;

	// Batches resource barriers together until it's explicitly flushed
	FD3D12ResourceBarrierBatcher ResourceBarrierBatcher;

	// The active D3D12 command list where recorded D3D commands are directed.
	// This is swapped when command lists are split (e.g. when signalling a fence).
	FD3D12CommandList* CommandList = nullptr;

	// The command allocator used to open command lists within this context.
	// The allocator is reused for each new command list until the context is finalized.
	FD3D12CommandAllocator* CommandAllocator = nullptr;

	// The array of recorded payloads the submission thread will process.
	// These are returned when the context is finalized.
	TArray<FD3D12Payload*> Payloads;

	// A sync point signaled when all payloads in this context have completed.
	FD3D12SyncPointRef ContextSyncPoint;

	// Returns the current command list (or creates a new one if the command list was not open).
	FD3D12CommandList& GetCommandList()
	{
		if (!CommandList)
		{
			OpenCommandList();
		}

		return *CommandList;
	}

protected:
	enum class EPhase
	{
		Wait,
		Execute,
		Signal
	} CurrentPhase = EPhase::Wait;

	FD3D12Payload* GetPayload(EPhase Phase)
	{
		if (Payloads.Num() == 0 || Phase < CurrentPhase)
			Payloads.Add(new FD3D12Payload(Device, QueueType));

		CurrentPhase = Phase;
		return Payloads.Last();
	}

	uint32 ActiveQueries = 0;

public:
	// Flushes any pending commands in this context to the GPU.
	void FlushCommands(ED3D12FlushFlags FlushFlags = ED3D12FlushFlags::None);

	// Closes the current command list if the number of enqueued commands exceeds
	// the threshold defined by the "D3D12.MaxCommandsPerCommandList" cvar.
	void ConditionalSplitCommandList();

	auto BaseCommandList      () { return GetCommandList().BaseCommandList(); }
	auto CopyCommandList      () { return GetCommandList().CopyCommandList(); }
	auto GraphicsCommandList  () { return GetCommandList().GraphicsCommandList(); }
#if D3D12_MAX_COMMANDLIST_INTERFACE >= 1					    
	auto GraphicsCommandList1 () { return GetCommandList().GraphicsCommandList1(); }
#endif														    
#if D3D12_MAX_COMMANDLIST_INTERFACE >= 2					    
	auto GraphicsCommandList2 () { return GetCommandList().GraphicsCommandList2(); }
#endif														    
#if D3D12_MAX_COMMANDLIST_INTERFACE >= 3					    
	auto GraphicsCommandList3 () { return GetCommandList().GraphicsCommandList3(); }
#endif														    
#if D3D12_MAX_COMMANDLIST_INTERFACE >= 4					    
	auto GraphicsCommandList4 () { return GetCommandList().GraphicsCommandList4(); }
#endif														    
#if D3D12_MAX_COMMANDLIST_INTERFACE >= 5					    
	auto GraphicsCommandList5 () { return GetCommandList().GraphicsCommandList5(); }
#endif														    
#if D3D12_MAX_COMMANDLIST_INTERFACE >= 6					    
	auto GraphicsCommandList6 () { return GetCommandList().GraphicsCommandList6(); }
#endif														    
#if D3D12_PLATFORM_SUPPORTS_ASSERTRESOURCESTATES			    
	auto DebugCommandList     () { return GetCommandList().DebugCommandList(); }
#endif
#if D3D12_RHI_RAYTRACING
	auto RayTracingCommandList() { return GetCommandList().RayTracingCommandList(); }
#endif
#if NV_AFTERMATH
	auto AftermathHandle      () { return GetCommandList().AftermathHandle(); }
#endif

	void BeginQuery(FD3D12QueryLocation const& Location) { GetCommandList().BeginQuery(Location); }
	void EndQuery  (FD3D12QueryLocation const& Location) { GetCommandList().EndQuery  (Location); }

#if ENABLE_RESIDENCY_MANAGEMENT
	void UpdateResidency(TConstArrayView<FD3D12ResidencyHandle*> Handles) { GetCommandList().UpdateResidency(Handles); }
	void UpdateResidency(FD3D12ResidencyHandle& Handle                  ) { GetCommandList().UpdateResidency({ &Handle }); }
	void UpdateResidency(FD3D12ResidencyHandle* Handle                  ) { check(Handle  ); GetCommandList().UpdateResidency({ Handle }); }
	void UpdateResidency(FD3D12Resource* Resource                       ) { check(Resource); GetCommandList().UpdateResidency({ &Resource->GetResidencyHandle() }); }
#else
	void UpdateResidency(TConstArrayView<FD3D12ResidencyHandle*> Handles) { }
	void UpdateResidency(FD3D12ResidencyHandle& Handle                  ) { }
	void UpdateResidency(FD3D12ResidencyHandle* Handle                  ) { }
	void UpdateResidency(FD3D12Resource* Resource                       ) { }
#endif

	// Pending resource barriers are resolved by a dedicated barrier command list, generated during command list submission.
	void AddPendingResourceBarrier(FD3D12Resource* Resource, D3D12_RESOURCE_STATES After, uint32 SubResource, CResourceState& ResourceState_OnCommandList);

	// Resource transition / barrier functions. These get batched and recorded into the command list when FlushResourceBarriers() is called.
	void AddTransitionBarrier(FD3D12Resource* pResource, D3D12_RESOURCE_STATES Before, D3D12_RESOURCE_STATES After, uint32 Subresource, CResourceState* ResourceState_OnCommandList = nullptr);
	void AddAliasingBarrier(ID3D12Resource* InResourceBefore, ID3D12Resource* InResourceAfter);
	void AddUAVBarrier();

	// Flushes the batched resource barriers to the current command list
	void FlushResourceBarriers();

	// Helper functions for transitioning resource views when the previous state is unknown.
	// Uses a combination of pending barriers and command list state tracking to resolve the unknown previous state.
	void TransitionResource(class FD3D12DepthStencilView*    View);
	void TransitionResource(class FD3D12DepthStencilView*    View, D3D12_RESOURCE_STATES After);
	void TransitionResource(class FD3D12RenderTargetView*    View, D3D12_RESOURCE_STATES After);
	void TransitionResource(class FD3D12ShaderResourceView*  View, D3D12_RESOURCE_STATES After);
	void TransitionResource(class FD3D12UnorderedAccessView* View, D3D12_RESOURCE_STATES After);

	// Functions for transitioning a resource. The Before state may be D3D12_RESOURCE_STATE_TBD, in which case a
	// combination of pending barriers and command list state tracking is used to resolve the unknown previous state.
	bool TransitionResource(FD3D12Resource* Resource, D3D12_RESOURCE_STATES Before, D3D12_RESOURCE_STATES After, uint32 Subresource);
	bool TransitionResource(FD3D12Resource* Resource, D3D12_RESOURCE_STATES Before, D3D12_RESOURCE_STATES After, const CViewSubresourceSubset& SubresourceSubset);

private:
	// Private helper for transitioning resources
	bool TransitionResource(FD3D12Resource* InResource, CResourceState& ResourceState_OnCommandList, uint32 InSubresourceIndex, D3D12_RESOURCE_STATES InBeforeState, D3D12_RESOURCE_STATES InAfterState, bool bInForceAfterState);
};

//
// Context for the copy queue. Doesn't implement an RHI interface 
// since the copy queue is not directly exposed to the renderer.
//
class FD3D12ContextCopy final : public FD3D12ContextCommon
{
public:
	FD3D12ContextCopy(FD3D12Device* Device)
		: FD3D12ContextCommon(Device, ED3D12QueueType::Copy, false)
	{}
};

//
// Helper for recording and submitting copy queue work.
// Used for buffer / texture data upload etc.
//
class FD3D12CopyScope final
{
private:
	FD3D12Device* const Device;
	FD3D12SyncPointRef SyncPoint;

#if DO_CHECK
	mutable bool bSyncPointRetrieved = false;
#endif

public:
	FD3D12ContextCopy& Context;

	FD3D12SyncPoint* GetSyncPoint() const;

	FD3D12CopyScope(FD3D12Device* Device, ED3D12SyncPointType SyncPointType, FD3D12SyncPointRef const& WaitSyncPoint = {});
	~FD3D12CopyScope();
};

// Base class used to define commands that are not device specific, or that broadcast to all devices.
// @todo mgpu - try to remove this class
class FD3D12CommandContextBase : public IRHICommandContext, public FD3D12AdapterChild
{
public:
	FD3D12CommandContextBase(FD3D12Adapter* InParent, FRHIGPUMask InGPUMask);

	void RHIBeginDrawingViewport(FRHIViewport* Viewport, FRHITexture* RenderTargetRHI) final override;
	void RHIEndDrawingViewport(FRHIViewport* Viewport, bool bPresent, bool bLockToVsync) final override;

	void RHIEndFrame() final override;

	virtual void UpdateMemoryStats();

	FRHIGPUMask GetGPUMask() const { return GPUMask; }
	FRHIGPUMask GetPhysicalGPUMask() const { return PhysicalGPUMask; }

	virtual void RHISetAsyncComputeBudget(EAsyncComputeBudget Budget) {}

	bool IsDrawingSceneOrViewport() const {	return bDrawingScene || bDrawingViewport; }

	virtual class FD3D12CommandContextRedirector* AsRedirector() { return nullptr; }

	static FD3D12CommandContextBase& Get(FRHICommandListBase& RHICmdList)
	{
		return static_cast<FD3D12CommandContextBase&>(RHICmdList.GetComputeContext().GetLowestLevelContext());
	}

protected:
	friend class FD3D12CommandContext;
	virtual FD3D12CommandContext* GetContext(uint32 InGPUIndex) = 0;

	FRHIGPUMask GPUMask;
	FRHIGPUMask PhysicalGPUMask;

	bool bDrawingViewport = false;
	bool bDrawingScene = false;
};

// RHI Context type used for graphics and async compute command lists.
class FD3D12CommandContext : public FD3D12ContextCommon, public FD3D12CommandContextBase, public FD3D12DeviceChild
{
public:
	FD3D12CommandContext(class FD3D12Device* InParent, ED3D12QueueType QueueType, bool InIsDefaultContext);
	virtual ~FD3D12CommandContext();

	static FD3D12CommandContext& Get(FRHICommandListBase& RHICmdList, uint32 GPUIndex)
	{
		FD3D12CommandContextBase& Base = FD3D12CommandContextBase::Get(RHICmdList);
#if WITH_MGPU
		return *Base.GetContext(GPUIndex);
#else
		return static_cast<FD3D12CommandContext&>(Base);
#endif
	}

	virtual void OpenCommandList() override final;
	virtual void CloseCommandList() override final;

	virtual ERHIPipeline GetPipeline() const override
	{
		return QueueType == ED3D12QueueType::Direct
			? ERHIPipeline::Graphics
			: ERHIPipeline::AsyncCompute;
	}

	virtual void ClearState(EClearStateMode ClearStateMode = EClearStateMode::All) override final;
	void ConditionalClearShaderResource(FD3D12ResourceLocation* Resource);
	void ClearAllShaderResources();

	FD3D12FastConstantAllocator ConstantsAllocator;

	// Current GPU event stack
	TArray<uint32> GPUEventStack;

	FD3D12StateCache StateCache;

	/** Track the currently bound uniform buffers. */
	FD3D12UniformBuffer* BoundUniformBuffers[SF_NumStandardFrequencies][MAX_CBS] = {};
	FUniformBufferRHIRef BoundUniformBufferRefs[SF_NumStandardFrequencies][MAX_CBS] = {};

	/** Bit array to track which uniform buffers have changed since the last draw call. */
	uint16 DirtyUniformBuffers[SF_NumStandardFrequencies] = {};

	/** Handle for the dummy outer occlusion query we optionally insert for performance reasons */
	FRenderQueryRHIRef OuterOcclusionQuery;
	bool bOuterOcclusionQuerySubmitted = false;

	/** When a new graphics PSO is set, we discard all old constants set for the previous shader. */
	bool bDiscardSharedGraphicsConstants = false;

	/** When a new compute PSO is set, we discard all old constants set for the previous shader. */
	bool bDiscardSharedComputeConstants = false;

	bool bTrackingEvents = false;

	/** Used by variable rate shading to cache the current state of the combiners and the constant shading rate*/
#if PLATFORM_SUPPORTS_VARIABLE_RATE_SHADING
	static_assert(D3D12_RS_SET_SHADING_RATE_COMBINER_COUNT == ED3D12VRSCombinerStages::Num);
	D3D12_SHADING_RATE_COMBINER		VRSCombiners[D3D12_RS_SET_SHADING_RATE_COMBINER_COUNT] = { D3D12_SHADING_RATE_COMBINER_PASSTHROUGH, D3D12_SHADING_RATE_COMBINER_PASSTHROUGH };
	D3D12_SHADING_RATE				VRSShadingRate = D3D12_SHADING_RATE_1X1;
#endif

	D3D12_RESOURCE_STATES SkipFastClearEliminateState = D3D12_RESOURCE_STATES(0);
	D3D12_RESOURCE_STATES ValidResourceStates;

#if PLATFORM_SUPPORTS_VIRTUAL_TEXTURES
	bool bNeedFlushTextureCache = false;
	void InvalidateTextureCache() { bNeedFlushTextureCache = true; }
	inline void FlushTextureCacheIfNeeded()
	{
		if (bNeedFlushTextureCache)
		{
			FlushTextureCache();

			bNeedFlushTextureCache = false;
		}
	}
	virtual void FlushTextureCache() {};
#endif

	/** Constant buffers for Set*ShaderParameter calls. */
	FD3D12ConstantBuffer VSConstantBuffer;
	FD3D12ConstantBuffer MSConstantBuffer;
	FD3D12ConstantBuffer ASConstantBuffer;
	FD3D12ConstantBuffer PSConstantBuffer;
	FD3D12ConstantBuffer GSConstantBuffer;
	FD3D12ConstantBuffer CSConstantBuffer;

	/** needs to be called before each draw call */
	void CommitNonComputeShaderConstants();

	/** needs to be called before each dispatch call */
	void CommitComputeShaderConstants();

	template <class ShaderType> void SetResourcesFromTables(const ShaderType* RESTRICT);
	template <class ShaderType> uint32 SetUAVPSResourcesFromTables(const ShaderType* RESTRICT Shader);

	void CommitGraphicsResourceTables();
	void CommitComputeResourceTables();

	template<typename TPixelShader>
	void ResolveTextureUsingShader(
		FD3D12Texture* SourceTexture,
		FD3D12Texture* DestTexture,
		FD3D12RenderTargetView* DestSurfaceRTV,
		FD3D12DepthStencilView* DestSurfaceDSV,
		const D3D12_RESOURCE_DESC& ResolveTargetDesc,
		const FResolveRect& SourceRect,
		const FResolveRect& DestRect,
		typename TPixelShader::FParameter PixelShaderParameter
		);

	virtual void SetDepthBounds(float MinDepth, float MaxDepth);
	virtual void SetShadingRate(EVRSShadingRate ShadingRate, const TStaticArray<EVRSRateCombiner, ED3D12VRSCombinerStages::Num>& Combiners);
	virtual void SetShadingRateImage(FD3D12Resource* ShadingRateImage);

	virtual void SetAsyncComputeBudgetInternal(EAsyncComputeBudget Budget) {}

	virtual void RHIBeginTransitions(TArrayView<const FRHITransition*> Transitions) final override;
	virtual void RHIEndTransitions(TArrayView<const FRHITransition*> Transitions) final override;

	// IRHIComputeContext interface
	virtual void RHISetComputePipelineState(FRHIComputePipelineState* ComputePipelineState) final override;
	virtual void RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ) final override;
	virtual void RHIDispatchIndirectComputeShader(FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset) final override;
	virtual void RHISetStaticUniformBuffers(const FUniformBufferStaticBindings& InUniformBuffers) final override;
	virtual void RHISetShaderTexture(FRHIComputeShader* PixelShader, uint32 TextureIndex, FRHITexture* NewTexture) final override;
	virtual void RHISetShaderSampler(FRHIComputeShader* ComputeShader, uint32 SamplerIndex, FRHISamplerState* NewState) final override;
	virtual void RHISetUAVParameter(FRHIPixelShader* PixelShaderRHI, uint32 UAVIndex, FRHIUnorderedAccessView* UAVRHI) final override;
	virtual void RHISetUAVParameter(FRHIComputeShader* ComputeShader, uint32 UAVIndex, FRHIUnorderedAccessView* UAV) final override;
	virtual void RHISetUAVParameter(FRHIComputeShader* ComputeShader, uint32 UAVIndex, FRHIUnorderedAccessView* UAV, uint32 InitialCount) final override;
	virtual void RHISetShaderResourceViewParameter(FRHIComputeShader* ComputeShader, uint32 SamplerIndex, FRHIShaderResourceView* SRV) final override;
	virtual void RHISetShaderUniformBuffer(FRHIComputeShader* ComputeShader, uint32 BufferIndex, FRHIUniformBuffer* Buffer) final override;
	virtual void RHISetShaderParameter(FRHIComputeShader* ComputeShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) final override;
	virtual void RHIPushEvent(const TCHAR* Name, FColor Color) final override;
	virtual void RHIPopEvent() final override;
	virtual void RHISubmitCommandsHint() final override;

	// IRHICommandContext interface
	virtual void RHISetMultipleViewports(uint32 Count, const FViewportBounds* Data) final override;
	virtual void RHIClearUAVFloat(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FVector4f& Values) final override;
	virtual void RHIClearUAVUint(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FUintVector4& Values) final override;
	virtual void RHICopyToResolveTarget(FRHITexture* SourceTexture, FRHITexture* DestTexture, const FResolveParams& ResolveParams) override;
	virtual void RHICopyTexture(FRHITexture* SourceTexture, FRHITexture* DestTexture, const FRHICopyTextureInfo& CopyInfo) final override;
	virtual void RHICopyBufferRegion(FRHIBuffer* DestBuffer, uint64 DstOffset, FRHIBuffer* SourceBuffer, uint64 SrcOffset, uint64 NumBytes) final override;
	virtual void RHICopyToStagingBuffer(FRHIBuffer* SourceBuffer, FRHIStagingBuffer* DestinationStagingBuffer, uint32 Offset, uint32 NumBytes) final override;
	virtual void RHIBeginRenderQuery(FRHIRenderQuery* RenderQuery) final override;
	virtual void RHIEndRenderQuery(FRHIRenderQuery* RenderQuery) final override;
	virtual void RHICalibrateTimers(FRHITimestampCalibrationQuery* CalibrationQuery) final override;
	virtual void RHIBeginScene() final override;
	virtual void RHIEndScene() final override;
	virtual void RHISetStreamSource(uint32 StreamIndex, FRHIBuffer* VertexBuffer, uint32 Offset) final override;
	virtual void RHISetViewport(float MinX, float MinY, float MinZ, float MaxX, float MaxY, float MaxZ) final override;
	virtual void RHISetStereoViewport(float LeftMinX, float RightMinX, float LeftMinY, float RightMinY, float MinZ, float LeftMaxX, float RightMaxX, float LeftMaxY, float RightMaxY, float MaxZ) override;
	virtual void RHISetScissorRect(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY) final override;
	virtual void RHISetGraphicsPipelineState(FRHIGraphicsPipelineState* GraphicsPipelineState, uint32 StencilRef, bool bApplyAdditionalState) final override;
	virtual void RHISetShaderTexture(FRHIGraphicsShader* Shader, uint32 TextureIndex, FRHITexture* NewTexture) final override;
	virtual void RHISetShaderSampler(FRHIGraphicsShader* Shader, uint32 SamplerIndex, FRHISamplerState* NewState) final override;
	virtual void RHISetShaderResourceViewParameter(FRHIGraphicsShader* Shader, uint32 SamplerIndex, FRHIShaderResourceView* SRV) final override;
	virtual void RHISetShaderUniformBuffer(FRHIGraphicsShader* Shader, uint32 BufferIndex, FRHIUniformBuffer* Buffer) final override;
	virtual void RHISetShaderParameter(FRHIGraphicsShader* Shader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) final override;
	virtual void RHISetStencilRef(uint32 StencilRef) final override;
	virtual void RHISetBlendFactor(const FLinearColor& BlendFactor) final override;

	void SetRenderTargets(uint32 NumSimultaneousRenderTargets, const FRHIRenderTargetView* NewRenderTargets, const FRHIDepthRenderTargetView* NewDepthStencilTarget);
	void SetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& RenderTargetsInfo);

	virtual void RHIDrawPrimitive(uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances) final override;
	virtual void RHIDrawPrimitiveIndirect(FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset) final override;
	virtual void RHIDrawIndexedIndirect(FRHIBuffer* IndexBufferRHI, FRHIBuffer* ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances) final override;
	virtual void RHIDrawIndexedPrimitive(FRHIBuffer* IndexBuffer, int32 BaseVertexIndex, uint32 FirstInstance, uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances) final override;
	virtual void RHIDrawIndexedPrimitiveIndirect(FRHIBuffer* IndexBuffer, FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset) final override;
#if PLATFORM_SUPPORTS_MESH_SHADERS
	virtual void RHIDispatchMeshShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ) final override;
	virtual void RHIDispatchIndirectMeshShader(FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset) final override;
#endif
	virtual void RHISetDepthBounds(float MinDepth, float MaxDepth) final override;
    virtual void RHISetShadingRate(EVRSShadingRate ShadingRate, EVRSRateCombiner Combiner) final override;

	virtual void RHIClearMRTImpl(bool* bClearColorArray, int32 NumClearColors, const FLinearColor* ColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil);

	void RHIBeginFrame() final override;

	virtual void RHIBeginRenderPass(const FRHIRenderPassInfo& InInfo, const TCHAR* InName)
	{
		FRHISetRenderTargetsInfo RTInfo;
		InInfo.ConvertToRenderTargetsInfo(RTInfo);
		SetRenderTargetsAndClear(RTInfo);

		RenderPassInfo = InInfo;
	}

	virtual void RHIEndRenderPass()
	{
		UE::RHICore::ResolveRenderPassTargets(RenderPassInfo, [this](UE::RHICore::FResolveTextureInfo Info)
		{
			ResolveTexture(Info);
		});
	}

	void ResolveTexture(UE::RHICore::FResolveTextureInfo Info);

	// When using Alternate Frame Rendering some temporal effects i.e. effects which consume GPU work from previous frames must synchronize their resources
	// to prevent visual corruption.

	// This should be called right before the effect consumes it's temporal resources.
	virtual void RHIWaitForTemporalEffect(const FName& InEffectName) final AFR_API_OVERRIDE;

	// This should be called right after the effect generates the resources which will be used in subsequent frame(s).
	virtual void RHIBroadcastTemporalEffect(const FName& InEffectName, const TArrayView<FRHITexture*> InTextures) final AFR_API_OVERRIDE;
	virtual void RHIBroadcastTemporalEffect(const FName& InEffectName, const TArrayView<FRHIBuffer*> InBuffers) final AFR_API_OVERRIDE;

#if D3D12_RHI_RAYTRACING
	virtual void RHIBindAccelerationStructureMemory(FRHIRayTracingScene* Scene, FRHIBuffer* Buffer, uint32 BufferOffset) final override;
	virtual void BuildAccelerationStructuresInternal(const TArrayView<D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC> BuildDesc);
#if WITH_MGPU
	// Should be called before RHIBuildAccelerationStructures when multiple GPU support is present (for example, from FD3D12CommandContextRedirector::RHIBuildAccelerationStructures)
	static void UnregisterAccelerationStructuresInternalMGPU(const TArrayView<const FRayTracingGeometryBuildParams> Params, FRHIGPUMask GPUMask);
#endif
	virtual void RHIBuildAccelerationStructures(const TArrayView<const FRayTracingGeometryBuildParams> Params, const FRHIBufferRange& ScratchBufferRange) final override;
	virtual void RHIBuildAccelerationStructure(const FRayTracingSceneBuildParams& SceneBuildParams) final override;
	virtual void RHIClearRayTracingBindings(FRHIRayTracingScene* Scene) final override;
	virtual void RHIRayTraceOcclusion(FRHIRayTracingScene* Scene,
		FRHIShaderResourceView* Rays,
		FRHIUnorderedAccessView* Output,
		uint32 NumRays) final override;
	virtual void RHIRayTraceIntersection(FRHIRayTracingScene* Scene,
		FRHIShaderResourceView* Rays,
		FRHIUnorderedAccessView* Output,
		uint32 NumRays) final override;
	virtual void RHIRayTraceDispatch(FRHIRayTracingPipelineState* RayTracingPipelineState, FRHIRayTracingShader* RayGenShader,
		FRHIRayTracingScene* Scene,
		const FRayTracingShaderBindings& GlobalResourceBindings,
		uint32 Width, uint32 Height) final override;
	virtual void RHIRayTraceDispatchIndirect(FRHIRayTracingPipelineState* RayTracingPipelineState, FRHIRayTracingShader* RayGenShader,
		FRHIRayTracingScene* Scene,
		const FRayTracingShaderBindings& GlobalResourceBindings,
		FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset) final override;
	virtual void RHISetRayTracingBindings(
		FRHIRayTracingScene* Scene, FRHIRayTracingPipelineState* Pipeline,
		uint32 NumBindings, const FRayTracingLocalShaderBindings* Bindings, ERayTracingBindingType BindingType) final override;
	virtual void RHISetRayTracingHitGroup(
		FRHIRayTracingScene* Scene, uint32 InstanceIndex, uint32 SegmentIndex, uint32 ShaderSlot,
		FRHIRayTracingPipelineState* Pipeline, uint32 HitGroupIndex,
		uint32 NumUniformBuffers, FRHIUniformBuffer* const* UniformBuffers,
		uint32 LooseParameterDataSize, const void* LooseParameterData,
		uint32 UserData) final override;
	virtual void RHISetRayTracingCallableShader(
		FRHIRayTracingScene* Scene, uint32 ShaderSlotInScene,
		FRHIRayTracingPipelineState* Pipeline, uint32 ShaderIndexInPipeline,
		uint32 NumUniformBuffers, FRHIUniformBuffer* const* UniformBuffers,
		uint32 UserData) final override;
	virtual void RHISetRayTracingMissShader(
		FRHIRayTracingScene* Scene, uint32 ShaderSlotInScene,
		FRHIRayTracingPipelineState* Pipeline, uint32 ShaderIndexInPipeline,
		uint32 NumUniformBuffers, FRHIUniformBuffer* const* UniformBuffers,
		uint32 UserData) final override;
#endif // D3D12_RHI_RAYTRACING

	template<typename ObjectType, typename RHIType>
	static FORCEINLINE_DEBUGGABLE ObjectType* RetrieveObject(RHIType RHIObject, uint32 GPUIndex)
	{
		return FD3D12DynamicRHI::ResourceCast(RHIObject, GPUIndex);
	}

	template<typename ObjectType, typename RHIType>
	FORCEINLINE_DEBUGGABLE ObjectType* RetrieveObject(RHIType RHIObject)
	{
		return RetrieveObject<ObjectType, RHIType>(RHIObject, GetGPUIndex());
	}

	static inline FD3D12Texture* RetrieveTexture(FRHITexture* Texture, uint32 GPUIndex)
	{
		FD3D12Texture* RHITexture = GetD3D12TextureFromRHITexture(Texture);
		return RHITexture ? RHITexture->GetLinkedObject(GPUIndex) : nullptr;
	}

	FORCEINLINE_DEBUGGABLE FD3D12Texture* RetrieveTexture(FRHITexture* Texture)
	{
		return RetrieveTexture(Texture, GetGPUIndex());
	}

	uint32 GetGPUIndex() const { return GPUMask.ToIndex(); }

	virtual void RHISetGPUMask(FRHIGPUMask InGPUMask) final override
	{
		// This is a single-GPU context so it doesn't make sense to ever change its GPU
		// mask. If multiple GPUs are supported we should be using the redirector context.
		ensure(InGPUMask == GPUMask);
	}

protected:

	FD3D12CommandContext* GetContext(uint32 InGPUIndex) final override 
	{  
		return InGPUIndex == GetGPUIndex() ? this : nullptr; 
	}
	
	void WriteGPUEventStackToBreadCrumbData(bool bBeginEvent);

private:

#if WITH_MGPU
	template <typename TD3D12Resource, typename TCopyFunction>
	void BroadcastTemporalEffect(const FName& InEffectName, const TArrayView<TD3D12Resource*> InResources, const TCopyFunction& InCopyFunction);
#endif

	static void ClearUAV(TRHICommandList_RecursiveHazardous<FD3D12CommandContext>& RHICmdList, FD3D12UnorderedAccessView* UAV, const void* ClearValues, bool bFloat);

	template <typename TRHIShader>
	void ApplyStaticUniformBuffers(TRHIShader* Shader)
	{
		if (Shader)
		{
			UE::RHICore::ApplyStaticUniformBuffers(this, Shader, Shader->StaticSlots, Shader->ShaderResourceTable.ResourceTableLayoutHashes, StaticUniformBuffers);
		}
	}

	void HandleDiscardResources          (TArrayView<const FRHITransition*> Transitions, bool bIsBeginTransition);
	void HandleResourceTransitions       (const struct FD3D12TransitionData* TransitionData, bool& bUAVBarrier);
	void HandleTransientAliasing         (const struct FD3D12TransitionData* TransitionData);
	void HandleResourceDiscardTransitions(const struct FD3D12TransitionData* TransitionData, TArray<struct FD3D12DiscardResource>& ResourcesToDiscard);

	TArray<FRHIUniformBuffer*> StaticUniformBuffers;
};

// This class is a shim to get AFR working. Currently the upper engine only queries for the 'Immediate Context'
// once. However when in AFR we need to switch which context is active every frame so we return an instance of this class
// as the default context so that we can control when to swap which device we talk to.
// Because IRHICommandContext is pure virtual we can return the normal FD3D12CommandContext when not using mGPU thus there
// is no additional overhead for the common case i.e. 1 GPU.
class FD3D12CommandContextRedirector final : public FD3D12CommandContextBase
{
public:
	// The type of command lists this context records.
	ED3D12QueueType const QueueType;
	bool const bIsDefaultContext;

	FD3D12CommandContextRedirector(class FD3D12Adapter* InParent, ED3D12QueueType QueueType, bool InIsDefaultContext);

	virtual FD3D12CommandContextRedirector* AsRedirector() override { return this; }

#define ContextRedirect(Call) { for (uint32 GPUIndex : GPUMask) PhysicalContexts[GPUIndex]-> Call; }
#define ContextGPU0(Call) { PhysicalContexts[0]-> Call; }

	FORCEINLINE virtual void RHISetComputePipelineState(FRHIComputePipelineState* ComputePipelineState) final override
	{
		ContextRedirect(RHISetComputePipelineState(ComputePipelineState));
	}
	FORCEINLINE virtual void RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ) final override
	{
		ContextRedirect(RHIDispatchComputeShader(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ));
	}
	FORCEINLINE virtual void RHIDispatchIndirectComputeShader(FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset) final override
	{
		ContextRedirect(RHIDispatchIndirectComputeShader(ArgumentBuffer, ArgumentOffset));
	}

	FORCEINLINE virtual void RHIBeginTransitions(TArrayView<const FRHITransition*> Transitions) final override
	{
		ContextRedirect(RHIBeginTransitions(Transitions));
	}
	FORCEINLINE virtual void RHIEndTransitions(TArrayView<const FRHITransition*> Transitions) final override
	{
		ContextRedirect(RHIEndTransitions(Transitions));
	}

	virtual void RHITransferResources(const TArrayView<const FTransferResourceParams> Params) final override;
	virtual void RHITransferResourceSignal(const TArrayView<FTransferResourceFenceData* const> FenceDatas, FRHIGPUMask SrcGPUMask) final override;
	virtual void RHITransferResourceWait(const TArrayView<FTransferResourceFenceData* const> FenceDatas) final override;

	FORCEINLINE virtual void RHICopyToStagingBuffer(FRHIBuffer* SourceBuffer, FRHIStagingBuffer* DestinationStagingBuffer, uint32 Offset, uint32 NumBytes) final override
	{
		ContextRedirect(RHICopyToStagingBuffer(SourceBuffer, DestinationStagingBuffer, Offset, NumBytes));
	}
	FORCEINLINE virtual void RHISetShaderTexture(FRHIComputeShader* PixelShader, uint32 TextureIndex, FRHITexture* NewTexture) final override
	{
		ContextRedirect(RHISetShaderTexture(PixelShader, TextureIndex, NewTexture));
	}
	FORCEINLINE virtual void RHISetShaderSampler(FRHIComputeShader* ComputeShader, uint32 SamplerIndex, FRHISamplerState* NewState) final override
	{
		ContextRedirect(RHISetShaderSampler(ComputeShader, SamplerIndex, NewState));
	}
	FORCEINLINE virtual void RHISetUAVParameter(FRHIPixelShader* PixelShader, uint32 UAVIndex, FRHIUnorderedAccessView* UAV) final override
	{
		ContextRedirect(RHISetUAVParameter(PixelShader, UAVIndex, UAV));
	}
	FORCEINLINE virtual void RHISetUAVParameter(FRHIComputeShader* ComputeShader, uint32 UAVIndex, FRHIUnorderedAccessView* UAV) final override
	{
		ContextRedirect(RHISetUAVParameter(ComputeShader, UAVIndex, UAV));
	}
	FORCEINLINE virtual void RHISetUAVParameter(FRHIComputeShader* ComputeShader, uint32 UAVIndex, FRHIUnorderedAccessView* UAV, uint32 InitialCount) final override
	{
		ContextRedirect(RHISetUAVParameter(ComputeShader, UAVIndex, UAV, InitialCount));
	}
	FORCEINLINE virtual void RHISetShaderResourceViewParameter(FRHIComputeShader* ComputeShader, uint32 SamplerIndex, FRHIShaderResourceView* SRV) final override
	{
		ContextRedirect(RHISetShaderResourceViewParameter(ComputeShader, SamplerIndex, SRV));
	}
	FORCEINLINE virtual void RHISetShaderUniformBuffer(FRHIComputeShader* ComputeShader, uint32 BufferIndex, FRHIUniformBuffer* Buffer) final override
	{
		ContextRedirect(RHISetShaderUniformBuffer(ComputeShader, BufferIndex, Buffer));
	}
	FORCEINLINE virtual void RHISetShaderParameter(FRHIComputeShader* ComputeShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) final override
	{
		ContextRedirect(RHISetShaderParameter(ComputeShader, BufferIndex, BaseIndex, NumBytes, NewValue));
	}
	FORCEINLINE virtual void RHIPushEvent(const TCHAR* Name, FColor Color) final override
	{
		ContextRedirect(RHIPushEvent(Name, Color));
	}
	FORCEINLINE virtual void RHIPopEvent() final override
	{
		ContextRedirect(RHIPopEvent());
	}
	FORCEINLINE virtual void RHISubmitCommandsHint() final override
	{
		ContextRedirect(RHISubmitCommandsHint());
	}

	// IRHICommandContext interface
	FORCEINLINE virtual void RHISetMultipleViewports(uint32 Count, const FViewportBounds* Data) final override
	{
		ContextRedirect(RHISetMultipleViewports(Count, Data));
	}
	FORCEINLINE virtual void RHIClearUAVFloat(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FVector4f& Values) final override
	{
		ContextRedirect(RHIClearUAVFloat(UnorderedAccessViewRHI, Values));
	}
	FORCEINLINE virtual void RHIClearUAVUint(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FUintVector4& Values) final override
	{
		ContextRedirect(RHIClearUAVUint(UnorderedAccessViewRHI, Values));
	}
	FORCEINLINE virtual void RHICopyToResolveTarget(FRHITexture* SourceTexture, FRHITexture* DestTexture, const FResolveParams& ResolveParams) final override
	{
		ContextRedirect(RHICopyToResolveTarget(SourceTexture, DestTexture, ResolveParams));
	}
	FORCEINLINE virtual void RHICopyTexture(FRHITexture* SourceTextureRHI, FRHITexture* DestTextureRHI, const FRHICopyTextureInfo& CopyInfo) final override
	{
		ContextRedirect(RHICopyTexture(SourceTextureRHI, DestTextureRHI, CopyInfo));
	}
	FORCEINLINE virtual void RHICopyBufferRegion(FRHIBuffer* DestBuffer, uint64 DstOffset, FRHIBuffer* SourceBuffer, uint64 SrcOffset, uint64 NumBytes) final override
	{
		ContextRedirect(RHICopyBufferRegion(DestBuffer, DstOffset, SourceBuffer, SrcOffset, NumBytes));
	}
	FORCEINLINE virtual void RHIBeginRenderQuery(FRHIRenderQuery* RenderQuery) final override
	{
		ContextRedirect(RHIBeginRenderQuery(RenderQuery));
	}
	FORCEINLINE virtual void RHIEndRenderQuery(FRHIRenderQuery* RenderQuery) final override
	{
		ContextRedirect(RHIEndRenderQuery(RenderQuery));
	}
	FORCEINLINE virtual void RHICalibrateTimers(FRHITimestampCalibrationQuery* CalibrationQuery) final override
	{
		ContextRedirect(RHICalibrateTimers(CalibrationQuery));
	}
	FORCEINLINE virtual void RHIBeginScene() final override
	{
		ContextRedirect(RHIBeginScene());
	}
	FORCEINLINE virtual void RHIEndScene() final override
	{
		ContextRedirect(RHIEndScene());
	}
	FORCEINLINE virtual void RHISetStreamSource(uint32 StreamIndex, FRHIBuffer* VertexBuffer, uint32 Offset) final override
	{
		ContextRedirect(RHISetStreamSource(StreamIndex, VertexBuffer, Offset));
	}
	FORCEINLINE virtual void RHISetViewport(float MinX, float MinY, float MinZ, float MaxX, float MaxY, float MaxZ) final override
	{
		ContextRedirect(RHISetViewport(MinX, MinY, MinZ, MaxX, MaxY, MaxZ));
	}
	FORCEINLINE virtual void RHISetStereoViewport(float LeftMinX, float RightMinX, float LeftMinY, float RightMinY, float MinZ, float LeftMaxX, float RightMaxX, float LeftMaxY, float RightMaxY, float MaxZ) override
	{
		ContextRedirect(RHISetStereoViewport(LeftMinX, RightMinX, LeftMinY, RightMinY, MinZ, LeftMaxX, RightMaxX, LeftMaxY, RightMaxY, MaxZ));
	}

	FORCEINLINE virtual void RHISetScissorRect(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY) final override
	{
		ContextRedirect(RHISetScissorRect(bEnable, MinX, MinY, MaxX, MaxY));
	}
	FORCEINLINE void RHISetGraphicsPipelineState(FRHIGraphicsPipelineState* GraphicsPipelineState, uint32 StencilRef, bool bApplyAdditionalState) final override
	{
		ContextRedirect(RHISetGraphicsPipelineState(GraphicsPipelineState, StencilRef, bApplyAdditionalState));
	}
	FORCEINLINE virtual void RHISetShaderTexture(FRHIGraphicsShader* Shader, uint32 TextureIndex, FRHITexture* NewTexture) final override
	{
		ContextRedirect(RHISetShaderTexture(Shader, TextureIndex, NewTexture));
	}
	FORCEINLINE virtual void RHISetShaderSampler(FRHIGraphicsShader* Shader, uint32 SamplerIndex, FRHISamplerState* NewState) final override
	{
		ContextRedirect(RHISetShaderSampler(Shader, SamplerIndex, NewState));
	}
	FORCEINLINE virtual void RHISetShaderResourceViewParameter(FRHIGraphicsShader* Shader, uint32 SamplerIndex, FRHIShaderResourceView* SRV) final override
	{
		ContextRedirect(RHISetShaderResourceViewParameter(Shader, SamplerIndex, SRV));
	}
	FORCEINLINE virtual void RHISetStaticUniformBuffers(const FUniformBufferStaticBindings& InUniformBuffers) final override
	{
		ContextRedirect(RHISetStaticUniformBuffers(InUniformBuffers));
	}
	FORCEINLINE virtual void RHISetShaderUniformBuffer(FRHIGraphicsShader* Shader, uint32 BufferIndex, FRHIUniformBuffer* Buffer) final override
	{
		ContextRedirect(RHISetShaderUniformBuffer(Shader, BufferIndex, Buffer));
	}
	FORCEINLINE virtual void RHISetShaderParameter(FRHIGraphicsShader* Shader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) final override
	{
		ContextRedirect(RHISetShaderParameter(Shader, BufferIndex, BaseIndex, NumBytes, NewValue));
	}
	FORCEINLINE virtual void RHISetStencilRef(uint32 StencilRef) final override
	{
		ContextRedirect(RHISetStencilRef(StencilRef));
	}
	FORCEINLINE void RHISetBlendFactor(const FLinearColor& BlendFactor) final override
	{
		ContextRedirect(RHISetBlendFactor(BlendFactor));
	}
	FORCEINLINE virtual void RHIDrawPrimitive(uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances) final override
	{
		ContextRedirect(RHIDrawPrimitive(BaseVertexIndex, NumPrimitives, NumInstances));
	}
	FORCEINLINE virtual void RHIDrawPrimitiveIndirect(FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset) final override
	{
		ContextRedirect(RHIDrawPrimitiveIndirect(ArgumentBuffer, ArgumentOffset));
	}
	FORCEINLINE virtual void RHIDrawIndexedIndirect(FRHIBuffer* IndexBufferRHI, FRHIBuffer* ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances) final override
	{
		ContextRedirect(RHIDrawIndexedIndirect(IndexBufferRHI, ArgumentsBufferRHI, DrawArgumentsIndex, NumInstances));
	}
	FORCEINLINE virtual void RHIDrawIndexedPrimitive(FRHIBuffer* IndexBuffer, int32 BaseVertexIndex, uint32 FirstInstance, uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances) final override
	{
		ContextRedirect(RHIDrawIndexedPrimitive(IndexBuffer, BaseVertexIndex, FirstInstance, NumVertices, StartIndex, NumPrimitives, NumInstances));
	}
	FORCEINLINE virtual void RHIDrawIndexedPrimitiveIndirect(FRHIBuffer* IndexBuffer, FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset) final override
	{
		ContextRedirect(RHIDrawIndexedPrimitiveIndirect(IndexBuffer, ArgumentBuffer, ArgumentOffset));
	}
#if PLATFORM_SUPPORTS_MESH_SHADERS
	FORCEINLINE virtual void RHIDispatchMeshShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ) final override
	{
		ContextRedirect(RHIDispatchMeshShader(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ));
	}
	FORCEINLINE virtual void RHIDispatchIndirectMeshShader(FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset) final override
	{
		ContextRedirect(RHIDispatchIndirectMeshShader(ArgumentBuffer, ArgumentOffset));
	}
#endif
	FORCEINLINE virtual void RHISetDepthBounds(float MinDepth, float MaxDepth) final override
	{
		ContextRedirect(RHISetDepthBounds(MinDepth, MaxDepth));
	}
	
	FORCEINLINE virtual void RHISetShadingRate(EVRSShadingRate ShadingRate, EVRSRateCombiner Combiner) final override
	{
		ContextRedirect(RHISetShadingRate(ShadingRate, Combiner));
	}

	FORCEINLINE virtual void RHIWaitForTemporalEffect(const FName& InEffectName) final AFR_API_OVERRIDE
	{
		ContextRedirect(RHIWaitForTemporalEffect(InEffectName));
	}

	FORCEINLINE virtual void RHIBroadcastTemporalEffect(const FName& InEffectName, const TArrayView<FRHITexture*> InTextures) final AFR_API_OVERRIDE
	{
		ContextRedirect(RHIBroadcastTemporalEffect(InEffectName, InTextures));
	}

	FORCEINLINE virtual void RHIBroadcastTemporalEffect(const FName& InEffectName, const TArrayView<FRHIBuffer*> InBuffers) final AFR_API_OVERRIDE
	{
		ContextRedirect(RHIBroadcastTemporalEffect(InEffectName, InBuffers));
	}

	FORCEINLINE virtual void RHIBeginFrame() final override
	{
		ContextRedirect(RHIBeginFrame());
	}

	virtual void RHIBeginRenderPass(const FRHIRenderPassInfo& InInfo, const TCHAR* InName) final override
	{
		ContextRedirect(RHIBeginRenderPass(InInfo, InName));
	}

	virtual void RHIEndRenderPass() final override
	{
		ContextRedirect(RHIEndRenderPass());
	}

	virtual void RHIBuildAccelerationStructures(const TArrayView<const FRayTracingGeometryBuildParams> Params, const FRHIBufferRange& ScratchBufferRange) final override
	{
#if WITH_MGPU
		FD3D12CommandContext::UnregisterAccelerationStructuresInternalMGPU(Params, GPUMask);
#endif 

		ContextRedirect(RHIBuildAccelerationStructures(Params, ScratchBufferRange));
	}

	virtual void RHIBuildAccelerationStructure(const FRayTracingSceneBuildParams& SceneBuildParams) final override
	{
		ContextRedirect(RHIBuildAccelerationStructure(SceneBuildParams));
	}

	virtual void RHIRayTraceOcclusion(FRHIRayTracingScene* Scene,
		FRHIShaderResourceView* Rays,
		FRHIUnorderedAccessView* Output,
		uint32 NumRays) final override
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		ContextRedirect(RHIRayTraceOcclusion(Scene, Rays, Output, NumRays));
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	virtual void RHIRayTraceIntersection(FRHIRayTracingScene* Scene,
		FRHIShaderResourceView* Rays,
		FRHIUnorderedAccessView* Output,
		uint32 NumRays) final override
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		ContextRedirect(RHIRayTraceIntersection(Scene, Rays, Output, NumRays));
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	virtual void RHIRayTraceDispatch(FRHIRayTracingPipelineState* RayTracingPipelineState, FRHIRayTracingShader* RayGenShader,
		FRHIRayTracingScene* Scene,
		const FRayTracingShaderBindings& GlobalResourceBindings,
		uint32 Width, uint32 Height) final override
	{
		ContextRedirect(RHIRayTraceDispatch(RayTracingPipelineState, RayGenShader, Scene, GlobalResourceBindings, Width, Height));
	}

	virtual void RHIRayTraceDispatchIndirect(FRHIRayTracingPipelineState* RayTracingPipelineState, FRHIRayTracingShader* RayGenShader,
		FRHIRayTracingScene* Scene,
		const FRayTracingShaderBindings& GlobalResourceBindings,
		FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset) final override
	{
		ContextRedirect(RHIRayTraceDispatchIndirect(RayTracingPipelineState, RayGenShader, Scene, GlobalResourceBindings, ArgumentBuffer, ArgumentOffset));
	}

	virtual void RHISetRayTracingHitGroup(
		FRHIRayTracingScene* Scene, uint32 InstanceIndex, uint32 SegmentIndex, uint32 ShaderSlot,
		FRHIRayTracingPipelineState* Pipeline, uint32 HitGroupIndex,
		uint32 NumUniformBuffers, FRHIUniformBuffer* const* UniformBuffers,
		uint32 LooseParameterDataSize, const void* LooseParameterData,
		uint32 UserData) final override
	{
		ContextRedirect(RHISetRayTracingHitGroup(Scene, InstanceIndex, SegmentIndex, ShaderSlot, Pipeline, HitGroupIndex, NumUniformBuffers, UniformBuffers, LooseParameterDataSize, LooseParameterData, UserData));
	}

	virtual void RHISetRayTracingBindings(FRHIRayTracingScene* Scene, FRHIRayTracingPipelineState* Pipeline, uint32 NumBindings, const FRayTracingLocalShaderBindings* Bindings, ERayTracingBindingType BindingType) final override
	{
		ContextRedirect(RHISetRayTracingBindings(Scene, Pipeline, NumBindings, Bindings, BindingType));
	}

	virtual void RHISetRayTracingCallableShader(
		FRHIRayTracingScene* Scene, uint32 ShaderSlotInScene,
		FRHIRayTracingPipelineState* Pipeline, uint32 ShaderIndexInPipeline,
		uint32 NumUniformBuffers, FRHIUniformBuffer* const* UniformBuffers,
		uint32 UserData) final override
	{
		ContextRedirect(RHISetRayTracingCallableShader(Scene, ShaderSlotInScene, Pipeline, ShaderIndexInPipeline, NumUniformBuffers, UniformBuffers, UserData));
	}

	virtual void RHISetRayTracingMissShader(
		FRHIRayTracingScene* Scene, uint32 ShaderSlotInScene,
		FRHIRayTracingPipelineState* Pipeline, uint32 ShaderIndexInPipeline,
		uint32 NumUniformBuffers, FRHIUniformBuffer* const* UniformBuffers,
		uint32 UserData) final override
	{
		ContextRedirect(RHISetRayTracingMissShader(Scene, ShaderSlotInScene, Pipeline, ShaderIndexInPipeline, NumUniformBuffers, UniformBuffers, UserData));
	}

	virtual void RHISetGPUMask(FRHIGPUMask InGPUMask) final override
	{
		GPUMask = InGPUMask;
		check(PhysicalGPUMask.ContainsAll(GPUMask));
	}

	virtual FRHIGPUMask RHIGetGPUMask() const final override
	{
		return GPUMask;
	}

	// Sets the mask of which GPUs can be supported, as opposed to the currently active
	// set. RHISetGPUMask checks that the active mask is a subset of the physical mask.
	FORCEINLINE void SetPhysicalGPUMask(FRHIGPUMask InGPUMask)
	{
		PhysicalGPUMask = InGPUMask;
	}

	virtual void RHIClearRayTracingBindings(FRHIRayTracingScene* Scene) final override
	{
		ContextRedirect(RHIClearRayTracingBindings(Scene));
	}

	virtual void RHIBindAccelerationStructureMemory(FRHIRayTracingScene* Scene, FRHIBuffer* Buffer, uint32 BufferOffset) final override
	{
		ContextRedirect(RHIBindAccelerationStructureMemory(Scene, Buffer, BufferOffset));
	}

	FORCEINLINE void SetPhysicalContext(FD3D12CommandContext* Context)
	{
		check(Context);
		const uint32 GPUIndex = Context->GetGPUIndex();
		check(PhysicalGPUMask.Contains(GPUIndex));
		PhysicalContexts[GPUIndex] = Context;
	}

	FORCEINLINE FD3D12CommandContext* GetContext(uint32 GPUIndex) final override
	{
		return PhysicalContexts[GPUIndex];
	}

private:
	TStaticArray<FD3D12CommandContext*, MAX_NUM_GPUS> PhysicalContexts;
};

struct FD3D12TransitionData
{
	ERHIPipeline SrcPipelines, DstPipelines;
	ERHITransitionCreateFlags CreateFlags = ERHITransitionCreateFlags::None;

	TArray<FRHITransitionInfo, TInlineAllocator<4, FConcurrentLinearArrayAllocator>> TransitionInfos;
	TArray<FRHITransientAliasingInfo, TInlineAllocator<4, FConcurrentLinearArrayAllocator>> AliasingInfos;
	TArray<FRHITransientAliasingOverlap, TInlineAllocator<4, FConcurrentLinearArrayAllocator>> AliasingOverlaps;

	TArray<TRHIPipelineArray<FD3D12SyncPointRef>, TInlineAllocator<MAX_NUM_GPUS>> SyncPoints;

	bool bCrossPipeline = false;
};