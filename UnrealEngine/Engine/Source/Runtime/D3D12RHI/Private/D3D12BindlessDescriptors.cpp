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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FD3D12BindlessDescriptorHeapManager

FD3D12BindlessDescriptorHeapManager::FD3D12BindlessDescriptorHeapManager(FD3D12Device* InDevice, ERHIDescriptorHeapType InType, ERHIBindlessConfiguration InConfiguration, uint32 InNumDescriptorsPerHeap, TConstArrayView<TStatId> InStats)
	: FD3D12DeviceChild(InDevice)
	, Type(InType)
	, Configuration(InConfiguration)
	, NumDescriptorsPerHeap(InNumDescriptorsPerHeap)
{
#if STATS
	Stats = InStats;
#endif

	SetupInitialState(InNumDescriptorsPerHeap);
}

FRHIDescriptorHandle FD3D12BindlessDescriptorHeapManager::Allocate()
{
	FScopeLock Lock(&CriticalSection);

	int32 AllocatedIndex = INDEX_NONE;

	while ((AllocatedIndex = Allocations.FindAndSetFirstZeroBit()) == INDEX_NONE)
	{
		const int32 NewNumDescriptorsPerHeap = NumDescriptorsPerHeap * 1.5;
		ResizeHeaps(NewNumDescriptorsPerHeap);
	}

	NumAllocations++;
	RecordAlloc();

	checkSlow(NumAllocations == Allocations.CountSetBits());

	return FRHIDescriptorHandle(Type, AllocatedIndex);
}

void FD3D12BindlessDescriptorHeapManager::Free(FRHIDescriptorHandle InHandle)
{
	if (InHandle.IsValid())
	{
		FScopeLock Lock(&CriticalSection);

		const int32 Index = InHandle.GetIndex();
		check(Allocations[Index] == true);
		Allocations[Index] = false;

		NumAllocations--;
		RecordFree();

		checkSlow(NumAllocations == Allocations.CountSetBits());
	}
}

void FD3D12BindlessDescriptorHeapManager::UpdateImmediately(FRHIDescriptorHandle InHandle, D3D12_CPU_DESCRIPTOR_HANDLE InSourceCpuHandle)
{
	UE::D3D12Descriptors::CopyDescriptor(GetParentDevice(), CpuHeap, InHandle, InSourceCpuHandle);
	UE::D3D12Descriptors::CopyDescriptor(GetParentDevice(), GpuHeap, InHandle, InSourceCpuHandle);
}

void FD3D12BindlessDescriptorHeapManager::UpdateDeferred(FRHIDescriptorHandle InHandle, D3D12_CPU_DESCRIPTOR_HANDLE InSourceCpuHandle)
{
	// TODO: implement deferred updates
	UpdateImmediately(InHandle, InSourceCpuHandle);
}

void FD3D12BindlessDescriptorHeapManager::SetupInitialState(uint32 InNumDescriptorsPerHeap)
{
	CpuHeap = CreateCpuHeapInternal(InNumDescriptorsPerHeap);
	GpuHeap = CreateGpuHeapInternal(InNumDescriptorsPerHeap);

	Allocations = AllocationListType(false, InNumDescriptorsPerHeap);
	NumAllocations = 0;

	NumDescriptorsPerHeap = InNumDescriptorsPerHeap;
}

void FD3D12BindlessDescriptorHeapManager::ResizeHeaps(uint32 InNewNumDescriptorsPerHeap)
{
	if (InNewNumDescriptorsPerHeap > NumDescriptorsPerHeap)
	{
		// Save off the old heaps and descriptor count
		FD3D12DescriptorHeapPtr OldCpuHeap = CpuHeap;
		FD3D12DescriptorHeapPtr OldGpuHeap = GpuHeap;
		const uint32 OldNumDescriptorsPerHeap = NumDescriptorsPerHeap;

		// Create the new heaps
		CpuHeap = CreateCpuHeapInternal(InNewNumDescriptorsPerHeap);
		GpuHeap = CreateGpuHeapInternal(InNewNumDescriptorsPerHeap);

		// Resize the allocation list
		Allocations.SetNum(InNewNumDescriptorsPerHeap, false);

		// Validate the count is still correct
		checkSlow(NumAllocations == Allocations.CountSetBits());

		// Now we can switch to the new size
		NumDescriptorsPerHeap = InNewNumDescriptorsPerHeap;

		// Copy the old descriptor range from the old heap to the new heaps
		UE::D3D12Descriptors::CopyDescriptors(GetParentDevice(), CpuHeap, OldCpuHeap, OldNumDescriptorsPerHeap);
		UE::D3D12Descriptors::CopyDescriptors(GetParentDevice(), GpuHeap, OldCpuHeap, OldNumDescriptorsPerHeap);

		// Finally, queue the old heaps for deletion
		FD3D12DescriptorHeapManager& HeapManager = GetParentDevice()->GetDescriptorHeapManager();
		HeapManager.DeferredFreeHeap(OldCpuHeap);
		HeapManager.DeferredFreeHeap(OldGpuHeap);
	}
}

FD3D12DescriptorHeap* FD3D12BindlessDescriptorHeapManager::CreateCpuHeapInternal(uint32 InNewNumDescriptorsPerHeap)
{
	const TCHAR* const HeapName = (GetType() == ERHIDescriptorHeapType::Standard) ? TEXT("BindlessResourcesCPU") : TEXT("BindlessSamplersCPU");

	return GetParentDevice()->GetDescriptorHeapManager().AllocateIndependentHeap(
		HeapName,
		GetType(),
		InNewNumDescriptorsPerHeap,
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE
	);
}

