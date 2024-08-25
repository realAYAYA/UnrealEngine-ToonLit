// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Helper namespace for handling index based evaluations. */
namespace PCGIndexing
{
	/** A simple data structure to represent a range of indices [X,Y]. Validating ranges are client responsibility. */
	struct FPCGIndexRange
	{
		FPCGIndexRange(const int32 InStartIndex, const int32 InEndIndex) : StartIndex(InStartIndex), EndIndex(InEndIndex)
		{
		}

		/** Returns true if the index can be found in this range. */
		bool ContainsIndex(int32 Index) const;

		/** Returns the total discrete indices within the range. */
		int32 GetIndexCount() const;

		bool operator<(const FPCGIndexRange& OtherRange) const;
		bool operator==(const FPCGIndexRange& OtherRange) const;
		bool operator!=(const FPCGIndexRange& OtherRange) const;

		int32 StartIndex;
		int32 EndIndex;
	};

	/** An abstract collection of FPCGIndexRange data that represents a concrete set of indices. */
	class FPCGIndexCollection
	{
	public:
		/** The constructor must accept the size of the array to support negative terminating indices. */
		explicit FPCGIndexCollection(const int32 InArraySize) : ArraySize(InArraySize)
		{
			check(InArraySize > 0);
		}

		/** Add a new index range to the collection directly via start and end indices. */
		bool AddRange(int32 StartIndex, int32 EndIndex);

		/** Add a new index range to the collection directly via an index range. */
		bool AddRange(const FPCGIndexRange& NewRange);

		/** Validates that a range is acceptable for this collection. Returns true if valid, false otherwise */
		bool RangeIsValid(const FPCGIndexRange& Range) const;

		/** Returns true if this collection contains the given index. */
		bool ContainsIndex(int32 Index) const;

		/** Returns the abstract size of the array tied to the collection. */
		int32 GetArraySize() const;

		/** Gets the number of range structures currently in the collection. */
		int32 GetTotalRangeCount() const;

		/** Gets the total number of concrete indices within the collection. */
		int32 GetTotalIndexCount() const;

		bool operator==(const FPCGIndexCollection& Other) const;

	private:
		int32 AdjustIndex(int32 Index) const;

		/** Checks two ranges for an overlap and returns true if they overlap. */
		bool CheckOverlap(const FPCGIndexRange& FirstRange, const FPCGIndexRange& SecondRange) const;

		/** Returns the resultant range after merging two overlapping ranges. */
		FPCGIndexRange MergeRanges(const FPCGIndexRange& FirstRange, const FPCGIndexRange& SecondRange) const;

		/** The size of the representative array associated with this collection. Needed for negative terminating indices. */
		int32 ArraySize = 0;

		/** A collection of the abstract index ranges in the collection. */
		TArray<FPCGIndexRange> IndexRanges;
	};
}
