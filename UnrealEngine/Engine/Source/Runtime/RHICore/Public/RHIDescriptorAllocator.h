// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "HAL/CriticalSection.h"
#include "RHIDefinitions.h"

struct FRHIDescriptorAllocatorRange;

class RHICORE_API FRHIDescriptorAllocator
{
public:
	FRHIDescriptorAllocator();
	FRHIDescriptorAllocator(uint32 InNumDescriptors);
	~FRHIDescriptorAllocator();

	void Init(uint32 InNumDescriptors);
	void Shutdown();

	FRHIDescriptorHandle Allocate(ERHIDescriptorHeapType InType);
	void Free(FRHIDescriptorHandle InHandle);

	bool Allocate(uint32 NumDescriptors, uint32& OutSlot);
	void Free(uint32 Slot, uint32 NumDescriptors);

	inline uint32 GetCapacity() const { return Capacity; }

private:
	TArray<FRHIDescriptorAllocatorRange> Ranges;
	uint32 Capacity = 0;

	FCriticalSection CriticalSection;
};

class RHICORE_API FRHIHeapDescriptorAllocator : protected FRHIDescriptorAllocator
{
public:
	FRHIHeapDescriptorAllocator() = delete;
	FRHIHeapDescriptorAllocator(ERHIDescriptorHeapType InType, uint32 InDescriptorCount);

	FRHIDescriptorHandle Allocate();
	void Free(FRHIDescriptorHandle InHandle);

	bool Allocate(uint32 NumDescriptors, uint32& OutSlot);
	void Free(uint32 Slot, uint32 NumDescriptors);

	using FRHIDescriptorAllocator::GetCapacity;
	inline ERHIDescriptorHeapType GetType() const { return Type; }

	inline bool HandlesAllocation(ERHIDescriptorHeapType InType) const { return GetType() == InType; }

private:
	ERHIDescriptorHeapType Type;
};
