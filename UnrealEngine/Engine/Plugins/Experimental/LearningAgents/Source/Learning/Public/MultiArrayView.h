// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/ArrayView.h"
#include "Misc/AssertionMacros.h"
#include "Templates/Invoke.h"
#include "Templates/UnrealTypeTraits.h"
#include "Traits/ElementType.h"
#include "Math/UnrealMathUtility.h"
#include <type_traits>

namespace UE::MultiArrayView::Private
{
	/**
	 * Copied from Core/Public/Containers/ArrayView.h
	 *
	 * Trait testing whether a type is compatible with the view type
	 *
	 * The extra stars here are *IMPORTANT*
	 * They prevent TMultiArrayView<Base>(TArray<Derived>&) from compiling!
	 */
	template <typename T, typename ElementType>
	constexpr bool TIsCompatibleElementType_V = std::is_convertible_v<T**, ElementType* const*>;

	// Copied from Core/Public/Containers/ArrayView.h
	//
	// Simply forwards to an unqualified GetData(), but can be called from within TArrayView
	// where GetData() is already a member and so hides any others.
	template <typename T>
	FORCEINLINE decltype(auto) GetDataHelper(T&& Arg)
	{
		return GetData(Forward<T>(Arg));
	}

	// Copied from Core/Public/Containers/ArrayView.h
	//
	// Gets the data from the passed argument and proceeds to reinterpret the resulting elements
	template <typename T>
	FORCEINLINE decltype(auto) GetReinterpretedDataHelper(T&& Arg)
	{
		auto NaturalPtr = GetData(Forward<T>(Arg));
		using NaturalElementType = std::remove_pointer_t<decltype(NaturalPtr)>;

		auto EndPtr = NaturalPtr + GetNum(Arg);
		TContainerElementTypeCompatibility<NaturalElementType>::ReinterpretRange(NaturalPtr, EndPtr);

		return reinterpret_cast<typename TContainerElementTypeCompatibility<NaturalElementType>::ReinterpretType*>(NaturalPtr);
	}

	/**
	 * Copied from Core/Public/Containers/ArrayView.h
	 *
	 * Trait testing whether a type is compatible with the view type
	 */
	template <typename RangeType, typename ElementType>
	struct TIsCompatibleRangeType
	{
		static constexpr bool Value = TIsCompatibleElementType_V<std::remove_pointer_t<decltype(GetData(DeclVal<RangeType&>()))>, ElementType>;

		template <typename T>
		static decltype(auto) GetData(T&& Arg)
		{
			return UE::MultiArrayView::Private::GetDataHelper(Forward<T>(Arg));
		}
	};

	/**
	 * Copied from Core/Public/Containers/ArrayView.h
	 *
	 * Trait testing whether a type is reinterpretable in a way that permits use with the view type
	 */
	template <typename RangeType, typename ElementType>
	struct TIsReinterpretableRangeType
	{
	private:
		using NaturalElementType = std::remove_pointer_t<decltype(GetData(DeclVal<RangeType&>()))>;

	public:
		static constexpr bool Value =
			!std::is_same_v<typename TContainerElementTypeCompatibility<NaturalElementType>::ReinterpretType, NaturalElementType>
			&&
			TIsCompatibleElementType_V<typename TContainerElementTypeCompatibility<NaturalElementType>::ReinterpretType, ElementType>;

		template <typename T>
		static decltype(auto) GetData(T&& Arg)
		{
			return UE::MultiArrayView::Private::GetReinterpretedDataHelper(Forward<T>(Arg));
		}
	};
}

template<uint8 InDimNum, typename InSizeType = FDefaultAllocator::SizeType>
class TMultiArrayShape
{
public:
	static constexpr uint8 DimNum = InDimNum;
	using SizeType = InSizeType;

	static_assert(DimNum >= 1, "TMultiArrayShape requires a positive, non-zero number of dimensions");
	static_assert(std::is_signed_v<SizeType>, "TMultiArrayShape only supports signed index types");

public:

	TMultiArrayShape() = default;

	TMultiArrayShape(std::initializer_list<SizeType> InNums)
	{
		checkf(InNums.size() == DimNum, TEXT("Wrong number of elements in constructor"));

		uint8 Idx = 0;
		for (SizeType InNum : InNums)
		{
			Nums[Idx] = InNum;
			Idx++;
		}
	}

