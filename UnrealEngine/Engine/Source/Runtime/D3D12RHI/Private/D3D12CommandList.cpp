// Copyright Epic Games, Inc. All Rights Reserved.

#include "D3D12RHIPrivate.h"
#include "D3D12CommandList.h"
#include "RHIValidation.h"

static int32 GD3D12BatchResourceBarriers = 1;
static FAutoConsoleVariableRef CVarD3D12BatchResourceBarriers(
	TEXT("d3d12.BatchResourceBarriers"),
	GD3D12BatchResourceBarriers,
	TEXT("Whether to allow batching resource barriers"));

static int32 GD3D12ExtraDepthTransitions = 0;
static FAutoConsoleVariableRef CVarD3D12ExtraDepthTransitions(
	TEXT("d3d12.ExtraDepthTransitions"),
	GD3D12ExtraDepthTransitions,
	TEXT("Adds extra transitions for the depth buffer to fix validation issues. However, this currently breaks async compute"));


int64 FD3D12CommandList::FState::NextCommandListID = 0;

void FD3D12CommandList::UpdateResidency(TConstArrayView<FD3D12ResidencyHandle*> Handles)
{
#if ENABLE_RESIDENCY_MANAGEMENT
	for (FD3D12ResidencyHandle* Handle : Handles)
	{
		if (D3DX12Residency::IsInitialized(Handle))
		{
			check(Device->GetGPUMask() == Handle->GPUObject->GetGPUMask());
			D3DX12Residency::Insert(*ResidencySet, *Handle);
		}
	}
#endif
}

void FD3D12ContextCommon::AddPendingResourceBarrier(FD3D12Resource* Resource, D3D12_RESOURCE_STATES After, uint32 SubResource, CResourceState& ResourceState_OnCommandList)
{
	check(After != D3D12_RESOURCE_STATE_TBD);
	check(Resource->RequiresResourceStateTracking());
	check(&GetCommandList().GetResourceState_OnCommandList(Resource) == &ResourceState_OnCommandList);
	
	GetCommandList().State.PendingResourceBarriers.Emplace(Resource, After, SubResource);
	ResourceState_OnCommandList.SetSubresourceState(SubResource, After);	
}

void FD3D12ContextCommon::AddTransitionBarrier(FD3D12Resource* pResource, D3D12_RESOURCE_STATES Before, D3D12_RESOURCE_STATES After, uint32 Subresource, CResourceState* ResourceState_OnCommandList)
{
	if (Before != After)
	{
		ResourceBarrierBatcher.AddTransition(pResource, Before, After, Subresource);

		UpdateResidency(pResource);

		if (!GD3D12BatchResourceBarriers)
		{
			FlushResourceBarriers();
		}
	}
	else
	{
		ensureMsgf(0, TEXT("AddTransitionBarrier: Before == After (%d)"), (uint32)Before);
	}

	if (pResource->RequiresResourceStateTracking())
	{
		if (!ResourceState_OnCommandList)
		{
			ResourceState_OnCommandList = &GetCommandList().GetResourceState_OnCommandList(pResource);
		}
		else
		{
			// If a resource state was passed to avoid a repeat lookup, it must be the one we expected.
			check(&GetCommandList().GetResourceState_OnCommandList(pResource) == ResourceState_OnCommandList);
		}

		ResourceState_OnCommandList->SetSubresourceState(Subresource, After);
		ResourceState_OnCommandList->SetHasInternalTransition();
	}
}

void FD3D12ContextCommon::AddUAVBarrier()
{
	ResourceBarrierBatcher.AddUAV();

	if (!GD3D12BatchResourceBarriers)
	{
		FlushResourceBarriers();
	}
}

void FD3D12ContextCommon::AddAliasingBarrier(ID3D12Resource* InResourceBefore, ID3D12Resource* InResourceAfter)
{
	ResourceBarrierBatcher.AddAliasingBarrier(InResourceBefore, InResourceAfter);

	if (!GD3D12BatchResourceBarriers)
	{
		FlushResourceBarriers();
	}
}


void FD3D12ContextCommon::FlushResourceBarriers()
{
	if (ResourceBarrierBatcher.Num())
	{
		ResourceBarrierBatcher.FlushIntoCommandList(GetCommandList(), TimestampQueries);
	}
}

