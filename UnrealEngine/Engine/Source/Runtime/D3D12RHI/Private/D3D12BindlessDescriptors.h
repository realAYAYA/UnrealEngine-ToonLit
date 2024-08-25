// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "D3D12RHICommon.h"
#include "D3D12Descriptors.h"
#include "RHIDefinitions.h"
#include "RHIDescriptorAllocator.h"
#include "RHIPipeline.h"
#include "Templates/RefCounting.h"

class FD3D12CommandContext;
class FRHICommandListBase;
class FD3D12SamplerState;
class FD3D12ShaderResourceView;
class FD3D12UnorderedAccessView;

struct FD3D12Payload;

#if PLATFORM_SUPPORTS_BINDLESS_RENDERING

#include COMPILED_PLATFORM_HEADER(D3D12BindlessDescriptors.h)

namespace UE::D3D12BindlessDescriptors
{
	FD3D12DescriptorHeap* CreateCpuHeap(FD3D12Device* InDevice, ERHIDescriptorHeapType InType, uint32 InNewNumDescriptorsPerHeap);
	FD3D12DescriptorHeap* CreateGpuHeap(FD3D12Device* InDevice, ERHIDescriptorHeapType InType, uint32 InNewNumDescriptorsPerHeap);
	void DeferredFreeHeap(FD3D12Device* InDevice, FD3D12DescriptorHeap* InHeap);
}

/** Manager specifically for bindless sampler descriptors. */
class FD3D12BindlessSamplerManager : public FD3D12DeviceChild
{
public:
	FD3D12BindlessSamplerManager() = delete;
	FD3D12BindlessSamplerManager(FD3D12Device* InDevice, uint32 InNumDescriptors, TConstArrayView<TStatId> InStats);

	void CleanupResources();

	FRHIDescriptorHandle AllocateAndInitialize(FD3D12SamplerState* SamplerState);
	void                 Free(FRHIDescriptorHandle InHandle);

	void OpenCommandList(FD3D12CommandContext& Context);
	void CloseCommandList(FD3D12CommandContext& Context);

	FD3D12DescriptorHeap* GetHeap() { return GpuHeap.GetReference(); }

private:
	FD3D12DescriptorHeapPtr      GpuHeap;
	FRHIHeapDescriptorAllocator  Allocator;
};

#if !D3D12RHI_CUSTOM_BINDLESS_RESOURCE_MANAGER

struct FD3D12PendingDescriptorUpdates
{
	// List of handles that need updating
	TArray<FRHIDescriptorHandle>    Handles;
	// Copies of descriptors to update each handle with. These need to be freed in a specific way.
	TArray<FD3D12OfflineDescriptor> OfflineDescriptors;

	~FD3D12PendingDescriptorUpdates()
	{
		checkSlow(Handles.Num() == 0);
	}

	void Add(FD3D12Device* Device, FRHIDescriptorHandle DestinationHandle, const FD3D12OfflineDescriptor& OfflineDescriptor)
	{
		if (ensure(DestinationHandle.IsValid()))
		{
			Handles.Emplace(DestinationHandle);
			OfflineDescriptors.Emplace(OfflineDescriptor);
		}
	}

	void Empty(FD3D12Device* Device)
	{
		for (FD3D12OfflineDescriptor& OfflineDescriptor : OfflineDescriptors)
		{
			UE::D3D12Descriptors::FreeOfflineCopy(Device, OfflineDescriptor, ERHIDescriptorHeapType::Standard);
		}

		Handles.Empty();
		OfflineDescriptors.Empty();
	}

	int32 Num()     const { return Handles.Num(); }
	bool  IsEmpty() const { return Num() == 0; }
};

// Helper container for all context related bindless state.
struct FD3D12ContextBindlessState
{
	FD3D12PendingDescriptorUpdates   PendingDescriptorRollbacks;
	FD3D12DescriptorHeapPtr          CurrentGpuHeap;

	// All heaps used on the context. Used for lifetime management.
	TArray<FD3D12DescriptorHeapPtr> UsedHeaps;

	FD3D12ContextBindlessState() = default;
	~FD3D12ContextBindlessState()
	{
		check(PendingDescriptorRollbacks.IsEmpty());
	}

	bool HasAnyPending() const
	{
		return UsedHeaps.Num() > 0 || PendingDescriptorRollbacks.Num() > 0;
	}
};

/** Manager specifically for bindless resource descriptors. Has to handle renames on command lists. */
class FD3D12BindlessResourceManager : public FD3D12DeviceChild
{
public:
	FD3D12BindlessResourceManager() = delete;
	FD3D12BindlessResourceManager(FD3D12Device* InDevice, uint32 InNumDescriptors, TConstArrayView<TStatId> InStats);

