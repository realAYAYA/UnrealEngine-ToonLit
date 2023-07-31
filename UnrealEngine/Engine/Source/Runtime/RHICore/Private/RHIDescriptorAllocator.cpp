// Copyright Epic Games, Inc. All Rights Reserved.

#include "RHIDescriptorAllocator.h"
#include "Misc/ScopeLock.h"

struct FRHIDescriptorAllocatorRange
{
	FRHIDescriptorAllocatorRange(uint32 InFirst, uint32 InLast) : First(InFirst), Last(InLast) {}
	uint32 First;
	uint32 Last;
};

FRHIDescriptorAllocator::FRHIDescriptorAllocator()
{
}

FRHIDescriptorAllocator::FRHIDescriptorAllocator(uint32 InNumDescriptors)
{
	Init(InNumDescriptors);
}

FRHIDescriptorAllocator::~FRHIDescriptorAllocator()
{
}

void FRHIDescriptorAllocator::Init(uint32 InNumDescriptors)
{
	Capacity = InNumDescriptors;
	Ranges.Emplace(0, InNumDescriptors - 1);
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
					Ranges.RemoveAt(Index);
				}
				else
				{
					// Just grow range
					Ranges[Index].Last += NumDescriptors;
				}
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

FRHIHeapDescriptorAllocator::FRHIHeapDescriptorAllocator(ERHIDescriptorHeapType InType, uint32 InDescriptorCount)
	: FRHIDescriptorAllocator(InDescriptorCount)
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