	TMultiArrayShape(const SizeType* InNums)
	{
		for (uint8 Idx = 0; Idx < DimNum; Idx++)
		{
			Nums[Idx] = InNums[Idx];
		}
	}

	SizeType& operator[](uint8 Dimension)
	{
		return Nums[Dimension];
	}

	const SizeType& operator[](uint8 Dimension) const
	{
		return Nums[Dimension];
	}

	SizeType Total() const
	{
		if constexpr (DimNum == 1)
		{
			return Nums[0];
		}
		else
		{
			InSizeType Total = Nums[0];
			for (uint8 Idx = 1; Idx < DimNum; Idx++)
			{
				Total *= Nums[Idx];
			}
			return Total;
		}
	}

private:
	SizeType Nums[DimNum] = { 0 };
};

/**
 * Templated fixed-size view of multi-dimensional array
 *
 * A statically sized view of a multi-dimensional array of typed elements. 
 *
 * @see TArrayView
 */
template<uint8 InDimNum, typename InElementType, bool bInIsChecked = true, bool bInIsRestrict = false, typename InSizeType = FDefaultAllocator::SizeType>
class TMultiArrayView
{
public:
	static constexpr uint8 DimNum = InDimNum;
	using ElementType = InElementType;
	static constexpr bool bIsChecked = bInIsChecked;
	static constexpr bool bIsRestrict = bInIsRestrict;
	using SizeType = InSizeType;

	using PointerType = std::conditional_t<
		bIsRestrict,
		ElementType* RESTRICT,
		ElementType*>;

	static_assert(DimNum > 1, "TMultiArrayView requires a positive, non-zero number of dimensions");
	static_assert(std::is_signed_v<SizeType>, "TMultiArrayView only supports signed index types");

public:

	/**
	 * Constructor.
	 */
	TMultiArrayView() : DataPtr(nullptr) { }

	/**
	 * Construct a view of an arbitrary pointer
	 *
	 * @param InData	The data to view
	 * @param InShape	The number of elements on each dimension
	 */
	FORCEINLINE TMultiArrayView(ElementType* InData, TMultiArrayShape<DimNum, SizeType> InShape)
		: DataPtr(InData)
		, ArrayShape(InShape)
	{
		CheckInvariants();
	}

	/**
	 * Array bracket operator. Returns reference to sub-view at given index.
	 *
	 * @returns Reference to indexed element.
	 */
	FORCEINLINE TMultiArrayView<DimNum - 1, ElementType, bIsChecked, bIsRestrict, SizeType> operator[](SizeType Index) const
	{
		RangeCheck(0, Index);

		TMultiArrayShape<DimNum - 1, SizeType> NewShape;
		for (uint8 Idx = 0; Idx < DimNum - 1; Idx++)
		{
			NewShape[Idx] = ArrayShape[Idx + 1];
		}

		return TMultiArrayView<DimNum - 1, ElementType, bIsChecked, bIsRestrict, SizeType>(DataPtr + Index * Stride(0), NewShape);
	}

	/**
	 * Flattens the array view into a single dimension.
	 *
	 * @returns Flattened array view.
	 */
	FORCEINLINE TMultiArrayView<1, ElementType, bIsChecked, bIsRestrict, SizeType> Flatten() const
	{
		return TMultiArrayView<1, ElementType, bIsChecked, bIsRestrict, SizeType>(DataPtr, Num());
	}

