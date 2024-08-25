// Copyright Epic Games, Inc. All Rights Reserved.

#include "D3D12RHIPrivate.h"
#include "D3D12BindlessDescriptors.h"
#include "D3D12Descriptors.h"

#if PLATFORM_SUPPORTS_BINDLESS_RENDERING

int32 GBindlessResourceDescriptorHeapSize = 1000 * 1000;
static FAutoConsoleVariableRef CVarBindlessResourceDescriptorHeapSize(
	TEXT("D3D12.Bindless.ResourceDescriptorHeapSize"),
	GBindlessResourceDescriptorHeapSize,
	TEXT("Bindless resource descriptor heap size"),
	ECVF_ReadOnly
);

int32 GBindlessSamplerDescriptorHeapSize = 2048;
static FAutoConsoleVariableRef CVarBindlessSamplerDescriptorHeapSize(
	TEXT("D3D12.Bindless.SamplerDescriptorHeapSize"),
	GBindlessSamplerDescriptorHeapSize,
	TEXT("Bindless sampler descriptor heap size"),
	ECVF_ReadOnly
);

FD3D12DescriptorHeap* UE::D3D12BindlessDescriptors::CreateCpuHeap(FD3D12Device* InDevice, ERHIDescriptorHeapType InType, uint32 InNewNumDescriptorsPerHeap)
{
	const TCHAR* const HeapName = (InType == ERHIDescriptorHeapType::Standard) ? TEXT("BindlessResourcesCPU") : TEXT("BindlessSamplersCPU");

	return InDevice->GetDescriptorHeapManager().AllocateIndependentHeap(
		HeapName,
		InType,
		InNewNumDescriptorsPerHeap,
		ED3D12DescriptorHeapFlags::None
	);
}

FD3D12DescriptorHeap* UE::D3D12BindlessDescriptors::CreateGpuHeap(FD3D12Device* InDevice, ERHIDescriptorHeapType InType, uint32 InNewNumDescriptorsPerHeap)
{
	SCOPED_NAMED_EVENT_F(TEXT("CreateNewBindlessHeap (%d)"), FColor::Turquoise, InNewNumDescriptorsPerHeap);

	const TCHAR* const HeapName = (InType == ERHIDescriptorHeapType::Standard) ? TEXT("BindlessResources") : TEXT("BindlessSamplers");

	return InDevice->GetDescriptorHeapManager().AllocateIndependentHeap(
		HeapName,
		InType,
		InNewNumDescriptorsPerHeap,
		ED3D12DescriptorHeapFlags::GpuVisible | ED3D12DescriptorHeapFlags::Poolable
	);
}

