// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12Commands.cpp: D3D RHI commands implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"
#include "ProfilingDebugging/AssetMetadataTrace.h"
#include "ProfilingDebugging/RealtimeGPUProfiler.h"

static int32 GD3D12TransientAllocatorFullAliasingBarrier = 0;
static FAutoConsoleVariableRef CVarD3D12TransientAllocatorFullAliasingBarrier(
	TEXT("d3d12.TransientAllocator.FullAliasingBarrier"),
	GD3D12TransientAllocatorFullAliasingBarrier,
	TEXT("Inserts a full aliasing barrier on an transient acquire operation. Useful to debug if an aliasing barrier is missing."),
	ECVF_RenderThreadSafe);

static int32 GD3D12AllowDiscardResources = 1;
static FAutoConsoleVariableRef CVarD3D12AllowDiscardResources(
	TEXT("d3d12.AllowDiscardResources"),
	GD3D12AllowDiscardResources,
	TEXT("Whether to call DiscardResources after transient aliasing acquire. This is not needed on some platforms if newly acquired resources are cleared before use."),
	ECVF_RenderThreadSafe);

using namespace D3D12RHI;

inline void ValidateBoundShader(FD3D12StateCache& InStateCache, FRHIShader* InShaderRHI)
{
#if DO_CHECK
	const EShaderFrequency ShaderFrequency = InShaderRHI->GetFrequency();
	FRHIShader* CachedShader = InStateCache.GetShader(ShaderFrequency);
	ensureMsgf(CachedShader == InShaderRHI, TEXT("Parameters are being set for a %sShader which is not currently bound"), GetShaderFrequencyString(ShaderFrequency, false));
#endif
}

inline FD3D12ShaderData* GetShaderData(FRHIShader* InShaderRHI)
{
	switch (InShaderRHI->GetFrequency())
	{
	case SF_Vertex:        return FD3D12DynamicRHI::ResourceCast(static_cast<FRHIVertexShader*>(InShaderRHI));
#if PLATFORM_SUPPORTS_MESH_SHADERS
	case SF_Mesh:          return FD3D12DynamicRHI::ResourceCast(static_cast<FRHIMeshShader*>(InShaderRHI));
	case SF_Amplification: return FD3D12DynamicRHI::ResourceCast(static_cast<FRHIAmplificationShader*>(InShaderRHI));
#endif
	case SF_Pixel:         return FD3D12DynamicRHI::ResourceCast(static_cast<FRHIPixelShader*>(InShaderRHI));
	case SF_Geometry:      return FD3D12DynamicRHI::ResourceCast(static_cast<FRHIGeometryShader*>(InShaderRHI));
	case SF_Compute:       return FD3D12DynamicRHI::ResourceCast(static_cast<FRHIComputeShader*>(InShaderRHI));
	}
	return nullptr;
}

inline void ValidateBoundUniformBuffer(FD3D12UniformBuffer * InUniformBuffer, FRHIShader* InShaderRHI, uint32 InBufferIndex)
{
#if DO_CHECK
	if (FD3D12ShaderData* ShaderData = GetShaderData(InShaderRHI))
	{
		if (InBufferIndex < (uint32)ShaderData->ShaderResourceTable.ResourceTableLayoutHashes.Num())
		{
			uint32 UniformBufferHash = InUniformBuffer->GetLayout().GetHash();
			uint32 ShaderTableHash = ShaderData->ShaderResourceTable.ResourceTableLayoutHashes[InBufferIndex];
			ensureMsgf(ShaderTableHash == 0 || UniformBufferHash == ShaderTableHash,
				TEXT("Invalid uniform buffer %s bound on %sShader at index %d."),
				*(InUniformBuffer->GetLayout().GetDebugName()),
				GetShaderFrequencyString(InShaderRHI->GetFrequency(), false),
				InBufferIndex);
		}
	}
#endif
}

static void BindUniformBuffer(FD3D12CommandContext& Context, FRHIShader* Shader, EShaderFrequency ShaderFrequency, uint32 BufferIndex, FD3D12UniformBuffer* InBuffer)
{
	ValidateBoundUniformBuffer(InBuffer, Shader, BufferIndex);

	Context.StateCache.SetConstantsFromUniformBuffer(ShaderFrequency, BufferIndex, InBuffer);

	Context.BoundUniformBuffers[ShaderFrequency][BufferIndex] = InBuffer;
	Context.DirtyUniformBuffers[ShaderFrequency] |= (1 << BufferIndex);
}

void FD3D12CommandContext::FlushPendingDescriptorUpdates()
{
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
	GetParentDevice()->GetBindlessDescriptorManager().FlushPendingDescriptorUpdates(*this);
#endif
}

// Vertex state.
void FD3D12CommandContext::RHISetStreamSource(uint32 StreamIndex, FRHIBuffer* VertexBufferRHI, uint32 Offset)
{
	FD3D12Buffer* VertexBuffer = RetrieveObject<FD3D12Buffer>(VertexBufferRHI);

	StateCache.SetStreamSource(VertexBuffer ? &VertexBuffer->ResourceLocation : nullptr, StreamIndex, Offset);
}