	/**
	 * Flattens the array on a given dimension, merging that dimension and the following one.
	 * e.g. a 3D MultiArrayView with shape [10, 5, 3] flattened on dimension 0 will become a 2D MultiArrayView with shape [10 * 5, 3]
	 *
	 * @returns View flattened on the given dimension.
	 */
	FORCEINLINE TMultiArrayView<DimNum - 1, ElementType, bIsChecked, bIsRestrict, SizeType> Flatten(uint8 Dimension) const
	{
		if (bIsChecked)
		{
			checkf((Dimension < DimNum - 1), TEXT("MultiArray flatten dimension out of bounds: %lld from a rank of %lld"), (long long)Dimension, (long long)DimNum); // & for one branch
		}

		TMultiArrayShape<DimNum - 1, SizeType> NewShape;
		uint8 SrcIdx = 0;
		for (uint8 Idx = 0; Idx < DimNum - 1; Idx++)
		{
			if (Idx == Dimension)
			{
				NewShape[Idx] = ArrayShape[SrcIdx] * ArrayShape[SrcIdx+1];
				SrcIdx += 2;
			}
			else
			{
				NewShape[Idx] = ArrayShape[SrcIdx];
				SrcIdx += 1;
			}
		}

		return TMultiArrayView<DimNum - 1, ElementType, bIsChecked, bIsRestrict, SizeType>(DataPtr, NewShape);
	}

	/**
	 * Flattens the array on a given dimension, merging that dimension and the following one.
	 * e.g. a 3D MultiArrayView with shape [10, 5, 3] flattened on dimension 0 will become a 2D MultiArrayView with shape [10 * 5, 3]
	 *
	 * @returns View flattened on the given dimension.
	 */
	template<uint8 Dimension>
	FORCEINLINE TMultiArrayView<DimNum - 1, ElementType, bIsChecked, bIsRestrict, SizeType> Flatten() const
	{
		static_assert(Dimension < DimNum - 1, "MultiArray flatten dimension out of bounds");

		TMultiArrayShape<DimNum - 1, SizeType> NewShape;
		uint8 SrcIdx = 0;
		for (uint8 Idx = 0; Idx < DimNum - 1; Idx++)
		{
			if (Idx == Dimension)
			{
				NewShape[Idx] = ArrayShape[SrcIdx] * ArrayShape[SrcIdx + 1];
				SrcIdx += 2;
			}
			else
			{
				NewShape[Idx] = ArrayShape[SrcIdx];
				SrcIdx += 1;
			}
		}

		return TMultiArrayView<DimNum - 1, ElementType, bIsChecked, bIsRestrict, SizeType>(DataPtr, NewShape);
	}

public:

	/**
	 * Helper function for returning a typed pointer to the first array entry.
	 *
	 * @returns Pointer to first array entry or nullptr if empty
	 */
	FORCEINLINE PointerType GetData()
	{
		return DataPtr;
	}

	/**
	 * Helper function for returning a typed pointer to the first array entry.
	 *
	 * @returns Pointer to first array entry or nullptr if empty
	 */
	FORCEINLINE const PointerType GetData() const
	{
		return DataPtr;
	}


	/**
	 * DO NOT USE DIRECTLY
	 * STL-like iterators to enable range-based for loop support.
	 */
	FORCEINLINE ElementType* begin() const { return GetData(); }
	FORCEINLINE ElementType* end() const { return GetData() + Num(); }

	/**
	 * Helper function returning the size of the inner type.
	 *
	 * @returns Size in bytes of array type.
	 */
	FORCEINLINE static constexpr size_t GetTypeSize()
	{
		return sizeof(ElementType);
	}

	/**
	 * Helper function returning the alignment of the inner type.
	 */
	FORCEINLINE static constexpr size_t GetTypeAlignment()
	{
		return alignof(ElementType);
	}

	/**
	 * Checks array invariants: if array size is greater than zero on each dimension
	 */
	FORCEINLINE void CheckInvariants() const
	{
		if (bIsChecked)
		{
			for (SizeType Idx = 0; Idx < DimNum; Idx++)
			{
				checkSlow(ArrayShape[Idx] >= 0);
			}
		}
	}

	/**
	 * Checks if a dimension is within the allowed number of dimensions
	 *
	 * @param Dimension Dimension of the array.
	 */
	FORCEINLINE void DimensionCheck(uint8 Dimension) const
	{
		if (bIsChecked)
		{
			checkf((Dimension < DimNum), TEXT("MultiArray dimension out of bounds: %lld from a rank of %lld"), (long long)Dimension, (long long)DimNum); // & for one branch
		}
	}

