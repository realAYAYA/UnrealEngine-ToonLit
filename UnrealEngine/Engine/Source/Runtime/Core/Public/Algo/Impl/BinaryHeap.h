// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

//#include "Templates/Decay.h"
#include "Templates/Invoke.h"
#include "Templates/ReversePredicate.h"

#include <type_traits>

namespace AlgoImpl
{
	/**
	 * Gets the index of the left child of node at Index.
	 *
	 * @param	Index Node for which the left child index is to be returned.
	 * @returns	Index of the left child.
	 */
	template <typename IndexType>
	FORCEINLINE IndexType HeapGetLeftChildIndex(IndexType Index)
	{
		return Index * 2 + 1;
	}

	/** 
	 * Checks if node located at Index is a leaf or not.
	 *
	 * @param	Index Node index.
	 * @returns	true if node is a leaf, false otherwise.
	 */
	template <typename IndexType>
	FORCEINLINE bool HeapIsLeaf(IndexType Index, IndexType Count)
	{
		return HeapGetLeftChildIndex(Index) >= Count;
	}

	/**
	 * Gets the parent index for node at Index.
	 *
	 * @param	Index node index.
	 * @returns	Parent index.
	 */
	template <typename IndexType>
	FORCEINLINE IndexType HeapGetParentIndex(IndexType Index)
	{
		return (Index - 1) / 2;
	}

	/**
	 * Fixes a possible violation of order property between node at Index and a child.
	 *
	 * @param	Heap		Pointer to the first element of a binary heap.
	 * @param	Index		Node index.
	 * @param	Count		Size of the heap.
	 * @param	Projection	The projection to apply to the elements.
	 * @param	Predicate	A binary predicate object used to specify if one element should precede another.
	 */
	template <typename RangeValueType, typename IndexType, typename ProjectionType, typename PredicateType>
	FORCEINLINE void HeapSiftDown(RangeValueType* Heap, IndexType Index, const IndexType Count, const ProjectionType& Projection, const PredicateType& Predicate)
	{
		while (!HeapIsLeaf(Index, Count))
		{
			const IndexType LeftChildIndex = HeapGetLeftChildIndex(Index);
			const IndexType RightChildIndex = LeftChildIndex + 1;

			IndexType MinChildIndex = LeftChildIndex;
			if (RightChildIndex < Count)
			{
				MinChildIndex = Predicate( Invoke(Projection, Heap[LeftChildIndex]), Invoke(Projection, Heap[RightChildIndex]) ) ? LeftChildIndex : RightChildIndex;
			}

			if (!Predicate( Invoke(Projection, Heap[MinChildIndex]), Invoke(Projection, Heap[Index]) ))
			{
				break;
			}

			Swap(Heap[Index], Heap[MinChildIndex]);
			Index = MinChildIndex;
		}
	}

	/**
	 * Fixes a possible violation of order property between node at NodeIndex and a parent.
	 *
	 * @param	Heap		Pointer to the first element of a binary heap.
	 * @param	RootIndex	How far to go up?
	 * @param	NodeIndex	Node index.
	 * @param	Projection	The projection to apply to the elements.
	 * @param	Predicate	A binary predicate object used to specify if one element should precede another.
	 *
	 * @return	The new index of the node that was at NodeIndex
	 */
	template <class RangeValueType, typename IndexType, typename ProjectionType, class PredicateType>
	FORCEINLINE IndexType HeapSiftUp(RangeValueType* Heap, IndexType RootIndex, IndexType NodeIndex, const ProjectionType& Projection, const PredicateType& Predicate)
	{
		while (NodeIndex > RootIndex)
		{
			IndexType ParentIndex = HeapGetParentIndex(NodeIndex);
			if (!Predicate( Invoke(Projection, Heap[NodeIndex]), Invoke(Projection, Heap[ParentIndex]) ))
			{
				break;
			}

			Swap(Heap[NodeIndex], Heap[ParentIndex]);
			NodeIndex = ParentIndex;
		}

		return NodeIndex;
	}

	/** 
	 * Builds an implicit min-heap from a range of elements.
	 * This is the internal function used by Heapify overrides.
	 *
	 * @param	First		pointer to the first element to heapify
	 * @param	Num			the number of items to heapify
	 * @param	Projection	The projection to apply to the elements.
	 * @param	Predicate	A binary predicate object used to specify if one element should precede another.
	 */
	template <typename RangeValueType, typename IndexType, typename ProjectionType, typename PredicateType>
	FORCEINLINE void HeapifyInternal(RangeValueType* First, IndexType Num, ProjectionType Projection, PredicateType Predicate)
	{
		if constexpr (std::is_signed_v<IndexType>)
		{
			checkf(Num >= 0, TEXT("Algo::HeapifyInternal called with negative count"));
		}

		if (Num == 0)
		{
			return;
		}

		IndexType Index = HeapGetParentIndex(Num - 1);
		for (;;)
		{
			HeapSiftDown(First, Index, Num, Projection, Predicate);
			if (Index == 0)
			{
				return;
			}
			--Index;
		}
	}

	/**
	 * Performs heap sort on the elements.
	 * This is the internal sorting function used by HeapSort overrides.
	 *
	 * @param	First		pointer to the first element to sort
	 * @param	Num			the number of elements to sort
	 * @param	Predicate	predicate class
	 */
	template <typename RangeValueType, typename IndexType, typename ProjectionType, class PredicateType>
	void HeapSortInternal(RangeValueType* First, IndexType Num, ProjectionType Projection, PredicateType Predicate)
	{
		if constexpr (std::is_signed_v<IndexType>)
		{
			checkf(Num >= 0, TEXT("Algo::HeapSortInternal called with negative count"));
		}

		if (Num == 0)
		{
			return;
		}

		TReversePredicate< PredicateType > ReversePredicateWrapper(Predicate); // Reverse the predicate to build a max-heap instead of a min-heap
		HeapifyInternal(First, Num, Projection, ReversePredicateWrapper);

		for (IndexType Index = Num - 1; Index > 0; Index--)
		{
			Swap(First[0], First[Index]);

			HeapSiftDown(First, (IndexType)0, Index, Projection, ReversePredicateWrapper);
		}
	}
}
