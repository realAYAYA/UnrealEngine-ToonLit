// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "HAL/CriticalSection.h"
#include "Stats/Stats.h"

enum class ERHIDescriptorHeapType : uint8;
struct FRHIDescriptorHandle;

struct FRHIDescriptorAllocatorRange
{
	FRHIDescriptorAllocatorRange(uint32 InFirst, uint32 InLast) : First(InFirst), Last(InLast) {}
	uint32 First;
	uint32 Last;
};

class FRHIDescriptorAllocator
{
public:
	RHICORE_API FRHIDescriptorAllocator();
	RHICORE_API FRHIDescriptorAllocator(uint32 InNumDescriptors, TConstArrayView<TStatId> InStats);
	RHICORE_API ~FRHIDescriptorAllocator();

	RHICORE_API void Init(uint32 InNumDescriptors, TConstArrayView<TStatId> InStats);
	RHICORE_API void Shutdown();

	RHICORE_API FRHIDescriptorHandle Allocate(ERHIDescriptorHeapType InType);
	RHICORE_API void Free(FRHIDescriptorHandle InHandle);

	RHICORE_API bool Allocate(uint32 NumDescriptors, uint32& OutSlot);
	RHICORE_API void Free(uint32 Slot, uint32 NumDescriptors);

	// Get the range of allocated descriptors. Useful for determining the smallest range to copy between heaps.
	RHICORE_API bool GetAllocatedRange(FRHIDescriptorAllocatorRange& OutRange);

	uint32 GetCapacity() const { return Capacity; }

private:
	void RecordAlloc(uint32 Count)
	{
#if STATS
		for (TStatId Stat : Stats)
		{
			INC_DWORD_STAT_BY_FName(Stat.GetName(), Count);
		}
#endif
	}

	void RecordFree(uint32 Count)
	{
#if STATS
		for (TStatId Stat : Stats)
		{
			DEC_DWORD_STAT_BY_FName(Stat.GetName(), Count);
		}
#endif
	}

	TArray<FRHIDescriptorAllocatorRange> Ranges;
	uint32 Capacity = 0;

	FCriticalSection CriticalSection;

#if STATS
	TArray<TStatId> Stats;
#endif
};


class FRHIHeapDescriptorAllocator : protected FRHIDescriptorAllocator
{
public:
	FRHIHeapDescriptorAllocator() = delete;
	RHICORE_API FRHIHeapDescriptorAllocator(ERHIDescriptorHeapType InType, uint32 InDescriptorCount, TConstArrayView<TStatId> InStats);

	RHICORE_API FRHIDescriptorHandle Allocate();
	RHICORE_API void Free(FRHIDescriptorHandle InHandle);

	RHICORE_API bool Allocate(uint32 NumDescriptors, uint32& OutSlot);
	RHICORE_API void Free(uint32 Slot, uint32 NumDescriptors);

	using FRHIDescriptorAllocator::GetAllocatedRange;

	using FRHIDescriptorAllocator::GetCapacity;
	inline ERHIDescriptorHeapType GetType() const { return Type; }

	inline bool HandlesAllocation(ERHIDescriptorHeapType InType) const { return GetType() == InType; }

private:
	ERHIDescriptorHeapType Type;
};

class FRHIOffsetHeapDescriptorAllocator : protected FRHIHeapDescriptorAllocator
{
public:
	FRHIOffsetHeapDescriptorAllocator() = delete;
	RHICORE_API FRHIOffsetHeapDescriptorAllocator(ERHIDescriptorHeapType InType, uint32 InDescriptorCount, uint32 InHeapOffset, TConstArrayView<TStatId> InStats);

	RHICORE_API FRHIDescriptorHandle Allocate();
	RHICORE_API void Free(FRHIDescriptorHandle InHandle);

	using FRHIHeapDescriptorAllocator::GetCapacity;
	using FRHIHeapDescriptorAllocator::GetType;
	using FRHIHeapDescriptorAllocator::HandlesAllocation;

private:
	// Offset from start of heap we belong to
	uint32 HeapOffset;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "RHIDefinitions.h"
#endif