	/**
	 * Checks if index is in dimension range.
	 *
	 * @param Dimension Dimension of the array.
	 * @param Index Index to check.
	 */
	FORCEINLINE void RangeCheck(uint8 Dimension, SizeType Index) const
	{
		DimensionCheck(Dimension);
		CheckInvariants();

		if (bIsChecked)
		{
			checkf((Index >= 0) & (Index < ArrayShape[Dimension]), TEXT("MultiArray index out of bounds: %lld from a dimension of size %lld"), (long long)Index, (long long)ArrayShape[Dimension]); // & for one branch
		}
	}

	/**
	 * Checks if a slice range [Index, Index+InNum) is in dimension range.
	 * Length is 0 is allowed on empty dimensions; Index must be 0 in that case.
	 *
	 * @param Dimension Dimension of the array.
	 * @param Index Starting index of the slice.
	 * @param InNum Length of the slice.
	 */
	FORCEINLINE void SliceRangeCheck(uint8 Dimension, SizeType Index, SizeType InNum) const
	{
		DimensionCheck(Dimension);

		if (bIsChecked)
		{
			checkf(Index >= 0, TEXT("Invalid index (%lld)"), (long long)Index);
			checkf(InNum >= 0, TEXT("Invalid count (%lld)"), (long long)InNum);
			checkf(Index + InNum <= ArrayShape[Dimension], TEXT("Range (index: %lld, count: %lld) lies outside the view of %lld elements"), (long long)Index, (long long)InNum, (long long)ArrayShape[Dimension]);
		}
	}

	/**
	 * Returns true if the array is empty and contains no elements.
	 *
	 * @returns True if the array is empty.
	 * @see Num
	 */
	bool IsEmpty() const
	{
		return Num() == 0;
	}

	/**
	 * Returns true if the dimension is empty and contains no elements.
	 *
	 * @param Dimension Dimension of the array.
	 * @returns True if the dimension is empty.
	 * @see Num
	 */
	bool IsEmpty(uint8 Dimension) const
	{
		DimensionCheck(Dimension);

		return ArrayShape[Dimension] == 0;
	}

	/**
	 * Returns the number of dimensions.
	 *
	 * @returns Number of dimensions in array.
	 */
	FORCEINLINE SizeType Rank() const
	{
		return DimNum;
	}

	/**
	 * Returns the total number of elements
	 *
	 * @returns Total number of elements in array.
	 */
	FORCEINLINE SizeType Num() const
	{
		return ArrayShape.Total();
	}

	/**
	 * Returns the number of elements in a dimension.
	 *
	 * @returns Number of elements in array.
	 */
	template<uint8 InDimIdx>
	FORCEINLINE SizeType Num() const
	{
		static_assert(InDimIdx < DimNum);
		return ArrayShape[InDimIdx];
	}

	/**
	 * Returns the number of elements in a dimension.
	 *
	 * @param Dimension Dimension of the array.
	 * @returns Number of elements in array.
	 */
	FORCEINLINE SizeType Num(uint8 Dimension) const
	{
		DimensionCheck(Dimension);

		return ArrayShape[Dimension];
	}

	/**
	 * Returns the number of elements in each dimension.
	 *
	 * @returns Number of elements in each dimension.
	 */
	FORCEINLINE TMultiArrayShape<DimNum, SizeType> Shape() const
	{
		return ArrayShape;
	}

	/**
	 * Returns the stride for a dimension.
	 *
	 * @param Dimension Dimension of the array.
	 * @returns Stride of that dimension.
	 */
	FORCEINLINE SizeType Stride(uint8 Dimension) const
	{
		DimensionCheck(Dimension);

		SizeType Total = 1;
		for (uint8 Idx = Dimension + 1; Idx < DimNum; Idx++)
		{
			Total *= ArrayShape[Idx];
		}
		return Total;
	}

	/**
	 * Returns a sliced view. Slicing outside of the range of the view is illegal.
	 *
	 * @param Index Starting index of the new view
	 * @param InNum Number of elements in the new view
	 * @returns Sliced view
	 */
	[[nodiscard]] FORCEINLINE TMultiArrayView Slice(SizeType Index, SizeType InNum) const
	{
		SliceRangeCheck(0, Index, InNum);
		TMultiArrayShape NewShape = ArrayShape;
		NewShape[0] = InNum;
		return TMultiArrayView(DataPtr + Index * Stride(0), NewShape);
	}