void UE::D3D12BindlessDescriptors::DeferredFreeHeap(FD3D12Device* InDevice, FD3D12DescriptorHeap* InHeap)
{
	InDevice->GetDescriptorHeapManager().DeferredFreeHeap(InHeap);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FD3D12BindlessSamplerManager

FD3D12BindlessSamplerManager::FD3D12BindlessSamplerManager(FD3D12Device* InDevice, uint32 InNumDescriptors, TConstArrayView<TStatId> InStats)
	: FD3D12DeviceChild(InDevice)
	, Allocator(ERHIDescriptorHeapType::Sampler, InNumDescriptors, InStats)
{
	GpuHeap = UE::D3D12BindlessDescriptors::CreateGpuHeap(InDevice, ERHIDescriptorHeapType::Sampler, InNumDescriptors);
}

void FD3D12BindlessSamplerManager::CleanupResources()
{
	GpuHeap = nullptr;
}

FRHIDescriptorHandle FD3D12BindlessSamplerManager::AllocateAndInitialize(FD3D12SamplerState* SamplerState)
{
	FRHIDescriptorHandle Result = Allocator.Allocate();
	if (ensure(Result.IsValid()))
	{
		UE::D3D12Descriptors::CopyDescriptor(GetParentDevice(), GpuHeap, Result, SamplerState->OfflineDescriptor);
	}
	check(Result.IsValid());
	return Result;
}

void FD3D12BindlessSamplerManager::Free(FRHIDescriptorHandle InHandle)
{
	if (InHandle.IsValid())
	{
		Allocator.Free(InHandle);
	}
}

void FD3D12BindlessSamplerManager::OpenCommandList(FD3D12CommandContext& Context)
{
	Context.StateCache.GetDescriptorCache()->SetBindlessSamplersHeapDirectly(GetHeap());
}

void FD3D12BindlessSamplerManager::CloseCommandList(FD3D12CommandContext& Context)
{
	Context.StateCache.GetDescriptorCache()->SetBindlessSamplersHeapDirectly(nullptr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FD3D12BindlessResourceManager

#if !D3D12RHI_CUSTOM_BINDLESS_RESOURCE_MANAGER

FD3D12BindlessResourceManager::FD3D12BindlessResourceManager(FD3D12Device* InDevice, uint32 InNumDescriptors, TConstArrayView<TStatId> InStats)
	: FD3D12DeviceChild(InDevice)
	, CpuHeap(UE::D3D12BindlessDescriptors::CreateCpuHeap(InDevice, ERHIDescriptorHeapType::Standard, InNumDescriptors))
	, Allocator(ERHIDescriptorHeapType::Standard, InNumDescriptors, InStats)
{
}

void FD3D12BindlessResourceManager::CleanupResources()
{
	CpuHeap.SafeRelease();
}

FRHIDescriptorHandle FD3D12BindlessResourceManager::Allocate()
{
	FRHIDescriptorHandle Result = Allocator.Allocate();
	check(Result.IsValid());
	return Result;
}

void FD3D12BindlessResourceManager::Free(FRHIDescriptorHandle InHandle)
{
	if (InHandle.IsValid())
	{
		Allocator.Free(InHandle);
	}
}

void FD3D12BindlessResourceManager::UpdateDescriptorImmediately(FRHIDescriptorHandle DstHandle, FD3D12View* View)
{
	if (DstHandle.IsValid())
	{
		UE::D3D12Descriptors::CopyDescriptor(GetParentDevice(), CpuHeap, DstHandle, View->GetOfflineCpuHandle());
	}
}

void FD3D12BindlessResourceManager::UpdateDescriptor(FRHICommandListBase& RHICmdList, FRHIDescriptorHandle DstHandle, FD3D12View* View)
{
	if (DstHandle.IsValid())
	{
		for (ERHIPipeline PipelineIndex : GetRHIPipelines())
		{
			FRHICommandListScopedPipeline Scope(RHICmdList, PipelineIndex);
			RHICmdList.EnqueueLambda([this, View, DstHandle](FRHICommandListBase& ExecutingCmdList)
			{
				FD3D12CommandContext& Context =
					ExecutingCmdList.IsGraphics()
					? static_cast<FD3D12CommandContext&>(ExecutingCmdList.GetContext())
					: static_cast<FD3D12CommandContext&>(ExecutingCmdList.GetComputeContext());

				if (Context.IsOpen())
				{
					FD3D12OfflineDescriptor CopyOfPreviousDescriptorValue = UE::D3D12Descriptors::CreateOfflineCopy(GetParentDevice(), CpuHeap, DstHandle);
					Context.GetBindlessState().PendingDescriptorRollbacks.Add(GetParentDevice(), DstHandle, CopyOfPreviousDescriptorValue);
				}
			});
		}

		RHICmdList.EnqueueLambda([this, DstHandle, View](FRHICommandListBase& ExecutingCmdList)
		{
			UpdateDescriptorImmediately(DstHandle, View);
		});
	}
}

void FD3D12BindlessResourceManager::FlushPendingDescriptorUpdates(FD3D12CommandContext& Context)
{
	FD3D12ContextBindlessState& State = Context.GetBindlessState();

	if (State.PendingDescriptorRollbacks.Num() > 0)
	{
		// If we have rollbacks to apply, we have to move to a new heap.

		// First finalize the previous heap if it was set.
		FinalizeHeapOnState(State);

		// Then create a new heap to use
		CreateHeapOnState(State);

		if (ensure(Context.IsOpen()))
		{
			// Finally tell the Context that we're using this heap,
			// this call also makes sure the heap is set on the d3d command list.
			Context.StateCache.GetDescriptorCache()->SwitchToNewBindlessResourceHeap(State.CurrentGpuHeap);
		}
	}
}

void FD3D12BindlessResourceManager::OpenCommandList(FD3D12CommandContext& Context)
{
	FD3D12ContextBindlessState& State = Context.GetBindlessState();

	// Always create a new descriptor heap to use.
	// TODO: defer this to the first FlushPendingDescriptorUpdates
	CreateHeapOnState(State);

	// Assign the heap to the descriptor cache
	Context.StateCache.GetDescriptorCache()->SetBindlessResourcesHeapDirectly(State.CurrentGpuHeap);
}

void FD3D12BindlessResourceManager::CloseCommandList(FD3D12CommandContext& Context)
{
	FD3D12ContextBindlessState& State = Context.GetBindlessState();

	// First finalize the current heap if any was set
	FinalizeHeapOnState(State);

	// Then clear the reference from the state cache
	Context.StateCache.GetDescriptorCache()->SetBindlessResourcesHeapDirectly(nullptr);
}

void FD3D12BindlessResourceManager::FinalizeContext(FD3D12CommandContext& Context)
{
	if (Context.IsOpen())
	{
		Context.CloseCommandList();
	}

	FD3D12ContextBindlessState& State = Context.GetBindlessState();

	if (State.UsedHeaps.Num() > 0)
	{
		for (const FD3D12DescriptorHeapPtr& UsedHeap : State.UsedHeaps)
		{
			checkSlow(UsedHeap);

			// Now queue it up for deletion
			UE::D3D12BindlessDescriptors::DeferredFreeHeap(GetParentDevice(), UsedHeap);
		}

		State.UsedHeaps.Empty();
	}

	check(!Context.GetBindlessState().HasAnyPending());
}

void FD3D12BindlessResourceManager::CopyCpuHeap(FD3D12DescriptorHeap* DestinationHeap)
{
	// Copy the smallest possible set of descriptors from the CPU heap to the new GPU heap.
	FRHIDescriptorAllocatorRange AllocatedRange(0, 0);
	if (Allocator.GetAllocatedRange(AllocatedRange))
	{
		const uint32 NumDescriptorsToCopy = AllocatedRange.Last - AllocatedRange.First + 1;
		UE::D3D12Descriptors::CopyDescriptors(GetParentDevice(), DestinationHeap, CpuHeap, AllocatedRange.First, NumDescriptorsToCopy);
	}
}

void FD3D12BindlessResourceManager::CreateHeapOnState(FD3D12ContextBindlessState& State)
{
	checkf(State.CurrentGpuHeap == nullptr, TEXT("FinalizeHeapOnState was not called before CreateHeapOnState"));

	State.CurrentGpuHeap = UE::D3D12BindlessDescriptors::CreateGpuHeap(GetParentDevice(), Allocator.GetType(), Allocator.GetCapacity());
}

void FD3D12BindlessResourceManager::FinalizeHeapOnState(FD3D12ContextBindlessState& State)
{
	if (State.CurrentGpuHeap)
	{
		// Since we're about to stop using this heap, make sure it's in the correct state for the GPU:
		//  1. Update wholesale from the CPU copy
		//  2. Take the set of previous descriptor values and apply them to this heap since they were updated in the CPU copy
		// This effectively "rolls back" the dynamic updates to their correct state while keeping all other immediate updates

		CopyCpuHeap(State.CurrentGpuHeap);

		FD3D12PendingDescriptorUpdates& Updates = State.PendingDescriptorRollbacks;

		if (!Updates.IsEmpty())
		{
			UE::D3D12Descriptors::CopyDescriptors(GetParentDevice(), State.CurrentGpuHeap, Updates.Handles, Updates.OfflineDescriptors);

			Updates.Empty(GetParentDevice());
		}

		// Move this heap to the used list for cleanup at context finalize.
		State.UsedHeaps.Emplace(MoveTemp(State.CurrentGpuHeap));
	}

	// Always clear the pending rollbacks. If we didn't previously bind a descriptor heap, we don't need to roll any descriptors back
	State.PendingDescriptorRollbacks.Empty(GetParentDevice());
}

#endif // D3D12RHI_CUSTOM_BINDLESS_RESOURCE_MANAGER

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FD3D12BindlessDescriptorManager

FD3D12BindlessDescriptorManager::FD3D12BindlessDescriptorManager(FD3D12Device* InDevice)
	: FD3D12DeviceChild(InDevice)
{
}

FD3D12BindlessDescriptorManager::~FD3D12BindlessDescriptorManager() = default;

void FD3D12BindlessDescriptorManager::Init()
{
	ResourcesConfiguration = RHIGetRuntimeBindlessResourcesConfiguration(GMaxRHIShaderPlatform);
	SamplersConfiguration  = RHIGetRuntimeBindlessSamplersConfiguration(GMaxRHIShaderPlatform);

	if (ResourcesConfiguration != ERHIBindlessConfiguration::Disabled)
	{
		const TStatId Stats[] =
		{
			GET_STATID(STAT_ResourceDescriptorsAllocated),
			GET_STATID(STAT_BindlessResourceDescriptorsAllocated),
		};

		uint32 NumResourceDescriptors = GBindlessResourceDescriptorHeapSize;
#if D3D12RHI_USE_CONSTANT_BUFFER_VIEWS
		NumResourceDescriptors += GBindlessOnlineDescriptorHeapBlockSize;
#endif

		ResourceManager = MakeUnique<FD3D12BindlessResourceManager>(GetParentDevice(), NumResourceDescriptors, Stats);
	}

	if (SamplersConfiguration != ERHIBindlessConfiguration::Disabled)
	{
		const TStatId Stats[] =
		{
			GET_STATID(STAT_SamplerDescriptorsAllocated),
			GET_STATID(STAT_BindlessSamplerDescriptorsAllocated),
		};

		uint32 NumSamplerDescriptors = GBindlessSamplerDescriptorHeapSize;
		if (NumSamplerDescriptors > D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE)
		{
			UE_LOG(LogD3D12RHI, Error, TEXT("D3D12.Bindless.SamplerDescriptorHeapSize was set to %d, which is higher than the D3D12 maximum of %d. Adjusting the value to prevent a crash."),
				NumSamplerDescriptors,
				D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE
			);
			NumSamplerDescriptors = D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE;
		}

		SamplerManager = MakeUnique<FD3D12BindlessSamplerManager>(GetParentDevice(), NumSamplerDescriptors, Stats);
	}
}

void FD3D12BindlessDescriptorManager::CleanupResources()
{
	if (ResourceManager)
	{
		ResourceManager->CleanupResources();
	}

	if (SamplerManager)
	{
		SamplerManager->CleanupResources();
	}
}

FRHIDescriptorHandle FD3D12BindlessDescriptorManager::AllocateResourceHandle()
{
	if (ResourceManager)
	{
		return ResourceManager->Allocate();
	}

	return FRHIDescriptorHandle();
}

FRHIDescriptorHandle FD3D12BindlessDescriptorManager::AllocateAndInitialize(FD3D12SamplerState* SamplerState)
{
	if (SamplerManager)
	{
		return SamplerManager->AllocateAndInitialize(SamplerState);
	}

	return FRHIDescriptorHandle();
}

void FD3D12BindlessDescriptorManager::ImmediateFree(FRHIDescriptorHandle InHandle)
{
	if (InHandle.GetType() == ERHIDescriptorHeapType::Standard && ResourceManager)
	{
		ResourceManager->Free(InHandle);
		return;
	}

	if (InHandle.GetType() == ERHIDescriptorHeapType::Sampler && SamplerManager)
	{
		SamplerManager->Free(InHandle);
		return;
	}

	// Bad configuration?
	checkNoEntry();
}

void FD3D12BindlessDescriptorManager::DeferredFreeFromDestructor(FRHIDescriptorHandle InHandle)
{
	if (InHandle.IsValid())
	{
		FD3D12DynamicRHI::GetD3DRHI()->DeferredDelete(InHandle, GetParentDevice());
	}
}

void FD3D12BindlessDescriptorManager::UpdateDescriptorImmediately(FRHIDescriptorHandle DstHandle, FD3D12View* View)
{
	if (DstHandle.GetType() == ERHIDescriptorHeapType::Standard && ResourceManager)
	{
		ResourceManager->UpdateDescriptorImmediately(DstHandle, View);
		return;
	}

	// Bad configuration?
	checkNoEntry();
}

void FD3D12BindlessDescriptorManager::UpdateDescriptor(FRHICommandListBase& RHICmdList, FRHIDescriptorHandle DstHandle, FD3D12View* View)
{
	if (ResourceManager)
	{
		ResourceManager->UpdateDescriptor(RHICmdList, DstHandle, View);
		return;
	}

	// Bad configuration?
	checkNoEntry();
}

void FD3D12BindlessDescriptorManager::FinalizeContext(FD3D12CommandContext& Context)
{
	if (ResourceManager)
	{
		ResourceManager->FinalizeContext(Context);
	}
}

void FD3D12BindlessDescriptorManager::OpenCommandList(FD3D12CommandContext& Context)
{
	if (ResourceManager)
	{
		ResourceManager->OpenCommandList(Context);
	}

	if (SamplerManager)
	{
		SamplerManager->OpenCommandList(Context);
	}
}

void FD3D12BindlessDescriptorManager::CloseCommandList(FD3D12CommandContext& Context)
{
	if (ResourceManager)
	{
		ResourceManager->CloseCommandList(Context);
	}

	if (SamplerManager)
	{
		SamplerManager->CloseCommandList(Context);
	}
}

void FD3D12BindlessDescriptorManager::FlushPendingDescriptorUpdates(FD3D12CommandContext& Context)
{
	if (ResourceManager)
	{
		ResourceManager->FlushPendingDescriptorUpdates(Context);
	}
}

#if D3D12RHI_USE_CONSTANT_BUFFER_VIEWS
TRHIPipelineArray<FD3D12DescriptorHeapPtr> FD3D12BindlessDescriptorManager::AllocateResourceHeapsForAllPipelines(int32 InSize)
{
	if (ResourceManager)
	{
		return ResourceManager->AllocateResourceHeapsForAllPipelines(InSize);
	}

	// Bad configuration?
	checkNoEntry();
	return TRHIPipelineArray<FD3D12DescriptorHeapPtr>();
}
#endif // D3D12RHI_USE_CONSTANT_BUFFER_VIEWS

FD3D12DescriptorHeap* FD3D12BindlessDescriptorManager::GetResourceHeap(ERHIPipeline Pipeline)
{
	return ResourceManager->GetHeap(Pipeline);
}

FD3D12DescriptorHeap* FD3D12BindlessDescriptorManager::GetSamplerHeap()
{
	return SamplerManager->GetHeap();
}

FD3D12DescriptorHeap* FD3D12BindlessDescriptorManager::GetResourceHeap(ERHIPipeline Pipeline, ERHIBindlessConfiguration InConfiguration)
{
	if (AreResourcesBindless(InConfiguration))
	{
		return GetResourceHeap(Pipeline);
	}

	return nullptr;
}

FD3D12DescriptorHeap* FD3D12BindlessDescriptorManager::GetSamplerHeap(ERHIBindlessConfiguration InConfiguration)
{
	if (AreSamplersBindless(InConfiguration))
	{
		return GetSamplerHeap();
	}

	return nullptr;
}

#endif // PLATFORM_SUPPORTS_BINDLESS_RENDERING
