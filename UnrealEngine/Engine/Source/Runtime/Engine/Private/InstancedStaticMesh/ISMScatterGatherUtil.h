// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "HAL/UnrealMemory.h"
#include "Math/UnrealMathUtility.h"
#include "Containers/Array.h"

/**
 * Write InSource that was previously gathered using the above methods to the final destination array OutDest
 * using the same delta information. 
 * If there is no delta, it performs a move of the source data to the final array, saving a copy.
 */
template <typename DeltaType, typename ValueType>
void Scatter(const DeltaType &Delta, TArray<ValueType> &OutDest, int32 DestNumElements, TArray<ValueType> &&InSource, int32 ElementStride = 1)
{
	check(InSource.Num() == Delta.GetNumItems() * ElementStride);
	if (Delta.IsDelta())
	{
		OutDest.SetNumUninitialized(DestNumElements * ElementStride);
		for (auto It = Delta.GetIterator(); It; ++It)
		{
			int32 ItemIndex = It.GetItemIndex();
			int32 DestIndex = It.GetIndex();
			FMemory::Memcpy(&OutDest[DestIndex * ElementStride], &InSource[ItemIndex * ElementStride], ElementStride * sizeof(ValueType));
		}
	}
	else
	{
		check(InSource.Num() == DestNumElements * ElementStride);
		OutDest = MoveTemp(InSource);
	}
}
/**
 * Write InSource that was previously gathered using the above methods to the final destination array OutDest
 * using the same delta information. 
 * Never takes move shortcut as there is an index remap.
 */
template <typename DeltaType, typename ValueType, typename IndexRemapType>
void Scatter(const DeltaType &Delta, TArray<ValueType> &OutDest, int32 DestNumElements, const TArray<ValueType> &InSource, const IndexRemapType &IndexRemap, int32 ElementStride = 1)
{
	check(InSource.Num() == Delta.GetNumItems() * ElementStride);
	OutDest.SetNumUninitialized(DestNumElements * ElementStride);
	for (auto It = Delta.GetIterator(); It; ++It)
	{
		int32 SrcIndex = It.GetItemIndex();
		int32 DestIndex = It.GetIndex();

		if (IndexRemap.Remap(SrcIndex, DestIndex))
		{
			FMemory::Memcpy(&OutDest[DestIndex * ElementStride], &InSource[SrcIndex * ElementStride], ElementStride * sizeof(ValueType));
		}
	}
}

/**
 * Gather the needed values from InSource to OutDest, according to the delta.
 * If there is no delta, it will perform a bulk copy.
 */
template <typename DeltaType, typename ValueType, typename InValueArrayType>
void Gather(const DeltaType &Delta, TArray<ValueType> &OutDest, const InValueArrayType &InSource, int32 ElementStride = 1)
{
	// strides & element count matches - just copy the data
	if (InSource.Num() == Delta.GetNumItems() * ElementStride)
	{
		OutDest = InSource;
	}
	else if (Delta.IsEmpty())
	{
		OutDest.Reset();
	}
	else if (Delta.IsDelta() || ElementStride != 1)
	{
		OutDest.Reset(Delta.GetNumItems() * ElementStride);
		for (auto It = Delta.GetIterator(); It; ++It)
		{
			check(OutDest.Num() < Delta.GetNumItems() * ElementStride);
			OutDest.Append(&InSource[It.GetIndex() * ElementStride], ElementStride);
		}
	}
}


/**
 * Gather the needed values from InSource to OutDest, according to the delta.
 * If there is no delta, it will perform a bulk copy.
 */
template <typename DeltaType, typename OutValueType, typename InValueArrayType, typename LambdaType>
void GatherTransform(const DeltaType &Delta, TArray<OutValueType> &OutDest, const InValueArrayType &InSource, LambdaType TransformLambda)
{
	if (Delta.IsEmpty())
	{
		OutDest.Reset();
	}
	else
	{
		OutDest.Reset(Delta.GetNumItems());
		for (auto It = Delta.GetIterator(); It; ++It)
		{
			check(OutDest.Num() < Delta.GetNumItems());
			check(OutDest.Num() == It.GetItemIndex());
			OutDest.Add(TransformLambda(InSource[It.GetIndex()]));
		}
	}
}
