// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassEntityTypes.h"
#include "StructUtilsTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MassEntityTypes)

DEFINE_STAT(STAT_Mass_Total);

DEFINE_TYPEBITSET(FMassFragmentBitSet);
DEFINE_TYPEBITSET(FMassTagBitSet);
DEFINE_TYPEBITSET(FMassChunkFragmentBitSet);
DEFINE_TYPEBITSET(FMassSharedFragmentBitSet);
DEFINE_TYPEBITSET(FMassExternalSubsystemBitSet);

//-----------------------------------------------------------------------------
// FMassArchetypeSharedFragmentValues
//-----------------------------------------------------------------------------
uint32 FMassArchetypeSharedFragmentValues::CalculateHash() const
{
	checkf(bSorted, TEXT("Expecting the containers to be sorted for the hash caluclation to be correct"));

	// Fragments are not part of the uniqueness 
	uint32 Hash = 0;
	for (const FConstSharedStruct& Fragment : ConstSharedFragments)
	{
		Hash = PointerHash(Fragment.GetMemory(), Hash);
	}

	for (const FSharedStruct& Fragment : SharedFragments)
	{
		Hash = PointerHash(Fragment.GetMemory(), Hash);
	}

	return Hash;
}

//-----------------------------------------------------------------------------
// FMassGenericPayloadView
//-----------------------------------------------------------------------------
void FMassGenericPayloadView::SwapElementsToEnd(const int32 StartIndex, int32 NumToMove)
{
	check(StartIndex >= 0 && NumToMove >= 0);

	if (UNLIKELY(NumToMove <= 0 || StartIndex < 0))
	{
		return;
	}

	TArray<uint8, TInlineAllocator<16>> MovedElements;

	for (FStructArrayView& StructArrayView : Content)
	{
		check((StartIndex + NumToMove) <= StructArrayView.Num());
		if (StartIndex + NumToMove >= StructArrayView.Num() - 1)
		{
			// nothing to do here, the elements are already at the back
			continue;
		}

		uint8* ViewData = static_cast<uint8*>(StructArrayView.GetData());
		const uint32 ElementSize = StructArrayView.GetTypeSize();
		const uint32 MovedStartOffset = StartIndex * ElementSize;
		const uint32 MovedSize = NumToMove * ElementSize;
		const uint32 MoveOffset = (StructArrayView.Num() - (StartIndex + NumToMove)) * ElementSize;

		MovedElements.Reset();
		MovedElements.Append(ViewData + MovedStartOffset, MovedSize);
		FMemory::Memmove(ViewData + MovedStartOffset, ViewData + MovedStartOffset + MovedSize, MoveOffset);
		FMemory::Memcpy(ViewData + MovedStartOffset + MoveOffset, MovedElements.GetData(), MovedSize);
	}
}