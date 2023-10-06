// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "D3D12RHICommon.h"
#include "RHIDefinitions.h"
#include "Templates/RefCounting.h"

#if PLATFORM_SUPPORTS_BINDLESS_RENDERING

struct FD3D12DescriptorHeap;
using FD3D12DescriptorHeapPtr = TRefCountPtr<FD3D12DescriptorHeap>;

class FD3D12BindlessDescriptorHeapManager : public FD3D12DeviceChild
{
public:
	FD3D12BindlessDescriptorHeapManager() = delete;
	FD3D12BindlessDescriptorHeapManager(FD3D12Device* InDevice, ERHIDescriptorHeapType InType, ERHIBindlessConfiguration InConfiguration, uint32 InNumDescriptorsPerHeap, TConstArrayView<TStatId> InStats);

	FRHIDescriptorHandle Allocate();
	void                 Free(FRHIDescriptorHandle InHandle);

	void UpdateImmediately(FRHIDescriptorHandle InHandle, D3D12_CPU_DESCRIPTOR_HANDLE InSourceCpuHandle);
	void UpdateDeferred   (FRHIDescriptorHandle InHandle, D3D12_CPU_DESCRIPTOR_HANDLE InSourceCpuHandle);

	      FD3D12DescriptorHeap* GetHeap()       { return GpuHeap.GetReference(); }
	const FD3D12DescriptorHeap* GetHeap() const { return GpuHeap.GetReference(); }
	ERHIDescriptorHeapType      GetType() const { return Type; }
	
	bool HandlesAllocation(ERHIDescriptorHeapType InType) const { return GetType() == InType; }
	bool HandlesConfiguration(ERHIBindlessConfiguration InConfiguration) const { return Configuration != ERHIBindlessConfiguration::Disabled && Configuration <= InConfiguration; }

	bool HandlesAllocation(ERHIDescriptorHeapType InType, ERHIBindlessConfiguration InConfiguration) const
	{
		return HandlesAllocation(InType) && HandlesConfiguration(InConfiguration);
	}

private:
	void SetupInitialState(uint32 InNumDescriptorsPerHeap);
	void ResizeHeaps(uint32 InNewNumDescriptorsPerHeap);

	FD3D12DescriptorHeap* CreateCpuHeapInternal(uint32 InNumDescriptorsPerHeap);
	FD3D12DescriptorHeap* CreateGpuHeapInternal(uint32 InNumDescriptorsPerHeap);

	void UpdateGpuHeap(FD3D12DescriptorHeap* GpuHeap);

	void RecordAlloc()
	{
#if STATS
		for (TStatId Stat : Stats)
		{
			INC_DWORD_STAT_FName(Stat.GetName());
		}
#endif
	}

	void RecordFree()
	{
#if STATS
		for (TStatId Stat : Stats)
		{
			DEC_DWORD_STAT_FName(Stat.GetName());
		}
#endif
	}

private:
	using AllocationListType = TBitArray<>;

	FCriticalSection        CriticalSection;
	
	FD3D12DescriptorHeapPtr CpuHeap;
	FD3D12DescriptorHeapPtr GpuHeap;

	AllocationListType      Allocations;
	int32                   NumAllocations = 0;

	const ERHIDescriptorHeapType    Type;
	const ERHIBindlessConfiguration Configuration;
	uint32                          NumDescriptorsPerHeap;

#if STATS
	TArray<TStatId> Stats;
#endif
};

/** Manager for resource descriptors used in bindless rendering. */
class FD3D12BindlessDescriptorManager : public FD3D12DeviceChild
{
public:
	FD3D12BindlessDescriptorManager(FD3D12Device* InDevice);
	~FD3D12BindlessDescriptorManager();

	void Init();

	ERHIBindlessConfiguration GetResourcesConfiguration() const { return ResourcesConfiguration; }
	ERHIBindlessConfiguration GetSamplersConfiguration()  const { return SamplersConfiguration; }

	bool AreResourcesFullyBindless() const { return GetResourcesConfiguration() == ERHIBindlessConfiguration::AllShaders; }
	bool AreSamplersFullyBindless () const { return GetSamplersConfiguration()  == ERHIBindlessConfiguration::AllShaders; }

	FRHIDescriptorHandle Allocate(ERHIDescriptorHeapType InType);
	void                 ImmediateFree(FRHIDescriptorHandle InHandle);
	void                 DeferredFreeFromDestructor(FRHIDescriptorHandle InHandle);

	void UpdateImmediately(FRHIDescriptorHandle InHandle, D3D12_CPU_DESCRIPTOR_HANDLE InSourceCpuHandle);
	void UpdateDeferred(FRHIDescriptorHandle InHandle, D3D12_CPU_DESCRIPTOR_HANDLE InSourceCpuHandle);

	FD3D12DescriptorHeap* GetHeap(ERHIDescriptorHeapType InType);
	FD3D12DescriptorHeap* GetHeap(ERHIDescriptorHeapType InType, ERHIBindlessConfiguration InConfiguration);
	bool                  HasHeap(ERHIDescriptorHeapType InType, ERHIBindlessConfiguration InConfiguration) const;

	D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(FRHIDescriptorHandle InHandle) const;

private:
	TArray<FD3D12BindlessDescriptorHeapManager> Managers;

	ERHIBindlessConfiguration ResourcesConfiguration{};
	ERHIBindlessConfiguration SamplersConfiguration{};
};

#endif // PLATFORM_SUPPORTS_BINDLESS_RENDERING