	/**
	 * Checks if this array contains the element.
	 *
	 * @returns	True if found. False otherwise.
	 */
	template <typename ComparisonType>
	bool Contains(const ComparisonType& Item) const
	{
		for (const ElementType* RESTRICT Data = GetData(), *RESTRICT DataEnd = Data + Num(); Data != DataEnd; ++Data)
		{
			if (*Data == Item)
			{
				return true;
			}
		}
		return false;
	}

public:

	/** Implicit cast for Checked and Restrict. */
	FORCEINLINE operator TMultiArrayView<DimNum, ElementType, true, false, SizeType>()
	{
		return TMultiArrayView<DimNum, ElementType, true, false, SizeType>(DataPtr, ArrayShape);
	}

	/** Implicit cast for Checked and Restrict. */
	FORCEINLINE operator TMultiArrayView<DimNum, ElementType, false, false, SizeType>()
	{
		return TMultiArrayView<DimNum, ElementType, false, false, SizeType>(DataPtr, ArrayShape);
	}

	/** Implicit cast for Checked and Restrict. */
	FORCEINLINE operator TMultiArrayView<DimNum, ElementType, true, true, SizeType>()
	{
		return TMultiArrayView<DimNum, ElementType, true, true, SizeType>(DataPtr, ArrayShape);
	}

	/** Implicit cast for Checked and Restrict. */
	FORCEINLINE operator TMultiArrayView<DimNum, ElementType, false, true, SizeType>()
	{
		return TMultiArrayView<DimNum, ElementType, false, true, SizeType>(DataPtr, ArrayShape);
	}

	/** Implicit cast for Checked and Restrict. */
	FORCEINLINE operator TMultiArrayView<DimNum, const ElementType, true, false, SizeType>() const
	{
		return TMultiArrayView<DimNum, const ElementType, true, false, SizeType>(DataPtr, ArrayShape);
	}

	/** Implicit cast for Checked and Restrict. */
	FORCEINLINE operator TMultiArrayView<DimNum, const ElementType, false, false, SizeType>() const
	{
		return TMultiArrayView<DimNum, const ElementType, false, false, SizeType>(DataPtr, ArrayShape);
	}

	/** Implicit cast for Checked and Restrict. */
	FORCEINLINE operator TMultiArrayView<DimNum, const ElementType, true, true, SizeType>() const
	{
		return TMultiArrayView<DimNum, const ElementType, true, true, SizeType>(DataPtr, ArrayShape);
	}

	/** Implicit cast for Checked and Restrict. */
	FORCEINLINE operator TMultiArrayView<DimNum, const ElementType, false, true, SizeType>() const
	{
		return TMultiArrayView<DimNum, const ElementType, false, true, SizeType>(DataPtr, ArrayShape);
	}


private:

	PointerType DataPtr;
	TMultiArrayShape<DimNum, SizeType> ArrayShape;
};


/**
* Specialization for single dimensional MultiArrayView
*/
template<typename InElementType, bool bInIsChecked, bool bInIsRestrict, typename InSizeType>
class TMultiArrayView<1, InElementType, bInIsChecked, bInIsRestrict, InSizeType>
{
public:
	static constexpr uint8 DimNum = 1;
	using ElementType = InElementType;
	static constexpr bool bIsChecked = bInIsChecked;
	static constexpr bool bIsRestrict = bInIsRestrict;
	using SizeType = InSizeType;

	using PointerType = std::conditional_t<
		bIsRestrict,
		ElementType* RESTRICT,
		ElementType*>;

	static_assert(std::is_signed_v<SizeType>, "TMultiArrayView only supports signed index types");

	/**
	 * Constructor.
	 */
	TMultiArrayView() : DataPtr(nullptr) { }

private:
	template <typename T>
	using TIsCompatibleRangeType = UE::MultiArrayView::Private::TIsCompatibleRangeType<T, ElementType>;