void FD3D12CommandContext::SetupDispatch(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{
	if (IsDefaultContext())
	{
		GetParentDevice()->RegisterGPUDispatch(FIntVector(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ));
	}

	CommitComputeShaderConstants();
	CommitComputeResourceTables();

	StateCache.ApplyState(GetPipeline(), ED3D12PipelineType::Compute);

	FlushPendingDescriptorUpdates();
}

FD3D12ResourceLocation& FD3D12CommandContext::SetupIndirectArgument(FRHIBuffer* ArgumentBufferRHI, D3D12_RESOURCE_STATES ExtraStates)
{
	FD3D12ResourceLocation& ArgumentBufferLocation = RetrieveObject<FD3D12Buffer>(ArgumentBufferRHI)->ResourceLocation;

	// Indirect args buffer can be a previously pending UAV, which becomes PS\Non-PS read. ApplyState will flush pending transitions, so enqueue the indirect arg transition and flush here.
	TransitionResource(ArgumentBufferLocation.GetResource(), D3D12_RESOURCE_STATE_TBD, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT | ExtraStates, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

	// Must flush so the desired state is actually set.
	FlushResourceBarriers();

	UpdateResidency(ArgumentBufferLocation.GetResource());

	return ArgumentBufferLocation;
}

void FD3D12CommandContext::PostGpuEvent()
{
	ConditionalSplitCommandList();
	DEBUG_EXECUTE_COMMAND_LIST(this);
}

void FD3D12CommandContext::RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{
	SetupDispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);

	GraphicsCommandList()->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
	
	PostGpuEvent();
}

void FD3D12CommandContext::RHIDispatchIndirectComputeShader(FRHIBuffer* ArgumentBufferRHI, uint32 ArgumentOffset)
{
	SetupDispatch(1, 1, 1);
	
	FD3D12ResourceLocation& ArgumentBufferLocation = SetupIndirectArgument(ArgumentBufferRHI, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	FD3D12Adapter* Adapter = GetParentDevice()->GetParentAdapter();
	ID3D12CommandSignature* CommandSignature = IsAsyncComputeContext()
		? Adapter->GetDispatchIndirectComputeCommandSignature()
		: Adapter->GetDispatchIndirectGraphicsCommandSignature();
	
	GraphicsCommandList()->ExecuteIndirect(
		CommandSignature,
		1,
		ArgumentBufferLocation.GetResource()->GetResource(),
		ArgumentBufferLocation.GetOffsetFromBaseOfResource() + ArgumentOffset,
		NULL,
		0
	);
	
	PostGpuEvent();
}

template <typename FunctionType>
void EnumerateSubresources(FD3D12Resource* Resource, const FRHITransitionInfo& Info, FD3D12Texture* Texture, FunctionType Function)
{
	uint32 FirstMipSlice = 0;
	uint32 FirstArraySlice = 0;
	uint32 FirstPlaneSlice = 0;

	uint32 MipCount = Resource->GetMipLevels();
	uint32 ArraySize = Resource->GetArraySize();
	uint32 PlaneCount = Resource->GetPlaneCount();

	uint32 IterationMipCount = MipCount;
	uint32 IterationArraySize = ArraySize;
	uint32 IterationPlaneCount = PlaneCount;

	if (!Info.IsAllMips())
	{
		FirstMipSlice = Info.MipIndex;
		IterationMipCount = 1;
	}

	if (!Info.IsAllArraySlices())
	{
		FirstArraySlice = Info.ArraySlice;
		IterationArraySize = 1;
	}

	if (!Info.IsAllPlaneSlices())
	{
		FirstPlaneSlice = Info.PlaneSlice;
		IterationPlaneCount = 1;
	}

	for (uint32 PlaneSlice = FirstPlaneSlice; PlaneSlice < FirstPlaneSlice + IterationPlaneCount; ++PlaneSlice)
	{
		for (uint32 ArraySlice = FirstArraySlice; ArraySlice < FirstArraySlice + IterationArraySize; ++ArraySlice)
		{
			for (uint32 MipSlice = FirstMipSlice; MipSlice < FirstMipSlice + IterationMipCount; ++MipSlice)
			{
				const uint32 Subresource = D3D12CalcSubresource(MipSlice, ArraySlice, PlaneSlice, MipCount, ArraySize);
				const FD3D12RenderTargetView* RTV = nullptr;
#if PLATFORM_REQUIRES_TYPELESS_RESOURCE_DISCARD_WORKAROUND
				if (Texture)
				{
					RTV = Texture->GetRenderTargetView(MipSlice, ArraySlice);
				}
#endif // PLATFORM_REQUIRES_TYPELESS_RESOURCE_DISCARD_WORKAROUND
				Function(Subresource, RTV);
			}
		}
	}
}

template <typename FunctionType>
void ProcessResource(FD3D12CommandContext& Context, const FRHITransitionInfo& Info, FunctionType Function)
{
	switch (Info.Type)
	{
	case FRHITransitionInfo::EType::UAV:
	{
		FD3D12UnorderedAccessView* UAV = Context.RetrieveObject<FD3D12UnorderedAccessView_RHI>(Info.UAV);
		check(UAV);

		FRHITransitionInfo LocalInfo = Info;
		LocalInfo.MipIndex = UAV->GetViewSubset().Range.MostDetailedMip();
		Function(LocalInfo, UAV->GetResource());
		break;
	}
	case FRHITransitionInfo::EType::Buffer:
	{
		// Resource may be null if this is a multi-GPU resource not present on the current GPU
		FD3D12Buffer* Buffer = Context.RetrieveObject<FD3D12Buffer>(Info.Buffer);
		check(Buffer || GNumExplicitGPUsForRendering > 1);
		if (Buffer)
		{
			Function(Info, Buffer->GetResource());
		}
		break;
	}
	case FRHITransitionInfo::EType::Texture:
	{
		// Resource may be null if this is a multi-GPU resource not present on the current GPU
		FD3D12Texture* Texture = Context.RetrieveTexture(Info.Texture);
		check(Texture || GNumExplicitGPUsForRendering > 1);
		if (Texture)
		{
			FD3D12Texture* TextureOut = nullptr;
#if PLATFORM_REQUIRES_TYPELESS_RESOURCE_DISCARD_WORKAROUND
			if (Texture->GetRequiresTypelessResourceDiscardWorkaround())
			{
				TextureOut = Texture;
			}
#endif // #if PLATFORM_REQUIRES_TYPELESS_RESOURCE_DISCARD_WORKAROUND
			Function(Info, Texture->GetResource(), TextureOut);
		}
		break;
	}
	default:
		checkNoEntry();
		break;
	}
}

static bool ProcessTransitionDuringBegin(const FD3D12TransitionData* Data)
{
	// Pipe changes which are not ending with graphics or targeting all pipelines are handle during begin
	return ((Data->SrcPipelines != Data->DstPipelines && Data->DstPipelines != ERHIPipeline::Graphics) || EnumHasAllFlags(Data->DstPipelines, ERHIPipeline::All));
}

struct FD3D12DiscardResource
{
	FD3D12DiscardResource(FD3D12Resource* InResource, EResourceTransitionFlags InFlags, uint32 InSubresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, const FD3D12Texture* InTexture = nullptr, const FD3D12RenderTargetView* InRTV = nullptr)
		: Resource(InResource)
		, Flags(InFlags)
		, Subresource(InSubresource)
#if PLATFORM_REQUIRES_TYPELESS_RESOURCE_DISCARD_WORKAROUND
		, Texture(InTexture)
		, RTV(InRTV)
#endif // #if PLATFORM_REQUIRES_TYPELESS_RESOURCE_DISCARD_WORKAROUND
	{}

	FD3D12Resource* Resource;
	EResourceTransitionFlags Flags;
	uint32 Subresource;
#if PLATFORM_REQUIRES_TYPELESS_RESOURCE_DISCARD_WORKAROUND
	const FD3D12Texture* Texture = nullptr;
	const FD3D12RenderTargetView* RTV = nullptr;
#endif // #if PLATFORM_REQUIRES_TYPELESS_RESOURCE_DISCARD_WORKAROUND
};

void FD3D12CommandContext::HandleReservedResourceCommits(const struct FD3D12TransitionData* TransitionData)
{
	for (const FRHITransitionInfo& Info : TransitionData->TransitionInfos)
	{
		if (const FRHICommitResourceInfo* CommitInfo = Info.CommitInfo.GetPtrOrNull())
		{
			if (Info.Type == FRHITransitionInfo::EType::Buffer)
			{
				FD3D12Buffer* Buffer = RetrieveObject<FD3D12Buffer>(Info.Buffer);
				SetReservedBufferCommitSize(Buffer, CommitInfo->SizeInBytes);
			}
			else
			{
				checkNoEntry();
			}
		}
	}
}

void FD3D12CommandContext::HandleResourceDiscardTransitions(
	const FD3D12TransitionData* TransitionData,
	TArray<FD3D12DiscardResource>& ResourcesToDiscard)
{
	for (const FRHITransitionInfo& Info : TransitionData->TransitionInfos)
	{
		if (Info.AccessBefore != ERHIAccess::Discard || Info.Type != FRHITransitionInfo::EType::Texture || !EnumHasAnyFlags(Info.Flags, EResourceTransitionFlags::Clear | EResourceTransitionFlags::Discard))
		{
			continue;
		}

		ProcessResource(*this, Info, [&](const FRHITransitionInfo& Info, FD3D12Resource* Resource, FD3D12Texture* Texture = nullptr)
		{
			if (!Resource->RequiresResourceStateTracking())
			{
				return;
			}

			// Get the initial state to force a 'nop' transition so the internal command list resource tracking has the correct state
			// already to make sure it's not added to the pending transition list to be kicked before this command list (resource is not valid then yet)
			D3D12_RESOURCE_STATES InitialState = GetInitialResourceState(Resource->GetDesc());
			if (Info.IsWholeResource() || Resource->GetSubresourceCount() == 1)
			{
				TransitionResource(Resource, InitialState, InitialState, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

				const FD3D12RenderTargetView* RTV = nullptr;
#if PLATFORM_REQUIRES_TYPELESS_RESOURCE_DISCARD_WORKAROUND
				if (Texture)
				{
					RTV = Texture->GetRenderTargetView(0, -1);
				}
#endif // #if PLATFORM_REQUIRES_TYPELESS_RESOURCE_DISCARD_WORKAROUND				
				ResourcesToDiscard.Emplace(Resource, Info.Flags, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, Texture, RTV);
			}
			else
			{
				EnumerateSubresources(Resource, Info, Texture, [&](uint32 Subresource, const FD3D12RenderTargetView* RTV)
				{
					TransitionResource(Resource, InitialState, InitialState, Subresource);
					ResourcesToDiscard.Emplace(Resource, Info.Flags, Subresource, Texture, RTV);
				});
			}
		});
	}
}

void FD3D12CommandContext::HandleDiscardResources(
	TArrayView<const FRHITransition*> Transitions,
	bool bIsBeginTransition)
{
	TArray<FD3D12DiscardResource> ResourcesToDiscard;

	for (const FRHITransition* Transition : Transitions)
	{
		const FD3D12TransitionData* Data = Transition->GetPrivateData<FD3D12TransitionData>();

		if (ProcessTransitionDuringBegin(Data) == bIsBeginTransition)
		{
			HandleResourceDiscardTransitions(Data, ResourcesToDiscard);
		}
	}

	if (!GD3D12AllowDiscardResources)
	{
		return;
	}

	if (!ResourcesToDiscard.IsEmpty())
	{
		FlushResourceBarriers();
	}

	for (const FD3D12DiscardResource& DiscardResource : ResourcesToDiscard)
	{
		if (EnumHasAnyFlags(DiscardResource.Flags, EResourceTransitionFlags::Clear))
		{
			// add correct ops for clear
			check(false);
		}
		else if (EnumHasAnyFlags(DiscardResource.Flags, EResourceTransitionFlags::Discard))
		{
#if PLATFORM_REQUIRES_TYPELESS_RESOURCE_DISCARD_WORKAROUND
			if (DiscardResource.Texture && DiscardResource.RTV)
			{
				FLinearColor ClearColor = DiscardResource.Texture->GetClearColor();
				GraphicsCommandList()->ClearRenderTargetView(DiscardResource.RTV->GetOfflineCpuHandle(), reinterpret_cast<float*>(&ClearColor), 0, nullptr);
				UpdateResidency(DiscardResource.RTV->GetResource());
			}
			else
#endif // #if PLATFORM_REQUIRES_TYPELESS_RESOURCE_DISCARD_WORKAROUND
			{
				if (DiscardResource.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
				{
					GraphicsCommandList()->DiscardResource(DiscardResource.Resource->GetResource(), nullptr);
				}
				else
				{
					D3D12_DISCARD_REGION Region;
					Region.NumRects = 0;
					Region.pRects = nullptr;
					Region.FirstSubresource = DiscardResource.Subresource;
					Region.NumSubresources = 1;

					GraphicsCommandList()->DiscardResource(DiscardResource.Resource->GetResource(), &Region);
				}
			}
		}
	}
}

void FD3D12CommandContext::HandleTransientAliasing(const FD3D12TransitionData* TransitionData)
{
	for (const FRHITransientAliasingInfo& Info : TransitionData->AliasingInfos)
	{
		FD3D12BaseShaderResource* BaseShaderResource = nullptr;
		switch (Info.Type)
		{
		case FRHITransientAliasingInfo::EType::Buffer:
		{
			// Resource may be null if this is a multi-GPU resource not present on the current GPU
			FD3D12Buffer* Buffer = RetrieveObject<FD3D12Buffer>(Info.Buffer);
			check(Buffer || GNumExplicitGPUsForRendering > 1);
			BaseShaderResource = Buffer;
			break;
		}
		case FRHITransientAliasingInfo::EType::Texture:
		{
			// Resource may be null if this is a multi-GPU resource not present on the current GPU
			FD3D12Texture* Texture = RetrieveTexture(Info.Texture);
			check(Texture || GNumExplicitGPUsForRendering > 1);
			BaseShaderResource = Texture;
			break;
		}
		default:
			checkNoEntry();
			break;
		}

		// Resource may be null if this is a multi-GPU resource not present on the current GPU
		if (!BaseShaderResource)
		{
			continue;
		}

		FD3D12Resource* Resource = BaseShaderResource->ResourceLocation.GetResource();
		if (Info.Action == FRHITransientAliasingInfo::EAction::Acquire)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(D3D12RHI::AcquireTransient);

			if (GD3D12TransientAllocatorFullAliasingBarrier != 0)
			{
				AddAliasingBarrier(nullptr, Resource->GetResource());
			}
			else
			{
				for (const FRHITransientAliasingOverlap& Overlap : Info.Overlaps)
				{
					FD3D12Resource* ResourceBefore{};

					switch (Overlap.Type)
					{
					case FRHITransientAliasingOverlap::EType::Texture:
						{
							const FD3D12Texture* Texture = RetrieveTexture(Overlap.Texture);
							if (Texture)
							{
								ResourceBefore = Texture->GetResource();
							}
						}
						break;
					case FRHITransientAliasingOverlap::EType::Buffer:
						{
							const FD3D12Buffer* Buffer = RetrieveObject<FD3D12Buffer>(Overlap.Buffer);
							if (Buffer)
							{
								ResourceBefore = Buffer->GetResource();
							}
						}
						break;
					}

					// Resource may be null if this is a multi-GPU resource not present on the current GPU
					check(ResourceBefore || GNumExplicitGPUsForRendering > 1);
					if (ResourceBefore)
					{
						AddAliasingBarrier(ResourceBefore->GetResource(), Resource->GetResource());
					}
				}
			}
		}
		else
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(D3D12RHI::DiscardTransient);

			// Restore the resource back to the initial state when done
			D3D12_RESOURCE_STATES FinalState = GetInitialResourceState(Resource->GetDesc());
			TransitionResource(Resource, D3D12_RESOURCE_STATE_TBD, FinalState, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
			
			// Remove from caches
			ClearShaderResources(BaseShaderResource, EShaderParameterTypeMask::SRVMask | EShaderParameterTypeMask::UAVMask);
		}
	}
}

void FD3D12CommandContext::HandleResourceTransitions(const FD3D12TransitionData* TransitionData, bool& bUAVBarrier)
{
	const bool bDstAllPipelines = EnumHasAllFlags(TransitionData->DstPipelines, ERHIPipeline::All);

	for (const FRHITransitionInfo& Info : TransitionData->TransitionInfos)
	{
		const bool bUAVAccessAfter = EnumHasAnyFlags(Info.AccessAfter, ERHIAccess::UAVMask);

		// If targeting all pipelines and UAV access then add UAV barrier even with RHI based transitions
		if (bDstAllPipelines)
		{
			bUAVBarrier |= bUAVAccessAfter;
		}

		// Only need to check for UAV barriers here, all other transitions are handled inside the RHI
		// Can't rely on before state to either be unknown or UAV because there could be a UAV->SRV transition already done (and ignored here)
		// and the transition SRV->UAV needs a UAV barrier then to work correctly otherwise there is no synchronization at all
		bUAVBarrier |= bUAVAccessAfter;

		// Process transitions which are forced during begin because those contain transition from Graphics to Compute and should
		// help remove forced patch up command lists for async compute to run on the graphics queue
		if (Info.Resource && ProcessTransitionDuringBegin(TransitionData))
		{
			ProcessResource(*this, Info, [&](const FRHITransitionInfo& Info, FD3D12Resource* Resource, FD3D12Texture* UnusedTexture = nullptr)
			{
				if (!Resource->RequiresResourceStateTracking())
				{
					return;
				}

				const bool bIsAsyncCompute = EnumHasAnyFlags(TransitionData->DstPipelines, ERHIPipeline::AsyncCompute);

				// Use D3D12_RESOURCE_STATE_TBD as before state for now and don't use provided before state because there is not validation nor handling if the provided state
				// doesn't match up with the tracked state - this needs to be improved and checked
				D3D12_RESOURCE_STATES StateBefore = D3D12_RESOURCE_STATE_TBD;
				D3D12_RESOURCE_STATES StateAfter = Info.AccessAfter == ERHIAccess::Discard ? GetInitialResourceState(Resource->GetDesc()) : GetD3D12ResourceState(Info.AccessAfter, bIsAsyncCompute);

				// enqueue the correct transitions
				if (Info.IsWholeResource() || Resource->GetSubresourceCount() == 1)
				{
					TransitionResource(Resource, StateBefore, StateAfter, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
				}
				else
				{
					EnumerateSubresources(Resource, Info, nullptr, [&](uint32 Subresource, const FD3D12RenderTargetView* UnusedRTV = nullptr)
					{
						TransitionResource(Resource, StateBefore, StateAfter, Subresource);
					});
				}
			});
		}
	}
}

void FD3D12CommandContext::RHIBeginTransitions(TArrayView<const FRHITransition*> Transitions)
{
	static IConsoleVariable* CVarShowTransitions = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ProfileGPU.ShowTransitions"));
	const bool bShowTransitionEvents = CVarShowTransitions->GetInt() != 0;
	SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(*this, RHIBeginTransitions, bShowTransitionEvents, TEXT("RHIBeginTransitions"));

	for (const FRHITransition* Transition : Transitions)
	{
		const FD3D12TransitionData* Data = Transition->GetPrivateData<FD3D12TransitionData>();

		if (ProcessTransitionDuringBegin(Data))
		{
			HandleTransientAliasing(Data);
		}
	}

	HandleDiscardResources(Transitions, true /** bIsBeginTransitions */);

	bool bUAVBarrier = false;

	for (const FRHITransition* Transition : Transitions)
	{
		const FD3D12TransitionData* Data = Transition->GetPrivateData<FD3D12TransitionData>();

		// Handle transition during BeginTransitions?
		if (ProcessTransitionDuringBegin(Data))
		{
			HandleResourceTransitions(Data, bUAVBarrier);
		}
	}

	if (bUAVBarrier)
	{
		StateCache.FlushComputeShaderCache(true);
	}

	// Signal fences
	const ERHIPipeline SourcePipeline = GetPipeline();
	for (const FRHITransition* Transition : Transitions)
	{
		const auto* Data = Transition->GetPrivateData<FD3D12TransitionData>();
		if (Data->bCrossPipeline)
		{
			const TRHIPipelineArray<FD3D12SyncPointRef>& DeviceSyncPoints = Data->SyncPoints[GetGPUIndex()];

			if (DeviceSyncPoints[SourcePipeline])
			{
				SignalSyncPoint(DeviceSyncPoints[SourcePipeline]);
			}
		}
	}
}

void FD3D12CommandContext::RHIEndTransitions(TArrayView<const FRHITransition*> Transitions)
{
	// Wait for fences
	{
		const ERHIPipeline DstPipeline = GetPipeline();
		for (const FRHITransition* Transition : Transitions)
		{
			const auto* Data = Transition->GetPrivateData<FD3D12TransitionData>();
			if (Data->bCrossPipeline)
			{
				const TRHIPipelineArray<FD3D12SyncPointRef>& DeviceSyncPoints = Data->SyncPoints[GetGPUIndex()];

				EnumerateRHIPipelines(Data->SrcPipelines, [&](ERHIPipeline SrcPipeline)
				{
					if (SrcPipeline != DstPipeline && DeviceSyncPoints[SrcPipeline])
					{
						WaitSyncPoint(DeviceSyncPoints[SrcPipeline]);
					}
				});
			}
		}
	}

	static IConsoleVariable* CVarShowTransitions = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ProfileGPU.ShowTransitions"));
	const bool bShowTransitionEvents = CVarShowTransitions->GetInt() != 0;
	SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(*this, RHIEndTransitions, bShowTransitionEvents, TEXT("RHIEndTransitions"));

	// Update reserved resource memory mapping
	for (const FRHITransition* Transition : Transitions)
	{
		const FD3D12TransitionData* Data = Transition->GetPrivateData<FD3D12TransitionData>();
		HandleReservedResourceCommits(Data);
	}

	for (const FRHITransition* Transition : Transitions)
	{
		const FD3D12TransitionData* Data = Transition->GetPrivateData<FD3D12TransitionData>();

		if (!ProcessTransitionDuringBegin(Data))
		{
			HandleTransientAliasing(Data);
		}
	}

	HandleDiscardResources(Transitions, false /** bIsBeginTransitions */);

	bool bUAVBarrier = false;

	for (const FRHITransition* Transition : Transitions)
	{
		const FD3D12TransitionData* Data = Transition->GetPrivateData<FD3D12TransitionData>();

		// Handle transition during EndTransitions?
		if (!ProcessTransitionDuringBegin(Data))
		{
			HandleResourceTransitions(Data, bUAVBarrier);
		}
	}

	if (bUAVBarrier)
	{
		StateCache.FlushComputeShaderCache(true);
	}
}

void FD3D12CommandContext::RHISetStaticUniformBuffers(const FUniformBufferStaticBindings& InUniformBuffers)
{
	FMemory::Memzero(StaticUniformBuffers.GetData(), StaticUniformBuffers.Num() * sizeof(FRHIUniformBuffer*));

	for (int32 Index = 0; Index < InUniformBuffers.GetUniformBufferCount(); ++Index)
	{
		StaticUniformBuffers[InUniformBuffers.GetSlot(Index)] = InUniformBuffers.GetUniformBuffer(Index);
	}
}

void FD3D12CommandContext::RHICopyToStagingBuffer(FRHIBuffer* SourceBufferRHI, FRHIStagingBuffer* StagingBufferRHI, uint32 Offset, uint32 NumBytes)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12CopyToStagingBufferTime);

	const static FLazyName RHIStagingBufferName(TEXT("FRHIStagingBuffer"));
	UE_TRACE_METADATA_SCOPE_ASSET_FNAME(SourceBufferRHI->GetName(), RHIStagingBufferName, SourceBufferRHI->GetOwnerName());

	FD3D12StagingBuffer* StagingBuffer = FD3D12DynamicRHI::ResourceCast(StagingBufferRHI);
	check(StagingBuffer);
	ensureMsgf(!StagingBuffer->bIsLocked, TEXT("Attempting to Copy to a locked staging buffer. This may have undefined behavior"));

	FD3D12Buffer* VertexBuffer = RetrieveObject<FD3D12Buffer>(SourceBufferRHI);
	check(VertexBuffer);

	// Ensure our shadow buffer is large enough to hold the readback.
	if (!StagingBuffer->ResourceLocation.IsValid() || StagingBuffer->ShadowBufferSize < NumBytes)
	{
		StagingBuffer->SafeRelease();

		// Unknown aligment requirement for sub allocated read back buffer data
		uint32 AllocationAlignment = 16;
		const D3D12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(NumBytes, D3D12_RESOURCE_FLAG_NONE);		
		GetParentDevice()->GetDefaultBufferAllocator().AllocDefaultResource(D3D12_HEAP_TYPE_READBACK, BufferDesc, BUF_None, ED3D12ResourceStateMode::SingleState, D3D12_RESOURCE_STATE_COPY_DEST, StagingBuffer->ResourceLocation, AllocationAlignment, TEXT("StagedRead"));
		check(StagingBuffer->ResourceLocation.GetSize() == NumBytes);
		StagingBuffer->ShadowBufferSize = NumBytes;
	}

	// No need to check the GPU mask as staging buffers are in CPU memory and visible to all GPUs.
	
	{
		FD3D12Resource* pSourceResource = VertexBuffer->ResourceLocation.GetResource();
		D3D12_RESOURCE_DESC const& SourceBufferDesc = pSourceResource->GetDesc();
		uint32 SourceOffset = VertexBuffer->ResourceLocation.GetOffsetFromBaseOfResource();

		FD3D12Resource* pDestResource = StagingBuffer->ResourceLocation.GetResource();
		D3D12_RESOURCE_DESC const& DestBufferDesc = pDestResource->GetDesc();
		uint32 DestOffset = StagingBuffer->ResourceLocation.GetOffsetFromBaseOfResource();

		if (pSourceResource->RequiresResourceStateTracking())
		{
			TransitionResource(pSourceResource, D3D12_RESOURCE_STATE_TBD, D3D12_RESOURCE_STATE_COPY_SOURCE, 0);
		}
		
		FlushResourceBarriers();	// Must flush so the desired state is actually set.

		GraphicsCommandList()->CopyBufferRegion(pDestResource->GetResource(), DestOffset, pSourceResource->GetResource(), Offset + SourceOffset, NumBytes);
		UpdateResidency(pDestResource);
		UpdateResidency(pSourceResource);

		ConditionalSplitCommandList();
	}
}

void FD3D12CommandContext::RHISetViewport(float MinX, float MinY, float MinZ, float MaxX, float MaxY, float MaxZ)
{
	// These are the maximum viewport extents for D3D12. Exceeding them leads to badness.
	check(MinX <= (uint32)D3D12_VIEWPORT_BOUNDS_MAX);
	check(MinY <= (uint32)D3D12_VIEWPORT_BOUNDS_MAX);
	check(MaxX <= (uint32)D3D12_VIEWPORT_BOUNDS_MAX);
	check(MaxY <= (uint32)D3D12_VIEWPORT_BOUNDS_MAX);

	D3D12_VIEWPORT Viewport = { MinX, MinY, (MaxX - MinX), (MaxY - MinY), MinZ, MaxZ };
	//avoid setting a 0 extent viewport, which the debug runtime doesn't like
	if (Viewport.Width > 0 && Viewport.Height > 0)
	{
		// Setting a viewport will also set the scissor rect appropriately.
		StateCache.SetViewport(Viewport);
		RHISetScissorRect(true, MinX, MinY, MaxX, MaxY);
	}
}

void FD3D12CommandContext::RHISetStereoViewport(float LeftMinX, float RightMinX, float LeftMinY, float RightMinY, float MinZ, float LeftMaxX, float RightMaxX, float LeftMaxY, float RightMaxY, float MaxZ)
{
	// Set up both viewports
	D3D12_VIEWPORT Viewports[2] = {};

	Viewports[0].TopLeftX = FMath::FloorToInt(LeftMinX);
	Viewports[0].TopLeftY = FMath::FloorToInt(LeftMinY);
	Viewports[0].Width = FMath::CeilToInt(LeftMaxX - LeftMinX);
	Viewports[0].Height = FMath::CeilToInt(LeftMaxY - LeftMinY);
	Viewports[0].MinDepth = MinZ;
	Viewports[0].MaxDepth = MaxZ;

	Viewports[1].TopLeftX = FMath::FloorToInt(RightMinX);
	Viewports[1].TopLeftY = FMath::FloorToInt(RightMinY);
	Viewports[1].Width = FMath::CeilToInt(RightMaxX - RightMinX);
	Viewports[1].Height = FMath::CeilToInt(RightMaxY - RightMinY);
	Viewports[1].MinDepth = MinZ;
	Viewports[1].MaxDepth = MaxZ;

	D3D12_RECT ScissorRects[2] =
	{
		{ Viewports[0].TopLeftX, Viewports[0].TopLeftY, Viewports[0].TopLeftX + Viewports[0].Width, Viewports[0].TopLeftY + Viewports[0].Height },
		{ Viewports[1].TopLeftX, Viewports[1].TopLeftY, Viewports[1].TopLeftX + Viewports[1].Width, Viewports[1].TopLeftY + Viewports[1].Height }
	};

	StateCache.SetViewports(2, Viewports);
	// Set the scissor rects appropriately.
	StateCache.SetScissorRects(2, ScissorRects);
}

void FD3D12CommandContext::RHISetScissorRect(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY)
{
	if (bEnable)
	{
		const CD3DX12_RECT ScissorRect(MinX, MinY, MaxX, MaxY);
		StateCache.SetScissorRect(ScissorRect);
	}
	else
	{
		const D3D12_VIEWPORT& Viewport = StateCache.GetViewport();
		const CD3DX12_RECT ScissorRect((LONG) Viewport.TopLeftX, (LONG) Viewport.TopLeftY, (LONG) Viewport.TopLeftX + (LONG) Viewport.Width, (LONG) Viewport.TopLeftY + (LONG) Viewport.Height);
		StateCache.SetScissorRect(ScissorRect);
	}
}

static void ApplyStaticUniformBuffersOnContext(FD3D12CommandContext& Context, FRHIShader* Shader, FD3D12ShaderData* ShaderData)
{
	if (Shader)
	{
		const uint32 GpuIndex = Context.GetGPUIndex();
		const EShaderFrequency ShaderFrequency = Shader->GetFrequency();

		UE::RHICore::ApplyStaticUniformBuffers(
			Shader,
			ShaderData->StaticSlots,
			ShaderData->ShaderResourceTable.ResourceTableLayoutHashes,
			Context.GetStaticUniformBuffers(),
			[&Context, Shader, ShaderFrequency, GpuIndex](int32 BufferIndex, FRHIUniformBuffer* Buffer)
			{
				BindUniformBuffer(Context, Shader, ShaderFrequency, BufferIndex, FD3D12CommandContext::RetrieveObject<FD3D12UniformBuffer>(Buffer, GpuIndex));
			}
		);
	}
}

template<typename TShader>
static void ApplyStaticUniformBuffersOnContext(FD3D12CommandContext& Context, TShader* Shader)
{
	ApplyStaticUniformBuffersOnContext(Context, Shader, static_cast<FD3D12ShaderData*>(Shader));
}

void FD3D12CommandContext::RHISetGraphicsPipelineState(FRHIGraphicsPipelineState* GraphicsState, uint32 StencilRef, bool bApplyAdditionalState)
{
	FD3D12GraphicsPipelineState* GraphicsPipelineState = FD3D12DynamicRHI::ResourceCast(GraphicsState);

	// TODO: [PSO API] Every thing inside this scope is only necessary to keep the PSO shadow in sync while we convert the high level to only use PSOs
	// Ensure the command buffers are reset to reduce the amount of data that needs to be versioned.
	for (int32 Index = 0; Index < SF_NumGraphicsFrequencies; Index++)
	{
		StageConstantBuffers[Index].Reset();
	}
	
	// @TODO : really should only discard the constants if the shader state has actually changed.
	bDiscardSharedGraphicsConstants = true;

	if (!GraphicsPipelineState->PipelineStateInitializer.bDepthBounds)
	{
		StateCache.SetDepthBounds(0.0f, 1.0f);
	}

	if (GRHISupportsPipelineVariableRateShading && GRHIVariableRateShadingEnabled)
	{
		if (GraphicsPipelineState->PipelineStateInitializer.bAllowVariableRateShading)
		{
			StateCache.SetShadingRate(GraphicsPipelineState->PipelineStateInitializer.ShadingRate, VRSRB_Passthrough, VRSRB_Max);
		}
		else
		{
			StateCache.SetShadingRate(EVRSShadingRate::VRSSR_1x1, VRSRB_Passthrough, VRSRB_Passthrough);
		}
	}

	StateCache.SetGraphicsPipelineState(GraphicsPipelineState);
	StateCache.SetStencilRef(StencilRef);

	if (bApplyAdditionalState)
	{
		ApplyStaticUniformBuffersOnContext(*this, GraphicsPipelineState->GetVertexShader());
		ApplyStaticUniformBuffersOnContext(*this, GraphicsPipelineState->GetMeshShader());
		ApplyStaticUniformBuffersOnContext(*this, GraphicsPipelineState->GetAmplificationShader());
		ApplyStaticUniformBuffersOnContext(*this, GraphicsPipelineState->GetGeometryShader());
		ApplyStaticUniformBuffersOnContext(*this, GraphicsPipelineState->GetPixelShader());
	}
}

void FD3D12CommandContext::RHISetComputePipelineState(FRHIComputePipelineState* ComputeState)
{
#if D3D12_RHI_RAYTRACING
	StateCache.TransitionComputeState(ED3D12PipelineType::Compute);
#endif

	FD3D12ComputePipelineState* ComputePipelineState = FD3D12DynamicRHI::ResourceCast(ComputeState);

	StageConstantBuffers[SF_Compute].Reset();
	bDiscardSharedComputeConstants = true;

	StateCache.SetComputePipelineState(ComputePipelineState);

	ApplyStaticUniformBuffersOnContext(*this, ComputePipelineState->ComputeShader.GetReference());
}

void FD3D12CommandContext::SetUAVParameter(EShaderFrequency Frequency, uint32 UAVIndex, FD3D12UnorderedAccessView* UAV)
{
	ClearShaderResources(UAV, EShaderParameterTypeMask::SRVMask);
	StateCache.SetUAV(SF_Pixel, UAVIndex, UAV);
}

void FD3D12CommandContext::SetUAVParameter(EShaderFrequency Frequency, uint32 UAVIndex, FD3D12UnorderedAccessView* UAV, uint32 InitialCount)
{
	ClearShaderResources(UAV, EShaderParameterTypeMask::SRVMask);
	StateCache.SetUAV(SF_Pixel, UAVIndex, UAV, InitialCount);
}

void FD3D12CommandContext::SetSRVParameter(EShaderFrequency Frequency, uint32 SRVIndex, FD3D12ShaderResourceView* SRV)
{
	StateCache.SetShaderResourceView(Frequency, SRV, SRVIndex);
}

struct FD3D12ResourceBinder
{
	FD3D12CommandContext& Context;
	FD3D12ConstantBuffer& ConstantBuffer;
	const uint32 GpuIndex;
	const EShaderFrequency Frequency;
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
	const bool bBindlessResources;
	const bool bBindlessSamplers;
#endif

	FD3D12ResourceBinder(FD3D12CommandContext& InContext, EShaderFrequency InFrequency, const FD3D12ShaderData* ShaderData)
		: Context(InContext)
		, ConstantBuffer(InContext.StageConstantBuffers[InFrequency])
		, GpuIndex(InContext.GetGPUIndex())
		, Frequency(InFrequency)
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
		, bBindlessResources(EnumHasAnyFlags(ShaderData->ResourceCounts.UsageFlags, EShaderResourceUsageFlags::BindlessResources))
		, bBindlessSamplers(EnumHasAnyFlags(ShaderData->ResourceCounts.UsageFlags, EShaderResourceUsageFlags::BindlessSamplers))
#endif
	{
	}

#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
	void SetBindlessHandle(const FRHIDescriptorHandle& Handle, uint32 Offset)
	{
		if (Handle.IsValid())
		{
			const uint32 BindlessIndex = Handle.GetIndex();
			ConstantBuffer.UpdateConstant(reinterpret_cast<const uint8*>(&BindlessIndex), Offset, 4);
		}
	}
#endif

	void SetUAV(FRHIUnorderedAccessView* InUnorderedAccessView, uint32 Index, bool bClearResources = false)
	{
		FD3D12UnorderedAccessView_RHI* D3D12UnorderedAccessView = FD3D12CommandContext::RetrieveObject<FD3D12UnorderedAccessView_RHI>(InUnorderedAccessView, GpuIndex);
		if (bClearResources)
		{
			Context.ClearShaderResources(D3D12UnorderedAccessView, EShaderParameterTypeMask::SRVMask);
		}

#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
		if (bBindlessResources)
		{
			Context.StateCache.QueueBindlessUAV(Frequency, D3D12UnorderedAccessView);
		}
		else
#endif
		{
			Context.StateCache.SetUAV(Frequency, Index, D3D12UnorderedAccessView);
		}
	}

	void SetSRV(FRHIShaderResourceView* InShaderResourceView, uint32 Index)
	{
		FD3D12ShaderResourceView_RHI* D3D12ShaderResourceView = FD3D12CommandContext::RetrieveObject<FD3D12ShaderResourceView_RHI>(InShaderResourceView, GpuIndex);

#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
		if (bBindlessResources)
		{
			Context.StateCache.QueueBindlessSRV(Frequency, D3D12ShaderResourceView);
		}
		else
#endif
		{
			Context.StateCache.SetShaderResourceView(Frequency, D3D12ShaderResourceView, Index);
		}
	}

	void SetTexture(FRHITexture* InTexture, uint32 Index)
	{
		FD3D12Texture* D3D12Texture = FD3D12CommandContext::RetrieveTexture(InTexture, GpuIndex);
		FD3D12ShaderResourceView* D3D12ShaderResourceView = D3D12Texture ? D3D12Texture->GetShaderResourceView() : nullptr;

#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
		if (bBindlessResources)
		{
			Context.StateCache.QueueBindlessSRV(Frequency, D3D12ShaderResourceView);
		}
		else
#endif
		{
			Context.StateCache.SetShaderResourceView(Frequency, D3D12ShaderResourceView, Index);
		}
	}

	void SetSampler(FRHISamplerState* Sampler, uint32 Index)
	{
		FD3D12SamplerState* D3D12SamplerState = FD3D12CommandContext::RetrieveObject<FD3D12SamplerState>(Sampler, GpuIndex);

#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
		if (bBindlessSamplers)
		{
			// Nothing to do, only needs constants set
		}
		else
#endif
		{
			Context.StateCache.SetSamplerState(Frequency, D3D12SamplerState, Index);
		}
	}
};

static void SetShaderParametersOnContext(
	FD3D12CommandContext& Context
	, FRHIShader* Shader
	, EShaderFrequency ShaderFrequency
	, TArrayView<const uint8> InParametersData
	, TArrayView<const FRHIShaderParameter> InParameters
	, TArrayView<const FRHIShaderParameterResource> InResourceParameters
	, TArrayView<const FRHIShaderParameterResource> InBindlessParameters)
{
	FD3D12ConstantBuffer& ConstantBuffer = Context.StageConstantBuffers[ShaderFrequency];
	FD3D12StateCache& StateCache = Context.StateCache;
	const uint32 GpuIndex = Context.GetGPUIndex();

	for (const FRHIShaderParameter& Parameter : InParameters)
	{
		checkSlow(Parameter.BufferIndex == 0);
		ConstantBuffer.UpdateConstant(&InParametersData[Parameter.ByteOffset], Parameter.BaseIndex, Parameter.ByteSize);
	}

	FD3D12ResourceBinder Binder(Context, ShaderFrequency, GetShaderData(Shader));

#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
	for (const FRHIShaderParameterResource& Parameter : InBindlessParameters)
	{
		if (FRHIResource* Resource = Parameter.Resource)
		{
			FRHIDescriptorHandle Handle;

			switch (Parameter.Type)
			{
			case FRHIShaderParameterResource::EType::Texture:
				Handle = static_cast<FRHITexture*>(Resource)->GetDefaultBindlessHandle();
				Binder.SetTexture(static_cast<FRHITexture*>(Resource), Parameter.Index);
				break;
			case FRHIShaderParameterResource::EType::ResourceView:
				Handle = static_cast<FRHIShaderResourceView*>(Resource)->GetBindlessHandle();
				Binder.SetSRV(static_cast<FRHIShaderResourceView*>(Resource), Parameter.Index);
				break;
			case FRHIShaderParameterResource::EType::UnorderedAccessView:
				Handle = static_cast<FRHIUnorderedAccessView*>(Resource)->GetBindlessHandle();
				Binder.SetUAV(static_cast<FRHIUnorderedAccessView*>(Resource), Parameter.Index, true);
				break;
			case FRHIShaderParameterResource::EType::Sampler:
				Handle = static_cast<FRHISamplerState*>(Resource)->GetBindlessHandle();
				Binder.SetSampler(static_cast<FRHISamplerState*>(Resource), Parameter.Index);
				break;
			}

			checkf(Handle.IsValid(), TEXT("D3D12 resource did not provide a valid descriptor handle. Please validate that all D3D12 types can provide this or that the resource is still valid."));
			Binder.SetBindlessHandle(Handle, Parameter.Index);
		}
	}
#endif

	for (const FRHIShaderParameterResource& Parameter : InResourceParameters)
	{
		if (Parameter.Type == FRHIShaderParameterResource::EType::UnorderedAccessView)
		{
			if (ShaderFrequency == SF_Pixel || ShaderFrequency == SF_Compute)
			{
				Binder.SetUAV(static_cast<FRHIUnorderedAccessView*>(Parameter.Resource), Parameter.Index, true);
			}
			else
			{
				checkf(false, TEXT("TShaderRHI Can't have compute shader to be set. UAVs are not supported on vertex, tessellation and geometry shaders."));
			}
		}
	}

	for (const FRHIShaderParameterResource& Parameter : InResourceParameters)
	{
		switch (Parameter.Type)
		{
		case FRHIShaderParameterResource::EType::Texture:
			Binder.SetTexture(static_cast<FRHITexture*>(Parameter.Resource), Parameter.Index);
			break;
		case FRHIShaderParameterResource::EType::ResourceView:
			Binder.SetSRV(static_cast<FRHIShaderResourceView*>(Parameter.Resource), Parameter.Index);
			break;
		case FRHIShaderParameterResource::EType::UnorderedAccessView:
			break;
		case FRHIShaderParameterResource::EType::Sampler:
			Binder.SetSampler(static_cast<FRHISamplerState*>(Parameter.Resource), Parameter.Index);
			break;
		case FRHIShaderParameterResource::EType::UniformBuffer:
			BindUniformBuffer(Context, Shader, ShaderFrequency, Parameter.Index, FD3D12CommandContext::RetrieveObject<FD3D12UniformBuffer>(Parameter.Resource, GpuIndex));
			break;
		default:
			checkf(false, TEXT("Unhandled resource type?"));
			break;
		}
	}
}

void FD3D12CommandContext::RHISetShaderParameters(FRHIGraphicsShader* Shader, TArrayView<const uint8> InParametersData, TArrayView<const FRHIShaderParameter> InParameters, TArrayView<const FRHIShaderParameterResource> InResourceParameters, TArrayView<const FRHIShaderParameterResource> InBindlessParameters)
{
	const EShaderFrequency ShaderFrequency = Shader->GetFrequency();
	if (IsValidGraphicsFrequency(ShaderFrequency))
	{
		ValidateBoundShader(StateCache, Shader);

		SetShaderParametersOnContext(
			*this
			, Shader
			, ShaderFrequency
			, InParametersData
			, InParameters
			, InResourceParameters
			, InBindlessParameters
		);
	}
	else
	{
		checkf(0, TEXT("Unsupported FRHIGraphicsShader Type '%s'!"), GetShaderFrequencyString(ShaderFrequency, false));
	}
}

void FD3D12CommandContext::RHISetShaderParameters(FRHIComputeShader* Shader, TArrayView<const uint8> InParametersData, TArrayView<const FRHIShaderParameter> InParameters, TArrayView<const FRHIShaderParameterResource> InResourceParameters, TArrayView<const FRHIShaderParameterResource> InBindlessParameters)
{
	//ValidateBoundShader(StateCache, Shader);

	SetShaderParametersOnContext(
		*this
		, Shader
		, SF_Compute
		, InParametersData
		, InParameters
		, InResourceParameters
		, InBindlessParameters
	);
}

static void SetShaderUnbindsOnContext(
	FD3D12CommandContext& Context
	, FRHIShader* Shader
	, EShaderFrequency ShaderFrequency
	, TConstArrayView<FRHIShaderParameterUnbind> InUnbinds)
{
	for (const FRHIShaderParameterUnbind& Unbind : InUnbinds)
	{
		switch (Unbind.Type)
		{
		case FRHIShaderParameterUnbind::EType::ResourceView:
			Context.StateCache.SetShaderResourceView(ShaderFrequency, nullptr, Unbind.Index);
			break;
		case FRHIShaderParameterUnbind::EType::UnorderedAccessView:
			if (ShaderFrequency == SF_Pixel || ShaderFrequency == SF_Compute)
			{
				Context.StateCache.SetUAV(ShaderFrequency, Unbind.Index, nullptr);
			}
			else
			{
				checkf(false, TEXT("TShaderRHI Can't have compute shader to be set. UAVs are not supported on vertex, tessellation and geometry shaders."));
			}
			break;
		default:
			checkf(false, TEXT("Unhandled resource type?"));
			break;
		}
	}
}

void FD3D12CommandContext::RHISetShaderUnbinds(FRHIGraphicsShader* Shader, TConstArrayView<FRHIShaderParameterUnbind> InUnbinds)
{
	const EShaderFrequency ShaderFrequency = Shader->GetFrequency();
	if (IsValidGraphicsFrequency(ShaderFrequency))
	{
		ValidateBoundShader(StateCache, Shader);

		SetShaderUnbindsOnContext(*this, Shader, ShaderFrequency, InUnbinds);
	}
	else
	{
		checkf(0, TEXT("Unsupported FRHIGraphicsShader Type '%s'!"), GetShaderFrequencyString(ShaderFrequency, false));
	}
}

void FD3D12CommandContext::RHISetShaderUnbinds(FRHIComputeShader* Shader, TConstArrayView<FRHIShaderParameterUnbind> InUnbinds)
{
	//ValidateBoundShader(StateCache, Shader);

	SetShaderUnbindsOnContext(*this, Shader, SF_Compute, InUnbinds);
}

void FD3D12CommandContext::RHISetStencilRef(uint32 StencilRef)
{
	StateCache.SetStencilRef(StencilRef);
}

void FD3D12CommandContext::RHISetBlendFactor(const FLinearColor& BlendFactor)
{
	StateCache.SetBlendFactor((const float*)&BlendFactor);
}

struct FRTVDesc
{
	uint32 Width;
	uint32 Height;
	DXGI_SAMPLE_DESC SampleDesc;
};

// Return an FRTVDesc structure whose
// Width and height dimensions are adjusted for the RTV's miplevel.
FRTVDesc GetRenderTargetViewDesc(FD3D12RenderTargetView* RenderTargetView)
{
	const D3D12_RENDER_TARGET_VIEW_DESC &TargetDesc = RenderTargetView->GetD3DDesc();

	FD3D12Resource* BaseResource = RenderTargetView->GetResource();
	uint32 MipIndex = 0;
	FRTVDesc ret;
	memset(&ret, 0, sizeof(ret));

	switch (TargetDesc.ViewDimension)
	{
	case D3D12_RTV_DIMENSION_TEXTURE2D:
	case D3D12_RTV_DIMENSION_TEXTURE2DMS:
	case D3D12_RTV_DIMENSION_TEXTURE2DARRAY:
	case D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY:
	{
		D3D12_RESOURCE_DESC const& Desc = BaseResource->GetDesc();
		ret.Width = (uint32)Desc.Width;
		ret.Height = Desc.Height;
		ret.SampleDesc = Desc.SampleDesc;
		if (TargetDesc.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2D || TargetDesc.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2DARRAY)
		{
			// All the non-multisampled texture types have their mip-slice in the same position.
			MipIndex = TargetDesc.Texture2D.MipSlice;
		}
		break;
	}
	case D3D12_RTV_DIMENSION_TEXTURE3D:
	{
		D3D12_RESOURCE_DESC const& Desc = BaseResource->GetDesc();
		ret.Width = (uint32)Desc.Width;
		ret.Height = Desc.Height;
		ret.SampleDesc.Count = 1;
		ret.SampleDesc.Quality = 0;
		MipIndex = TargetDesc.Texture3D.MipSlice;
		break;
	}
	default:
	{
		// not expecting 1D targets.
		checkNoEntry();
	}
	}
	ret.Width >>= MipIndex;
	ret.Height >>= MipIndex;
	return ret;
}

void FD3D12CommandContext::SetRenderTargets(
	uint32 NewNumSimultaneousRenderTargets,
	const FRHIRenderTargetView* NewRenderTargetsRHI,
	const FRHIDepthRenderTargetView* NewDepthStencilTargetRHI
	)
{
	FD3D12Texture* NewDepthStencilTarget = NewDepthStencilTargetRHI ? RetrieveTexture(NewDepthStencilTargetRHI->Texture) : nullptr;

	check(NewNumSimultaneousRenderTargets <= MaxSimultaneousRenderTargets);

	// Set the appropriate depth stencil view depending on whether depth writes are enabled or not
	FD3D12DepthStencilView* DepthStencilView = nullptr;
	if (NewDepthStencilTarget)
	{
		check(NewDepthStencilTargetRHI);	// Calm down static analysis
		DepthStencilView = NewDepthStencilTarget->GetDepthStencilView(NewDepthStencilTargetRHI->GetDepthStencilAccess());

		// Unbind any shader views of the depth stencil target that are bound.
		ClearShaderResources(NewDepthStencilTarget, EShaderParameterTypeMask::SRVMask | EShaderParameterTypeMask::UAVMask);
	}

	// Gather the render target views for the new render targets.
	FD3D12RenderTargetView* NewRenderTargetViews[MaxSimultaneousRenderTargets];
	for (uint32 RenderTargetIndex = 0;RenderTargetIndex < MaxSimultaneousRenderTargets;++RenderTargetIndex)
	{
		FD3D12RenderTargetView* RenderTargetView = NULL;
		if (RenderTargetIndex < NewNumSimultaneousRenderTargets && NewRenderTargetsRHI[RenderTargetIndex].Texture != nullptr)
		{
			int32 RTMipIndex = NewRenderTargetsRHI[RenderTargetIndex].MipIndex;
			int32 RTSliceIndex = NewRenderTargetsRHI[RenderTargetIndex].ArraySliceIndex;
			FD3D12Texture* NewRenderTarget = RetrieveTexture(NewRenderTargetsRHI[RenderTargetIndex].Texture);
			RenderTargetView = NewRenderTarget->GetRenderTargetView(RTMipIndex, RTSliceIndex);

			ensureMsgf(RenderTargetView, TEXT("Texture being set as render target has no RTV"));

			// Unbind any shader views of the render target that are bound.
			ClearShaderResources(NewRenderTarget, EShaderParameterTypeMask::SRVMask | EShaderParameterTypeMask::UAVMask);
		}

		NewRenderTargetViews[RenderTargetIndex] = RenderTargetView;
	}

	StateCache.SetRenderTargets(NewNumSimultaneousRenderTargets, NewRenderTargetViews, DepthStencilView);
	StateCache.ClearUAVs(SF_Pixel);

	// Set the viewport to the full size of render target 0.
	if (NewRenderTargetViews[0])
	{
		// check target 0 is valid
		check(0 < NewNumSimultaneousRenderTargets && NewRenderTargetsRHI[0].Texture != nullptr);
		FRTVDesc RTTDesc = GetRenderTargetViewDesc(NewRenderTargetViews[0]);
		RHISetViewport(0.0f, 0.0f, 0.0f, (float)RTTDesc.Width, (float)RTTDesc.Height, 1.0f);
	}
	else if (DepthStencilView)
	{
		FD3D12Resource* DepthTargetTexture = DepthStencilView->GetResource();
		D3D12_RESOURCE_DESC const& DTTDesc = DepthTargetTexture->GetDesc();
		RHISetViewport(0.0f, 0.0f, 0.0f, (float)DTTDesc.Width, (float)DTTDesc.Height, 1.0f);
	}
}

#if PLATFORM_SUPPORTS_VARIABLE_RATE_SHADING
static D3D12_SHADING_RATE_COMBINER ConvertShadingRateCombiner(EVRSRateCombiner InCombiner)
{
	switch (InCombiner)
	{
	case VRSRB_Override:
		return D3D12_SHADING_RATE_COMBINER_OVERRIDE;
	case VRSRB_Min:
		return D3D12_SHADING_RATE_COMBINER_MIN;
	case VRSRB_Max:
		return D3D12_SHADING_RATE_COMBINER_MAX;
	case VRSRB_Sum:
		return D3D12_SHADING_RATE_COMBINER_SUM;
	case VRSRB_Passthrough:
	default:
		return D3D12_SHADING_RATE_COMBINER_PASSTHROUGH;
	}
}
#endif

void FD3D12CommandContext::SetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& RenderTargetsInfo)
{
	FRHIUnorderedAccessView* UAVs[MaxSimultaneousUAVs] = {};

	this->SetRenderTargets(RenderTargetsInfo.NumColorRenderTargets,
		RenderTargetsInfo.ColorRenderTarget,
		&RenderTargetsInfo.DepthStencilRenderTarget);

	FD3D12RenderTargetView* RenderTargetViews[MaxSimultaneousRenderTargets];
	FD3D12DepthStencilView* DSView = nullptr;
	uint32 NumSimultaneousRTs = 0;
	StateCache.GetRenderTargets(RenderTargetViews, &NumSimultaneousRTs, &DSView);
	FD3D12BoundRenderTargets BoundRenderTargets(RenderTargetViews, NumSimultaneousRTs, DSView);
	FD3D12DepthStencilView* DepthStencilView = BoundRenderTargets.GetDepthStencilView();

	if (RenderTargetsInfo.bClearColor || RenderTargetsInfo.bClearStencil || RenderTargetsInfo.bClearDepth)
	{
		FLinearColor ClearColors[MaxSimultaneousRenderTargets];
		bool bClearColorArray[MaxSimultaneousRenderTargets];
		float DepthClear = 0.0;
		uint32 StencilClear = 0;

		if (RenderTargetsInfo.bClearColor)
		{
			for (int32 i = 0; i < RenderTargetsInfo.NumColorRenderTargets; ++i)
			{
				if (RenderTargetsInfo.ColorRenderTarget[i].Texture != nullptr)
				{
					const FClearValueBinding& ClearValue = RenderTargetsInfo.ColorRenderTarget[i].Texture->GetClearBinding();
					checkf(ClearValue.ColorBinding == EClearBinding::EColorBound, TEXT("Texture: %s does not have a color bound for fast clears"), *RenderTargetsInfo.ColorRenderTarget[i].Texture->GetName().GetPlainNameString());
					ClearColors[i] = ClearValue.GetClearColor();
				}
				else
				{
					ClearColors[i] = FLinearColor(ForceInitToZero);
				}
				bClearColorArray[i] = RenderTargetsInfo.ColorRenderTarget[i].LoadAction == ERenderTargetLoadAction::EClear;
			}
		}
		if (RenderTargetsInfo.bClearDepth || RenderTargetsInfo.bClearStencil)
		{
			const FClearValueBinding& ClearValue = RenderTargetsInfo.DepthStencilRenderTarget.Texture->GetClearBinding();
			checkf(ClearValue.ColorBinding == EClearBinding::EDepthStencilBound, TEXT("Texture: %s does not have a DS value bound for fast clears"), *RenderTargetsInfo.DepthStencilRenderTarget.Texture->GetName().GetPlainNameString());
			ClearValue.GetDepthStencil(DepthClear, StencilClear);
		}

		this->RHIClearMRTImpl(RenderTargetsInfo.bClearColor ? bClearColorArray : nullptr, RenderTargetsInfo.NumColorRenderTargets, ClearColors, RenderTargetsInfo.bClearDepth, DepthClear, RenderTargetsInfo.bClearStencil, StencilClear);
	}

#if PLATFORM_SUPPORTS_VARIABLE_RATE_SHADING
	if (GRHISupportsPipelineVariableRateShading && GRHIVariableRateShadingEnabled)
	{
		if (GRHISupportsAttachmentVariableRateShading && GRHIAttachmentVariableRateShadingEnabled)
		{
			if (RenderTargetsInfo.ShadingRateTexture != nullptr)
			{
				FD3D12Resource* Resource = RetrieveTexture(RenderTargetsInfo.ShadingRateTexture)->GetResource();

				TransitionResource(
					Resource,
					D3D12_RESOURCE_STATE_TBD,
					D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE,
					D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

				StateCache.SetShadingRateImage(Resource);
			}
			else
			{
				StateCache.SetShadingRateImage(nullptr);
			}
		}
		else
		{
			StateCache.SetShadingRateImage(nullptr);
		}
	}
#endif
}

void FD3D12CommandContext::RHICalibrateTimers(FRHITimestampCalibrationQuery* CalibrationQuery)
{
	FGPUTimingCalibrationTimestamp Timestamp = GetParentDevice()->GetCalibrationTimestamp(QueueType);
	CalibrationQuery->CPUMicroseconds[GetGPUIndex()] = Timestamp.CPUMicroseconds;
	CalibrationQuery->GPUMicroseconds[GetGPUIndex()] = Timestamp.GPUMicroseconds;
}

// Primitive drawing.

void FD3D12CommandContext::CommitNonComputeShaderConstants()
{
	//SCOPE_CYCLE_COUNTER(STAT_D3D12CommitGraphicsConstants);

	const FD3D12GraphicsPipelineState* const RESTRICT GraphicPSO = StateCache.GetGraphicsPipelineState();

	check(GraphicPSO);

	// Only set the constant buffer if this shader needs the global constant buffer bound
	// Otherwise we will overwrite a different constant buffer

	for (int32 Index = 0; Index < SF_NumGraphicsFrequencies; Index++)
	{
		const EShaderFrequency ShaderFrequency = static_cast<EShaderFrequency>(Index);
		if (IsValidGraphicsFrequency(ShaderFrequency) && GraphicPSO->bShaderNeedsGlobalConstantBuffer[Index])
		{
			StateCache.SetConstantBuffer(ShaderFrequency, StageConstantBuffers[Index], bDiscardSharedGraphicsConstants);
		}
	}

	bDiscardSharedGraphicsConstants = false;
}

void FD3D12CommandContext::CommitComputeShaderConstants()
{
	const FD3D12ComputePipelineState* const RESTRICT ComputePSO = StateCache.GetComputePipelineState();

	check(ComputePSO);

	if (ComputePSO->bShaderNeedsGlobalConstantBuffer)
	{
		StateCache.SetConstantBuffer(SF_Compute, StageConstantBuffers[SF_Compute], bDiscardSharedComputeConstants);
	}

	bDiscardSharedComputeConstants = false;
}

template <class ShaderType>
void FD3D12CommandContext::SetResourcesFromTables(const ShaderType* Shader)
{
	checkSlow(Shader);

	static constexpr EShaderFrequency Frequency = static_cast<EShaderFrequency>(ShaderType::StaticFrequency);

	FD3D12ResourceBinder Binder(*this, Frequency, Shader);
	UE::RHICore::SetResourcesFromTables(
		  Binder
		, *Shader
		, Shader->ShaderResourceTable
		, DirtyUniformBuffers[Frequency]
		, BoundUniformBuffers[Frequency]
#if ENABLE_RHI_VALIDATION
		, Tracker
#endif
	);
}

void FD3D12CommandContext::CommitGraphicsResourceTables()
{
	//SCOPE_CYCLE_COUNTER(STAT_D3D12CommitResourceTables);

	const FD3D12GraphicsPipelineState* const RESTRICT GraphicPSO = StateCache.GetGraphicsPipelineState();
	check(GraphicPSO);

	if (FD3D12PixelShader* Shader = GraphicPSO->GetPixelShader())
	{
		SetResourcesFromTables(Shader);
	}

	if (FD3D12VertexShader* Shader = GraphicPSO->GetVertexShader())
	{
		SetResourcesFromTables(Shader);
	}

#if PLATFORM_SUPPORTS_MESH_SHADERS
	if (FD3D12MeshShader* Shader = GraphicPSO->GetMeshShader())
	{
		SetResourcesFromTables(Shader);
	}
	if (FD3D12AmplificationShader* Shader = GraphicPSO->GetAmplificationShader())
	{
		SetResourcesFromTables(Shader);
	}
#endif

#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
	if (FD3D12GeometryShader* Shader = GraphicPSO->GetGeometryShader())
	{
		SetResourcesFromTables(Shader);
	}
#endif
}

void FD3D12CommandContext::CommitComputeResourceTables()
{
	//SCOPE_CYCLE_COUNTER(STAT_D3D12CommitResourceTables);

	const FD3D12ComputePipelineState* const RESTRICT ComputePSO = StateCache.GetComputePipelineState();

	SetResourcesFromTables(ComputePSO->GetComputeShader());
}

void FD3D12CommandContext::RHISetShaderRootConstants(const FUint32Vector4& Constants)
{
	StateCache.SetRootConstants(Constants);
}

void FD3D12CommandContext::RHIDispatchShaderBundle(
	FRHIShaderBundle* ShaderBundle,
	FRHIShaderResourceView* RecordArgBufferSRV,
	TConstArrayView<FRHIShaderBundleDispatch> Dispatches,
	bool bEmulated
)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(RHIDispatchShaderBundle);
	SCOPE_CYCLE_COUNTER(STAT_D3D12DispatchShaderBundle);

	check(ShaderBundle != nullptr && Dispatches.Num() > 0);
	TRHICommandList_RecursiveHazardous<FD3D12CommandContext> RHICmdList(this);
	UE::RHICore::DispatchShaderBundleEmulation(RHICmdList, ShaderBundle, RecordArgBufferSRV->GetBuffer(), Dispatches);
}

void FD3D12CommandContext::SetupDraw(FRHIBuffer* IndexBufferRHI, uint32 NumPrimitives /* = 0 */, uint32 NumVertices /* = 0 */)
{
	if (bTrackingEvents)
	{
		GetParentDevice()->RegisterGPUWork(NumPrimitives, NumVertices);
	}

	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();

	if (IndexBufferRHI)
	{
		FD3D12Buffer* IndexBuffer = RetrieveObject<FD3D12Buffer>(IndexBufferRHI);

		// determine 16bit vs 32bit indices
		const DXGI_FORMAT Format = (IndexBuffer->GetStride() == sizeof(uint16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT);

		StateCache.SetIndexBuffer(IndexBuffer->ResourceLocation, Format, 0);
	}

	StateCache.ApplyState(GetPipeline(), ED3D12PipelineType::Graphics);

	FlushPendingDescriptorUpdates();
}

void FD3D12CommandContext::SetupDispatchDraw(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{
	if (bTrackingEvents)
	{
		GetParentDevice()->RegisterGPUDispatch(FIntVector(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ));
	}

	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();

	StateCache.ApplyState(GetPipeline(), ED3D12PipelineType::Graphics);

	FlushPendingDescriptorUpdates();
}

void FD3D12CommandContext::RHIDrawPrimitive(uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances)
{
	RHI_DRAW_CALL_STATS(StateCache.GetGraphicsPipelinePrimitiveType(), FMath::Max(NumInstances, 1U) * NumPrimitives);

	uint32 VertexCount = StateCache.GetVertexCount(NumPrimitives);
	NumInstances = FMath::Max<uint32>(1, NumInstances);

	SetupDraw(nullptr, NumPrimitives * NumInstances, VertexCount * NumInstances);

	GraphicsCommandList()->DrawInstanced(VertexCount, NumInstances, BaseVertexIndex, 0);

	PostGpuEvent();
}

void FD3D12CommandContext::RHIDrawPrimitiveIndirect(FRHIBuffer* ArgumentBufferRHI, uint32 ArgumentOffset)
{
	RHI_DRAW_CALL_INC();

	SetupDraw(nullptr, 0, 0);

	FD3D12ResourceLocation& ArgumentBufferLocation = SetupIndirectArgument(ArgumentBufferRHI);

	GraphicsCommandList()->ExecuteIndirect(
		GetParentDevice()->GetParentAdapter()->GetDrawIndirectCommandSignature(),
		1,
		ArgumentBufferLocation.GetResource()->GetResource(),
		ArgumentBufferLocation.GetOffsetFromBaseOfResource() + ArgumentOffset,
		NULL,
		0
	);

	PostGpuEvent();
}

void FD3D12CommandContext::RHIDrawIndexedIndirect(FRHIBuffer* IndexBufferRHI, FRHIBuffer* ArgumentBufferRHI, int32 DrawArgumentsIndex, uint32 /*NumInstances*/)
{
	RHI_DRAW_CALL_INC();

	SetupDraw(IndexBufferRHI, 1);

	FD3D12ResourceLocation& ArgumentBufferLocation = SetupIndirectArgument(ArgumentBufferRHI);

	GraphicsCommandList()->ExecuteIndirect(
		GetParentDevice()->GetParentAdapter()->GetDrawIndexedIndirectCommandSignature(),
		1,
		ArgumentBufferLocation.GetResource()->GetResource(),
		ArgumentBufferLocation.GetOffsetFromBaseOfResource() + DrawArgumentsIndex * ArgumentBufferRHI->GetStride(),
		NULL,
		0
	);

	PostGpuEvent();
}

void FD3D12CommandContext::RHIDrawIndexedPrimitive(FRHIBuffer* IndexBufferRHI, int32 BaseVertexIndex, uint32 FirstInstance, uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances)
{
	FD3D12Buffer* IndexBuffer = RetrieveObject<FD3D12Buffer>(IndexBufferRHI);

	// called should make sure the input is valid, this avoid hidden bugs
	ensure(NumPrimitives > 0);
	ensure(IndexBufferRHI->GetSize() > 0);
	ensure(IndexBuffer->ResourceLocation.GetResource() != nullptr);

	if (IndexBufferRHI->GetSize() == 0 || IndexBuffer->ResourceLocation.GetResource() == nullptr)
	{
		return;
	}

	RHI_DRAW_CALL_STATS(StateCache.GetGraphicsPipelinePrimitiveType(), FMath::Max(NumInstances, 1U) * NumPrimitives);

	NumInstances = FMath::Max<uint32>(1, NumInstances);

	uint32 IndexCount = StateCache.GetVertexCount(NumPrimitives);

	// Verify that we are not trying to read outside the index buffer range
	// test is an optimized version of: StartIndex + IndexCount <= IndexBuffer->GetSize() / IndexBuffer->GetStride() 
	checkf((StartIndex + IndexCount) * IndexBuffer->GetStride() <= IndexBuffer->GetSize(),
		TEXT("Start %u, Count %u, Type %u, Buffer Size %u, Buffer stride %u"), StartIndex, IndexCount, StateCache.GetGraphicsPipelinePrimitiveType(), IndexBuffer->GetSize(), IndexBuffer->GetStride());

	SetupDraw(IndexBufferRHI, NumPrimitives * NumInstances, NumVertices * NumInstances);

	GraphicsCommandList()->DrawIndexedInstanced(IndexCount, NumInstances, StartIndex, BaseVertexIndex, FirstInstance);

	PostGpuEvent();
}

void FD3D12CommandContext::RHIMultiDrawIndexedPrimitiveIndirect(FRHIBuffer* IndexBufferRHI, FRHIBuffer* ArgumentBufferRHI, uint32 ArgumentOffset, FRHIBuffer* CountBufferRHI, uint32 CountBufferOffset, uint32 MaxDrawArguments)
{
	FD3D12Buffer* IndexBuffer = RetrieveObject<FD3D12Buffer>(IndexBufferRHI);

	// called should make sure the input is valid, this avoid hidden bugs
	if (!ensure(IndexBufferRHI->GetSize() > 0) || !ensure(IndexBuffer->ResourceLocation.GetResource() != nullptr))
	{
		return;
	}

	ID3D12Resource* CountBufferResource = nullptr;
	uint64 CountBufferOffsetFromResourceBase = 0;
	if (CountBufferRHI)
	{
		FD3D12Buffer* CountBuffer = RetrieveObject<FD3D12Buffer>(CountBufferRHI);
		FD3D12ResourceLocation& CounterLocation = CountBuffer->ResourceLocation;
		CountBufferResource = CounterLocation.GetResource()->GetResource();
		TransitionResource(CounterLocation.GetResource(), D3D12_RESOURCE_STATE_TBD, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
		CountBufferOffsetFromResourceBase = CounterLocation.GetOffsetFromBaseOfResource() + CountBufferOffset;
		UpdateResidency(CounterLocation.GetResource());
	}

	RHI_DRAW_CALL_INC();

	SetupDraw(IndexBufferRHI, 0, 0);

	FD3D12ResourceLocation& ArgumentBufferLocation = SetupIndirectArgument(ArgumentBufferRHI);

	GraphicsCommandList()->ExecuteIndirect(
		GetParentDevice()->GetParentAdapter()->GetDrawIndexedIndirectCommandSignature(),
		MaxDrawArguments,
		ArgumentBufferLocation.GetResource()->GetResource(),
		ArgumentBufferLocation.GetOffsetFromBaseOfResource() + ArgumentOffset,
		CountBufferResource,
		CountBufferOffsetFromResourceBase
	);

	PostGpuEvent();
}

void FD3D12CommandContext::RHIDrawIndexedPrimitiveIndirect(FRHIBuffer* IndexBufferRHI, FRHIBuffer* ArgumentBufferRHI, uint32 ArgumentOffset)
{
	// DrawIndexedPrimitiveIndirect is a special case of a more general MDI in D3D12
	RHIMultiDrawIndexedPrimitiveIndirect(IndexBufferRHI, ArgumentBufferRHI, ArgumentOffset, nullptr /*CounterBuffer*/, 0 /*CounterBufferOffset*/, 1 /*MaxDrawArguments*/);
}

#if PLATFORM_SUPPORTS_MESH_SHADERS
void FD3D12CommandContext::RHIDispatchMeshShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{
	SetupDispatchDraw(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);

	GraphicsCommandList6()->DispatchMesh(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);

	PostGpuEvent();
}

void FD3D12CommandContext::RHIDispatchIndirectMeshShader(FRHIBuffer* ArgumentBufferRHI, uint32 ArgumentOffset)
{
	RHI_DRAW_CALL_INC();

	SetupDispatchDraw(1, 1, 1);

	FD3D12ResourceLocation& ArgumentBufferLocation = SetupIndirectArgument(ArgumentBufferRHI);

	GraphicsCommandList()->ExecuteIndirect(
		GetParentDevice()->GetParentAdapter()->GetDispatchIndirectMeshCommandSignature(),
		1,
		ArgumentBufferLocation.GetResource()->GetResource(),
		ArgumentBufferLocation.GetOffsetFromBaseOfResource() + ArgumentOffset,
		NULL,
		0
	);

	PostGpuEvent();
}
#endif // PLATFORM_SUPPORTS_MESH_SHADERS

// Raster operations.
void FD3D12CommandContext::RHIClearMRTImpl(bool* bClearColorArray, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12ClearMRT);

	const D3D12_VIEWPORT& Viewport = StateCache.GetViewport();
	const D3D12_RECT& ScissorRect = StateCache.GetScissorRect();

	if (ScissorRect.left >= ScissorRect.right || ScissorRect.top >= ScissorRect.bottom)
	{
		return;
	}

	FD3D12RenderTargetView* RenderTargetViews[MaxSimultaneousRenderTargets];
	FD3D12DepthStencilView* DSView = nullptr;
	uint32 NumSimultaneousRTs = 0;
	StateCache.GetRenderTargets(RenderTargetViews, &NumSimultaneousRTs, &DSView);
	FD3D12BoundRenderTargets BoundRenderTargets(RenderTargetViews, NumSimultaneousRTs, DSView);
	FD3D12DepthStencilView* DepthStencilView = BoundRenderTargets.GetDepthStencilView();

	// Use rounding for when the number can't be perfectly represented by a float
	const LONG Width = static_cast<LONG>(FMath::RoundToInt(Viewport.Width));
	const LONG Height = static_cast<LONG>(FMath::RoundToInt(Viewport.Height));

	// When clearing we must pay attention to the currently set scissor rect
	bool bClearCoversEntireSurface = false;
	if (ScissorRect.left <= 0 && ScissorRect.top <= 0 &&
		ScissorRect.right >= Width && ScissorRect.bottom >= Height)
	{
		bClearCoversEntireSurface = true;
	}

	// Must specify enough clear colors for all active RTs
	check(!bClearColorArray || NumClearColors >= BoundRenderTargets.GetNumActiveTargets());

	const bool bSupportsFastClear = true;
	uint32 ClearRectCount = 0;
	D3D12_RECT* pClearRects = nullptr;
	D3D12_RECT ClearRects[4];

	// Only pass a rect down to the driver if we specifically want to clear a sub-rect
	if (!bSupportsFastClear || !bClearCoversEntireSurface)
	{
		{
			ClearRects[ClearRectCount] = ScissorRect;
			ClearRectCount++;
		}

		pClearRects = ClearRects;

		static const bool bSpewPerfWarnings = false;

		if (bSpewPerfWarnings)
		{
			UE_LOG(LogD3D12RHI, Warning, TEXT("RHIClearMRTImpl: Using non-fast clear path! This has performance implications"));
			UE_LOG(LogD3D12RHI, Warning, TEXT("       Viewport: Width %d, Height: %d"), static_cast<LONG>(FMath::RoundToInt(Viewport.Width)), static_cast<LONG>(FMath::RoundToInt(Viewport.Height)));
			UE_LOG(LogD3D12RHI, Warning, TEXT("   Scissor Rect: Width %d, Height: %d"), ScissorRect.right, ScissorRect.bottom);
		}
	}

	const bool ClearRTV = bClearColorArray && BoundRenderTargets.GetNumActiveTargets() > 0;
	const bool ClearDSV = (bClearDepth || bClearStencil) && DepthStencilView;

	uint32 ClearFlags = 0;
	if (ClearDSV)
	{
		if (bClearDepth && DepthStencilView->HasDepth())
		{
			ClearFlags |= D3D12_CLEAR_FLAG_DEPTH;
		}
		else if (bClearDepth)
		{
			UE_LOG(LogD3D12RHI, Warning, TEXT("RHIClearMRTImpl: Asking to clear a DSV that does not store depth."));
		}

		if (bClearStencil && DepthStencilView->HasStencil())
		{
			ClearFlags |= D3D12_CLEAR_FLAG_STENCIL;
		}
		else if (bClearStencil)
		{
			UE_LOG(LogD3D12RHI, Warning, TEXT("RHIClearMRTImpl: Asking to clear a DSV that does not store stencil."));
		}
	}

	if (ClearRTV || ClearDSV)
	{
		FlushResourceBarriers();

		if (ClearRTV)
		{
			for (int32 TargetIndex = 0; TargetIndex < BoundRenderTargets.GetNumActiveTargets(); TargetIndex++)
			{
				FD3D12RenderTargetView* RTView = BoundRenderTargets.GetRenderTargetView(TargetIndex);

				if (RTView != nullptr && bClearColorArray[TargetIndex])
				{
					GraphicsCommandList()->ClearRenderTargetView(RTView->GetOfflineCpuHandle(), (float*)&ClearColorArray[TargetIndex], ClearRectCount, pClearRects);
					UpdateResidency(RTView->GetResource());
				}
			}
		}

		if (ClearDSV)
		{
			GraphicsCommandList()->ClearDepthStencilView(DepthStencilView->GetOfflineCpuHandle(), (D3D12_CLEAR_FLAGS)ClearFlags, Depth, Stencil, ClearRectCount, pClearRects);
			UpdateResidency(DepthStencilView->GetResource());
		}

		ConditionalSplitCommandList();
	}

	if (IsDefaultContext())
	{
		GetParentDevice()->RegisterGPUWork(0);
	}

	DEBUG_EXECUTE_COMMAND_LIST(this);
}

// Blocks the CPU until the GPU catches up and goes idle.
void FD3D12DynamicRHI::RHIBlockUntilGPUIdle()
{
	RHISubmitCommandsAndFlushGPU();

	const int32 NumAdapters = ChosenAdapters.Num();
	for (int32 Index = 0; Index < NumAdapters; ++Index)
	{
		GetAdapter(Index).BlockUntilIdle();
	}
}

void FD3D12DynamicRHI::RHISubmitCommandsAndFlushGPU()
{
	FD3D12Adapter& Adapter = GetAdapter();
	for (uint32 GPUIndex : FRHIGPUMask::All())
	{
		Adapter.GetDevice(GPUIndex)->GetDefaultCommandContext().RHISubmitCommandsHint();
	}
}

/*
* Returns the total GPU time taken to render the last frame. Same metric as FPlatformTime::Cycles().
*/
uint32 FD3D12DynamicRHI::RHIGetGPUFrameCycles(uint32 GPUIndex)
{
	return GGPUFrameTime;
}


void FD3D12CommandContext::RHISetDepthBounds(float MinDepth, float MaxDepth)
{
	StateCache.SetDepthBounds(MinDepth, MaxDepth);
}

void FD3D12CommandContext::SetDepthBounds(float MinDepth, float MaxDepth)
{
#if PLATFORM_WINDOWS
	if (GSupportsDepthBoundsTest && GraphicsCommandList1())
	{
		// This should only be called if Depth Bounds Test is supported.
		GraphicsCommandList1()->OMSetDepthBounds(MinDepth, MaxDepth);
	}
#endif
}

void FD3D12CommandContext::RHISetShadingRate(EVRSShadingRate ShadingRate, EVRSRateCombiner Combiner)
{
#if PLATFORM_SUPPORTS_VARIABLE_RATE_SHADING
	// Note - this will override per-material VRS opt-out, but FRHICommandSetShadingRate isn't called from anywhere
	StateCache.SetShadingRate(ShadingRate, Combiner, VRSRB_Max);
#endif
}

void FD3D12CommandContext::SetShadingRate(EVRSShadingRate ShadingRate, const TStaticArray<EVRSRateCombiner, ED3D12VRSCombinerStages::Num>& Combiners)
{
 #if PLATFORM_SUPPORTS_VARIABLE_RATE_SHADING
 	if (GRHISupportsPipelineVariableRateShading && GRHIVariableRateShadingEnabled && GraphicsCommandList5())
 	{
		for (int32 CombinerIndex = 0; CombinerIndex < Combiners.Num(); ++CombinerIndex)
		{
			VRSCombiners[CombinerIndex] = ConvertShadingRateCombiner(Combiners[CombinerIndex]);
		}
 		VRSShadingRate = static_cast<D3D12_SHADING_RATE>(ShadingRate);
 		GraphicsCommandList5()->RSSetShadingRate(VRSShadingRate, VRSCombiners);
 	}
	else
	{
		// Ensure we're at a reasonable default in the case we're not supporting VRS.
		for (int32 CombinerIndex = 0; CombinerIndex < Combiners.Num(); ++CombinerIndex)
		{
			VRSCombiners[CombinerIndex] = D3D12_SHADING_RATE_COMBINER_PASSTHROUGH;
		}
	}
 #endif
}

void FD3D12CommandContext::SetShadingRateImage(FD3D12Resource* RateImageTexture)
{
#if PLATFORM_SUPPORTS_VARIABLE_RATE_SHADING
	if (GRHISupportsAttachmentVariableRateShading && GRHIAttachmentVariableRateShadingEnabled && GraphicsCommandList5())
	{
		if (RateImageTexture)
		{
			GraphicsCommandList5()->RSSetShadingRateImage(RateImageTexture->GetResource());
		}
		else
		{
			GraphicsCommandList5()->RSSetShadingRateImage(nullptr);
		}
	}
 #endif
}

void FD3D12CommandContext::RHISubmitCommandsHint()
{
	// Nothing to do
}

void FD3D12CommandContext::UpdateBuffer(FD3D12ResourceLocation* Dest, uint32 DestOffset, FD3D12ResourceLocation* Source, uint32 SourceOffset, uint32 NumBytes)
{
	FD3D12Resource* SourceResource = Source->GetResource();
	uint32 SourceFullOffset = Source->GetOffsetFromBaseOfResource() + SourceOffset;

	FD3D12Resource* DestResource = Dest->GetResource();
	uint32 DestFullOffset = Dest->GetOffsetFromBaseOfResource() + DestOffset;

	// Clear the resource if still bound to make sure the SRVs are rebound again on next operation (and get correct resource transitions enqueued)
	ConditionalClearShaderResource(Dest, EShaderParameterTypeMask::SRVMask);

	FScopedResourceBarrier ScopeResourceBarrierDest(*this, DestResource, Dest, D3D12_RESOURCE_STATE_COPY_DEST, 0);
	// Don't need to transition upload heaps

	FlushResourceBarriers();
	GraphicsCommandList()->CopyBufferRegion(DestResource->GetResource(), DestFullOffset, SourceResource->GetResource(), SourceFullOffset, NumBytes);
	UpdateResidency(DestResource);
	UpdateResidency(SourceResource);

	ConditionalSplitCommandList();

	DEBUG_RHI_EXECUTE_COMMAND_LIST(this);
}