FD3D12DescriptorHeap* FD3D12BindlessDescriptorHeapManager::CreateGpuHeapInternal(uint32 InNewNumDescriptorsPerHeap)
{
	const TCHAR* const HeapName = (GetType() == ERHIDescriptorHeapType::Standard) ? TEXT("BindlessResources") : TEXT("BindlessSamplers");

	return GetParentDevice()->GetDescriptorHeapManager().AllocateIndependentHeap(
		HeapName,
		GetType(),
		InNewNumDescriptorsPerHeap,
		D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
	);
}

void FD3D12BindlessDescriptorHeapManager::UpdateGpuHeap(FD3D12DescriptorHeap* InGpuHeap)
{
	UE::D3D12Descriptors::CopyDescriptors(GetParentDevice(), InGpuHeap, CpuHeap, NumDescriptorsPerHeap);
}

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

	const uint32 NumResourceDescriptors = ResourcesConfiguration != ERHIBindlessConfiguration::Disabled ? GBindlessResourceDescriptorHeapSize : 0;
	const uint32 NumSamplerDescriptors = SamplersConfiguration != ERHIBindlessConfiguration::Disabled ? GBindlessSamplerDescriptorHeapSize  : 0;

	if (NumResourceDescriptors > 0)
	{
		const TStatId Stats[] =
		{
			GET_STATID(STAT_ResourceDescriptorsAllocated),
			GET_STATID(STAT_BindlessResourceDescriptorsAllocated),
		};

		Managers.Emplace(GetParentDevice(), ERHIDescriptorHeapType::Standard, ResourcesConfiguration, NumResourceDescriptors, Stats);
	}

	if (NumSamplerDescriptors > 0)
	{
		const TStatId Stats[] =
		{
			GET_STATID(STAT_SamplerDescriptorsAllocated),
			GET_STATID(STAT_BindlessSamplerDescriptorsAllocated),
		};

		Managers.Emplace(GetParentDevice(), ERHIDescriptorHeapType::Sampler, SamplersConfiguration, NumSamplerDescriptors, Stats);
	}
}

FRHIDescriptorHandle FD3D12BindlessDescriptorManager::Allocate(ERHIDescriptorHeapType InType)
{
	for (FD3D12BindlessDescriptorHeapManager& Manager : Managers)
	{
		if (Manager.HandlesAllocation(InType))
		{
			return Manager.Allocate();
		}
	}
	return FRHIDescriptorHandle();
}

void FD3D12BindlessDescriptorManager::ImmediateFree(FRHIDescriptorHandle InHandle)
{
	for (FD3D12BindlessDescriptorHeapManager& Manager : Managers)
	{
		if (Manager.HandlesAllocation(InHandle.GetType()))
		{
			Manager.Free(InHandle);
			return;
		}
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

void FD3D12BindlessDescriptorManager::UpdateImmediately(FRHIDescriptorHandle InHandle, D3D12_CPU_DESCRIPTOR_HANDLE InSourceCpuHandle)
{
	for (FD3D12BindlessDescriptorHeapManager& Manager : Managers)
	{
		if (Manager.HandlesAllocation(InHandle.GetType()))
		{
			Manager.UpdateImmediately(InHandle, InSourceCpuHandle);
			return;
		}
	}

	// Bad configuration?
	checkNoEntry();
}

void FD3D12BindlessDescriptorManager::UpdateDeferred(FRHIDescriptorHandle InHandle, D3D12_CPU_DESCRIPTOR_HANDLE InSourceCpuHandle)
{
	for (FD3D12BindlessDescriptorHeapManager& Manager : Managers)
	{
		if (Manager.HandlesAllocation(InHandle.GetType()))
		{
			Manager.UpdateDeferred(InHandle, InSourceCpuHandle);
			return;
		}
	}

	// Bad configuration?
	checkNoEntry();
}

FD3D12DescriptorHeap* FD3D12BindlessDescriptorManager::GetHeap(ERHIDescriptorHeapType InType)
{
	for (FD3D12BindlessDescriptorHeapManager& Manager : Managers)
	{
		if (Manager.HandlesAllocation(InType))
		{
			return Manager.GetHeap();
		}
	}

	return nullptr;
}

FD3D12DescriptorHeap* FD3D12BindlessDescriptorManager::GetHeap(ERHIDescriptorHeapType InType, ERHIBindlessConfiguration InConfiguration)
{
	for (FD3D12BindlessDescriptorHeapManager& Manager : Managers)
	{
		if (Manager.HandlesAllocation(InType, InConfiguration))
		{
			return Manager.GetHeap();
		}
	}
	return nullptr;
}


bool FD3D12BindlessDescriptorManager::HasHeap(ERHIDescriptorHeapType InType, ERHIBindlessConfiguration InConfiguration) const
{
	for (const FD3D12BindlessDescriptorHeapManager& Manager : Managers)
	{
		if (Manager.HandlesAllocation(InType, InConfiguration))
		{
			return true;
		}
	}

	return false;
}

D3D12_GPU_DESCRIPTOR_HANDLE FD3D12BindlessDescriptorManager::GetGpuHandle(FRHIDescriptorHandle InHandle) const
{
	for (const FD3D12BindlessDescriptorHeapManager& Manager : Managers)
	{
		if (Manager.HandlesAllocation(InHandle.GetType()))
		{
			return Manager.GetHeap()->GetGPUSlotHandle(InHandle.GetIndex());
		}
	}
	return D3D12_GPU_DESCRIPTOR_HANDLE{};
}

#endif // PLATFORM_SUPPORTS_BINDLESS_RENDERING