	template <typename T>
	using TIsReinterpretableRangeType = UE::MultiArrayView::Private::TIsReinterpretableRangeType<T, ElementType>;

public:
	/**
	 * Constructor from another range
	 *
	 * @param Other The source range to copy
	 */
	template <
		typename OtherRangeType,
		typename CVUnqualifiedOtherRangeType = std::remove_cv_t<std::remove_reference_t<OtherRangeType>>
		UE_REQUIRES(
			TAnd<
				TIsContiguousContainer<CVUnqualifiedOtherRangeType>,
				TOr<
					TIsCompatibleRangeType<OtherRangeType>,
					TIsReinterpretableRangeType<OtherRangeType>
				>
			>::Value
		)
	>
	FORCEINLINE TMultiArrayView(OtherRangeType&& Other)
		: DataPtr(std::conditional_t<
			TIsCompatibleRangeType<OtherRangeType>::Value,
			TIsCompatibleRangeType<OtherRangeType>,
			TIsReinterpretableRangeType<OtherRangeType>
		>::GetData(Forward<OtherRangeType>(Other)))
	{
		const auto InCount = GetNum(Forward<OtherRangeType>(Other));
		check((InCount >= 0) && ((sizeof(InCount) < sizeof(SizeType)) || (InCount <= static_cast<decltype(InCount)>(TNumericLimits<SizeType>::Max()))));
		ArrayShape[0] = (SizeType)InCount;
	}

	/**
	 * Construct a view of an arbitrary pointer
	 *
	 * @param InData	The data to view
	 * @param InNums	The number of elements on each dimension
	 */
	FORCEINLINE TMultiArrayView(ElementType* InData, TMultiArrayShape<DimNum, SizeType> InShape)
		: DataPtr(InData)
		, ArrayShape(InShape)
	{
		CheckInvariants();
	}

	/**
	 * Construct a view of an arbitrary pointer
	 *
	 * @param InData	The data to view
	 * @param InNum	The number of elements
	 */
	template <
		typename OtherElementType
		UE_REQUIRES(UE::MultiArrayView::Private::TIsCompatibleElementType_V<OtherElementType, ElementType>)
	>
	FORCEINLINE TMultiArrayView(OtherElementType* InData, SizeType InNum)
		: DataPtr(InData)
	{
		ArrayShape[0] = InNum;
		CheckInvariants();
	}

	/**
	 * Construct a view of an initializer list.
	 *
	 * The caller is responsible for ensuring that the view does not outlive the initializer list.
	 */
	FORCEINLINE TMultiArrayView(std::initializer_list<ElementType> List)
		: DataPtr(UE::MultiArrayView::Private::GetDataHelper(List))
	{
		ArrayShape[0] = GetNum(List);
		CheckInvariants();
	}

public:

	/**
	 * Array bracket operator. Returns a reference to the element at given index.
	 *
	 * @returns Reference to indexed element.
	 */
	FORCEINLINE ElementType& operator[](SizeType Index) const
	{
		RangeCheck(0, Index);
		return DataPtr[Index];
	}

	/** Implicit cast to TArrayView. */
	FORCEINLINE operator TArrayView<ElementType, SizeType>() const
	{
		return TArrayView(DataPtr, ArrayShape[0]);
	}

public:

	/**
	 * Helper function for returning a typed pointer to the first array entry.
	 *
	 * @returns Pointer to first array entry or nullptr if empty
	 */
	FORCEINLINE PointerType GetData()
	{
		return DataPtr;
	}

	/**
	 * Helper function for returning a typed pointer to the first array entry.
	 *
	 * @returns Pointer to first array entry or nullptr if empty
	 */
	FORCEINLINE const PointerType GetData() const
	{
		return DataPtr;
	}

	/**
	 * DO NOT USE DIRECTLY
	 * STL-like iterators to enable range-based for loop support.
	 */
	FORCEINLINE ElementType* begin() const { return GetData(); }
	FORCEINLINE ElementType* end() const { return GetData() + Num(); }

	/**
	 * Helper function returning the size of the inner type.
	 *
	 * @returns Size in bytes of array type.
	 */
	FORCEINLINE static constexpr size_t GetTypeSize()
	{
		return sizeof(ElementType);
	}

	/**
	 * Helper function returning the alignment of the inner type.
	 */
	FORCEINLINE static constexpr size_t GetTypeAlignment()
	{
		return alignof(ElementType);
	}

