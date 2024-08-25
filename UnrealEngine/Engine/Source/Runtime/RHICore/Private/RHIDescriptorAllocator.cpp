// Copyright Epic Games, Inc. All Rights Reserved.

#include "RHIDescriptorAllocator.h"
#include "Misc/ScopeLock.h"
#include "RHIDefinitions.h"

FRHIDescriptorAllocator::FRHIDescriptorAllocator()
{
}

FRHIDescriptorAllocator::FRHIDescriptorAllocator(uint32 InNumDescriptors, TConstArrayView<TStatId> InStats)
{
	Init(InNumDescriptors, InStats);
}

FRHIDescriptorAllocator::~FRHIDescriptorAllocator()
{
}

void FRHIDescriptorAllocator::Init(uint32 InNumDescriptors, TConstArrayView<TStatId> InStats)
{
	Capacity = InNumDescriptors;
	Ranges.Emplace(0, InNumDescriptors - 1);

#if STATS
	Stats = InStats;
#endif
}

void FRHIDescriptorAllocator::Shutdown()
{
	Ranges.Empty();
	Capacity = 0;
}

FRHIDescriptorHandle FRHIDescriptorAllocator::Allocate(ERHIDescriptorHeapType InType)
{
	uint32 Index{};
	if (Allocate(1, Index))
	{
		return FRHIDescriptorHandle(InType, Index);
	}
	return FRHIDescriptorHandle();
}

void FRHIDescriptorAllocator::Free(FRHIDescriptorHandle InHandle)
{
	if (InHandle.IsValid())
	{
		Free(InHandle.GetIndex(), 1);
	}
}

bool FRHIDescriptorAllocator::Allocate(uint32 NumDescriptors, uint32& OutSlot)
{
	FScopeLock Lock(&CriticalSection);

	if (const uint32 NumRanges = Ranges.Num(); NumRanges > 0)
	{
		uint32 Index = 0;
		do
		{
			FRHIDescriptorAllocatorRange& CurrentRange = Ranges[Index];
			const uint32 Size = 1 + CurrentRange.Last - CurrentRange.First;
			if (NumDescriptors <= Size)
			{
				uint32 First = CurrentRange.First;
				if (NumDescriptors == Size && Index + 1 < NumRanges)
				{
					// Range is full and a new range exists, so move on to that one
					Ranges.RemoveAt(Index);
				}
				else
				{
					CurrentRange.First += NumDescriptors;
				}
				OutSlot = First;

				RecordAlloc(NumDescriptors);

				return true;
			}
			++Index;
		} while (Index < NumRanges);
	}

	OutSlot = UINT_MAX;
	return false;
}

void FRHIDescriptorAllocator::Free(uint32 Offset, uint32 NumDescriptors)
{
	if (Offset == UINT_MAX || NumDescriptors == 0)
	{
		return;
	}

	FScopeLock Lock(&CriticalSection);

	const uint32 End = Offset + NumDescriptors;
	// Binary search of the range list
	uint32 Index0 = 0;
	uint32 Index1 = Ranges.Num() - 1;
	for (;;)
	{
		const uint32 Index = (Index0 + Index1) / 2;
		if (Offset < Ranges[Index].First)
		{
			// Before current range, check if neighboring
			if (End >= Ranges[Index].First)
			{
				check(End == Ranges[Index].First); // Can't overlap a range of free IDs
				// Neighbor id, check if neighboring previous range too
				if (Index > Index0 && Offset - 1 == Ranges[Index - 1].Last)
				{
					// Merge with previous range
					Ranges[Index - 1].Last = Ranges[Index].Last;
					Ranges.RemoveAt(Index);
				}
				else
				{
					// Just grow range
					Ranges[Index].First = Offset;
				}

				RecordFree(NumDescriptors);
				return;
			}
			else
			{
				// Non-neighbor id
				if (Index != Index0)
				{
					// Cull upper half of list
					Index1 = Index - 1;
				}
				else
				{
					// Found our position in the list, insert the deleted range here
					Ranges.EmplaceAt(Index, Offset, End - 1);

					RecordFree(NumDescriptors);
					return;
				}
			}
		}
		else if (Offset > Ranges[Index].Last)
		{
			// After current range, check if neighboring
			if (Offset - 1 == Ranges[Index].Last)
			{
				// Neighbor id, check if neighboring next range too
				if (Index < Index1 && End == Ranges[Index + 1].First)
				{
					// Merge with next range
					Ranges[Index].Last = Ranges[Index + 1].Last;
					Ranges.RemoveAt(Index + 1);
				}
				else
				{
					// Just grow range
					Ranges[Index].Last += NumDescriptors;
				}

				RecordFree(NumDescriptors);
				return;
			}
			else
			{
				// Non-neighbor id
				if (Index != Index1)
				{
					// Cull bottom half of list
					Index0 = Index + 1;
				}
				else
				{
					// Found our position in the list, insert the deleted range here
					Ranges.EmplaceAt(Index + 1, Offset, End - 1);

					RecordFree(NumDescriptors);
					return;
				}
			}
		}
		else
		{
			// Inside a free block, not a valid offset
			checkNoEntry();
		}
	}
}