CResourceState& FD3D12CommandList::GetResourceState_OnCommandList(FD3D12Resource* pResource)
{
	// Only certain resources should use this
	check(pResource->RequiresResourceStateTracking());

	CResourceState& ResourceState = State.TrackedResourceState.FindOrAdd(pResource);

	// If there is no entry, all subresources should be in the resource's TBD state.
	// This means we need to have pending resource barrier(s).
	if (!ResourceState.CheckResourceStateInitalized())
	{
		ResourceState.Initialize(pResource->GetSubresourceCount());
		check(ResourceState.CheckResourceState(D3D12_RESOURCE_STATE_TBD));
	}

	check(ResourceState.CheckResourceStateInitalized());

	return ResourceState;
}

FD3D12CommandAllocator::FD3D12CommandAllocator(FD3D12Device* Device, ED3D12QueueType QueueType)
	: Device(Device)
	, QueueType(QueueType)
{
	VERIFYD3D12RESULT(Device->GetDevice()->CreateCommandAllocator(GetD3DCommandListType(QueueType), IID_PPV_ARGS(CommandAllocator.GetInitReference())));
	INC_DWORD_STAT(STAT_D3D12NumCommandAllocators);
}

FD3D12CommandAllocator::~FD3D12CommandAllocator()
{
	CommandAllocator.SafeRelease();
	DEC_DWORD_STAT(STAT_D3D12NumCommandAllocators);
}

void FD3D12CommandAllocator::Reset()
{
	VERIFYD3D12RESULT(CommandAllocator->Reset());
}

FD3D12CommandList::FD3D12CommandList(FD3D12CommandAllocator* CommandAllocator, FD3D12QueryAllocator* TimestampAllocator, FD3D12QueryAllocator* PipelineStatsAllocator)
	: Device(CommandAllocator->Device)
	, QueueType(CommandAllocator->QueueType)
	, ResidencySet(D3DX12Residency::CreateResidencySet(Device->GetResidencyManager()))
	, State(CommandAllocator, TimestampAllocator, PipelineStatsAllocator)
{
	switch (QueueType)
	{
	case ED3D12QueueType::Direct:
	case ED3D12QueueType::Async:
		VERIFYD3D12RESULT(Device->GetDevice()->CreateCommandList(
			Device->GetGPUMask().GetNative(),
			GetD3DCommandListType(QueueType),
			*CommandAllocator,
			nullptr,
			IID_PPV_ARGS(Interfaces.GraphicsCommandList.GetInitReference())
		));
		Interfaces.CommandList = Interfaces.GraphicsCommandList;

		Interfaces.CommandList->QueryInterface(IID_PPV_ARGS(Interfaces.CopyCommandList.GetInitReference()));

		// Optionally obtain the versioned ID3D12GraphicsCommandList[0-9]+ interfaces, we don't check the HRESULT.
#if D3D12_MAX_COMMANDLIST_INTERFACE >= 1
		Interfaces.CommandList->QueryInterface(IID_PPV_ARGS(Interfaces.GraphicsCommandList1.GetInitReference()));
#endif
#if D3D12_MAX_COMMANDLIST_INTERFACE >= 2
		Interfaces.CommandList->QueryInterface(IID_PPV_ARGS(Interfaces.GraphicsCommandList2.GetInitReference()));
#endif
#if D3D12_MAX_COMMANDLIST_INTERFACE >= 3
		Interfaces.CommandList->QueryInterface(IID_PPV_ARGS(Interfaces.GraphicsCommandList3.GetInitReference()));
#endif
#if D3D12_MAX_COMMANDLIST_INTERFACE >= 4
		Interfaces.CommandList->QueryInterface(IID_PPV_ARGS(Interfaces.GraphicsCommandList4.GetInitReference()));
#endif
#if D3D12_MAX_COMMANDLIST_INTERFACE >= 5
		Interfaces.CommandList->QueryInterface(IID_PPV_ARGS(Interfaces.GraphicsCommandList5.GetInitReference()));
#endif
#if D3D12_MAX_COMMANDLIST_INTERFACE >= 6
		Interfaces.CommandList->QueryInterface(IID_PPV_ARGS(Interfaces.GraphicsCommandList6.GetInitReference()));
#endif
#if D3D12_MAX_COMMANDLIST_INTERFACE >= 7
		Interfaces.CommandList->QueryInterface(IID_PPV_ARGS(Interfaces.GraphicsCommandList7.GetInitReference()));
#endif
#if D3D12_MAX_COMMANDLIST_INTERFACE >= 8
		Interfaces.CommandList->QueryInterface(IID_PPV_ARGS(Interfaces.GraphicsCommandList8.GetInitReference()));
#endif
#if D3D12_MAX_COMMANDLIST_INTERFACE >= 9
		Interfaces.CommandList->QueryInterface(IID_PPV_ARGS(Interfaces.GraphicsCommandList9.GetInitReference()));
#endif
#if D3D12_PLATFORM_SUPPORTS_ASSERTRESOURCESTATES
		Interfaces.CommandList->QueryInterface(IID_PPV_ARGS(Interfaces.DebugCommandList.GetInitReference()));
#endif
		break;

	case ED3D12QueueType::Copy:
		VERIFYD3D12RESULT(Device->GetDevice()->CreateCommandList(
			Device->GetGPUMask().GetNative(),
			GetD3DCommandListType(QueueType),
			*CommandAllocator,
			nullptr,
			IID_PPV_ARGS(Interfaces.CopyCommandList.GetInitReference())
		));
		Interfaces.CommandList = Interfaces.CopyCommandList;

		break;

	default:
		checkNoEntry();
		return;
	}

	INC_DWORD_STAT(STAT_D3D12NumCommandLists);

#if NV_AFTERMATH
	if (GDX12NVAfterMathEnabled)
	{
		GFSDK_Aftermath_Result Result = GFSDK_Aftermath_DX12_CreateContextHandle(Interfaces.CommandList, &Interfaces.AftermathHandle);

		check(Result == GFSDK_Aftermath_Result_Success);
		Device->GetGPUProfiler().RegisterCommandList(Interfaces.GraphicsCommandList, Interfaces.AftermathHandle);
	}
#endif

#if NAME_OBJECTS
	FString Name = FString::Printf(TEXT("FD3D12CommandList (GPU %d)"), Device->GetGPUIndex());
	SetName(Interfaces.CommandList, Name.GetCharArray().GetData());
#endif

	D3DX12Residency::Open(ResidencySet);
	BeginLocalQueries();
}

