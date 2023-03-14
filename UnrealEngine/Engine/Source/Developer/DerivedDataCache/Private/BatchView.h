// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "HAL/Platform.h"
#include "Misc/AssertionMacros.h"

enum class EBatchView
{
	/** Add the item to the previous batch. If no previous batch, treated the same as NewBatch. */
	Continue,
	/** End the previous batch (if it exists), start a new batch, add the item to the new batch. */
	NewBatch,
};

/**
 * Takes a list of items and a ShouldAddToBatch function and provides
 * a ranged-for over the set of batches created from the list.
 */
template <typename T>
class TBatchView
{
public:
	/**
	 * Constructor from list of elements and a function that is called on each element
	 * to decide whether to add the element to the current batch.
	 *
	 * EBatchView ShouldAddToBatch(const T& Item);
	 */
	template <typename ShouldAddToBatchFunc>
	TBatchView(TArrayView<T> InItems, ShouldAddToBatchFunc&& ShouldAddToBatch)
		: Items(InItems)
	{
		if (Items.IsEmpty())
		{
			return;
		}
		ShouldAddToBatch(Items[0]);
		BatchStarts.Add(0);
		for (int32 Index = 1; Index < Items.Num(); ++Index)
		{
			if (ShouldAddToBatch(Items[Index]) == EBatchView::NewBatch)
			{
				BatchStarts.Add(Index);
			}
		}
	}

	struct TIterator
	{
		TIterator(const TBatchView& InBatchList, bool bEnd)
			: BatchList(InBatchList)
			, BatchIndex(bEnd ? BatchList.BatchStarts.Num() : 0)
		{
		}
		TIterator& operator++()
		{
			BatchIndex++;
			return *this;
		}
		TArrayView<T> operator*() const
		{
			int32 Start = BatchList.BatchStarts[BatchIndex];
			int32 End = BatchIndex < BatchList.BatchStarts.Num() - 1 ? BatchList.BatchStarts[BatchIndex + 1] : BatchList.Items.Num();
			return TArrayView<T>(BatchList.Items.GetData() + Start, End - Start);
		}
		bool operator!=(TIterator& Other) const
		{
			return BatchIndex != Other.BatchIndex;
		}
		const TBatchView& BatchList;
		int32 BatchIndex;
	};
	TIterator begin() const
	{
		return TIterator(*this, false /* bEnd */);
	}
	TIterator end() const
	{
		return TIterator(*this, true /* bEnd */);
	}

private:
	TArrayView<T> Items;
	TArray<int32> BatchStarts;
};