	/**
	 * Checks array invariants: if array size is greater than zero on each dimension
	 */
	FORCEINLINE void CheckInvariants() const
	{
		if (bIsChecked)
		{
			for (SizeType Idx = 0; Idx < DimNum; Idx++)
			{
				checkSlow(ArrayShape[Idx] >= 0);
			}
		}
	}

	/**
	 * Checks if a dimension is within the allowed number of dimensions
	 *
	 * @param Dimension Dimension of the array.
	 */
	FORCEINLINE void DimensionCheck(uint8 Dimension) const
	{
		if (bIsChecked)
		{
			checkf((Dimension < DimNum), TEXT("MultiArray dimension out of bounds: %lld from a rank of %lld"), (long long)Dimension, (long long)DimNum); // & for one branch
		}
	}

	/**
	 * Checks if index is in dimension range.
	 *
	 * @param Dimension Dimension of the array.
	 * @param Index Index to check.
	 */
	FORCEINLINE void RangeCheck(uint8 Dimension, SizeType Index) const
	{
		DimensionCheck(Dimension);
		CheckInvariants();

		if (bIsChecked)
		{
			checkf((Index >= 0) & (Index < ArrayShape[Dimension]), TEXT("MultiArray index out of bounds: %lld from a dimension of size %lld"), (long long)Index, (long long)ArrayShape[Dimension]); // & for one branch
		}
	}

	/**
	 * Checks if a slice range [Index, Index+InNum) is in dimension range.
	 * Length is 0 is allowed on empty dimensions; Index must be 0 in that case.
	 *
	 * @param Dimension Dimension of the array.
	 * @param Index Starting index of the slice.
	 * @param InNum Length of the slice.
	 */
	FORCEINLINE void SliceRangeCheck(uint8 Dimension, SizeType Index, SizeType InNum) const
	{
		DimensionCheck(Dimension);

		if (bIsChecked)
		{
			checkf(Index >= 0, TEXT("Invalid index (%lld)"), (long long)Index);
			checkf(InNum >= 0, TEXT("Invalid count (%lld)"), (long long)InNum);
			checkf(Index + InNum <= ArrayShape[Dimension], TEXT("Range (index: %lld, count: %lld) lies outside the view of %lld elements"), (long long)Index, (long long)InNum, (long long)ArrayShape[Dimension]);
		}
	}

	/**
	 * Returns true if the array is empty and contains no elements.
	 *
	 * @returns True if the array is empty.
	 * @see Num
	 */
	bool IsEmpty() const
	{
		return Num() == 0;
	}

	/**
	 * Returns true if the dimension is empty and contains no elements.
	 *
	 * @param Dimension Dimension of the array.
	 * @returns True if the dimension is empty.
	 * @see Num
	 */
	bool IsEmpty(uint8 Dimension) const
	{
		DimensionCheck(Dimension);

		return ArrayShape[Dimension] == 0;
	}

	/**
	 * Returns the number of dimensions.
	 *
	 * @returns Number of dimensions in array.
	 */
	FORCEINLINE SizeType Rank() const
	{
		return DimNum;
	}

	/**
	 * Returns the total number of elements
	 *
	 * @returns Total number of elements in array.
	 */
	FORCEINLINE SizeType Num() const
	{
		return ArrayShape.Total();
	}

	/**
	 * Returns the number of elements in a dimension.
	 *
	 * @returns Number of elements in array.
	 */
	template<uint8 InDimIdx>
	FORCEINLINE SizeType Num() const
	{
		static_assert(InDimIdx < DimNum);
		return ArrayShape[InDimIdx];
	}

	/**
	 * Returns the number of elements in a dimension.
	 *
	 * @param Dimension Dimension of the array.
	 * @returns Number of elements in array.
	 */
	FORCEINLINE SizeType Num(uint8 Dimension) const
	{
		DimensionCheck(Dimension);

		return ArrayShape[Dimension];
	}

	/**
	 * Returns the number of elements in each dimension.
	 *
	 * @returns Number of elements in each dimension.
	 */
	FORCEINLINE TMultiArrayShape<DimNum, SizeType> Shape() const
	{
		return ArrayShape;
	}