FD3D12CommandList::~FD3D12CommandList()
{
	D3DX12Residency::DestroyResidencySet(Device->GetResidencyManager(), ResidencySet);

#if NV_AFTERMATH
	if (Interfaces.AftermathHandle)
	{
		Device->GetGPUProfiler().UnregisterCommandList(Interfaces.AftermathHandle);

		GFSDK_Aftermath_Result Result = GFSDK_Aftermath_ReleaseContextHandle(Interfaces.AftermathHandle);
		check(Result == GFSDK_Aftermath_Result_Success);
	}
#endif

	DEC_DWORD_STAT(STAT_D3D12NumCommandLists);
}

void FD3D12CommandList::Reset(FD3D12CommandAllocator* NewCommandAllocator, FD3D12QueryAllocator* TimestampAllocator, FD3D12QueryAllocator* PipelineStatsAllocator)
{
	check(IsClosed());
	check(NewCommandAllocator->Device == Device && NewCommandAllocator->QueueType == QueueType);
	if (Interfaces.CopyCommandList)
	{
		VERIFYD3D12RESULT(Interfaces.CopyCommandList->Reset(*NewCommandAllocator, nullptr));
	}
	else
	{
		VERIFYD3D12RESULT(Interfaces.GraphicsCommandList->Reset(*NewCommandAllocator, nullptr));
	}
	D3DX12Residency::Open(ResidencySet);

	State = FState(NewCommandAllocator, TimestampAllocator, PipelineStatsAllocator);
	BeginLocalQueries();
}

void FD3D12CommandList::Close()
{
	check(IsOpen());
	EndLocalQueries();

	if (Interfaces.CopyCommandList)
	{
		VERIFYD3D12RESULT(Interfaces.CopyCommandList->Close());
	}
	else
	{
		VERIFYD3D12RESULT(Interfaces.GraphicsCommandList->Close());
	}
	D3DX12Residency::Close(ResidencySet);
	State.IsClosed = true;
}

