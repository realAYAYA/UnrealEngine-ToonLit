// Copyright Epic Games, Inc. All Rights Reserved.

#include "D3D12RHIPrivate.h"
#include "D3D12PoolAllocator.h"
#include "HAL/LowLevelMemTracker.h"

#ifndef NEEDS_D3D12_INDIRECT_ARGUMENT_HEAP_WORKAROUND
#define NEEDS_D3D12_INDIRECT_ARGUMENT_HEAP_WORKAROUND 0
#endif

LLM_DECLARE_TAG(D3D12AllocatorUnused);

//-----------------------------------------------------------------------------
//	FD3D12MemoryPool
//-----------------------------------------------------------------------------

FD3D12MemoryPool::FD3D12MemoryPool(
		  FD3D12Device* ParentDevice
		, FRHIGPUMask VisibleNodes
		, const FD3D12ResourceInitConfig& InInitConfig
		, const FString& InName
		, EResourceAllocationStrategy InAllocationStrategy
		, int16 InPoolIndex
		, uint64 InPoolSize
		, uint32 InPoolAlignment
		, ERHIPoolResourceTypes InSupportedResourceTypes
		, EFreeListOrder InFreeListOrder
		, HeapId InTraceParentHeapId
	)
	: FRHIMemoryPool(InPoolIndex, InPoolSize, InPoolAlignment, InSupportedResourceTypes, InFreeListOrder)
	, FD3D12DeviceChild(ParentDevice)
	, FD3D12MultiNodeGPUObject(ParentDevice->GetGPUMask(), VisibleNodes)
	, InitConfig(InInitConfig)
	, Name(InName)
	, AllocationStrategy(InAllocationStrategy)
	, LastUsedFrameFence(0)
{
#if UE_MEMORY_TRACE_ENABLED
	TraceHeapId = MemoryTrace_HeapSpec(InTraceParentHeapId, AllocationStrategy == EResourceAllocationStrategy::kPlacedResource ? TEXT("D3D12MemoryPool (PlacedResource)") : TEXT("D3D12MemoryPool (ManualSubAllocation)"));
#endif
}


FD3D12MemoryPool::~FD3D12MemoryPool()
{
	Destroy();
}


void FD3D12MemoryPool::Init()
{
	if (PoolSize == 0)
	{
		return;
	}

	FD3D12Device* Device = GetParentDevice();
	FD3D12Adapter* Adapter = Device->GetParentAdapter();

	if (AllocationStrategy == EResourceAllocationStrategy::kPlacedResource)
	{
		// Alignment should be either 4K or 64K for places resources
		check(PoolAlignment == D3D12_SMALL_RESOURCE_PLACEMENT_ALIGNMENT || PoolAlignment == D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);

		D3D12_HEAP_PROPERTIES HeapProps = CD3DX12_HEAP_PROPERTIES(InitConfig.HeapType);
		HeapProps.CreationNodeMask = GetGPUMask().GetNative();
		HeapProps.VisibleNodeMask = GetVisibilityMask().GetNative();

		D3D12_HEAP_DESC Desc = {};
		Desc.SizeInBytes = PoolSize;
		Desc.Properties = HeapProps;
		Desc.Alignment = 0;
		Desc.Flags = InitConfig.HeapFlags;

		// All all resources types on heap when tracking allocations (need to be able to create a buffer on it to extract the base gpu address)
		// (Tier 2 support already checked when this flag is enabled)
		if (Adapter->IsTrackingAllAllocations())
		{
			Desc.Flags = D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES;
		}

		if (Adapter->IsHeapNotZeroedSupported())
		{
			Desc.Flags |= FD3D12_HEAP_FLAG_CREATE_NOT_ZEROED;
		}

		ID3D12Heap* Heap = nullptr;
		{
			LLM_PLATFORM_SCOPE(ELLMTag::GraphicsPlatform);

			// we are tracking allocations ourselves, so don't let XMemAlloc track these as well
			LLM_SCOPED_PAUSE_TRACKING_FOR_TRACKER(ELLMTracker::Default, ELLMAllocType::System);
			VERIFYD3D12RESULT(Adapter->GetD3DDevice()->CreateHeap(&Desc, IID_PPV_ARGS(&Heap)));
		}

		BackingHeap = new FD3D12Heap(GetParentDevice(), GetVisibilityMask(), TraceHeapId);
		BackingHeap->SetHeap(Heap, TEXT("PoolAllocator Heap"));

		// Only track resources that cannot be accessed on the CPU.
		if (IsGPUOnly(InitConfig.HeapType))
		{
			BackingHeap->BeginTrackingResidency(Desc.SizeInBytes);
		}
	}
	else
	{
		{
			LLM_SCOPED_PAUSE_TRACKING_FOR_TRACKER(ELLMTracker::Default, ELLMAllocType::System);
			const D3D12_HEAP_PROPERTIES HeapProps = CD3DX12_HEAP_PROPERTIES(InitConfig.HeapType, GetGPUMask().GetNative(), GetVisibilityMask().GetNative());
			VERIFYD3D12RESULT(Adapter->CreateBuffer(HeapProps, GetGPUMask(), InitConfig.InitialResourceState, ED3D12ResourceStateMode::SingleState, InitConfig.InitialResourceState, PoolSize, BackingResource.GetInitReference(), TEXT("Resource Allocator Underlying Buffer"), InitConfig.ResourceFlags));
#if UE_MEMORY_TRACE_ENABLED
			MemoryTrace_MarkAllocAsHeap(BackingResource->GetGPUVirtualAddress(), TraceHeapId);
#endif
		}

		if (IsCPUAccessible(InitConfig.HeapType))
		{
			BackingResource->Map();
		}
	}	

#if D3D12_RHI_RAYTRACING
	// Elevates the raytracing acceleration structure heap priority, which may help performance / stability in low memory conditions.
	if (IsGPUOnly(InitConfig.HeapType) && InitConfig.InitialResourceState == D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE)
	{
		for (uint32 GPUIndex : GetGPUMask())
		{
			ID3D12Pageable* HeapResource = AllocationStrategy == EResourceAllocationStrategy::kPlacedResource
				? BackingHeap->GetHeap()
				: BackingResource->GetPageable();
			D3D12_RESIDENCY_PRIORITY HeapPriority = D3D12_RESIDENCY_PRIORITY_HIGH;
			FD3D12Device* NodeDevice = Adapter->GetDevice(GPUIndex);
			NodeDevice->GetDevice5()->SetResidencyPriority(1, &HeapResource, &HeapPriority);
		}
	}
#endif // D3D12_RHI_RAYTRACING

	LLM_SCOPED_PAUSE_TRACKING_WITH_ENUM_AND_AMOUNT_BYTAG(D3D12AllocatorUnused, int64(PoolSize), ELLMTracker::Platform, ELLMAllocType::System);
	FRHIMemoryPool::Init();
}


