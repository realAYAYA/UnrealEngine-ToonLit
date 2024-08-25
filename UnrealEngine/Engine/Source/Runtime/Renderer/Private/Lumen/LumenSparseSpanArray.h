// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LumenSparseSpanArray.h:
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "SpanAllocator.h"

// Sparse array with stable indices and contiguous span allocation
template <typename ElementType>
class TSparseSpanArray
{
public:
	int32 Num() const
	{
		return Elements.Num();
	}

	void Reserve(int32 NumElements)
	{
		Elements.Reserve(NumElements);
	}

	int32 AddSpan(int32 NumElements)
	{
		check(NumElements > 0);

		const int32 InsertIndex = SpanAllocator.Allocate(NumElements);
		
		// Resize element array
		if (SpanAllocator.GetMaxSize() > Elements.Num())
		{
			const int32 NumElementsToAdd = SpanAllocator.GetMaxSize() - Elements.Num();
			Elements.AddDefaulted(NumElementsToAdd);
			AllocatedElementsBitArray.Add(false, NumElementsToAdd);
		}

		// Reuse existing elements
		for (int32 ElementIndex = InsertIndex; ElementIndex < InsertIndex + NumElements; ++ElementIndex)
		{
			checkSlow(!IsAllocated(ElementIndex));
			Elements[ElementIndex] = ElementType();
		}

		AllocatedElementsBitArray.SetRange(InsertIndex, NumElements, true);

		return InsertIndex;
	}

	void RemoveSpan(int32 FirstElementIndex, int32 NumElements)
	{
		check(NumElements > 0);

		for (int32 ElementIndex = FirstElementIndex; ElementIndex < FirstElementIndex + NumElements; ++ElementIndex)
		{
			checkSlow(IsAllocated(ElementIndex));
			Elements[ElementIndex] = ElementType();
		}

		SpanAllocator.Free(FirstElementIndex, NumElements);
		AllocatedElementsBitArray.SetRange(FirstElementIndex, NumElements, false);
	}

	void Consolidate()
	{
		SpanAllocator.Consolidate();

		if (Elements.Num() > SpanAllocator.GetMaxSize())
		{
			Elements.SetNum(SpanAllocator.GetMaxSize());
			AllocatedElementsBitArray.SetNumUninitialized(SpanAllocator.GetMaxSize());
		}
	}

	void Reset()
	{
		Elements.Reset();
		SpanAllocator.Reset();
		AllocatedElementsBitArray.SetNumUninitialized(0);
	}

	ElementType& operator[](int32 Index)
	{
		checkSlow(IsAllocated(Index));
		return Elements[Index];
	}

	const ElementType& operator[](int32 Index) const
	{
		checkSlow(IsAllocated(Index));
		return Elements[Index];
	}

	bool IsAllocated(int32 ElementIndex) const
	{
		if (ElementIndex < AllocatedElementsBitArray.Num())
		{
			return AllocatedElementsBitArray[ElementIndex];
		}

		return false;
	}

	SIZE_T GetAllocatedSize() const
	{
		return Elements.GetAllocatedSize() + AllocatedElementsBitArray.GetAllocatedSize() + SpanAllocator.GetAllocatedSize();
	}

	class TRangedForIterator
	{
	public:
		TRangedForIterator(TSparseSpanArray<ElementType>& InArray, int32 InElementIndex)
			: Array(InArray)
			, ElementIndex(InElementIndex)
		{
			// Scan for the first valid element.
			while (ElementIndex < Array.Elements.Num() && !Array.AllocatedElementsBitArray[ElementIndex])
			{
				++ElementIndex;
			} 
		}

		TRangedForIterator operator++()
		{
			// Scan for the next first valid element.
			do
			{
				++ElementIndex;
			} while (ElementIndex < Array.Elements.Num() && !Array.AllocatedElementsBitArray[ElementIndex]);

			return *this;
		}

		bool operator!=(const TRangedForIterator& Other) const
		{
			return ElementIndex != Other.ElementIndex;
		}

		ElementType& operator*()
		{
			return Array.Elements[ElementIndex];
		}

	private:
		TSparseSpanArray<ElementType>& Array;
		int32 ElementIndex;
	};

	class TRangedForConstIterator
	{
	public:
		TRangedForConstIterator(const TSparseSpanArray<ElementType>& InArray, int32 InElementIndex)
			: Array(InArray)
			, ElementIndex(InElementIndex)
		{
			// Scan for the first valid element.
			while (ElementIndex < Array.Elements.Num() && !Array.AllocatedElementsBitArray[ElementIndex])
			{
				++ElementIndex;
			}
		}

		TRangedForConstIterator operator++()
		{ 
			// Scan for the next first valid element.
			do
			{
				++ElementIndex;
			} while (ElementIndex < Array.Elements.Num() && !Array.AllocatedElementsBitArray[ElementIndex]);

			return *this;
		}

		bool operator!=(const TRangedForConstIterator& Other) const
		{ 
			return ElementIndex != Other.ElementIndex;
		}

		const ElementType& operator*() const
		{ 
			return Array.Elements[ElementIndex];
		}

	private:
		const TSparseSpanArray<ElementType>& Array;
		int32 ElementIndex;
	};

	// Iterate over all allocated elements (skip free ones)
	TRangedForIterator begin() { return TRangedForIterator(*this, 0); }
	TRangedForIterator end() { return TRangedForIterator(*this, Elements.Num()); }
	TRangedForConstIterator begin() const { return TRangedForConstIterator(*this, 0); }
	TRangedForConstIterator end() const { return TRangedForConstIterator(*this, Elements.Num()); }

private:

	TArray<ElementType> Elements;
	TBitArray<> AllocatedElementsBitArray;
	FSpanAllocator SpanAllocator;
};