void FD3D12CommandList::BeginLocalQueries()
{
	if (!State.bLocalQueriesBegun)
	{
		if (State.BeginTimestamp)
		{
			EndQuery(State.BeginTimestamp);
		}

		if (State.PipelineStats)
		{
			BeginQuery(State.PipelineStats);
		}

		State.bLocalQueriesBegun = true;
	}
}

void FD3D12CommandList::EndLocalQueries()
{
	if (!State.bLocalQueriesEnded)
	{
		if (State.PipelineStats)
		{
			EndQuery(State.PipelineStats);
		}

		if (State.EndTimestamp)
		{
			EndQuery(State.EndTimestamp);
		}

		State.bLocalQueriesEnded = true;
	}
}

void FD3D12CommandList::BeginQuery(FD3D12QueryLocation const& Location)
{
	check(Location);
	check(Location.Heap->QueryType == D3D12_QUERY_TYPE_OCCLUSION || Location.Heap->QueryType == D3D12_QUERY_TYPE_PIPELINE_STATISTICS);

	GraphicsCommandList()->BeginQuery(
		Location.Heap->GetD3DQueryHeap(),
		Location.Heap->QueryType,
		Location.Index
	);
}

void FD3D12CommandList::EndQuery(FD3D12QueryLocation const& Location)
{
	check(Location);
	switch (Location.Heap->QueryType)
	{
	default:
		checkNoEntry();
		break;

	case D3D12_QUERY_TYPE_PIPELINE_STATISTICS:
		GraphicsCommandList()->EndQuery(
			Location.Heap->GetD3DQueryHeap(),
			Location.Heap->QueryType,
			Location.Index
		);
		State.PipelineStatsQueries.Add(Location);
		break;

	case D3D12_QUERY_TYPE_OCCLUSION:
		GraphicsCommandList()->EndQuery(
			Location.Heap->GetD3DQueryHeap(),
			Location.Heap->QueryType,
			Location.Index
		);
		State.OcclusionQueries.Add(Location);
		break;

	case D3D12_QUERY_TYPE_TIMESTAMP:
		WriteTimestamp(Location);

		// Command list begin/end timestamps are handled separately by the 
		// submission thread, so shouldn't be in the TimestampQueries array.
		if (Location.Type != ED3D12QueryType::CommandListBegin && Location.Type != ED3D12QueryType::CommandListEnd)
		{
			State.TimestampQueries.Add(Location);
		}
		break;
	}
}

#if D3D12RHI_PLATFORM_USES_TIMESTAMP_QUERIES
void FD3D12CommandList::WriteTimestamp(FD3D12QueryLocation const& Location)
{
	GraphicsCommandList()->EndQuery(
		Location.Heap->GetD3DQueryHeap(),
		Location.Heap->QueryType,
		Location.Index
	);
}
#endif // D3D12RHI_PLATFORM_USES_TIMESTAMP_QUERIES

FD3D12CommandList::FState::FState(FD3D12CommandAllocator* CommandAllocator, FD3D12QueryAllocator* TimestampAllocator, FD3D12QueryAllocator* PipelineStatsAllocator)
	: CommandAllocator(CommandAllocator)
	, CommandListID   (FPlatformAtomics::InterlockedIncrement(&NextCommandListID))
{
	PendingResourceBarriers.Reserve(256);

	if (TimestampAllocator)
	{
		BeginTimestamp = TimestampAllocator->Allocate(ED3D12QueryType::CommandListBegin, nullptr);
		EndTimestamp   = TimestampAllocator->Allocate(ED3D12QueryType::CommandListEnd  , nullptr);
	}

	if (PipelineStatsAllocator)
	{
		PipelineStats = PipelineStatsAllocator->Allocate(ED3D12QueryType::PipelineStats, nullptr);
	}
}