	/**
	 * Returns the stride for a dimension.
	 *
	 * @param Dimension Dimension of the array.
	 * @returns Stride of that dimension.
	 */
	FORCEINLINE SizeType Stride(uint8 Dimension) const
	{
		DimensionCheck(Dimension);

		SizeType Total = 1;
		for (uint8 Idx = Dimension + 1; Idx < DimNum; Idx++)
		{
			Total *= ArrayShape[Idx];
		}
		return Total;
	}

	/**
	 * Returns a sliced view. Slicing outside of the range of the view is illegal.
	 *
	 * @param Index Starting index of the new view
	 * @param InNum Number of elements in the new view
	 * @returns Sliced view
	 */
	[[nodiscard]] FORCEINLINE TMultiArrayView Slice(SizeType Index, SizeType InNum) const
	{
		SliceRangeCheck(0, Index, InNum);
		TMultiArrayShape NewShape = ArrayShape;
		NewShape[0] = InNum;
		return TMultiArrayView(DataPtr + Index * Stride(0), NewShape);
	}

	/**
	 * Checks if this array contains the element.
	 *
	 * @returns	True if found. False otherwise.
	 */
	template <typename ComparisonType>
	bool Contains(const ComparisonType& Item) const
	{
		for (const ElementType* RESTRICT Data = GetData(), *RESTRICT DataEnd = Data + Num(); Data != DataEnd; ++Data)
		{
			if (*Data == Item)
			{
				return true;
			}
		}
		return false;
	}

public:

	/** Implicit cast for Checked and Restrict. */
	FORCEINLINE operator TMultiArrayView<DimNum, ElementType, true, false, SizeType>()
	{
		return TMultiArrayView<DimNum, ElementType, true, false, SizeType>(DataPtr, ArrayShape);
	}

	/** Implicit cast for Checked and Restrict. */
	FORCEINLINE operator TMultiArrayView<DimNum, ElementType, false, false, SizeType>()
	{
		return TMultiArrayView<DimNum, ElementType, false, false, SizeType>(DataPtr, ArrayShape);
	}

	/** Implicit cast for Checked and Restrict. */
	FORCEINLINE operator TMultiArrayView<DimNum, ElementType, true, true, SizeType>()
	{
		return TMultiArrayView<DimNum, ElementType, true, true, SizeType>(DataPtr, ArrayShape);
	}

	/** Implicit cast for Checked and Restrict. */
	FORCEINLINE operator TMultiArrayView<DimNum, ElementType, false, true, SizeType>()
	{
		return TMultiArrayView<DimNum, ElementType, false, true, SizeType>(DataPtr, ArrayShape);
	}

	/** Implicit cast for Checked and Restrict. */
	FORCEINLINE operator TMultiArrayView<DimNum, const ElementType, true, false, SizeType>() const
	{
		return TMultiArrayView<DimNum, const ElementType, true, false, SizeType>(DataPtr, ArrayShape);
	}

	/** Implicit cast for Checked and Restrict. */
	FORCEINLINE operator TMultiArrayView<DimNum, const ElementType, false, false, SizeType>() const
	{
		return TMultiArrayView<DimNum, const ElementType, false, false, SizeType>(DataPtr, ArrayShape);
	}

	/** Implicit cast for Checked and Restrict. */
	FORCEINLINE operator TMultiArrayView<DimNum, const ElementType, true, true, SizeType>() const
	{
		return TMultiArrayView<DimNum, const ElementType, true, true, SizeType>(DataPtr, ArrayShape);
	}

	/** Implicit cast for Checked and Restrict. */
	FORCEINLINE operator TMultiArrayView<DimNum, const ElementType, false, true, SizeType>() const
	{
		return TMultiArrayView<DimNum, const ElementType, false, true, SizeType>(DataPtr, ArrayShape);
	}

private:

	PointerType DataPtr;
	TMultiArrayShape<DimNum, SizeType> ArrayShape;
};

template<uint8 InDimNum, typename InElementType, bool bInIsChecked = true, bool bInIsRestrict = false, typename InSizeType = FDefaultAllocator::SizeType>
using TConstMultiArrayView = TMultiArrayView<InDimNum, const InElementType, bInIsChecked, bInIsRestrict, InSizeType>;

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "Templates/IsConst.h"
#include "Templates/IsSigned.h"
#include "Templates/PointerIsConvertibleFromTo.h"
#endif