// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LumenUniqueList.h:
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Experimental/Containers/SherwoodHashTable.h"

/**
 * Crude list of unique elements
 * Flat array, backed by a set for faster insertion
 */
template <typename ElementType, typename Allocator>
struct TSparseUniqueList
{
	void Add(ElementType Element)
	{
		bool bIsAlreadyInSet = false;
		Set.Add(Element, &bIsAlreadyInSet);
		if (!bIsAlreadyInSet)
		{
			Array.Add(Element);
		}
	}

	TArray<ElementType, Allocator> Array;
	Experimental::TSherwoodSet<ElementType> Set;
};

/**
 * List of unique indices
 * Flat array, backed by a bitset
 */
struct FUniqueIndexList
{
public:
	void Add(int32 Index)
	{
		if (Index + 1 > IndicesMarkedToUpdate.Num())
		{
			const int32 NewSize = Align(Index + 1, 64);
			IndicesMarkedToUpdate.Add(0, NewSize - IndicesMarkedToUpdate.Num());
		}

		// Make sure we aren't updating same index multiple times
		if (!IndicesMarkedToUpdate[Index])
		{
			Indices.Add(Index);
			IndicesMarkedToUpdate[Index] = true;
		}
	}

	int32 Num() const
	{
		return Indices.Num();
	}

	void Reset()
	{
		Indices.Reset();
		IndicesMarkedToUpdate.Reset();
	}

	void Empty(int32 Slack)
	{
		Indices.Empty(Slack);
		IndicesMarkedToUpdate.Reset();
	}

	// Iterate over indices
	auto begin() const { return Indices.begin(); }
	auto end() const { return Indices.end(); }


private:

	TArray<int32> Indices;
	TBitArray<>	IndicesMarkedToUpdate;
};