void FD3D12ContextCommon::TransitionResource(FD3D12DepthStencilView* View)
{
	// Determine the required subresource states from the view desc
	const D3D12_DEPTH_STENCIL_VIEW_DESC& DSVDesc = View->GetD3DDesc();
	const bool bDSVDepthIsWritable = (DSVDesc.Flags & D3D12_DSV_FLAG_READ_ONLY_DEPTH) == 0;
	const bool bDSVStencilIsWritable = (DSVDesc.Flags & D3D12_DSV_FLAG_READ_ONLY_STENCIL) == 0;
	// TODO: Check if the PSO depth stencil is writable. When this is done, we need to transition in SetDepthStencilState too.

	// This code assumes that the DSV always contains the depth plane
	check(View->HasDepth());
	const bool bHasDepth = true;
	const bool bHasStencil = View->HasStencil();
	const bool bDepthIsWritable = bHasDepth && bDSVDepthIsWritable;
	const bool bStencilIsWritable = bHasStencil && bDSVStencilIsWritable;

	// DEPTH_WRITE is suitable for read operations when used as a normal depth/stencil buffer.
	FD3D12Resource* Resource = View->GetResource();
	if (bDepthIsWritable)
	{
		TransitionResource(Resource, D3D12_RESOURCE_STATE_TBD, D3D12_RESOURCE_STATE_DEPTH_WRITE, View->GetDepthOnlySubset());
	}
	else if (bHasDepth && GD3D12ExtraDepthTransitions)
	{
		TransitionResource(Resource, D3D12_RESOURCE_STATE_TBD, D3D12_RESOURCE_STATE_DEPTH_READ, View->GetDepthOnlySubset());
	}

	if (bStencilIsWritable)
	{
		TransitionResource(Resource, D3D12_RESOURCE_STATE_TBD, D3D12_RESOURCE_STATE_DEPTH_WRITE, View->GetStencilOnlySubset());
	}
	else if (bHasStencil && GD3D12ExtraDepthTransitions)
	{
		TransitionResource(Resource, D3D12_RESOURCE_STATE_TBD, D3D12_RESOURCE_STATE_DEPTH_READ, View->GetStencilOnlySubset());
	}
}

void FD3D12ContextCommon::TransitionResource(FD3D12DepthStencilView* View, D3D12_RESOURCE_STATES After)
{
	FD3D12Resource* Resource = View->GetResource();

	const D3D12_DEPTH_STENCIL_VIEW_DESC& Desc = View->GetD3DDesc();
	switch (Desc.ViewDimension)
	{
	case D3D12_DSV_DIMENSION_TEXTURE2D:
	case D3D12_DSV_DIMENSION_TEXTURE2DMS:
		if (Resource->GetPlaneCount() > 1)
		{
			// Multiple subresources to transtion
			TransitionResource(Resource, D3D12_RESOURCE_STATE_TBD, After, View->GetViewSubset());
		}
		else
		{
			// Only one subresource to transition
			check(Resource->GetPlaneCount() == 1);
			TransitionResource(Resource, D3D12_RESOURCE_STATE_TBD, After, Desc.Texture2D.MipSlice);
		}
		break;

	case D3D12_DSV_DIMENSION_TEXTURE2DARRAY:
	case D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY:
		// Multiple subresources to transtion
		TransitionResource(Resource, D3D12_RESOURCE_STATE_TBD, After, View->GetViewSubset());
		break;

	default:
		checkNoEntry();
		break;
	}
}

void FD3D12ContextCommon::TransitionResource(FD3D12RenderTargetView* View, D3D12_RESOURCE_STATES After)
{
	FD3D12Resource* Resource = View->GetResource();

	const D3D12_RENDER_TARGET_VIEW_DESC& Desc = View->GetD3DDesc();
	switch (Desc.ViewDimension)
	{
	case D3D12_RTV_DIMENSION_TEXTURE3D:
		// Note: For volume (3D) textures, all slices for a given mipmap level are a single subresource index.
		// Fall-through
	case D3D12_RTV_DIMENSION_TEXTURE2D:
	case D3D12_RTV_DIMENSION_TEXTURE2DMS:
		// Only one subresource to transition
		TransitionResource(Resource, D3D12_RESOURCE_STATE_TBD, After, Desc.Texture2D.MipSlice);
		break;

	case D3D12_RTV_DIMENSION_TEXTURE2DARRAY:
	case D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY:
		// Multiple subresources to transition
		TransitionResource(Resource, D3D12_RESOURCE_STATE_TBD, After, View->GetViewSubset());
		break;

	default:
		UE_LOG(LogRHI, Fatal, TEXT("Unsupported resource dimension %d"), Desc.ViewDimension);
		break;
	}
}

