// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BoxTypes.h"
#include "IntBoxTypes.h"

// 2d version of TSparseGrid3

namespace UE
{
namespace Geometry
{

/**
 * Dynamic sparse 2D grid. Useful in cases where we have grid of some type of non-trivial 
 * object and we don't want to pre-allocate full grid of them. So we allocate on-demand.
 * This can be used to implement multi-grid schemes, eg for example the ElemType
 * could be sub-grid of fixed dimension.
 *
 * @todo support pooling/re-use of allocated elements? currently not required by any use cases.
 * @todo port AllocatedIndices() and Allocated() iterators 
 */
template<typename ElemType>
class TSparseGrid2
{
protected:
	/** sparse grid of allocated elements */
	TMap<FVector2i, ElemType*> Elements;
	/** accumulated bounds of all indices inserted into Elements. Not currently used internally. */
	FAxisAlignedBox2i Bounds;

public:

	/**
	 * Create empty grid
	 */
	TSparseGrid2()
	{
		Bounds = FAxisAlignedBox2i::Empty();
	}

	/**
	 * Deletes all grid elements
	 */
	~TSparseGrid2()
	{
		FreeAll();
	}


	TSparseGrid2(const TSparseGrid2& Other) = delete;
	TSparseGrid2& operator=(const TSparseGrid2& Other) = delete;
	TSparseGrid2(TSparseGrid2&& Other) noexcept : Elements(MoveTemp(Other.Elements)), Bounds(MoveTemp(Other.Bounds))
	{
		Other.Elements.Reset();
	}
	TSparseGrid2& operator=(TSparseGrid2&& Other) noexcept
	{
		if (this != &Other)
		{
			Elements = MoveTemp(Other.Elements);
			Bounds = MoveTemp(Other.Bounds);
			Other.Elements.Reset();
		}
		return *this;
	}

	/**
	 * @param Index an integer grid index
	 * @return true if there is an allocated element at this index
	 */
	bool Has(const FVector2i& Index) const
	{
		return Elements.Contains(Index);
	}


	/**
	 * Get the grid element at this index
	 * @param Index integer grid index
	 * @return pointer to ElemType instance, or nullptr if element doesn't exist
	 */
	const ElemType* Get(const FVector2i& Index) const
	{
		return Elements.FindRef(Index);
	}


	/**
	 * Get the grid element at this index, and optionally allocate it if it doesn't exist
	 * @param Index integer grid index
	 * @param bAllocateIfMissing if the element at this index is null, allocate a new one
	 * @return pointer to ElemType instance, or nullptr if element doesn't exist
	 */
	ElemType* Get(const FVector2i& Index, bool bAllocateIfMissing)
	{
		ElemType* result = Elements.FindRef(Index);
		if (result != nullptr)
		{
			return result;
		}
		if (bAllocateIfMissing == true)
		{
			return Allocate(Index);
		}
		return nullptr;
	}

	/**
	 * Delete an element in the grid
	 * @param Index integer grid index
	 * @return true if the element existed, false if it did not
	 */
	bool Free(const FVector2i& Index)
	{
		if (Elements.Contains(Index))
		{
			delete Elements[Index];
			Elements.Remove(Index);
			return true;
		}
		return false;
	}

	/**
	 * Delete all elements in the grid
	 */
	void FreeAll()
	{
		for (auto pair : Elements)
		{
			delete pair.Value;
		}
		Elements.Reset();
	}

	/**
	 * @return number of allocated elements in the grid
	 */
	int GetCount() const
	{
		return Elements.Num();
	}

	/**
	 * @return ratio of allocated elements to total number of possible cells for current bounds
	 */
	float GetDensity() const
	{
		return (float)Elements.Num() / (float)Bounds.Area();
	}

	/** 
	 * @return integer range of valid grid indices [min,max] (inclusive) 
	 */
	FAxisAlignedBox2i GetBoundsInclusive() const
	{
		return Bounds;
	}

	/** 
	 * @return dimensions of grid along each axis 
	 */
	FVector2i GetDimensions() const
	{
		return Bounds.Diagonal() + FVector2i::One();
	}


	/**
	 * Iterate over existing elements and apply ElementFunc(ElementType) to each of them
	 */
	template<typename Func>
	void AllocatedIteration(Func ElementFunc) const
	{
		for (auto Pair : Elements)
		{
			ElementFunc(Pair.Value);
		}
	}


	/**
	 * Iterate over existing allocated elements within the integer bounds defined 
	 * by Min and Max (inclusive), and apply ElementFunc(ElementType) to each of them
	 */
	template<typename Func>
	void RangeIteration(FVector2i MinIndex, FVector2i MaxIndex, Func ElementFunc) const
	{
		for (int32 yi = MinIndex.Y; yi <= MaxIndex.Y; ++yi)
		{
			for (int32 xi = MinIndex.X; xi <= MaxIndex.X; ++xi)
			{
				const ElemType* Found = Elements.FindRef(FVector2i(xi,yi));
				if (Found != nullptr)
				{
					ElementFunc(*Found);
				}
			}
		}
	}

	SIZE_T GetAllocatedSize() const
	{
		return Elements.GetAllocatedSize() + GetCount() * sizeof(ElemType);
	}

protected:

	ElemType* Allocate(const FVector2i& index)
	{
		ElemType* NewElem = new ElemType();
		Elements.Add(index, NewElem);
		Bounds.Contain(index);
		return NewElem;
	}
};

} // end namespace UE::Geometry
} // end namespace UE