bool FRHIDescriptorAllocator::GetAllocatedRange(FRHIDescriptorAllocatorRange& OutRange)
{
	const uint32 VeryFirstIndex = 0;
	const uint32 VeryLastIndex = GetCapacity() - 1;

	OutRange.First = VeryFirstIndex;
	OutRange.Last = VeryLastIndex;

	FScopeLock Lock(&CriticalSection);
	if (Ranges.Num() > 0)
	{
		const FRHIDescriptorAllocatorRange FirstRange = Ranges[0];

		// If the free range matches the entire usable range, that means we have zero allocations.
		if (FirstRange.First == VeryFirstIndex && FirstRange.Last == VeryLastIndex)
		{
			return false;
		}

		// If the first free range is at the start, then the first allocation is right after this range
		if (FirstRange.First == VeryFirstIndex)
		{
			OutRange.First = FMath::Min(FirstRange.Last + 1, VeryLastIndex);
		}

		const FRHIDescriptorAllocatorRange LastRange = Ranges[Ranges.Num() - 1];

		// If the last free range is at the end of the usable range, our last allocation is right before this range 
		if (LastRange.Last == VeryLastIndex)
		{
			OutRange.Last = LastRange.First > 0 ? LastRange.First - 1 : 0;
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FRHIHeapDescriptorAllocator

FRHIHeapDescriptorAllocator::FRHIHeapDescriptorAllocator(ERHIDescriptorHeapType InType, uint32 InDescriptorCount, TConstArrayView<TStatId> InStats)
	: FRHIDescriptorAllocator(InDescriptorCount, InStats)
	, Type(InType)
{
}

FRHIDescriptorHandle FRHIHeapDescriptorAllocator::Allocate()
{
	return FRHIDescriptorAllocator::Allocate(Type);
}

void FRHIHeapDescriptorAllocator::Free(FRHIDescriptorHandle InHandle)
{
	if (InHandle.IsValid())
	{
		check(InHandle.GetType() == Type);
		FRHIDescriptorAllocator::Free(InHandle);
	}
}

bool FRHIHeapDescriptorAllocator::Allocate(uint32 NumDescriptors, uint32& OutSlot)
{
	return FRHIDescriptorAllocator::Allocate(NumDescriptors, OutSlot);
}

void FRHIHeapDescriptorAllocator::Free(uint32 Slot, uint32 NumDescriptors)
{
	FRHIDescriptorAllocator::Free(Slot, NumDescriptors);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FRHIOffsetHeapDescriptorAllocator

FRHIOffsetHeapDescriptorAllocator::FRHIOffsetHeapDescriptorAllocator(ERHIDescriptorHeapType InType, uint32 InDescriptorCount, uint32 InHeapOffset, TConstArrayView<TStatId> InStats)
	: FRHIHeapDescriptorAllocator(InType, InDescriptorCount, InStats)
	, HeapOffset(InHeapOffset)
{
}

FRHIDescriptorHandle FRHIOffsetHeapDescriptorAllocator::Allocate()
{
	const FRHIDescriptorHandle AlocatorHandle = FRHIHeapDescriptorAllocator::Allocate();
	if (AlocatorHandle.IsValid())
	{
		return FRHIDescriptorHandle(AlocatorHandle.GetType(), AlocatorHandle.GetIndex() + HeapOffset);
	}
	return FRHIDescriptorHandle();
}

void FRHIOffsetHeapDescriptorAllocator::Free(const FRHIDescriptorHandle InHandle)
{
	if (InHandle.IsValid())
	{
		const FRHIDescriptorHandle AdjustedHandle(InHandle.GetType(), InHandle.GetIndex() - HeapOffset);
		FRHIHeapDescriptorAllocator::Free(AdjustedHandle);
	}
}