void FD3D12ContextCommon::TransitionResource(FD3D12ShaderResourceView* View, D3D12_RESOURCE_STATES After)
{
	FD3D12Resource* Resource = View->GetResource();

	// Early out if we never need to do state tracking, the resource should always be in an SRV state.
	if (!Resource || !Resource->RequiresResourceStateTracking())
		return;

	const D3D12_RESOURCE_DESC& ResDesc = Resource->GetDesc();
	const FD3D12ViewSubset& ViewSubset = View->GetViewSubset();

	const D3D12_SHADER_RESOURCE_VIEW_DESC& Desc = View->GetD3DDesc();
	switch (Desc.ViewDimension)
	{
	default:
		// Transition the resource
		TransitionResource(Resource, D3D12_RESOURCE_STATE_TBD, After, ViewSubset);
		break;

	case D3D12_SRV_DIMENSION_BUFFER:
		if (Resource->GetHeapType() == D3D12_HEAP_TYPE_DEFAULT)
		{
			// Transition the resource
			TransitionResource(Resource, D3D12_RESOURCE_STATE_TBD, After, ViewSubset);
		}
		break;
	}
}

void FD3D12ContextCommon::TransitionResource(FD3D12UnorderedAccessView* View, D3D12_RESOURCE_STATES After)
{
	FD3D12Resource* Resource = View->GetResource();

	const D3D12_UNORDERED_ACCESS_VIEW_DESC& Desc = View->GetD3DDesc();
	switch (Desc.ViewDimension)
	{
	case D3D12_UAV_DIMENSION_BUFFER:
		TransitionResource(Resource, D3D12_RESOURCE_STATE_TBD, After, 0);
		break;

	case D3D12_UAV_DIMENSION_TEXTURE2D:
		// Only one subresource to transition
		TransitionResource(Resource, D3D12_RESOURCE_STATE_TBD, After, Desc.Texture2D.MipSlice);
		break;

	case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
		// Multiple subresources to transtion
		TransitionResource(Resource, D3D12_RESOURCE_STATE_TBD, After, View->GetViewSubset());
		break;

	case D3D12_UAV_DIMENSION_TEXTURE3D:
		// Multiple subresources to transtion
		TransitionResource(Resource, D3D12_RESOURCE_STATE_TBD, After, View->GetViewSubset());
		break;

	default:
		checkNoEntry();
		break;
	}
}

bool FD3D12ContextCommon::TransitionResource(FD3D12Resource* Resource, D3D12_RESOURCE_STATES Before, D3D12_RESOURCE_STATES After, uint32 Subresource)
{
	check(Resource);
	check(Resource->RequiresResourceStateTracking());
	check(!((After & (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)) && (Resource->GetDesc().Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)));

#ifdef PLATFORM_SUPPORTS_RESOURCE_COMPRESSION
	After |= Resource->GetCompressedState();
#endif

	UpdateResidency(Resource);

	bool bRequireUAVBarrier = false;

	CResourceState& ResourceState = GetCommandList().GetResourceState_OnCommandList(Resource);
	if (Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES && !ResourceState.AreAllSubresourcesSame())
	{
		// Slow path. Want to transition the entire resource (with multiple subresources). But they aren't in the same state.

		const uint32 SubresourceCount = Resource->GetSubresourceCount();
		for (uint32 SubresourceIndex = 0; SubresourceIndex < SubresourceCount; SubresourceIndex++)
		{
			bool bForceInAfterState = true;
			bRequireUAVBarrier |= TransitionResource(Resource, ResourceState, SubresourceIndex, Before, After, bForceInAfterState);
		}

		// The entire resource should now be in the after state on this command list (even if all barriers are pending)
		verify(ResourceState.CheckAllSubresourceSame());
		check(EnumHasAllFlags(ResourceState.GetSubresourceState(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES), After));
	}
	else
	{
		bool bForceInAfterState = false;
		bRequireUAVBarrier = TransitionResource(Resource, ResourceState, Subresource, Before, After, bForceInAfterState);
	}

	return bRequireUAVBarrier;
}