	void CleanupResources();

	FRHIDescriptorHandle Allocate();
	void                 Free(FRHIDescriptorHandle InHandle);

	void UpdateDescriptorImmediately(FRHIDescriptorHandle DstHandle, FD3D12View* View);
	void UpdateDescriptor(FRHICommandListBase& RHICmdList, FRHIDescriptorHandle DstHandle, FD3D12View* View);

	void FlushPendingDescriptorUpdates(FD3D12CommandContext& Context);

	void OpenCommandList(FD3D12CommandContext& Context);
	void CloseCommandList(FD3D12CommandContext& Context);
	void FinalizeContext(FD3D12CommandContext& Context);

	FD3D12DescriptorHeap* GetHeap(ERHIPipeline Pipeline)
	{
		checkNoEntry();
		return nullptr;
	}

private:
	void CopyCpuHeap(FD3D12DescriptorHeap* DestinationHeap);
	void CreateHeapOnState(FD3D12ContextBindlessState& State);
	void FinalizeHeapOnState(FD3D12ContextBindlessState& State);

	FD3D12DescriptorHeapPtr      CpuHeap;
	FRHIHeapDescriptorAllocator  Allocator;
};

#endif

/** Manager for descriptors used in bindless rendering. */
class FD3D12BindlessDescriptorManager : public FD3D12DeviceChild
{
public:
	FD3D12BindlessDescriptorManager(FD3D12Device* InDevice);
	~FD3D12BindlessDescriptorManager();

	void Init();
	void CleanupResources();

	ERHIBindlessConfiguration GetResourcesConfiguration() const { return ResourcesConfiguration; }
	ERHIBindlessConfiguration GetSamplersConfiguration()  const { return SamplersConfiguration; }

	bool AreResourcesBindless() const { return GetResourcesConfiguration() != ERHIBindlessConfiguration::Disabled; }
	bool AreSamplersBindless()  const { return GetSamplersConfiguration()  != ERHIBindlessConfiguration::Disabled; }

	bool AreResourcesBindless(ERHIBindlessConfiguration InConfiguration) const { return GetResourcesConfiguration() != ERHIBindlessConfiguration::Disabled && GetResourcesConfiguration() <= InConfiguration; }
	bool AreSamplersBindless(ERHIBindlessConfiguration InConfiguration)  const { return GetSamplersConfiguration()  != ERHIBindlessConfiguration::Disabled && GetSamplersConfiguration() <= InConfiguration; }

	bool AreResourcesFullyBindless() const { return GetResourcesConfiguration() == ERHIBindlessConfiguration::AllShaders; }
	bool AreSamplersFullyBindless () const { return GetSamplersConfiguration()  == ERHIBindlessConfiguration::AllShaders; }

	FRHIDescriptorHandle AllocateResourceHandle();
	FRHIDescriptorHandle AllocateAndInitialize(FD3D12SamplerState* SamplerState);
	void                 ImmediateFree(FRHIDescriptorHandle InHandle);
	void                 DeferredFreeFromDestructor(FRHIDescriptorHandle InHandle);

	void UpdateDescriptorImmediately(FRHIDescriptorHandle DstHandle, FD3D12View* View);
	void UpdateDescriptor(FRHICommandListBase& RHICmdList, FRHIDescriptorHandle DstHandle, FD3D12View* SourceView);

	void FinalizeContext(FD3D12CommandContext& Context);

	void OpenCommandList(FD3D12CommandContext& Context);
	void CloseCommandList(FD3D12CommandContext& Context);

	void FlushPendingDescriptorUpdates(FD3D12CommandContext& Context);

#if D3D12RHI_USE_CONSTANT_BUFFER_VIEWS
	TRHIPipelineArray<FD3D12DescriptorHeapPtr> AllocateResourceHeapsForAllPipelines(int32 InSize);
#endif

	FD3D12DescriptorHeap* GetResourceHeap(ERHIPipeline Pipeline);
	FD3D12DescriptorHeap* GetSamplerHeap();

	FD3D12DescriptorHeap* GetResourceHeap(ERHIPipeline Pipeline, ERHIBindlessConfiguration InConfiguration);
	FD3D12DescriptorHeap* GetSamplerHeap(ERHIBindlessConfiguration InConfiguration);

private:
	TUniquePtr<FD3D12BindlessResourceManager> ResourceManager;
	TUniquePtr<FD3D12BindlessSamplerManager>  SamplerManager;

	ERHIBindlessConfiguration ResourcesConfiguration{};
	ERHIBindlessConfiguration SamplersConfiguration{};
};

#endif // PLATFORM_SUPPORTS_BINDLESS_RENDERING