void FD3D12MemoryPool::Destroy()
{
	LLM_SCOPED_PAUSE_TRACKING_FOR_TRACKER(ELLMTracker::Default, ELLMAllocType::System);

	FRHIMemoryPool::Destroy();

	if (BackingResource)
	{
#if UE_MEMORY_TRACE_ENABLED
		MemoryTrace_UnmarkAllocAsHeap(BackingResource->GetGPUVirtualAddress(), TraceHeapId);
		MemoryTrace_Free(BackingResource->GetGPUVirtualAddress(), EMemoryTraceRootHeap::VideoMemory);
#endif
		ensure(BackingResource->GetRefCount() == 1 || GNumExplicitGPUsForRendering > 1);
		BackingResource = nullptr;
	}
}


//-----------------------------------------------------------------------------
//	FD3D12PoolAllocator
//-----------------------------------------------------------------------------


FD3D12ResourceInitConfig FD3D12PoolAllocator::GetResourceAllocatorInitConfig(D3D12_HEAP_TYPE InHeapType, D3D12_RESOURCE_FLAGS InResourceFlags, EBufferUsageFlags InBufferUsage)
{
	FD3D12ResourceInitConfig InitConfig;
	InitConfig.HeapType = InHeapType;
	InitConfig.ResourceFlags = InResourceFlags;

#if D3D12_RHI_RAYTRACING
	// Setup initial resource state depending on the requested buffer flags
	if (EnumHasAnyFlags(InBufferUsage, BUF_AccelerationStructure))
	{
		// should only have this flag and no other flags
		check(InBufferUsage == BUF_AccelerationStructure);
		InitConfig.InitialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	}
	else
#endif // D3D12_RHI_RAYTRACING
		if (InitConfig.HeapType == D3D12_HEAP_TYPE_READBACK)
		{
			InitConfig.InitialResourceState = D3D12_RESOURCE_STATE_COPY_DEST;
		}
		else if (EnumHasAnyFlags(InBufferUsage, BUF_UnorderedAccess))
		{
			check(InResourceFlags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			InitConfig.InitialResourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}
		else
		{
			InitConfig.InitialResourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
		}

	InitConfig.HeapFlags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
	if (EnumHasAnyFlags(InBufferUsage, BUF_DrawIndirect))
	{
		check(InResourceFlags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
#if !NEEDS_D3D12_INDIRECT_ARGUMENT_HEAP_WORKAROUND
		InitConfig.HeapFlags |= D3D12RHI_HEAP_FLAG_ALLOW_INDIRECT_BUFFERS;
#endif
	}

	return InitConfig;
}

EResourceAllocationStrategy FD3D12PoolAllocator::GetResourceAllocationStrategy(D3D12_RESOURCE_FLAGS InResourceFlags, ED3D12ResourceStateMode InResourceStateMode, uint32 Alignment)
{
	if (Alignment > kD3D12ManualSubAllocationAlignment)
	{
		return EResourceAllocationStrategy::kPlacedResource;
	}

	// Does the resource need state tracking and transitions
	ED3D12ResourceStateMode ResourceStateMode = InResourceStateMode;
	if (ResourceStateMode == ED3D12ResourceStateMode::Default)
	{
		ResourceStateMode = (InResourceFlags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) ? ED3D12ResourceStateMode::MultiState : ED3D12ResourceStateMode::SingleState;
	}

	// multi state resource need to placed because each allocation can be in a different state
	return (ResourceStateMode == ED3D12ResourceStateMode::MultiState) ? EResourceAllocationStrategy::kPlacedResource : EResourceAllocationStrategy::kManualSubAllocation;
}


FD3D12PoolAllocator::FD3D12PoolAllocator(FD3D12Device* ParentDevice, FRHIGPUMask VisibleNodes, const FD3D12ResourceInitConfig& InInitConfig, const FString& InName,
	EResourceAllocationStrategy InAllocationStrategy, uint64 InDefaultPoolSize, uint32 InPoolAlignment, uint32 InMaxAllocationSize, FRHIMemoryPool::EFreeListOrder InFreeListOrder, bool bInDefragEnabled, HeapId InTraceParentHeapId) :
	FRHIPoolAllocator(InDefaultPoolSize, InPoolAlignment, InMaxAllocationSize, InFreeListOrder, bInDefragEnabled), 
	FD3D12DeviceChild(ParentDevice), 
	FD3D12MultiNodeGPUObject(ParentDevice->GetGPUMask(), VisibleNodes), 
	InitConfig(InInitConfig), 
	Name(InName), 
	AllocationStrategy(InAllocationStrategy)
{
#if UE_MEMORY_TRACE_ENABLED
	TraceHeapId = MemoryTrace_HeapSpec(InTraceParentHeapId, *Name);
#endif
}


FD3D12PoolAllocator::~FD3D12PoolAllocator()
{
	Destroy();
}


bool FD3D12PoolAllocator::SupportsAllocation(D3D12_HEAP_TYPE InHeapType, D3D12_RESOURCE_FLAGS InResourceFlags, EBufferUsageFlags InBufferUsage, ED3D12ResourceStateMode InResourceStateMode, uint32 Alignment) const
{
	FD3D12ResourceInitConfig InInitConfig = GetResourceAllocatorInitConfig(InHeapType, InResourceFlags, InBufferUsage);
	EResourceAllocationStrategy InAllocationStrategy = GetResourceAllocationStrategy(InResourceFlags, InResourceStateMode, Alignment);

	// Don't check the resource flags & states for places resource but only the heap flags
	if (InAllocationStrategy == EResourceAllocationStrategy::kPlacedResource && AllocationStrategy == InAllocationStrategy)
	{
		return (InitConfig.HeapType == InInitConfig.HeapType && InitConfig.HeapFlags == InInitConfig.HeapFlags);
	}
	else
	{
		return (InitConfig == InInitConfig && AllocationStrategy == InAllocationStrategy);
	}
}


void FD3D12PoolAllocator::AllocDefaultResource(D3D12_HEAP_TYPE InHeapType, const FD3D12ResourceDesc& InDesc, EBufferUsageFlags InBufferUsage, ED3D12ResourceStateMode InResourceStateMode,
	D3D12_RESOURCE_STATES InCreateState, uint32 InAllocationAlignment, const TCHAR* InName, FD3D12ResourceLocation& ResourceLocation)
{
#if DO_CHECK
	// Validate the create state
	if (InHeapType == D3D12_HEAP_TYPE_READBACK)
	{
		check(InCreateState == D3D12_RESOURCE_STATE_COPY_DEST);
	}
	else if (InHeapType == D3D12_HEAP_TYPE_UPLOAD)
	{
		check(InCreateState == D3D12_RESOURCE_STATE_GENERIC_READ);
	}
	else if (InBufferUsage == BUF_UnorderedAccess && InResourceStateMode == ED3D12ResourceStateMode::SingleState)
	{
		check(InCreateState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}
#if D3D12_RHI_RAYTRACING
	else if (EnumHasAnyFlags(InBufferUsage, BUF_AccelerationStructure))
	{
		// RayTracing acceleration structures must be created in a particular state and may never transition out of it.
		check(InResourceStateMode == ED3D12ResourceStateMode::SingleState);
		check(InCreateState == D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
	}
#endif // D3D12_RHI_RAYTRACING
#endif  // DO_CHECK

	AllocateResource(GetParentDevice()->GetGPUIndex(), InHeapType, InDesc, InDesc.Width, InAllocationAlignment, InResourceStateMode, InCreateState, nullptr, InName, ResourceLocation);
}


void FD3D12PoolAllocator::AllocateResource(uint32 GPUIndex, D3D12_HEAP_TYPE InHeapType, const FD3D12ResourceDesc& InDesc, uint64 InSize, uint32 InAllocationAlignment, ED3D12ResourceStateMode InResourceStateMode,
	D3D12_RESOURCE_STATES InCreateState, const D3D12_CLEAR_VALUE* InClearValue, const TCHAR* InName, FD3D12ResourceLocation& ResourceLocation)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(D3D12RHI::AllocatePoolResource);

	// If the resource location owns a block, this will deallocate it.
	ResourceLocation.Clear();
	if (InSize == 0)
	{
		return;
	}

	FD3D12Device* Device = GetParentDevice();
	FD3D12Adapter* Adapter = Device->GetParentAdapter();

	check(GPUIndex == Device->GetGPUIndex());

	const bool bSharedResource = EnumHasAnyFlags(InDesc.Flags, D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS);
	
	bool bPoolResource = InSize <= MaxAllocationSize && !bSharedResource;
	bool bPlacedResource = bPoolResource && (AllocationStrategy == EResourceAllocationStrategy::kPlacedResource);
	if (bPlacedResource)
	{
		// Make sure that we can satisfy the alignment requirements. If not, we have to fall back to a committed resource. This can happen for MSAA targets,
		// because the pools are aligned to D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT, but MSAA needs more.
		if (InAllocationAlignment > PoolAlignment || InDesc.Alignment > PoolAlignment)
		{
			bPoolResource = false;
			bPlacedResource = false;
		}
	}

	// Disable pooling for VRAM allocated textures and force use the committed resource path
	bool bForceCommittedResourcePath = false;
#if PLATFORM_WINDOWS
	if (bPoolResource && GD3D12WorkaroundFlags.bForceCommittedResourceTextureAllocation && InHeapType == D3D12_HEAP_TYPE_DEFAULT && InDesc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER)
	{
		bForceCommittedResourcePath = true;
	}
#endif // PLATFORM_WINDOWS

	if (bPoolResource && !bForceCommittedResourcePath)
	{
		uint32 AllocationAlignment = InAllocationAlignment;

		// Ensure we're allocating from the correct pool
		if (bPlacedResource)
		{
			// If it's a placed resource then base offset will always be 0 from the actual d3d resource so ignore the allocation alignment - no extra offset required
			// for creating the views!
			AllocationAlignment = PoolAlignment;
		}
		else
		{
			// Read-only resources get suballocated from big resources, thus share ID3D12Resource* and resource state with other resources. Ensure it's suballocated from a resource with identical flags.
			check(InDesc.Flags == InitConfig.ResourceFlags);
		}

		// Find the correct allocation resource type
		ERHIPoolResourceTypes AllocationResourceType;
		if (InDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
		{
			AllocationResourceType = ERHIPoolResourceTypes::Buffers;
		}
		else if (EnumHasAnyFlags(InDesc.Flags, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL))
		{
			AllocationResourceType = ERHIPoolResourceTypes::RTDSTextures;
		}
		else
		{
			AllocationResourceType = ERHIPoolResourceTypes::NonRTDSTextures;
		}

		// Try to allocate in one of the pools
		FRHIPoolAllocationData& AllocationData = ResourceLocation.GetPoolAllocatorPrivateData().PoolData;
		verify(TryAllocateInternal(InSize, AllocationAlignment, AllocationResourceType, AllocationData));

		// Setup the resource location
		ResourceLocation.SetType(FD3D12ResourceLocation::ResourceLocationType::eSubAllocation);
		ResourceLocation.SetPoolAllocator(this);
		ResourceLocation.SetSize(InSize);

		AllocationData.SetOwner(&ResourceLocation);

		if (AllocationStrategy == EResourceAllocationStrategy::kManualSubAllocation)
		{
			FD3D12Resource* BackingResource = GetBackingResource(ResourceLocation);

			ResourceLocation.SetOffsetFromBaseOfResource(AllocationData.GetOffset());
			ResourceLocation.SetResource(BackingResource);
			ResourceLocation.SetGPUVirtualAddress(BackingResource->GetGPUVirtualAddress() + AllocationData.GetOffset());

			if (IsCPUAccessible(InitConfig.HeapType))
			{
				ResourceLocation.SetMappedBaseAddress((uint8*)BackingResource->GetResourceBaseAddress() + AllocationData.GetOffset());
			}
		}
		else
		{
			check(ResourceLocation.GetResource() == nullptr);
			
			FD3D12ResourceDesc Desc = InDesc;
			Desc.Alignment = AllocationAlignment;

			FD3D12Resource* NewResource = CreatePlacedResource(AllocationData, Desc, InCreateState, InResourceStateMode, InClearValue, InName);
			ResourceLocation.SetResource(NewResource);
			ResourceLocation.SetGPUVirtualAddress(NewResource->GetGPUVirtualAddress());
		}

		UpdateAllocationTracking(ResourceLocation, EAllocationType::Allocate);
	}
	else
	{
		FD3D12Resource* NewResource = nullptr;
		const D3D12_HEAP_PROPERTIES HeapProps = CD3DX12_HEAP_PROPERTIES(InHeapType, GetGPUMask().GetNative(), GetVisibilityMask().GetNative());
		FD3D12ResourceDesc Desc = InDesc;
		Desc.Alignment = 0;

		// If we are tracking all allocation data and allocating a standalone texture, then first create a heap so we can retrieve the GPU virtual address as well
		// UAV Aliasing needs a Heap to create the aliased resource in.
		if (InDesc.NeedsUAVAliasWorkarounds() || (Adapter->IsTrackingAllAllocations() && InDesc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER && !bForceCommittedResourcePath))
		{
			D3D12_HEAP_DESC HeapDesc = {};
			HeapDesc.SizeInBytes = FMath::Max((uint64)D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT, InSize);
			HeapDesc.Properties = HeapProps;
			HeapDesc.Alignment = InDesc.SampleDesc.Count > 1 ? D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT : 0;
			HeapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES;
			if (Adapter->IsHeapNotZeroedSupported())
			{
				HeapDesc.Flags |= FD3D12_HEAP_FLAG_CREATE_NOT_ZEROED;
			}

			if (Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS)
			{
				HeapDesc.Flags |= D3D12_HEAP_FLAG_SHARED;
			}

			ID3D12Heap* Heap = nullptr;
			VERIFYD3D12RESULT(Adapter->GetD3DDevice()->CreateHeap(&HeapDesc, IID_PPV_ARGS(&Heap)));
			TRefCountPtr<FD3D12Heap> BackingHeap = new FD3D12Heap(GetParentDevice(), GetVisibilityMask(), TraceHeapId);
			bool bTrack = false;
			BackingHeap->SetHeap(Heap, InName, bTrack);
			BackingHeap->BeginTrackingResidency(HeapDesc.SizeInBytes);

			VERIFYD3D12RESULT(Adapter->CreatePlacedResource(Desc, BackingHeap, 0, InCreateState, InResourceStateMode, InCreateState, InClearValue, &NewResource, InName));
		}
		else
		{
			// Allocate Standalone - move to owner of resource because this allocator should only manage pooled allocations (needed for now to do the same as FD3D12DefaultBufferPool)
			D3D12_RESOURCE_STATES DefaultState = (InResourceStateMode == ED3D12ResourceStateMode::Default) ? D3D12_RESOURCE_STATE_TBD : InCreateState;
			VERIFYD3D12RESULT(Adapter->CreateCommittedResource(Desc, GetGPUMask(), HeapProps, InCreateState, InResourceStateMode, DefaultState, InClearValue, &NewResource, InName, false));
		}

		ResourceLocation.AsStandAlone(NewResource, InSize);
	}

#if D3D12_RHI_RAYTRACING
	// Elevates the raytracing acceleration structure heap priority, which may help performance / stability in low memory conditions.
	// When allocation is pooled, the entire pool priority is boosted on creation.
	if (InCreateState == D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE
		&& ResourceLocation.GetAllocatorType() != FD3D12ResourceLocation::AT_Pool)
	{
		ID3D12Pageable* HeapResource = ResourceLocation.GetResource()->GetPageable();
		Adapter->SetResidencyPriority(HeapResource, D3D12_RESIDENCY_PRIORITY_HIGH, GPUIndex);
	}
#endif // D3D12_RHI_RAYTRACING
}


FD3D12Resource* FD3D12PoolAllocator::CreatePlacedResource(
	const FRHIPoolAllocationData& InAllocationData,
	const FD3D12ResourceDesc& InDesc,
	D3D12_RESOURCE_STATES InCreateState, 
	ED3D12ResourceStateMode InResourceStateMode, 
	const D3D12_CLEAR_VALUE* InClearValue, 
	const TCHAR* InName)
{
	FD3D12Adapter* Adapter = GetParentDevice()->GetParentAdapter();
	FD3D12HeapAndOffset HeapAndOffset = GetBackingHeapAndAllocationOffsetInBytes(InAllocationData);

	FD3D12Resource* NewResource = nullptr;
	VERIFYD3D12RESULT(Adapter->CreatePlacedResource(InDesc, HeapAndOffset.Heap, HeapAndOffset.Offset, InCreateState, InResourceStateMode, D3D12_RESOURCE_STATE_TBD, InClearValue, &NewResource, InName));
	return NewResource;
}


void FD3D12PoolAllocator::DeallocateResource(FD3D12ResourceLocation& ResourceLocation, bool bDefragFree)
{
	check(IsOwner(ResourceLocation));

	UpdateAllocationTracking(ResourceLocation, bDefragFree ? EAllocationType::DefragFree : EAllocationType::Free);
	
	FScopeLock Lock(&CS);

	// Mark allocation data as free
	FRHIPoolAllocationData& AllocationData = ResourceLocation.GetPoolAllocatorPrivateData().PoolData;
	check(AllocationData.IsAllocated());

	// If locked then assume still initial setup or in defragmentation unlock request
	// Mark as nop because block will be deleted anyway
	// TODO: optimize to not perform the full iteration here!
	if (AllocationData.IsLocked())
	{
		for (FrameFencedAllocationData& Operation : FrameFencedOperations)
		{
			if (Operation.AllocationData == &AllocationData)
			{
				check(Operation.Operation == FrameFencedAllocationData::EOperation::Unlock);
				Operation.Operation = FrameFencedAllocationData::EOperation::Nop;
				Operation.AllocationData = nullptr;
				break;
			}
		}
		
		// If still pending copy then clear the copy operation data
		for (FD3D12VRAMCopyOperation& CopyOperation : PendingCopyOps)
		{
			if (CopyOperation.SourceResource == ResourceLocation.GetResource() ||
				CopyOperation.DestResource == ResourceLocation.GetResource())
			{
				CopyOperation.SourceResource = nullptr;
				CopyOperation.DestResource = nullptr;
				break;
			}
		}
	}

	int16 PoolIndex = AllocationData.GetPoolIndex();
	FRHIPoolAllocationData* ReleasedAllocationData = (AllocationDataPool.Num() > 0) ? AllocationDataPool.Pop(EAllowShrinking::No) : new FRHIPoolAllocationData();
	bool bLocked = true;
	ReleasedAllocationData->MoveFrom(AllocationData, bLocked);

	// Clear the allocator data
	ResourceLocation.ClearAllocator();

	// Store fence when last used so we know when to unlock the free data
	FD3D12Adapter* Adapter = GetParentDevice()->GetParentAdapter();
	FD3D12ManualFence& FrameFence = Adapter->GetFrameFence();

	FrameFencedAllocationData& DeleteRequest = FrameFencedOperations.Emplace_GetRef();
	DeleteRequest.Operation = FrameFencedAllocationData::EOperation::Deallocate;
	DeleteRequest.FrameFence = FrameFence.GetNextFenceToSignal();
	DeleteRequest.AllocationData = ReleasedAllocationData;

	PendingDeleteRequestSize += DeleteRequest.AllocationData->GetSize();

	// Update the last used frame fence (used during garbage collection)
	((FD3D12MemoryPool*)Pools[PoolIndex])->UpdateLastUsedFrameFence(DeleteRequest.FrameFence);

	// Also store the placed resource so it can be correctly freed when fence is done
	if (ResourceLocation.GetResource()->IsPlacedResource())
	{
		DeleteRequest.PlacedResource = ResourceLocation.GetResource();
	}
	else
	{
		DeleteRequest.PlacedResource = nullptr;
	}
}


FRHIMemoryPool* FD3D12PoolAllocator::CreateNewPool(int16 InPoolIndex, uint32 InMinimumAllocationSize, ERHIPoolResourceTypes InAllocationResourceType)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FD3D12PoolAllocator::CreateNewPool);

	// Find out the pool size - use default, but if allocation doesn't fit then round up to next power of 2
	// so it 'always' fits the pool allocator
	uint32 PoolSize = DefaultPoolSize;
	if (InMinimumAllocationSize > PoolSize)
	{
		check(InMinimumAllocationSize <= MaxAllocationSize);
		PoolSize = FMath::Min(FMath::RoundUpToPowerOfTwo(InMinimumAllocationSize), (uint32)MaxAllocationSize);
	}

	FD3D12MemoryPool* NewPool = new FD3D12MemoryPool(GetParentDevice(),	GetVisibilityMask(), InitConfig,
		Name, AllocationStrategy, InPoolIndex, PoolSize, PoolAlignment, InAllocationResourceType, FreeListOrder, TraceHeapId);
	NewPool->Init();
	return NewPool;
}


bool FD3D12PoolAllocator::HandleDefragRequest(FRHICommandListBase& RHICmdList, FRHIPoolAllocationData* InSourceBlock, FRHIPoolAllocationData& InTmpTargetBlock)
{
	// Cache source copy data
	FD3D12ResourceLocation* Owner = (FD3D12ResourceLocation*)InSourceBlock->GetOwner();
	uint64 CurrentOffset = Owner->GetOffsetFromBaseOfResource();
	FD3D12Resource* CurrentResource = Owner->GetResource();

	// Release the current allocation (will only be freed on the next frame fence)
	bool bDefragFree = true;
	DeallocateResource(*Owner, bDefragFree);

	// Move temp allocation data to allocation data of the owner (part of different allocator now)		
	bool bLocked = true;
	InSourceBlock->MoveFrom(InTmpTargetBlock, bLocked);
	InSourceBlock->SetOwner(Owner);
	Owner->SetPoolAllocator(this);

	// Notify owner of moved allocation data (recreated resources and SRVs if needed)
	Owner->OnAllocationMoved(RHICmdList, InSourceBlock);

	// Add request to unlock the source block on the next fence value (copy operation should have been done by then)
	FD3D12Adapter* Adapter = GetParentDevice()->GetParentAdapter();
	FD3D12ManualFence& FrameFence = Adapter->GetFrameFence();

	FrameFencedAllocationData& UnlockRequest = FrameFencedOperations.Emplace_GetRef();
	UnlockRequest.Operation = FrameFencedAllocationData::EOperation::Unlock;
	UnlockRequest.FrameFence = FrameFence.GetNextFenceToSignal();
	UnlockRequest.AllocationData = InSourceBlock;

	// Schedule a copy operation of the actual data
	FD3D12VRAMCopyOperation CopyOp;
	CopyOp.SourceResource	= CurrentResource;
	CopyOp.SourceOffset		= CurrentOffset;
	CopyOp.DestResource		= Owner->GetResource();
	CopyOp.DestOffset		= Owner->GetOffsetFromBaseOfResource();
	CopyOp.Size				= InSourceBlock->GetSize();
	CopyOp.CopyType			= AllocationStrategy == EResourceAllocationStrategy::kManualSubAllocation ? FD3D12VRAMCopyOperation::ECopyType::BufferRegion : FD3D12VRAMCopyOperation::ECopyType::Resource;
	check(CopyOp.SourceResource != nullptr);
	check(CopyOp.DestResource != nullptr);
	PendingCopyOps.Add(CopyOp);

	UpdateAllocationTracking(*Owner, EAllocationType::Allocate);

	// TODO: Using aliasing buffer on whole heap for copies to reduce flushes and resource transitions

	return true;
}


void FD3D12PoolAllocator::CleanUpAllocations(uint64 InFrameLag, bool bForceFree)
{
	FScopeLock Lock(&CS);

	FD3D12Adapter* Adapter = GetParentDevice()->GetParentAdapter();
	FD3D12ManualFence& FrameFence = Adapter->GetFrameFence();

	uint32 PopCount = 0;
	for (int32 i = 0; i < FrameFencedOperations.Num(); i++)
	{
		FrameFencedAllocationData& Operation = FrameFencedOperations[i];
		if (bForceFree || FrameFence.IsFenceComplete(Operation.FrameFence, /* bUpdateCachedFenceValue */ false))
		{
			switch (Operation.Operation)
			{
			case FrameFencedAllocationData::EOperation::Deallocate:
			{
				// Not pending anymore
				check(PendingDeleteRequestSize >= Operation.AllocationData->GetSize());
				PendingDeleteRequestSize -= Operation.AllocationData->GetSize();
				// Deallocate the locked block (actually free now)
				DeallocateInternal(*Operation.AllocationData);
				Operation.AllocationData->Reset();
				AllocationDataPool.Add(Operation.AllocationData);

				// Free placed resource if created
				if (AllocationStrategy == EResourceAllocationStrategy::kPlacedResource)
				{
					// Release the resource
					check(Operation.PlacedResource != nullptr);
					Operation.PlacedResource->Release();
					Operation.PlacedResource = nullptr;
				}
				else
				{
					check(Operation.PlacedResource == nullptr);
				}
				break;
			}
			case FrameFencedAllocationData::EOperation::Unlock:
			{
				Operation.AllocationData->Unlock();
				break;
			}
			case FrameFencedAllocationData::EOperation::Nop:
			{
				break;
			}
			default: check(false);
			}

			PopCount = i + 1;
		}
		else
		{
			break;
		}
	}

	if (PopCount)
	{
		// clear out all of the released blocks, don't allow the array to shrink
		FrameFencedOperations.RemoveAt(0, PopCount, EAllowShrinking::No);
	}

	// Trim empty allocators if not used in last n frames
	const uint64 CompletedFence = FrameFence.GetCompletedFenceValue(/* bUpdateCachedFenceValue */ true);
	for (int32 PoolIndex = 0; PoolIndex < Pools.Num(); ++PoolIndex)
	{
		FD3D12MemoryPool* MemoryPool = (FD3D12MemoryPool*) Pools[PoolIndex];
		if (MemoryPool != nullptr && MemoryPool->IsEmpty() && (bForceFree || (MemoryPool->GetLastUsedFrameFence() + InFrameLag <= CompletedFence)))
		{
			LLM_SCOPED_PAUSE_TRACKING_WITH_ENUM_AND_AMOUNT_BYTAG(D3D12AllocatorUnused, 0 - int64(MemoryPool->GetPoolSize()), ELLMTracker::Platform, ELLMAllocType::System);
			MemoryPool->Destroy();
			delete(MemoryPool);
			Pools[PoolIndex] = nullptr;
		}
	}

	// Update the allocation order
	Algo::Sort(PoolAllocationOrder, [this](uint32 InLHS, uint32 InRHS)
		{
			// first allocate from 'fullest' pools when defrag is enabled, otherwise try and allocate from
			// default sized pools with stable order
			if (bDefragEnabled)
			{
				uint32 LHSUsePoolSize = Pools[InLHS] ? Pools[InLHS]->GetUsedSize() : UINT32_MAX;
				uint32 RHSUsePoolSize = Pools[InRHS] ? Pools[InRHS]->GetUsedSize() : UINT32_MAX;
				return LHSUsePoolSize > RHSUsePoolSize;
			}
			else
			{
				uint32 LHSPoolSize = Pools[InLHS] ? Pools[InLHS]->GetPoolSize() : UINT32_MAX;
				uint32 RHSPoolSize = Pools[InRHS] ? Pools[InRHS]->GetPoolSize() : UINT32_MAX;
				if (LHSPoolSize != RHSPoolSize)
				{
					return LHSPoolSize < RHSPoolSize;
				}

				uint32 LHSPoolIndex = Pools[InLHS] ? Pools[InLHS]->GetPoolIndex() : UINT32_MAX;
				uint32 RHSPoolIndex = Pools[InRHS] ? Pools[InRHS]->GetPoolIndex() : UINT32_MAX;
				return LHSPoolIndex < RHSPoolIndex;
			}
		});
}


void FD3D12PoolAllocator::TransferOwnership(FD3D12ResourceLocation& InSource, FD3D12ResourceLocation& InDest)
{
	FScopeLock Lock(&CS);

	check(IsOwner(InSource));

	FRHIPoolAllocationData& SourcePoolData = InSource.GetPoolAllocatorPrivateData().PoolData;
	FRHIPoolAllocationData& DestinationPoolData = InDest.GetPoolAllocatorPrivateData().PoolData;

	// If locked then assume the block is currently being defragged and then the frame fenced operation
	// also needs to be updated
	if (SourcePoolData.IsLocked())
	{
		bool bFound = false;
		for (FrameFencedAllocationData& Operation : FrameFencedOperations)
		{
			if (Operation.AllocationData == &SourcePoolData)
			{
				check(Operation.Operation == FrameFencedAllocationData::EOperation::Unlock);
				Operation.AllocationData = &DestinationPoolData;
				bFound = true;
				break;
			}
		}
		check(bFound);
	}

	// Take over lock as well - can be defragging
	bool bLocked = SourcePoolData.IsLocked();
	DestinationPoolData.MoveFrom(InSource.GetPoolAllocatorPrivateData().PoolData, bLocked);
	DestinationPoolData.SetOwner(&InDest);
}

FD3D12Resource* FD3D12PoolAllocator::GetBackingResource(FD3D12ResourceLocation& InResourceLocation) const
{
	check(IsOwner(InResourceLocation));
	FRHIPoolAllocationData& AllocationData = InResourceLocation.GetPoolAllocatorPrivateData().PoolData;
	return ((FD3D12MemoryPool*)Pools[AllocationData.GetPoolIndex()])->GetBackingResource();
}


FD3D12HeapAndOffset FD3D12PoolAllocator::GetBackingHeapAndAllocationOffsetInBytes(FD3D12ResourceLocation& InResourceLocation) const
{
	check(IsOwner(InResourceLocation));

	return GetBackingHeapAndAllocationOffsetInBytes(InResourceLocation.GetPoolAllocatorPrivateData().PoolData);
}


FD3D12HeapAndOffset FD3D12PoolAllocator::GetBackingHeapAndAllocationOffsetInBytes(const FRHIPoolAllocationData& InAllocationData) const
{
	FD3D12HeapAndOffset HeapAndOffset;
	HeapAndOffset.Heap = ((FD3D12MemoryPool*)Pools[InAllocationData.GetPoolIndex()])->GetBackingHeap();
	HeapAndOffset.Offset = uint64(AlignDown(InAllocationData.GetOffset(), PoolAlignment));
	return HeapAndOffset;
}


void FD3D12PoolAllocator::UpdateAllocationTracking(FD3D12ResourceLocation& InAllocation, EAllocationType InAllocationType)
{
#if TRACK_RESOURCE_ALLOCATIONS
	check(IsOwner(InAllocation));
	FRHIPoolAllocationData& AllocationData = InAllocation.GetPoolAllocatorPrivateData().PoolData;
	check(AllocationData.IsAllocated());
	uint64 AllocationSize = AllocationData.GetSize();

	// Track only when tracking all resources is set
	FD3D12Adapter* Adapter = GetParentDevice()->GetParentAdapter();
	if (InitConfig.HeapType == D3D12_HEAP_TYPE_DEFAULT && AllocationStrategy == EResourceAllocationStrategy::kPlacedResource && Adapter->IsTrackingAllAllocations())
	{
		if (InAllocationType == EAllocationType::Allocate)
		{
			bool bCollectCallstack = false;
			Adapter->TrackAllocationData(&InAllocation, AllocationSize, bCollectCallstack);
		}
		else
		{
			bool bDefragFree = InAllocationType == EAllocationType::DefragFree;
			Adapter->ReleaseTrackedAllocationData(&InAllocation, bDefragFree);
		}
	}
#endif // TRACK_RESOURCE_ALLOCATIONS

#if ENABLE_LOW_LEVEL_MEM_TRACKER
	{
		int64 AllocSize = int64(InAllocation.GetPoolAllocatorPrivateData().PoolData.GetSize());
		
		if (InAllocationType == EAllocationType::Allocate)
		{
			LLM_SCOPED_PAUSE_TRACKING_WITH_ENUM_AND_AMOUNT_BYTAG(D3D12AllocatorUnused, 0 - AllocSize, ELLMTracker::Platform, ELLMAllocType::System);
		}
		else
		{
			LLM_SCOPED_PAUSE_TRACKING_WITH_ENUM_AND_AMOUNT_BYTAG(D3D12AllocatorUnused, AllocSize, ELLMTracker::Platform, ELLMAllocType::System);
		}
	}
#endif
}


void FD3D12PoolAllocator::FlushPendingCopyOps(FD3D12CommandContext& InCommandContext)
{
	FScopeLock Lock(&CS);

	// TODO: sort the copy ops to reduce amount of transitions!!

	FD3D12Adapter* Adapter = GetParentDevice()->GetParentAdapter();
	FD3D12ManualFence& FrameFence = Adapter->GetFrameFence();

	for (FD3D12VRAMCopyOperation& CopyOperation : PendingCopyOps)
	{
		// Cleared copy op?
		if (CopyOperation.SourceResource == nullptr || CopyOperation.DestResource == nullptr)
		{
			continue;
		}

#if D3D12_RHI_RAYTRACING
		if (!CopyOperation.SourceResource->RequiresResourceStateTracking() && CopyOperation.SourceResource->GetDefaultResourceState() == D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE)
		{
			// can't make state changes to RT resources
			check(CopyOperation.DestResource->GetDefaultResourceState() == D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

			D3D12_GPU_VIRTUAL_ADDRESS SrcAddress = CopyOperation.SourceResource->GetResource()->GetGPUVirtualAddress() + CopyOperation.SourceOffset;
			D3D12_GPU_VIRTUAL_ADDRESS DestAddress = CopyOperation.DestResource->GetResource()->GetGPUVirtualAddress() + CopyOperation.DestOffset;
			InCommandContext.RayTracingCommandList()->CopyRaytracingAccelerationStructure(DestAddress, SrcAddress, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_CLONE);
		}
		else
#endif // D3D12_RHI_RAYTRACING
		{
			FScopedResourceBarrier ScopedResourceBarrierSrc(InCommandContext, CopyOperation.SourceResource, nullptr, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
			FScopedResourceBarrier ScopedResourceBarrierDst(InCommandContext, CopyOperation.DestResource  , nullptr, D3D12_RESOURCE_STATE_COPY_DEST  , D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

			InCommandContext.FlushResourceBarriers();

			switch (CopyOperation.CopyType)
			{
			case FD3D12VRAMCopyOperation::BufferRegion:
			{
				InCommandContext.GraphicsCommandList()->CopyBufferRegion(
					CopyOperation.DestResource->GetResource(),
					CopyOperation.DestOffset,
					CopyOperation.SourceResource->GetResource(),
					CopyOperation.SourceOffset,
					CopyOperation.Size
				);
				break;
			}
			case FD3D12VRAMCopyOperation::Resource:
			{
				InCommandContext.GraphicsCommandList()->CopyResource(
					CopyOperation.DestResource->GetResource(),
					CopyOperation.SourceResource->GetResource()
				);
				break;
			}
			}
		}

		InCommandContext.UpdateResidency(CopyOperation.SourceResource);
		InCommandContext.UpdateResidency(CopyOperation.DestResource);
	}

	InCommandContext.ConditionalSplitCommandList();

	PendingCopyOps.Empty(PendingCopyOps.Num());
}