// Transition subresources from current to a new state, using resource state tracking.
bool FD3D12ContextCommon::TransitionResource(FD3D12Resource* Resource, D3D12_RESOURCE_STATES Before, D3D12_RESOURCE_STATES After, FD3D12ViewSubset const& ViewSubset)
{
	check(Resource);
	check(Resource->RequiresResourceStateTracking());
	check(!((After & (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)) && (Resource->GetDesc().Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)));

#ifdef PLATFORM_SUPPORTS_RESOURCE_COMPRESSION
	After |= Resource->GetCompressedState();
#endif

	UpdateResidency(Resource);

	const bool bIsWholeResource = ViewSubset.IsWholeResource();
	CResourceState& ResourceState = GetCommandList().GetResourceState_OnCommandList(Resource);

	bool bRequireUAVBarrier = false;

	if (bIsWholeResource && ResourceState.AreAllSubresourcesSame())
	{
		// Fast path. Transition the entire resource from one state to another.
		bool bForceInAfterState = false;
		bRequireUAVBarrier = TransitionResource(Resource, ResourceState, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, Before, After, bForceInAfterState);
	}
	else
	{
		// Slower path. Either the subresources are in more than 1 state, or the view only partially covers the resource.
		// Either way, we'll need to loop over each subresource in the view...

		bool bWholeResourceWasTransitionedToSameState = bIsWholeResource;
		for (uint32 SubresourceIndex : ViewSubset)
		{
			bool bForceInAfterState = false;
			bRequireUAVBarrier |= TransitionResource(Resource, ResourceState, SubresourceIndex, Before, After, bForceInAfterState);

			// Subresource not in the same state, then whole resource is not in the same state anymore
			if (ResourceState.GetSubresourceState(SubresourceIndex) != After)
				bWholeResourceWasTransitionedToSameState = false;
		}

		// If we just transtioned every subresource to the same state, lets update it's tracking so it's on a per-resource level
		if (bWholeResourceWasTransitionedToSameState)
		{
			// Sanity check to make sure all subresources are really in the 'after' state
			verify(ResourceState.CheckAllSubresourceSame());
			check(EnumHasAllFlags(ResourceState.GetSubresourceState(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES), After));
		}
	}

	return bRequireUAVBarrier;
}

static inline bool IsTransitionNeeded(D3D12_RESOURCE_STATES Before, D3D12_RESOURCE_STATES& After)
{
	check(Before != D3D12_RESOURCE_STATE_CORRUPT && After != D3D12_RESOURCE_STATE_CORRUPT);
	check(Before != D3D12_RESOURCE_STATE_TBD && After != D3D12_RESOURCE_STATE_TBD);

	// Depth write is actually a suitable for read operations as a "normal" depth buffer.
	if (Before == D3D12_RESOURCE_STATE_DEPTH_WRITE && After == D3D12_RESOURCE_STATE_DEPTH_READ)
	{
		if (GD3D12ExtraDepthTransitions)
		{
			After = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		}
		return false;
	}

	// COMMON is an oddball state that doesn't follow the RESOURE_STATE pattern of 
	// having exactly one bit set so we need to special case these
	if (After == D3D12_RESOURCE_STATE_COMMON)
	{
		// Before state should not have the common state otherwise it's invalid transition
		check(Before != D3D12_RESOURCE_STATE_COMMON);
		return true;
	}

	// We should avoid doing read-to-read state transitions. But when we do, we should avoid turning off already transitioned bits,
	// e.g. VERTEX_BUFFER -> SHADER_RESOURCE is turned into VERTEX_BUFFER -> VERTEX_BUFFER | SHADER_RESOURCE.
	// This reduces the number of resource transitions and ensures automatic states from resource bindings get properly combined.
	D3D12_RESOURCE_STATES Combined = Before | After;
	if ((Combined & (D3D12_RESOURCE_STATE_GENERIC_READ | D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)) == Combined)
	{
		After = Combined;
	}

	return Before != After;
}

bool FD3D12ContextCommon::TransitionResource(FD3D12Resource* InResource, CResourceState& ResourceState_OnCommandList, uint32 InSubresourceIndex, D3D12_RESOURCE_STATES InBeforeState, D3D12_RESOURCE_STATES InAfterState, bool bInForceAfterState)
{
	// Try and get the correct D3D before state for the transition
	D3D12_RESOURCE_STATES const TrackedState = ResourceState_OnCommandList.GetSubresourceState(InSubresourceIndex);
	D3D12_RESOURCE_STATES BeforeState = TrackedState != D3D12_RESOURCE_STATE_TBD ? TrackedState : InBeforeState;

	// Make sure the before states match up or are unknown
	check(InBeforeState == D3D12_RESOURCE_STATE_TBD || BeforeState == InBeforeState);

	bool bRequireUAVBarrier = false;
	if (BeforeState != D3D12_RESOURCE_STATE_TBD)
	{
		bool bApplyTransitionBarrier = true;

		// Require UAV barrier when before and after are UAV
		if (BeforeState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS && InAfterState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		{
			bRequireUAVBarrier = true;
		}
		// Special case for UAV access resources
		else if (InResource->GetUAVAccessResource() && EnumHasAnyFlags(BeforeState | InAfterState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS))
		{
			// inject an aliasing barrier
			const bool bFromUAV = EnumHasAnyFlags(BeforeState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			const bool bToUAV = EnumHasAnyFlags(InAfterState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			check(bFromUAV != bToUAV);

			AddAliasingBarrier(
				bFromUAV ? InResource->GetUAVAccessResource() : InResource->GetResource(),
				bToUAV ? InResource->GetUAVAccessResource() : InResource->GetResource());

			if (bToUAV)
			{
				ResourceState_OnCommandList.SetUAVHiddenResourceState(BeforeState);
				bApplyTransitionBarrier = false;
			}
			else
			{
				D3D12_RESOURCE_STATES HiddenState = ResourceState_OnCommandList.GetUAVHiddenResourceState();

				// Still unknown in this command list?
				if (HiddenState == D3D12_RESOURCE_STATE_TBD)
				{
					AddPendingResourceBarrier(InResource, InAfterState, InSubresourceIndex, ResourceState_OnCommandList);
					bApplyTransitionBarrier = false;
				}
				else
				{
					// Use the hidden state as the before state on the resource
					BeforeState = HiddenState;
				}
			}
		}

		if (bApplyTransitionBarrier)
		{
			// We're not using IsTransitionNeeded() when bInForceAfterState is set because we do want to transition even if 'after' is a subset of 'before'
			// This is so that we can ensure all subresources are in the same state, simplifying future barriers
			// No state merging when using engine transitions - otherwise next before state might not match up anymore)
			if ((bInForceAfterState && BeforeState != InAfterState) || IsTransitionNeeded(BeforeState, InAfterState))
			{
				AddTransitionBarrier(InResource, BeforeState, InAfterState, InSubresourceIndex, &ResourceState_OnCommandList);
			}
			else
			{
				// Ensure the command list tracking is up to date, even if we skipped an unnecessary transition.
				ResourceState_OnCommandList.SetSubresourceState(InSubresourceIndex, InAfterState);
			}
		}
	}
	else
	{
		// BeforeState is TBD. We need a pending resource barrier.

		// Special handling for UAVAccessResource and transition to UAV - don't want to enqueue pending resource to UAV because the actual resource won't transition
		// Adding of patch up will only be done when transitioning to non UAV state
		if (InResource->GetUAVAccessResource() && EnumHasAnyFlags(InAfterState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS))
		{
			AddAliasingBarrier(InResource->GetResource(), InResource->GetUAVAccessResource());
			ResourceState_OnCommandList.SetUAVHiddenResourceState(D3D12_RESOURCE_STATE_TBD);
		}
		else
		{
			// We need a pending resource barrier so we can setup the state before this command list executes
			AddPendingResourceBarrier(InResource, InAfterState, InSubresourceIndex, ResourceState_OnCommandList);
		}
	}

	return bRequireUAVBarrier;
}

namespace D3D12RHI
{
	void GetGfxCommandListAndQueue(FRHICommandList& RHICmdList, void*& OutGfxCmdList, void*& OutCommandQueue)
	{
		IRHICommandContext& RHICmdContext = RHICmdList.GetContext();
		FD3D12CommandContext& CmdContext = static_cast<FD3D12CommandContext&>(RHICmdContext);
		check(CmdContext.IsDefaultContext());

		OutGfxCmdList = CmdContext.GraphicsCommandList().Get();
		OutCommandQueue = CmdContext.Device->GetQueue(CmdContext.QueueType).D3DCommandQueue;
	}
}
