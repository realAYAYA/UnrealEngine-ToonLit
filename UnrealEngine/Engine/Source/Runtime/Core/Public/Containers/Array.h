// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Misc/ReverseIterate.h"
#include "HAL/UnrealMemory.h"
#include "Templates/UnrealTypeTraits.h"
#include "Templates/UnrealTemplate.h"
#include "Containers/AllowShrinking.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/ContainerElementTypeCompatibility.h"
#include "Serialization/Archive.h"
#include "Serialization/MemoryImageWriter.h"

#include "Algo/Heapify.h"
#include "Algo/HeapSort.h"
#include "Algo/IsHeap.h"
#include "Algo/Impl/BinaryHeap.h"
#include "Algo/StableSort.h"
#include "Concepts/GetTypeHashable.h"
#include "Templates/IdentityFunctor.h"
#include "Templates/Invoke.h"
#include "Templates/Less.h"
#include "Templates/Sorting.h"
#include "Templates/AlignmentTemplates.h"
#include "Traits/ElementType.h"

#include <limits>
#include <type_traits>

#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	#define TARRAY_RANGED_FOR_CHECKS 0
#else
	#define TARRAY_RANGED_FOR_CHECKS 1
#endif

template <typename T>
struct TCanBulkSerialize
{
	enum { Value = std::is_arithmetic_v<T> };
};

// Forward declarations

template <typename T, typename AllocatorType> inline void* operator new(size_t Size, TArray<T, AllocatorType>& Array);
template <typename T, typename AllocatorType> inline void* operator new(size_t Size, TArray<T, AllocatorType>& Array, typename TArray<T, AllocatorType>::SizeType Index);

/**
 * Generic iterator which can operate on types that expose the following:
 * - A type called ElementType representing the contained type.
 * - A method SizeType Num() const that returns the number of items in the container.
 * - A method bool IsValidIndex(SizeType index) which returns whether a given index is valid in the container.
 * - A method T& operator\[\](SizeType index) which returns a reference to a contained object by index.
 * - A method void RemoveAt(SizeType index) which removes the element at index
 */
template <typename ContainerType, typename ElementType, typename SizeType>
class TIndexedContainerIterator
{
public:
	TIndexedContainerIterator(ContainerType& InContainer, SizeType StartIndex = 0)
		: Container(InContainer)
		, Index    (StartIndex)
	{
	}

	/** Advances iterator to the next element in the container. */
	TIndexedContainerIterator& operator++()
	{
		++Index;
		return *this;
	}
	TIndexedContainerIterator operator++(int)
	{
		TIndexedContainerIterator Tmp(*this);
		++Index;
		return Tmp;
	}

	/** Moves iterator to the previous element in the container. */
	TIndexedContainerIterator& operator--()
	{
		--Index;
		return *this;
	}
	TIndexedContainerIterator operator--(int)
	{
		TIndexedContainerIterator Tmp(*this);
		--Index;
		return Tmp;
	}

	/** iterator arithmetic support */
	TIndexedContainerIterator& operator+=(SizeType Offset)
	{
		Index += Offset;
		return *this;
	}

	TIndexedContainerIterator operator+(SizeType Offset) const
	{
		TIndexedContainerIterator Tmp(*this);
		return Tmp += Offset;
	}

	TIndexedContainerIterator& operator-=(SizeType Offset)
	{
		return *this += -Offset;
	}

	TIndexedContainerIterator operator-(SizeType Offset) const
	{
		TIndexedContainerIterator Tmp(*this);
		return Tmp -= Offset;
	}

	FORCEINLINE ElementType& operator* () const
	{
		return Container[ Index ];
	}

	FORCEINLINE ElementType* operator->() const
	{
		return &Container[ Index ];
	}

	/** conversion to "bool" returning true if the iterator has not reached the last element. */
	FORCEINLINE explicit operator bool() const
	{
		return Container.IsValidIndex(Index);
	}

	/** Returns an index to the current element. */
	SizeType GetIndex() const
	{
		return Index;
	}

	/** Resets the iterator to the first element. */
	void Reset()
	{
		Index = 0;
	}

	/** Sets the iterator to one past the last element. */
	void SetToEnd()
	{
		Index = Container.Num();
	}
	
	/** Removes current element in array. This invalidates the current iterator value and it must be incremented */
	void RemoveCurrent()
	{
		Container.RemoveAt(Index);
		Index--;
	}

	/**
	 * Removes current element in array by swapping it with the end element and popping it from the end.
	 * This invalidates the current iterator value and it must be incremented.
	 * Note this modifies the order of the remaining elements in the array.
	 */
	void RemoveCurrentSwap()
	{
		Container.RemoveAtSwap(Index);
		Index--;
	}

	FORCEINLINE bool operator==(const TIndexedContainerIterator& Rhs) const { return &Container == &Rhs.Container && Index == Rhs.Index; }
	FORCEINLINE bool operator!=(const TIndexedContainerIterator& Rhs) const { return &Container != &Rhs.Container || Index != Rhs.Index; }

private:

	ContainerType& Container;
	SizeType      Index;
};


/** operator + */
template <typename ContainerType, typename ElementType, typename SizeType>
FORCEINLINE TIndexedContainerIterator<ContainerType, ElementType, SizeType> operator+(SizeType Offset, TIndexedContainerIterator<ContainerType, ElementType, SizeType> RHS)
{
	return RHS + Offset;
}


#if TARRAY_RANGED_FOR_CHECKS
	/**
	 * Pointer-like iterator type for ranged-for loops which checks that the
	 * container hasn't been resized during iteration.
	 */
	template <typename ElementType, typename SizeType, bool bReverse = false>
	struct TCheckedPointerIterator
	{
		// This iterator type only supports the minimal functionality needed to support
		// C++ ranged-for syntax.  For example, it does not provide post-increment ++ nor ==.
		//
		// We do add an operator-- to help FString implementation

		explicit TCheckedPointerIterator(const SizeType& InNum, ElementType* InPtr)
			: Ptr       (InPtr)
			, CurrentNum(InNum)
			, InitialNum(InNum)
		{
		}

		FORCEINLINE ElementType& operator*() const
		{
			if constexpr (bReverse)
			{
				return *(Ptr - 1);
			}
			else
			{
				return *Ptr;
			}
		}

		FORCEINLINE TCheckedPointerIterator& operator++()
		{
			if constexpr (bReverse)
			{
				--Ptr;
			}
			else
			{
				++Ptr;
			}
			return *this;
		}

		FORCEINLINE TCheckedPointerIterator& operator--()
		{
			if constexpr (bReverse)
			{
				++Ptr;
			}
			else
			{
				--Ptr;
			}
			return *this;
		}

		FORCEINLINE bool operator!=(const TCheckedPointerIterator& Rhs) const
		{
			// We only need to do the check in this operator, because no other operator will be
			// called until after this one returns.
			//
			// Also, we should only need to check one side of this comparison - if the other iterator isn't
			// even from the same array then the compiler has generated bad code.
			ensureMsgf(CurrentNum == InitialNum, TEXT("Array has changed during ranged-for iteration!"));
			return Ptr != Rhs.Ptr;
		}

	private:
		ElementType*    Ptr;
		const SizeType& CurrentNum;
		SizeType        InitialNum;
	};
#endif


template <typename ElementType, typename IteratorType>
struct TDereferencingIterator
{
	explicit TDereferencingIterator(IteratorType InIter)
		: Iter(InIter)
	{
	}

	FORCEINLINE ElementType& operator*() const
	{
		return *(ElementType*)*Iter;
	}

	FORCEINLINE TDereferencingIterator& operator++()
	{
		++Iter;
		return *this;
	}

	FORCEINLINE bool operator!=(const TDereferencingIterator& Rhs) const
	{
		return Iter != Rhs.Iter;
	}

private:
	IteratorType Iter;
};

namespace UE4Array_Private
{
	// Simply forwards to an unqualified GetData(), but can be called from within a container or view
	// where GetData() is already a member and so hides any others.
	template <typename T>
	FORCEINLINE decltype(auto) GetDataHelper(T&& Arg)
	{
		return GetData(Forward<T>(Arg));
	}

	template <typename FromArrayType, typename ToArrayType>
	constexpr bool CanMoveTArrayPointersBetweenArrayTypes()
	{
		typedef typename FromArrayType::AllocatorType FromAllocatorType;
		typedef typename ToArrayType::AllocatorType ToAllocatorType;
		typedef typename FromArrayType::ElementType   FromElementType;
		typedef typename ToArrayType::ElementType   ToElementType;

		// Allocators must be equal or move-compatible...
		if constexpr (std::is_same_v<FromAllocatorType, ToAllocatorType> || TCanMoveBetweenAllocators<FromAllocatorType, ToAllocatorType>::Value)
		{
			return
				std::is_same_v         <ToElementType, FromElementType> ||      // The element type of the container must be the same, or...
				TIsBitwiseConstructible<ToElementType, FromElementType>::Value; // ... the element type of the source container must be bitwise constructible from the element type in the destination container
		}
		else
		{
			return false;
		}
	}

	// Assume elements are compatible with themselves - avoids problems with generated copy
	// constructors of arrays of forwarded types, e.g.:
	//
	// struct FThing;
	//
	// struct FOuter
	// {
	//     TArray<FThing> Arr; // this will cause errors without this workaround
	// };
	template <typename DestType, typename SourceType>
	constexpr bool TArrayElementsAreCompatible_V = std::disjunction_v<std::is_same<DestType, std::decay_t<SourceType>>, std::is_constructible<DestType, SourceType>>;

	template <typename ElementType, typename AllocatorType>
	static char (&ResolveIsTArrayPtr(const volatile TArray<ElementType, AllocatorType>*))[2];

	static char(&ResolveIsTArrayPtr(...))[1];

	template <typename T>
	constexpr bool TIsTArrayOrDerivedFromTArray_V = sizeof(ResolveIsTArrayPtr((T*)nullptr)) == 2;
}

namespace UE::Core::Private
{
	[[noreturn]] CORE_API void OnInvalidArrayNum(unsigned long long NewNum);
}


/**
 * Templated dynamic array
 *
 * A dynamically sized array of typed elements.  Makes the assumption that your elements are relocate-able; 
 * i.e. that they can be transparently moved to new memory without a copy constructor.  The main implication 
 * is that pointers to elements in the TArray may be invalidated by adding or removing other elements to the array. 
 * Removal of elements is O(N) and invalidates the indices of subsequent elements.
 *
 * Caution: as noted below some methods are not safe for element types that require constructors.
 *
 **/
template<typename InElementType, typename InAllocatorType>
class TArray
{
	template <typename OtherInElementType, typename OtherAllocator>
	friend class TArray;

public:
	typedef typename InAllocatorType::SizeType SizeType;
	typedef InElementType ElementType;
	typedef InAllocatorType AllocatorType;

private:
	using USizeType = typename std::make_unsigned_t<SizeType>;

public:
	using ElementAllocatorType = std::conditional_t<
		AllocatorType::NeedsElementType,
		typename AllocatorType::template ForElementType<ElementType>,
		typename AllocatorType::ForAnyElementType
	>;

	static_assert(std::is_signed_v<SizeType>, "TArray only supports signed index types");

	/**
	 * Constructor, initializes element number counters.
	 */
	FORCEINLINE TArray()
		: ArrayNum(0)
		, ArrayMax(AllocatorInstance.GetInitialCapacity())
	{}

	/**
	 * Constructor from a raw array of elements.
	 *
	 * @param Ptr   A pointer to an array of elements to copy.
	 * @param Count The number of elements to copy from Ptr.
	 * @see Append
	 */
	FORCEINLINE TArray(const ElementType* Ptr, SizeType Count)
	{
		if (Count < 0)
		{
			// Cast to USizeType first to prevent sign extension on negative sizes, producing unusually large values.
			UE::Core::Private::OnInvalidArrayNum((unsigned long long)(USizeType)Count);
		}

		check(Ptr != nullptr || Count == 0);

		CopyToEmpty(Ptr, Count, 0);
	}

	template <typename OtherElementType, typename OtherSizeType>
	explicit TArray(const TArrayView<OtherElementType, OtherSizeType>& Other);

	/**
	 * Initializer list constructor
	 */
	TArray(std::initializer_list<InElementType> InitList)
	{
		// This is not strictly legal, as std::initializer_list's iterators are not guaranteed to be pointers, but
		// this appears to be the case on all of our implementations.  Also, if it's not true on a new implementation,
		// it will fail to compile rather than behave badly.
		CopyToEmpty(InitList.begin(), (SizeType)InitList.size(), 0);
	}

	/**
	 * Copy constructor with changed allocator. Use the common routine to perform the copy.
	 *
	 * @param Other The source array to copy.
	 */
	template <
		typename OtherElementType,
		typename OtherAllocator
		UE_REQUIRES(UE4Array_Private::TArrayElementsAreCompatible_V<ElementType, const OtherElementType&>)
	>
	FORCEINLINE explicit TArray(const TArray<OtherElementType, OtherAllocator>& Other)
	{
		CopyToEmpty(Other.GetData(), Other.Num(), 0);
	}

	/**
	 * Copy constructor. Use the common routine to perform the copy.
	 *
	 * @param Other The source array to copy.
	 */
	FORCEINLINE TArray(const TArray& Other)
	{
		CopyToEmpty(Other.GetData(), Other.Num(), 0);
	}

	/**
	 * Copy constructor. Use the common routine to perform the copy.
	 *
	 * @param Other The source array to copy.
	 * @param ExtraSlack Tells how much extra memory should be preallocated
	 *                   at the end of the array in the number of elements.
	 */
	FORCEINLINE TArray(const TArray& Other, SizeType ExtraSlack)
	{
		CopyToEmptyWithSlack(Other.GetData(), Other.Num(), 0, ExtraSlack);
	}

	/**
	 * Initializer list assignment operator. First deletes all currently contained elements
	 * and then copies from initializer list.
	 *
	 * @param InitList The initializer_list to copy from.
	 */
	TArray& operator=(std::initializer_list<InElementType> InitList)
	{
		DestructItems(GetData(), ArrayNum);
		// This is not strictly legal, as std::initializer_list's iterators are not guaranteed to be pointers, but
		// this appears to be the case on all of our implementations.  Also, if it's not true on a new implementation,
		// it will fail to compile rather than behave badly.
		CopyToEmpty(InitList.begin(), (SizeType)InitList.size(), ArrayMax);
		return *this;
	}

	/**
	 * Assignment operator. First deletes all currently contained elements
	 * and then copies from other array.
	 *
	 * AllocatorType changing version.
	 *
	 * @param Other The source array to assign from.
	 */
	template<typename OtherAllocatorType>
	TArray& operator=(const TArray<ElementType, OtherAllocatorType>& Other)
	{
		DestructItems(GetData(), ArrayNum);
		CopyToEmpty(Other.GetData(), Other.Num(), ArrayMax);
		return *this;
	}

	/**
	 * Assignment operator. First deletes all currently contained elements
	 * and then copies from other array.
	 *
	 * @param Other The source array to assign from.
	 */
	TArray& operator=(const TArray& Other)
	{
		if (this != &Other)
		{
			DestructItems(GetData(), ArrayNum);
			CopyToEmpty(Other.GetData(), Other.Num(), ArrayMax);
		}
		return *this;
	}

	template <typename OtherElementType, typename OtherSizeType>
	TArray& operator=(const TArrayView<OtherElementType, OtherSizeType>& Other);

private:

	FORCEINLINE void SlackTrackerNumChanged()
	{
#if UE_ENABLE_ARRAY_SLACK_TRACKING
		if constexpr (TAllocatorTraits<InAllocatorType>::SupportsSlackTracking)
		{
			AllocatorInstance.SlackTrackerLogNum(ArrayNum);
		}
#endif
	}

	/**
	 * Moves or copies array. Depends on the array type traits.
	 *
	 * @param ToArray Array to move into.
	 * @param FromArray Array to move from.
	 * @param PrevMax The previous allocated size.
	 */
	template <typename FromArrayType, typename ToArrayType>
	static FORCEINLINE void MoveOrCopy(ToArrayType& ToArray, FromArrayType& FromArray, SizeType PrevMax)
	{
		if constexpr (UE4Array_Private::CanMoveTArrayPointersBetweenArrayTypes<FromArrayType, ToArrayType>())
		{
			// Move

			static_assert(std::is_same_v<TArray, ToArrayType>, "MoveOrCopy is expected to be called with the current array type as the destination");

			using FromAllocatorType = typename FromArrayType::AllocatorType;
			using ToAllocatorType   = typename ToArrayType::AllocatorType;

			if constexpr (TCanMoveBetweenAllocators<FromAllocatorType, ToAllocatorType>::Value)
			{
				ToArray.AllocatorInstance.template MoveToEmptyFromOtherAllocator<FromAllocatorType>(FromArray.AllocatorInstance);
			}
			else
			{
				ToArray.AllocatorInstance.MoveToEmpty(FromArray.AllocatorInstance);
			}

			ToArray  .ArrayNum = (SizeType)FromArray.ArrayNum;
			ToArray  .ArrayMax = (SizeType)FromArray.ArrayMax;

			// Ensure the destination container could hold the source range (when the allocator size types shrink)
			if constexpr (sizeof(USizeType) < sizeof(typename FromArrayType::USizeType))
			{
				if (ToArray.ArrayNum != FromArray.ArrayNum || ToArray.ArrayMax != FromArray.ArrayMax)
				{
					// Cast to USizeType first to prevent sign extension on negative sizes, producing unusually large values.
					UE::Core::Private::OnInvalidArrayNum((unsigned long long)(USizeType)ToArray.ArrayNum);
				}
			}

			FromArray.ArrayNum = 0;
			FromArray.ArrayMax = FromArray.AllocatorInstance.GetInitialCapacity();

			FromArray.SlackTrackerNumChanged();
			ToArray.SlackTrackerNumChanged();
		}
		else
		{
			// Copy

			ToArray.CopyToEmpty(FromArray.GetData(), FromArray.Num(), PrevMax);
		}
	}

	/**
	 * Moves or copies array. Depends on the array type traits.
	 *
	 * @param ToArray Array to move into.
	 * @param FromArray Array to move from.
	 * @param PrevMax The previous allocated size.
	 * @param ExtraSlack Tells how much extra memory should be preallocated
	 *                   at the end of the array in the number of elements.
	 */
	template <typename FromArrayType, typename ToArrayType>
	static FORCEINLINE void MoveOrCopyWithSlack(ToArrayType& ToArray, FromArrayType& FromArray, SizeType PrevMax, SizeType ExtraSlack)
	{
		if constexpr (UE4Array_Private::CanMoveTArrayPointersBetweenArrayTypes<FromArrayType, ToArrayType>())
		{
			// Move

			MoveOrCopy(ToArray, FromArray, PrevMax);

			USizeType LocalArrayNum = (USizeType)ToArray.ArrayNum;
			USizeType NewMax        = (USizeType)LocalArrayNum + (USizeType)ExtraSlack;

			// This should only happen when we've underflowed or overflowed SizeType
			if ((SizeType)NewMax < (SizeType)LocalArrayNum)
			{
				UE::Core::Private::OnInvalidArrayNum((unsigned long long)ExtraSlack);
			}

			ToArray.Reserve(NewMax);
		}
		else
		{
			// Copy

			ToArray.CopyToEmptyWithSlack(FromArray.GetData(), FromArray.Num(), PrevMax, ExtraSlack);
		}
	}

public:
	/**
	 * Move constructor.
	 *
	 * @param Other Array to move from.
	 */
	FORCEINLINE TArray(TArray&& Other)
	{
		MoveOrCopy(*this, Other, 0);
	}

	/**
	 * Move constructor.
	 *
	 * @param Other Array to move from.
	 */
	template <
		typename OtherElementType,
		typename OtherAllocator
		UE_REQUIRES(UE4Array_Private::TArrayElementsAreCompatible_V<ElementType, OtherElementType&&>)
	>
	FORCEINLINE explicit TArray(TArray<OtherElementType, OtherAllocator>&& Other)
	{
		MoveOrCopy(*this, Other, 0);
	}

	/**
	 * Move constructor.
	 *
	 * @param Other Array to move from.
	 * @param ExtraSlack Tells how much extra memory should be preallocated
	 *                   at the end of the array in the number of elements.
	 */
	template <
		typename OtherElementType
		UE_REQUIRES(UE4Array_Private::TArrayElementsAreCompatible_V<ElementType, OtherElementType&&>)
	>
	TArray(TArray<OtherElementType, AllocatorType>&& Other, SizeType ExtraSlack)
	{
		MoveOrCopyWithSlack(*this, Other, 0, ExtraSlack);
	}

	/**
	 * Move assignment operator.
	 *
	 * @param Other Array to assign and move from.
	 */
	TArray& operator=(TArray&& Other)
	{
		if (this != &Other)
		{
			DestructItems(GetData(), ArrayNum);
			MoveOrCopy(*this, Other, ArrayMax);
		}
		return *this;
	}

	/** Destructor. */
	~TArray()
	{
		DestructItems(GetData(), ArrayNum);

		// note ArrayNum, ArrayMax and data pointer are not invalidated
		// they are left unchanged and use-after-destruct will see them the same as before destruct
	}

	/**
	 * Helper function for returning a typed pointer to the first array entry.
	 *
	 * @returns Pointer to first array entry or nullptr if ArrayMax == 0.
	 */
	FORCEINLINE ElementType* GetData() UE_LIFETIMEBOUND
	{
		return (ElementType*)AllocatorInstance.GetAllocation();
	}

	/**
	 * Helper function for returning a typed pointer to the first array entry.
	 *
	 * @returns Pointer to first array entry or nullptr if ArrayMax == 0.
	 */
	FORCEINLINE const ElementType* GetData() const UE_LIFETIMEBOUND
	{
		return (const ElementType*)AllocatorInstance.GetAllocation();
	}

	/** 
	 * Helper function returning the size of the inner type.
	 *
	 * @returns Size in bytes of array type.
	 */
	FORCEINLINE static constexpr uint32 GetTypeSize()
	{
		return sizeof(ElementType);
	}

	/** 
	 * Helper function to return the amount of memory allocated by this
	 * container.
	 * Only returns the size of allocations made directly by the container, not the elements themselves.
	 *
	 * @returns Number of bytes allocated by this container.
	 */
	FORCEINLINE SIZE_T GetAllocatedSize(void) const
	{
		return AllocatorInstance.GetAllocatedSize(ArrayMax, sizeof(ElementType));
	}

	/**
	 * Returns the amount of slack in this array in elements.
	 *
	 * @see Num, Shrink
	 */
	FORCEINLINE SizeType GetSlack() const
	{
		return ArrayMax - ArrayNum;
	}

	/**
	 * Checks array invariants: if array size is greater than or equal to zero and less
	 * than or equal to the maximum.
	 */
	FORCEINLINE void CheckInvariants() const
	{
		checkSlow((ArrayNum >= 0) & (ArrayMax >= ArrayNum)); // & for one branch
	}

	/**
	 * Checks if index is in array range.
	 *
	 * @param Index Index to check.
	 */
	FORCEINLINE void RangeCheck(SizeType Index) const
	{
		CheckInvariants();

		// Template property, branch will be optimized out
		if constexpr (AllocatorType::RequireRangeCheck)
		{
			checkf((Index >= 0) & (Index < ArrayNum),TEXT("Array index out of bounds: %lld into an array of size %lld"),(long long)Index, (long long)ArrayNum); // & for one branch
		}
	}

	/**
	 * Checks if a range of indices are in the array range.
	 *
	 * @param Index Index of the start of the range to check.
	 * @param Count Number of elements in the range.
	 */
	FORCEINLINE void RangeCheck(SizeType Index, SizeType Count) const
	{
		CheckInvariants();

		// Template property, branch will be optimized out
		if constexpr (AllocatorType::RequireRangeCheck)
		{
			checkf((Count >= 0) & (Index >= 0) & (Index + Count <= ArrayNum), TEXT("Array range out of bounds: index %lld and length %lld into an array of size %lld"), (long long)Index, (long long)Count, (long long)ArrayNum); // & for one branch
		}
	}

	/**
	 * Tests if index is valid, i.e. greater than or equal to zero, and less than the number of elements in the array.
	 *
	 * @param Index Index to test.
	 * @returns True if index is valid. False otherwise.
	 */
	FORCEINLINE bool IsValidIndex(SizeType Index) const
	{
		return Index >= 0 && Index < ArrayNum;
	}

	/**
	 * Returns true if the array is empty and contains no elements. 
	 *
	 * @returns True if the array is empty.
	 * @see Num
	 */
	bool IsEmpty() const
	{
		return ArrayNum == 0;
	}

	/**
	 * Returns number of elements in array.
	 *
	 * @returns Number of elements in array.
	 * @see GetSlack
	 */
	FORCEINLINE SizeType Num() const
	{
		return ArrayNum;
	}

	/**
	 * Returns maximum number of elements in array.
	 *
	 * @returns Maximum number of elements in array.
	 * @see GetSlack
	 */
	FORCEINLINE SizeType Max() const
	{
		return ArrayMax;
	}

	/**
	 * Array bracket operator. Returns reference to element at given index.
	 *
	 * @returns Reference to indexed element.
	 */
	FORCEINLINE ElementType& operator[](SizeType Index) UE_LIFETIMEBOUND
	{
		RangeCheck(Index);
		return GetData()[Index];
	}

	/**
	 * Array bracket operator. Returns reference to element at given index.
	 *
	 * Const version of the above.
	 *
	 * @returns Reference to indexed element.
	 */
	FORCEINLINE const ElementType& operator[](SizeType Index) const UE_LIFETIMEBOUND
	{
		RangeCheck(Index);
		return GetData()[Index];
	}

	/**
	 * Pops element from the array.
	 *
	 * @param AllowShrinking If this call allows shrinking of the array during element remove.
	 * @returns Popped element.
	 */
	ElementType Pop(EAllowShrinking AllowShrinking = EAllowShrinking::Yes)
	{
		RangeCheck(0);
		ElementType Result = MoveTempIfPossible(GetData()[ArrayNum - 1]);
		RemoveAtImpl(ArrayNum - 1);
		if (AllowShrinking == EAllowShrinking::Yes)
		{
			ResizeShrink();
		}
		return Result;
	}
	UE_ALLOWSHRINKING_BOOL_DEPRECATED("Pop")
	FORCEINLINE ElementType Pop(bool bAllowShrinking)
	{
		return Pop(bAllowShrinking ? EAllowShrinking::Yes : EAllowShrinking::No);
	}

	/**
	 * Pushes element into the array.
	 *
	 * @param Item Item to push.
	 */
	FORCEINLINE void Push(ElementType&& Item)
	{
		Add(MoveTempIfPossible(Item));
	}

	/**
	 * Pushes element into the array.
	 *
	 * Const ref version of the above.
	 *
	 * @param Item Item to push.
	 * @see Pop, Top
	 */
	FORCEINLINE void Push(const ElementType& Item)
	{
		Add(Item);
	}

	/**
	 * Returns the top element, i.e. the last one.
	 *
	 * @returns Reference to the top element.
	 * @see Pop, Push
	 */
	FORCEINLINE ElementType& Top()
	{
		return Last();
	}

	/**
	 * Returns the top element, i.e. the last one.
	 *
	 * Const version of the above.
	 *
	 * @returns Reference to the top element.
	 * @see Pop, Push
	 */
	FORCEINLINE const ElementType& Top() const
	{
		return Last();
	}

	/**
	 * Returns n-th last element from the array.
	 *
	 * @param IndexFromTheEnd (Optional) Index from the end of array (default = 0).
	 * @returns Reference to n-th last element from the array.
	 */
	FORCEINLINE ElementType& Last(SizeType IndexFromTheEnd = 0)
	{
		RangeCheck(ArrayNum - IndexFromTheEnd - 1);
		return GetData()[ArrayNum - IndexFromTheEnd - 1];
	}

	/**
	 * Returns n-th last element from the array.
	 *
	 * Const version of the above.
	 *
	 * @param IndexFromTheEnd (Optional) Index from the end of array (default = 0).
	 * @returns Reference to n-th last element from the array.
	 */
	FORCEINLINE const ElementType& Last(SizeType IndexFromTheEnd = 0) const
	{
		RangeCheck(ArrayNum - IndexFromTheEnd - 1);
		return GetData()[ArrayNum - IndexFromTheEnd - 1];
	}

	/**
	 * Shrinks the array's used memory to smallest possible to store elements currently in it.
	 *
	 * @see Slack
	 */
	FORCEINLINE void Shrink()
	{
		CheckInvariants();
		if (ArrayMax != ArrayNum)
		{
			ResizeTo(ArrayNum);
		}
	}

	/**
	 * Finds element within the array.
	 *
	 * @param Item Item to look for.
	 * @param Index Will contain the found index.
	 * @returns True if found. False otherwise.
	 * @see FindLast, FindLastByPredicate
	 */
	FORCEINLINE bool Find(const ElementType& Item, SizeType& Index) const
	{
		Index = this->Find(Item);
		return Index != INDEX_NONE;
	}

	/**
	 * Finds element within the array.
	 *
	 * @param Item Item to look for.
	 * @returns Index of the found element. INDEX_NONE otherwise.
	 * @see FindLast, FindLastByPredicate
	 */
	SizeType Find(const ElementType& Item) const
	{
		const ElementType* RESTRICT Start = GetData();
		for (const ElementType* RESTRICT Data = Start, *RESTRICT DataEnd = Data + ArrayNum; Data != DataEnd; ++Data)
		{
			if (*Data == Item)
			{
				return static_cast<SizeType>(Data - Start);
			}
		}
		return INDEX_NONE;
	}

	/**
	 * Finds element within the array starting from the end.
	 *
	 * @param Item Item to look for.
	 * @param Index Output parameter. Found index.
	 * @returns True if found. False otherwise.
	 * @see Find, FindLastByPredicate
	 */
	FORCEINLINE bool FindLast(const ElementType& Item, SizeType& Index) const
	{
		Index = this->FindLast(Item);
		return Index != INDEX_NONE;
	}

	/**
	 * Finds element within the array starting from the end.
	 *
	 * @param Item Item to look for.
	 * @returns Index of the found element. INDEX_NONE otherwise.
	 */
	SizeType FindLast(const ElementType& Item) const
	{
		for (const ElementType* RESTRICT Start = GetData(), *RESTRICT Data = Start + ArrayNum; Data != Start; )
		{
			--Data;
			if (*Data == Item)
			{
				return static_cast<SizeType>(Data - Start);
			}
		}
		return INDEX_NONE;
	}

	/**
	 * Searches an initial subrange of the array for the last occurrence of an element which matches the specified predicate.
	 *
	 * @param Pred Predicate taking array element and returns true if element matches search criteria, false otherwise.
	 * @param Count The number of elements from the front of the array through which to search.
	 * @returns Index of the found element. INDEX_NONE otherwise.
	 */
	template <typename Predicate>
	SizeType FindLastByPredicate(Predicate Pred, SizeType Count) const
	{
		check(Count >= 0 && Count <= this->Num());
		for (const ElementType* RESTRICT Start = GetData(), *RESTRICT Data = Start + Count; Data != Start; )
		{
			--Data;
			if (::Invoke(Pred, *Data))
			{
				return static_cast<SizeType>(Data - Start);
			}
		}
		return INDEX_NONE;
	}

	/**
	 * Searches the array for the last occurrence of an element which matches the specified predicate.
	 *
	 * @param Pred Predicate taking array element and returns true if element matches search criteria, false otherwise.
	 * @returns Index of the found element. INDEX_NONE otherwise.
	 */
	template <typename Predicate>
	FORCEINLINE SizeType FindLastByPredicate(Predicate Pred) const
	{
		return FindLastByPredicate(Pred, ArrayNum);
	}

	/**
	 * Finds an item by key (assuming the ElementType overloads operator== for
	 * the comparison).
	 *
	 * @param Key The key to search by.
	 * @returns Index to the first matching element, or INDEX_NONE if none is found.
	 */
	template <typename KeyType>
	SizeType IndexOfByKey(const KeyType& Key) const
	{
		const ElementType* RESTRICT Start = GetData();
		for (const ElementType* RESTRICT Data = Start, *RESTRICT DataEnd = Start + ArrayNum; Data != DataEnd; ++Data)
		{
			if (*Data == Key)
			{
				return static_cast<SizeType>(Data - Start);
			}
		}
		return INDEX_NONE;
	}

	/**
	 * Finds an item by predicate.
	 *
	 * @param Pred The predicate to match.
	 * @returns Index to the first matching element, or INDEX_NONE if none is found.
	 */
	template <typename Predicate>
	SizeType IndexOfByPredicate(Predicate Pred) const
	{
		const ElementType* RESTRICT Start = GetData();
		for (const ElementType* RESTRICT Data = Start, *RESTRICT DataEnd = Start + ArrayNum; Data != DataEnd; ++Data)
		{
			if (::Invoke(Pred, *Data))
			{
				return static_cast<SizeType>(Data - Start);
			}
		}
		return INDEX_NONE;
	}

	/**
	 * Finds an item by key (assuming the ElementType overloads operator== for
	 * the comparison).
	 *
	 * @param Key The key to search by.
	 * @returns Pointer to the first matching element, or nullptr if none is found.
	 * @see Find
	 */
	template <typename KeyType>
	FORCEINLINE const ElementType* FindByKey(const KeyType& Key) const
	{
		return const_cast<TArray*>(this)->FindByKey(Key);
	}

	/**
	 * Finds an item by key (assuming the ElementType overloads operator== for
	 * the comparison). Time Complexity: O(n), starts iteration from the beginning so better performance if Key is in the front
	 *
	 * @param Key The key to search by.
	 * @returns Pointer to the first matching element, or nullptr if none is found.
	 * @see Find
	 */
	template <typename KeyType>
	ElementType* FindByKey(const KeyType& Key)
	{
		for (ElementType* RESTRICT Data = GetData(), *RESTRICT DataEnd = Data + ArrayNum; Data != DataEnd; ++Data)
		{
			if (*Data == Key)
			{
				return Data;
			}
		}

		return nullptr;
	}

	/**
	 * Finds an element which matches a predicate functor.
	 *
	 * @param Pred The functor to apply to each element.
	 * @returns Pointer to the first element for which the predicate returns true, or nullptr if none is found.
	 * @see FilterByPredicate, ContainsByPredicate
	 */
	template <typename Predicate>
	FORCEINLINE const ElementType* FindByPredicate(Predicate Pred) const
	{
		return const_cast<TArray*>(this)->FindByPredicate(Pred);
	}

	/**
	 * Finds an element which matches a predicate functor.
	 *
	 * @param Pred The functor to apply to each element. true, or nullptr if none is found.
	 * @see FilterByPredicate, ContainsByPredicate
	 */
	template <typename Predicate>
	ElementType* FindByPredicate(Predicate Pred)
	{
		for (ElementType* RESTRICT Data = GetData(), *RESTRICT DataEnd = Data + ArrayNum; Data != DataEnd; ++Data)
		{
			if (::Invoke(Pred, *Data))
			{
				return Data;
			}
		}

		return nullptr;
	}

	/**
	 * Filters the elements in the array based on a predicate functor.
	 *
	 * @param Pred The functor to apply to each element.
	 * @returns TArray with the same type as this object which contains
	 *          the subset of elements for which the functor returns true.
	 * @see FindByPredicate, ContainsByPredicate
	 */
	template <typename Predicate>
	TArray<ElementType> FilterByPredicate(Predicate Pred) const
	{
		TArray<ElementType> FilterResults;
		for (const ElementType* RESTRICT Data = GetData(), *RESTRICT DataEnd = Data + ArrayNum; Data != DataEnd; ++Data)
		{
			if (::Invoke(Pred, *Data))
			{
				FilterResults.Add(*Data);
			}
		}
		return FilterResults;
	}

	/**
	 * Checks if this array contains the element.
	 *
	 * @returns	True if found. False otherwise.
	 * @see ContainsByPredicate, FilterByPredicate, FindByPredicate
	 */
	template <typename ComparisonType>
	bool Contains(const ComparisonType& Item) const
	{
		for (const ElementType* RESTRICT Data = GetData(), *RESTRICT DataEnd = Data + ArrayNum; Data != DataEnd; ++Data)
		{
			if (*Data == Item)
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * Checks if this array contains an element for which the predicate is true.
	 *
	 * @param Predicate to use
	 * @returns	True if found. False otherwise.
	 * @see Contains, Find
	 */
	template <typename Predicate>
	FORCEINLINE bool ContainsByPredicate(Predicate Pred) const
	{
		return FindByPredicate(Pred) != nullptr;
	}

	/**
	 * Equality operator.
	 *
	 * @param OtherArray Array to compare.
	 * @returns True if this array is the same as OtherArray. False otherwise.
	 */
	bool operator==(const TArray& OtherArray) const
	{
		SizeType Count = Num();

		return Count == OtherArray.Num() && CompareItems(GetData(), OtherArray.GetData(), Count);
	}

	/**
	 * Inequality operator.
	 *
	 * @param OtherArray Array to compare.
	 * @returns True if this array is NOT the same as OtherArray. False otherwise.
	 */
#if !PLATFORM_COMPILER_HAS_GENERATED_COMPARISON_OPERATORS
	FORCEINLINE bool operator!=(const TArray& OtherArray) const
	{
		return !(*this == OtherArray);
	}
#endif



	/**
	 * Bulk serialize array as a single memory blob when loading. Uses regular serialization code for saving
	 * and doesn't serialize at all otherwise (e.g. transient, garbage collection, ...).
	 * 
	 * Requirements:
	 *   - T's << operator needs to serialize ALL member variables in the SAME order they are layed out in memory.
	 *   - T's << operator can NOT perform any fixup operations. This limitation can be lifted by manually copying
	 *     the code after the BulkSerialize call.
	 *   - T can NOT contain any member variables requiring constructor calls or pointers
	 *   - sizeof(ElementType) must be equal to the sum of sizes of it's member variables.
	 *        - e.g. use pragma pack (push,1)/ (pop) to ensure alignment
	 *        - match up uint8/ WORDs so everything always end up being properly aligned
	 *   - Code can not rely on serialization of T if neither IsLoading() nor IsSaving() is true.
	 *   - Can only be called platforms that either have the same endianness as the one the content was saved with
	 *     or had the endian conversion occur in a cooking process like e.g. for consoles.
	 *
	 * Notes:
	 *   - it is safe to call BulkSerialize on TTransArrays
	 *
	 * IMPORTANT:
	 *   - This is Overridden in XeD3dResourceArray.h Please make certain changes are propagated accordingly
	 *
	 * @param Ar	FArchive to bulk serialize this TArray to/from
	 */
	void BulkSerialize(FArchive& Ar, bool bForcePerElementSerialization = false)
	{
		constexpr int32 ElementSize = sizeof(ElementType);
		// Serialize element size to detect mismatch across platforms.
		int32 SerializedElementSize = ElementSize;
		Ar << SerializedElementSize;
	
		if (bForcePerElementSerialization
			|| (Ar.IsSaving()			// if we are saving, we always do the ordinary serialize as a way to make sure it matches up with bulk serialization
			&& !Ar.IsCooking()			// but cooking and transacting is performance critical, so we skip that
			&& !Ar.IsTransacting())		
			|| Ar.IsByteSwapping()		// if we are byteswapping, we need to do that per-element
			)
		{
			Ar << *this;
		}
		else
		{
			CountBytes(Ar);
			if (Ar.IsLoading())
			{
				// Basic sanity checking to ensure that sizes match.
				if (!ensure(SerializedElementSize == ElementSize))
				{
					Ar.SetError();
					return;
				}

				// Serialize the number of elements, block allocate the right amount of memory and deserialize
				// the data as a giant memory blob in a single call to Serialize. Please see the function header
				// for detailed documentation on limitations and implications.
				SizeType NewArrayNum = 0;
				Ar << NewArrayNum;
				if (!ensure(NewArrayNum >= 0 && std::numeric_limits<SizeType>::max() / (SizeType)ElementSize >= NewArrayNum))
				{
					Ar.SetError();
					return;
				}
				Empty(NewArrayNum);
				AddUninitialized(NewArrayNum);
				Ar.Serialize(GetData(), (int64)NewArrayNum * (int64)ElementSize);
			}
			else if (Ar.IsSaving())
			{
				SizeType ArrayCount = Num();
				Ar << ArrayCount;
				Ar.Serialize(GetData(), (int64)ArrayCount * (int64)ElementSize);
			}
		}
	}

	/**
	 * Count bytes needed to serialize this array.
	 *
	 * @param Ar Archive to count for.
	 */
	void CountBytes(FArchive& Ar) const
	{
		Ar.CountBytes(ArrayNum*sizeof(ElementType), ArrayMax*sizeof(ElementType));
	}

	/**
	 * Adds a given number of uninitialized elements into the array.
	 *
	 * Caution, AddUninitialized() will create elements without calling
	 * the constructor and this is not appropriate for element types that
	 * require a constructor to function properly.
	 *
	 * @param Count Number of elements to add.
	 * @returns Number of elements in array before addition.
	 */
	FORCEINLINE SizeType AddUninitialized()
	{
		CheckInvariants();

		const USizeType OldNum = (USizeType)ArrayNum;
		const USizeType NewNum = OldNum + (USizeType)1;
		ArrayNum = (SizeType)NewNum;
		if (NewNum > (USizeType)ArrayMax)
		{
			ResizeGrow((SizeType)OldNum);
		}
		else
		{
			SlackTrackerNumChanged();
		}

		return OldNum;
	}
	FORCEINLINE SizeType AddUninitialized(SizeType Count)
	{
		CheckInvariants();
		checkSlow(Count >= 0);

		const USizeType OldNum = (USizeType)ArrayNum;
		const USizeType NewNum = OldNum + (USizeType)Count;
		ArrayNum = (SizeType)NewNum;

#if DO_GUARD_SLOW
		if (NewNum > (USizeType)ArrayMax)
#else
		// SECURITY - This check will guard against negative counts too, in case the checkSlow(Count >= 0) above is compiled out.
		// However, it results in slightly worse code generation.
		if ((USizeType)Count > (USizeType)ArrayMax - OldNum)
#endif
		{
			ResizeGrow((SizeType)OldNum);
		}
		else
		{
			SlackTrackerNumChanged();
		}

		return OldNum;
	}

private:
	void InsertUninitializedImpl(SizeType Index)
	{
		CheckInvariants();
		checkSlow((Index >= 0) & (Index <= ArrayNum));

		const USizeType OldNum = (USizeType)ArrayNum;
		const USizeType NewNum = OldNum + (USizeType)1;
		ArrayNum = (SizeType)NewNum;

		if (NewNum > (USizeType)ArrayMax)
		{
			ResizeGrow((SizeType)OldNum);
		}
		else
		{
			SlackTrackerNumChanged();
		}
		ElementType* Data = GetData() + Index;
		RelocateConstructItems<ElementType>(Data + 1, Data, OldNum - Index);
	}
	template <typename OtherSizeType>
	void InsertUninitializedImpl(SizeType Index, OtherSizeType Count)
	{
		CheckInvariants();
		checkSlow((Count >= 0) & (Index >= 0) & (Index <= ArrayNum));

		SizeType ConvertedCount = Count;
		checkf((OtherSizeType)ConvertedCount == Count, TEXT("Invalid number of elements to add to this array type: %lld"), (long long)ConvertedCount);

		const USizeType OldNum = (USizeType)ArrayNum;
		const USizeType NewNum = OldNum + (USizeType)Count;
		ArrayNum = (SizeType)NewNum;

#if DO_GUARD_SLOW
		if (NewNum > (USizeType)ArrayMax)
#else
		// SECURITY - This check will guard against negative counts too, in case the checkSlow(Count >= 0) above is compiled out.
		// However, it results in slightly worse code generation.
		if ((USizeType)Count > (USizeType)ArrayMax - OldNum)
#endif
		{
			ResizeGrow((SizeType)OldNum);
		}
		else
		{
			SlackTrackerNumChanged();
		}
		ElementType* Data = GetData() + Index;
		RelocateConstructItems<ElementType>(Data + Count, Data, OldNum - Index);
	}

public:
	/**
	 * Inserts a given number of uninitialized elements into the array at given
	 * location.
	 *
	 * Caution, InsertUninitialized() will create elements without calling the
	 * constructor and this is not appropriate for element types that require
	 * a constructor to function properly.
	 *
	 * @param Index Tells where to insert the new elements.
	 * @param Count Number of elements to add.
	 * @see Insert, InsertZeroed, InsertDefaulted
	 */
	FORCEINLINE void InsertUninitialized(SizeType Index)
	{
		InsertUninitializedImpl(Index);
	}
	FORCEINLINE void InsertUninitialized(SizeType Index, SizeType Count)
	{
		InsertUninitializedImpl(Index, Count);
	}

	/**
	 * Inserts a given number of zeroed elements into the array at given
	 * location.
	 *
	 * Caution, InsertZeroed() will create elements without calling the
	 * constructor and this is not appropriate for element types that require
	 * a constructor to function properly.
	 *
	 * @param Index Tells where to insert the new elements.
	 * @param Count Number of elements to add.
	 * @see Insert, InsertUninitialized, InsertDefaulted
	 */
	void InsertZeroed(SizeType Index)
	{
		InsertUninitializedImpl(Index);
		FMemory::Memzero(GetData() + Index, sizeof(ElementType));
	}
	void InsertZeroed(SizeType Index, SizeType Count)
	{
		InsertUninitializedImpl(Index, Count);
		FMemory::Memzero(GetData() + Index, Count * sizeof(ElementType));
	}

	/**
	 * Inserts a zeroed element into the array at given location.
	 *
	 * Caution, InsertZeroed_GetRef() will create an element without calling the
	 * constructor and this is not appropriate for element types that require
	 * a constructor to function properly.
	 *
	 * @param Index Tells where to insert the new element.
	 * @return A reference to the newly-inserted element.
	 * @see Insert_GetRef, InsertDefaulted_GetRef
	 */
	ElementType& InsertZeroed_GetRef(SizeType Index)
	{
		InsertUninitializedImpl(Index, 1);
		ElementType* Ptr = GetData() + Index;
		FMemory::Memzero(Ptr, sizeof(ElementType));
		return *Ptr;
	}

	/**
	 * Inserts a given number of default-constructed elements into the array at a given
	 * location.
	 *
	 * @param Index Tells where to insert the new elements.
	 * @param Count Number of elements to add.
	 * @see Insert, InsertUninitialized, InsertZeroed
	 */
	void InsertDefaulted(SizeType Index)
	{
		InsertUninitializedImpl(Index);
		DefaultConstructItems<ElementType>(GetData() + Index, 1);
	}
	void InsertDefaulted(SizeType Index, SizeType Count)
	{
		InsertUninitializedImpl(Index, Count);
		DefaultConstructItems<ElementType>(GetData() + Index, Count);
	}

	/**
	 * Inserts a default-constructed element into the array at a given
	 * location.
	 *
	 * @param Index Tells where to insert the new element.
	 * @return A reference to the newly-inserted element.
	 * @see Insert_GetRef, InsertZeroed_GetRef
	 */
	ElementType& InsertDefaulted_GetRef(SizeType Index)
	{
		InsertUninitializedImpl(Index, 1);
		ElementType* Ptr = GetData() + Index;
		DefaultConstructItems<ElementType>(Ptr, 1);
		return *Ptr;
	}

	/**
	 * Inserts given elements into the array at given location.
	 *
	 * @param InitList Array of elements to insert.
	 * @param InIndex Tells where to insert the new elements.
	 * @returns Location at which the item was inserted.
	 */
	SizeType Insert(std::initializer_list<ElementType> InitList, const SizeType InIndex)
	{
		SizeType NumNewElements = (SizeType)InitList.size();

		InsertUninitializedImpl(InIndex, NumNewElements);
		ConstructItems<ElementType>(GetData() + InIndex, InitList.begin(), NumNewElements);

		return InIndex;
	}

	/**
	 * Inserts given elements into the array at given location.
	 *
	 * @param Items Array of elements to insert.
	 * @param InIndex Tells where to insert the new elements.
	 * @returns Location at which the item was inserted.
	 */
	template <typename OtherAllocator>
	SizeType Insert(const TArray<ElementType, OtherAllocator>& Items, const SizeType InIndex)
	{
		check((const void*)this != (const void*)&Items);

		auto NumNewElements = Items.Num();

		InsertUninitializedImpl(InIndex, NumNewElements);
		ConstructItems<ElementType>(GetData() + InIndex, Items.GetData(), NumNewElements);

		return InIndex;
	}

	/**
	 * Inserts given elements into the array at given location.
	 *
	 * @param Items Array of elements to insert.
	 * @param InIndex Tells where to insert the new elements.
	 * @returns Location at which the item was inserted.
	 */
	template <typename OtherAllocator>
	SizeType Insert(TArray<ElementType, OtherAllocator>&& Items, const SizeType InIndex)
	{
		check((const void*)this != (const void*)&Items);

		auto NumNewElements = Items.Num();

		InsertUninitializedImpl(InIndex, NumNewElements);
		RelocateConstructItems<ElementType>(GetData() + InIndex, Items.GetData(), NumNewElements);
		Items.ArrayNum = 0;

		Items.SlackTrackerNumChanged();

		return InIndex;
	}

	/**
	 * Inserts a raw array of elements at a particular index in the TArray.
	 *
	 * @param Ptr A pointer to an array of elements to add.
	 * @param Count The number of elements to insert from Ptr.
	 * @param Index The index to insert the elements at.
	 * @return The index of the first element inserted.
	 * @see Add, Remove
	 */
	SizeType Insert(const ElementType* Ptr, SizeType Count, SizeType Index)
	{
		check(Ptr != nullptr);

		InsertUninitializedImpl(Index, Count);
		ConstructItems<ElementType>(GetData() + Index, Ptr, Count);

		return Index;
	}

	/**
	 * Checks that the specified address is not part of an element within the
	 * container. Used for implementations to check that reference arguments
	 * aren't going to be invalidated by possible reallocation.
	 *
	 * @param Addr The address to check.
	 * @see Add, Remove
	 */
	FORCEINLINE void CheckAddress(const ElementType* Addr) const
	{
		checkf(Addr < GetData() || Addr >= (GetData() + ArrayMax), TEXT("Attempting to use a container element (%p) which already comes from the container being modified (%p, ArrayMax: %lld, ArrayNum: %lld, SizeofElement: %d)!"), Addr, GetData(), (long long)ArrayMax, (long long)ArrayNum, sizeof(ElementType));
	}

	/**
	 * Inserts a given element into the array at given location. Move semantics
	 * version.
	 *
	 * @param Item The element to insert.
	 * @param Index Tells where to insert the new elements.
	 * @returns Location at which the insert was done.
	 * @see Add, Remove
	 */
	SizeType Insert(ElementType&& Item, SizeType Index)
	{
		CheckAddress(&Item);

		// construct a copy in place at Index (this new operator will insert at 
		// Index, then construct that memory with Item)
		InsertUninitializedImpl(Index);
		new(GetData() + Index) ElementType(MoveTempIfPossible(Item));
		return Index;
	}

	/**
	 * Inserts a given element into the array at given location.
	 *
	 * @param Item The element to insert.
	 * @param Index Tells where to insert the new elements.
	 * @returns Location at which the insert was done.
	 * @see Add, Remove
	 */
	SizeType Insert(const ElementType& Item, SizeType Index)
	{
		CheckAddress(&Item);

		// construct a copy in place at Index (this new operator will insert at 
		// Index, then construct that memory with Item)
		InsertUninitializedImpl(Index);
		new(GetData() + Index) ElementType(Item);
		return Index;
	}

	/**
	 * Inserts a given element into the array at given location. Move semantics
	 * version.
	 *
	 * @param Item The element to insert.
	 * @param Index Tells where to insert the new element.
	 * @return A reference to the newly-inserted element.
	 * @see Add, Remove
	 */
	ElementType& Insert_GetRef(ElementType&& Item, SizeType Index)
	{
		CheckAddress(&Item);

		// construct a copy in place at Index (this new operator will insert at 
		// Index, then construct that memory with Item)
		InsertUninitializedImpl(Index);
		ElementType* Ptr = GetData() + Index;
		new(Ptr) ElementType(MoveTempIfPossible(Item));
		return *Ptr;
	}

	/**
	 * Inserts a given element into the array at given location.
	 *
	 * @param Item The element to insert.
	 * @param Index Tells where to insert the new element.
	 * @return A reference to the newly-inserted element.
	 * @see Add, Remove
	 */
	ElementType& Insert_GetRef(const ElementType& Item, SizeType Index)
	{
		CheckAddress(&Item);

		// construct a copy in place at Index (this new operator will insert at 
		// Index, then construct that memory with Item)
		InsertUninitializedImpl(Index);
		ElementType* Ptr = GetData() + Index;
		new(Ptr) ElementType(Item);
		return *Ptr;
	}

private:
	void RemoveAtImpl(SizeType Index)
	{
		ElementType* Dest = GetData() + Index;

		DestructItem(Dest);

		// Skip relocation in the common case that there is nothing to move.
		SizeType NumToMove = (ArrayNum - Index) - 1;
		if (NumToMove)
		{
			RelocateConstructItems<ElementType>(Dest, Dest + 1, NumToMove);
		}
		--ArrayNum;

		SlackTrackerNumChanged();
	}

	void RemoveAtImpl(SizeType Index, SizeType Count)
	{
		ElementType* Dest = GetData() + Index;

		DestructItems(Dest, Count);

		// Skip relocation in the common case that there is nothing to move.
		SizeType NumToMove = (ArrayNum - Index) - Count;
		if (NumToMove)
		{
			RelocateConstructItems<ElementType>(Dest, Dest + Count, NumToMove);
		}
		ArrayNum -= Count;

		SlackTrackerNumChanged();
	}

public:
	/**
	 * Removes an element (or elements) at given location, then shrinks
	 * the array.
	 *
	 * @param Index Location in array of the element to remove.
	 * @param AllowShrinking (Optional) Tells if this call can shrink array if suitable after remove. Default is yes.
	 */
	void RemoveAt(SizeType Index, EAllowShrinking AllowShrinking = EAllowShrinking::Yes)
	{
		RangeCheck(Index);
		RemoveAtImpl(Index);
		if (AllowShrinking == EAllowShrinking::Yes)
		{
			ResizeShrink();
		}
	}

	/**
	 * Removes an element (or elements) at given location, optionally shrinking
	 * the array.
	 *
	 * @param Index Location in array of the element to remove.
	 * @param Count (Optional) Number of elements to remove. Default is 1.
	 * @param AllowShrinking (Optional) Tells if this call can shrink array if suitable after remove. Default is yes.
	 */
	template <
		typename CountType
		UE_REQUIRES(std::is_integral_v<CountType>)
	>
	FORCEINLINE void RemoveAt(SizeType Index, CountType Count, EAllowShrinking AllowShrinking = EAllowShrinking::Yes)
	{
		static_assert(!std::is_same_v<CountType, bool>, "TArray::RemoveAt: unexpected bool passed as the Count argument");
		RangeCheck(Index, Count);
		if (Count)
		{
			RemoveAtImpl(Index, (SizeType)Count);
			if (AllowShrinking == EAllowShrinking::Yes)
			{
				ResizeShrink();
			}
		}
	}
	template <typename CountType>
	UE_ALLOWSHRINKING_BOOL_DEPRECATED("RemoveAt")
	FORCEINLINE void RemoveAt(SizeType Index, CountType Count, bool bAllowShrinking)
	{
		RemoveAt(Index, Count, bAllowShrinking ? EAllowShrinking::Yes : EAllowShrinking::No);
	}

private:
	void RemoveAtSwapImpl(SizeType Index)
	{
		ElementType* Data = GetData();
		ElementType* Dest = Data + Index;

		DestructItem(Dest);

		// Replace the elements in the hole created by the removal with elements from the end of the array, so the range of indices used by the array is contiguous.
		const SizeType NumElementsAfterHole = (ArrayNum - Index) - 1;
		const SizeType NumElementsToMoveIntoHole = FPlatformMath::Min(1, NumElementsAfterHole);
		if (NumElementsToMoveIntoHole)
		{
			RelocateConstructItems<ElementType>(Dest, Data + (ArrayNum - NumElementsToMoveIntoHole), NumElementsToMoveIntoHole);
		}
		--ArrayNum;

		SlackTrackerNumChanged();
	}

	void RemoveAtSwapImpl(SizeType Index, SizeType Count)
	{
		ElementType* Data = GetData();
		ElementType* Dest = Data + Index;

		DestructItems(Dest, Count);

		// Replace the elements in the hole created by the removal with elements from the end of the array, so the range of indices used by the array is contiguous.
		const SizeType NumElementsAfterHole = (ArrayNum - Index) - Count;
		const SizeType NumElementsToMoveIntoHole = FPlatformMath::Min(Count, NumElementsAfterHole);
		if (NumElementsToMoveIntoHole)
		{
			RelocateConstructItems<ElementType>(Dest, Data + (ArrayNum - NumElementsToMoveIntoHole), NumElementsToMoveIntoHole);
		}
		ArrayNum -= Count;

		SlackTrackerNumChanged();
	}

public:
	/**
	 * Removes an element (or elements) at given location, then shrinks
	 * the array.
	 *
	 * This version is much more efficient than RemoveAt (O(Count) instead of
	 * O(ArrayNum)), but does not preserve the order.
	 *
	 * @param Index Location in array of the element to remove.
	 * @param AllowShrinking (Optional) Tells if this call can shrink array if
	 *                        suitable after remove. Default is yes.
	 */
	FORCEINLINE void RemoveAtSwap(SizeType Index, EAllowShrinking AllowShrinking = EAllowShrinking::Yes)
	{
		RangeCheck(Index);
		RemoveAtSwapImpl(Index);
		if (AllowShrinking == EAllowShrinking::Yes)
		{
			ResizeShrink();
		}
	}

	/**
	 * Removes an element (or elements) at given location, optionally shrinking
	 * the array.
	 *
	 * This version is much more efficient than RemoveAt (O(Count) instead of
	 * O(ArrayNum)), but does not preserve the order.
	 *
	 * @param Index Location in array of the element to remove.
	 * @param Count (Optional) Number of elements to remove. Default is 1.
	 * @param AllowShrinking (Optional) Tells if this call can shrink array if
	 *                        suitable after remove. Default is yes.
	 */
	template <
		typename CountType
		UE_REQUIRES(std::is_integral_v<CountType>)
	>
	FORCEINLINE void RemoveAtSwap(SizeType Index, CountType Count, EAllowShrinking AllowShrinking = EAllowShrinking::Yes)
	{
		static_assert(!std::is_same_v<CountType, bool>, "TArray::RemoveAtSwap: unexpected bool passed as the Count argument");
		RangeCheck(Index, Count);
		if (Count)
		{
		    RemoveAtSwapImpl(Index, Count);
		    if (AllowShrinking == EAllowShrinking::Yes)
		    {
			    ResizeShrink();
		    }
		}
	}
	template <typename CountType>
	UE_ALLOWSHRINKING_BOOL_DEPRECATED("RemoveAtSwap")
	FORCEINLINE void RemoveAtSwap(SizeType Index, CountType Count, bool bAllowShrinking)
	{
		RemoveAtSwap(Index, Count, bAllowShrinking ? EAllowShrinking::Yes : EAllowShrinking::No);
	}

	/**
	 * Same as empty, but doesn't change memory allocations, unless the new size is larger than
	 * the current array. It calls the destructors on held items if needed and then zeros the ArrayNum.
	 *
	 * @param NewSize The expected usage size after calling this function.
	 */
	void Reset(SizeType NewSize = 0)
	{
		if (NewSize < 0)
		{
			// Cast to USizeType first to prevent sign extension on negative sizes, producing unusually large values.
			UE::Core::Private::OnInvalidArrayNum((unsigned long long)(USizeType)NewSize);
		}

		// If we have space to hold the excepted size, then don't reallocate
		if (NewSize <= ArrayMax)
		{
			DestructItems(GetData(), ArrayNum);
			ArrayNum = 0;

			SlackTrackerNumChanged();
		}
		else
		{
			Empty(NewSize);
		}
	}

	/**
	 * Empties the array. It calls the destructors on held items if needed.
	 *
	 * @param Slack (Optional) The expected usage size after empty operation. Default is 0.
	 */
	void Empty(SizeType Slack = 0)
	{
		if (Slack < 0)
		{
			// Cast to USizeType first to prevent sign extension on negative sizes, producing unusually large values.
			UE::Core::Private::OnInvalidArrayNum((unsigned long long)(USizeType)Slack);
		}

		DestructItems(GetData(), ArrayNum);

		checkSlow(Slack >= 0);
		ArrayNum = 0;

		SlackTrackerNumChanged();

		if (ArrayMax != Slack)
		{
			ResizeTo(Slack);
		}
	}

	/**
	 * Resizes array to given number of elements.
	 *
	 * @param NewNum New size of the array.
	 * @param AllowShrinking Tell if this function can shrink the memory in-use if suitable.
	 */
	void SetNum(SizeType NewNum, EAllowShrinking AllowShrinking = EAllowShrinking::Yes)
	{
		if (NewNum > Num())
		{
			const SizeType Diff = NewNum - ArrayNum;
			const SizeType Index = AddUninitialized(Diff);
			DefaultConstructItems<ElementType>((uint8*)AllocatorInstance.GetAllocation() + Index * sizeof(ElementType), Diff);
		}
		else if (NewNum < 0)
		{
			// Cast to USizeType first to prevent sign extension on negative sizes, producing unusually large values.
			UE::Core::Private::OnInvalidArrayNum((unsigned long long)(USizeType)NewNum);
		}
		else if (NewNum < Num())
		{
			RemoveAt(NewNum, Num() - NewNum, AllowShrinking);
		}
	}
	UE_ALLOWSHRINKING_BOOL_DEPRECATED("SetNum")
	FORCEINLINE void SetNum(SizeType NewNum, bool bAllowShrinking)
	{
		SetNum(NewNum, bAllowShrinking ? EAllowShrinking::Yes : EAllowShrinking::No);
	}

	/**
	 * Resizes array to given number of elements, optionally shrinking it.
	 * New elements will be zeroed.
	 *
	 * @param NewNum New size of the array.
	 * @param AllowShrinking Tell if this function can shrink the memory in-use if suitable.
	 */
	void SetNumZeroed(SizeType NewNum, EAllowShrinking AllowShrinking = EAllowShrinking::Yes)
	{
		if (NewNum > Num())
		{
			AddZeroed(NewNum - Num());
		}
		else if (NewNum < 0)
		{
			// Cast to USizeType first to prevent sign extension on negative sizes, producing unusually large values.
			UE::Core::Private::OnInvalidArrayNum((unsigned long long)(USizeType)NewNum);
		}
		else if (NewNum < Num())
		{
			RemoveAt(NewNum, Num() - NewNum, AllowShrinking);
		}
	}
	UE_ALLOWSHRINKING_BOOL_DEPRECATED("SetNumZeroed")
	FORCEINLINE void SetNumZeroed(SizeType NewNum, bool bAllowShrinking)
	{
		SetNumZeroed(NewNum, bAllowShrinking ? EAllowShrinking::Yes : EAllowShrinking::No);
	}

	/**
	 * Resizes array to given number of elements. New elements will be uninitialized.
	 *
	 * @param NewNum New size of the array.
	 * @param AllowShrinking Tell if this function can shrink the memory in-use if suitable.
	 */
	void SetNumUninitialized(SizeType NewNum, EAllowShrinking AllowShrinking = EAllowShrinking::Yes)
	{
		if (NewNum > Num())
		{
			AddUninitialized(NewNum - Num());
		}
		else if (NewNum < 0)
		{
			// Cast to USizeType first to prevent sign extension on negative sizes, producing unusually large values.
			UE::Core::Private::OnInvalidArrayNum((unsigned long long)(USizeType)NewNum);
		}
		else if (NewNum < Num())
		{
			RemoveAt(NewNum, Num() - NewNum, AllowShrinking);
		}
	}
	UE_ALLOWSHRINKING_BOOL_DEPRECATED("SetNumUninitialized")
	FORCEINLINE void SetNumUninitialized(SizeType NewNum, bool bAllowShrinking)
	{
		SetNumUninitialized(NewNum, bAllowShrinking ? EAllowShrinking::Yes : EAllowShrinking::No);
	}

	/**
	 * Does nothing except setting the new number of elements in the array. Does not destruct items, does not de-allocate memory.
	 * @param NewNum New number of elements in the array, must be <= the current number of elements in the array.
	 */
	void SetNumUnsafeInternal(SizeType NewNum)
	{
		checkSlow(NewNum <= Num() && NewNum >= 0);
		ArrayNum = NewNum;

		SlackTrackerNumChanged();
	}

	/**
	 * Appends the specified array to this array.
	 *
	 * AllocatorType changing version.
	 *
	 * @param Source The array to append.
	 * @see Add, Insert
	 */
	template <typename OtherElementType, typename OtherAllocatorType>
	void Append(const TArray<OtherElementType, OtherAllocatorType>& Source)
	{
		check((void*)this != (void*)&Source);

		SizeType SourceCount = Source.Num();

		// Do nothing if the source is empty.
		if (!SourceCount)
		{
			return;
		}

		// Allocate memory for the new elements.
		SizeType Pos = AddUninitialized(SourceCount);
		ConstructItems<ElementType>(GetData() + Pos, Source.GetData(), SourceCount);
	}

	/**
	 * Appends the specified array to this array.
	 *
	 * @param Source The array to append.
	 * @see Add, Insert
	 */
	template <typename OtherElementType, typename OtherAllocator>
	void Append(TArray<OtherElementType, OtherAllocator>&& Source)
	{
		check((void*)this != (void*)&Source);

		SizeType SourceCount = Source.Num();

		// Do nothing if the source is empty.
		if (!SourceCount)
		{
			return;
		}

		// Allocate memory for the new elements.
		SizeType Pos = AddUninitialized(SourceCount);
		RelocateConstructItems<ElementType>(GetData() + Pos, Source.GetData(), SourceCount);
		Source.ArrayNum = 0;

		Source.SlackTrackerNumChanged();
	}

	/**
	 * Appends the elements from a contiguous range to this array.
	 *
	 * @param Source The range of elements to append.
	 * @see Add, Insert
	 */
	template <
		typename RangeType
		UE_REQUIRES(
			TIsContiguousContainer<RangeType>::Value &&
			!UE4Array_Private::TIsTArrayOrDerivedFromTArray_V<std::remove_reference_t<RangeType>> &&
			UE4Array_Private::TArrayElementsAreCompatible_V<ElementType, TElementType_T<RangeType>>
		)
	>
	void Append(RangeType&& Source)
	{
		auto InCount = GetNum(Source);
		checkf((InCount >= 0) && ((sizeof(InCount) < sizeof(SizeType)) || (InCount <= static_cast<decltype(InCount)>(TNumericLimits<SizeType>::Max()))), TEXT("Invalid range size: %lld"), (long long)InCount);

		// Do nothing if the source is empty.
		if (!InCount)
		{
			return;
		}

		SizeType SourceCount = (SizeType)InCount;

		// Allocate memory for the new elements.
		SizeType Pos = AddUninitialized(SourceCount);
		ConstructItems<ElementType>(GetData() + Pos, UE4Array_Private::GetDataHelper(Source), SourceCount);
	}

	/**
	 * Adds a raw array of elements to the end of the TArray.
	 *
	 * @param Ptr   A pointer to an array of elements to add.
	 * @param Count The number of elements to insert from Ptr.
	 * @see Add, Insert
	 */
	void Append(const ElementType* Ptr, SizeType Count)
	{
		check(Ptr != nullptr || Count == 0);

		SizeType Pos = AddUninitialized(Count);
		ConstructItems<ElementType>(GetData() + Pos, Ptr, Count);
	}

	/**
	 * Adds an initializer list of elements to the end of the TArray.
	 *
	 * @param InitList The initializer list of elements to add.
	 * @see Add, Insert
	 */
	FORCEINLINE void Append(std::initializer_list<ElementType> InitList)
	{
		SizeType Count = (SizeType)InitList.size();

		SizeType Pos = AddUninitialized(Count);
		ConstructItems<ElementType>(GetData() + Pos, InitList.begin(), Count);
	}

	/**
	 * Appends the specified array to this array.
	 * Cannot append to self.
	 *
	 * Move semantics version.
	 *
	 * @param Other The array to append.
	 */
	TArray& operator+=(TArray&& Other)
	{
		Append(MoveTemp(Other));
		return *this;
	}

	/**
	 * Appends the specified array to this array.
	 * Cannot append to self.
	 *
	 * @param Other The array to append.
	 */
	TArray& operator+=(const TArray& Other)
	{
		Append(Other);
		return *this;
	}

	/**
	 * Appends the specified initializer list to this array.
	 *
	 * @param InitList The initializer list to append.
	 */
	TArray& operator+=(std::initializer_list<ElementType> InitList)
	{
		Append(InitList);
		return *this;
	}

	/**
	 * Constructs a new item at the end of the array, possibly reallocating the whole array to fit.
	 *
	 * @param Args	The arguments to forward to the constructor of the new item.
	 * @return		Index to the new item
	 */
	template <typename... ArgsType>
	FORCEINLINE SizeType Emplace(ArgsType&&... Args)
	{
		const SizeType Index = AddUninitialized();

		// If this fails to compile when trying to call Emplace with a non-public constructor,
		// do not make TArray a friend.
		//
		// Instead, prefer this pattern:
		//
		//     class FMyType
		//     {
		//     private:
		//         struct FPrivateToken { explicit FPrivateToken() = default; };
		//
		//     public:
		//         // This has an equivalent access level to a private constructor,
		//         // as only friends of FMyType will have access to FPrivateToken,
		//         // but Emplace can legally call it since it's public.
		//         explicit FMyType(FPrivateToken, int32 Int, float Real, const TCHAR* String);
		//     };
		//
		//     TArray<FMyType> Arr:
		//
		//     // Won't compile if the caller doesn't have access to FMyType::FPrivateToken
		//     Arr.Emplace(FMyType::FPrivateToken{}, 5, 3.14f, TEXT("Banana"));
		//
		new(GetData() + Index) ElementType(Forward<ArgsType>(Args)...);
		return Index;
	}

	/**
	 * Constructs a new item at the end of the array, possibly reallocating the whole array to fit.
	 *
	 * @param Args	The arguments to forward to the constructor of the new item.
	 * @return A reference to the newly-inserted element.
	 */
	template <typename... ArgsType>
	FORCEINLINE ElementType& Emplace_GetRef(ArgsType&&... Args)
	{
		const SizeType Index = AddUninitialized();
		ElementType* Ptr = GetData() + Index;
		new(Ptr) ElementType(Forward<ArgsType>(Args)...);
		return *Ptr;
	}

	/**
	 * Constructs a new item at a specified index, possibly reallocating the whole array to fit.
	 *
	 * @param Index	The index to add the item at.
	 * @param Args	The arguments to forward to the constructor of the new item.
	 */
	template <typename... ArgsType>
	FORCEINLINE void EmplaceAt(SizeType Index, ArgsType&&... Args)
	{
		InsertUninitializedImpl(Index, 1);
		new(GetData() + Index) ElementType(Forward<ArgsType>(Args)...);
	}

	/**
	 * Constructs a new item at a specified index, possibly reallocating the whole array to fit.
	 *
	 * @param Index	The index to add the item at.
	 * @param Args	The arguments to forward to the constructor of the new item.
	 * @return A reference to the newly-inserted element.
	 */
	template <typename... ArgsType>
	FORCEINLINE ElementType& EmplaceAt_GetRef(SizeType Index, ArgsType&&... Args)
	{
		InsertUninitializedImpl(Index, 1);
		ElementType* Ptr = GetData() + Index;
		new(Ptr) ElementType(Forward<ArgsType>(Args)...);
		return *Ptr;
	}

	/**
	 * Adds a new item to the end of the array, possibly reallocating the whole array to fit.
	 *
	 * Move semantics version.
	 *
	 * @param Item The item to add
	 * @return Index to the new item
	 * @see AddDefaulted, AddUnique, AddZeroed, Append, Insert
	 */
	FORCEINLINE SizeType Add(ElementType&& Item)
	{
		CheckAddress(&Item);
		return Emplace(MoveTempIfPossible(Item));
	}

	/**
	 * Adds a new item to the end of the array, possibly reallocating the whole array to fit.
	 *
	 * @param Item The item to add
	 * @return Index to the new item
	 * @see AddDefaulted, AddUnique, AddZeroed, Append, Insert
	 */
	FORCEINLINE SizeType Add(const ElementType& Item)
	{
		CheckAddress(&Item);
		return Emplace(Item);
	}

	/**
	 * Adds a new item to the end of the array, possibly reallocating the whole array to fit.
	 *
	 * Move semantics version.
	 *
	 * @param Item The item to add
	 * @return A reference to the newly-inserted element.
	 * @see AddDefaulted_GetRef, AddUnique_GetRef, AddZeroed_GetRef, Insert_GetRef
	 */
	FORCEINLINE ElementType& Add_GetRef(ElementType&& Item)
	{
		CheckAddress(&Item);
		return Emplace_GetRef(MoveTempIfPossible(Item));
	}

	/**
	 * Adds a new item to the end of the array, possibly reallocating the whole array to fit.
	 *
	 * @param Item The item to add
	 * @return A reference to the newly-inserted element.
	 * @see AddDefaulted_GetRef, AddUnique_GetRef, AddZeroed_GetRef, Insert_GetRef
	 */
	FORCEINLINE ElementType& Add_GetRef(const ElementType& Item)
	{
		CheckAddress(&Item);
		return Emplace_GetRef(Item);
	}

	/**
	 * Adds new items to the end of the array, possibly reallocating the whole
	 * array to fit. The new items will be zeroed.
	 *
	 * Caution, AddZeroed() will create elements without calling the
	 * constructor and this is not appropriate for element types that require
	 * a constructor to function properly.
	 *
	 * @param  Count  The number of new items to add.
	 * @return Index to the first of the new items.
	 * @see Add, AddDefaulted, AddUnique, Append, Insert
	 */
	SizeType AddZeroed()
	{
		const SizeType Index = AddUninitialized();
		FMemory::Memzero((uint8*)AllocatorInstance.GetAllocation() + Index * sizeof(ElementType), sizeof(ElementType));
		return Index;
	}
	SizeType AddZeroed(SizeType Count)
	{
		const SizeType Index = AddUninitialized(Count);
		FMemory::Memzero((uint8*)AllocatorInstance.GetAllocation() + Index*sizeof(ElementType), Count*sizeof(ElementType));
		return Index;
	}

	/**
	 * Adds a new item to the end of the array, possibly reallocating the whole
	 * array to fit. The new item will be zeroed.
	 *
	 * Caution, AddZeroed_GetRef() will create elements without calling the
	 * constructor and this is not appropriate for element types that require
	 * a constructor to function properly.
	 *
	 * @return A reference to the newly-inserted element.
	 * @see Add_GetRef, AddDefaulted_GetRef, AddUnique_GetRef, Insert_GetRef
	 */
	ElementType& AddZeroed_GetRef()
	{
		const SizeType Index = AddUninitialized();
		ElementType* Ptr = GetData() + Index;
		FMemory::Memzero(Ptr, sizeof(ElementType));
		return *Ptr;
	}

	/**
	 * Adds new items to the end of the array, possibly reallocating the whole
	 * array to fit. The new items will be default-constructed.
	 *
	 * @param  Count  The number of new items to add.
	 * @return Index to the first of the new items.
	 * @see Add, AddZeroed, AddUnique, Append, Insert
	 */
	SizeType AddDefaulted()
	{
		const SizeType Index = AddUninitialized();
		DefaultConstructItems<ElementType>((uint8*)AllocatorInstance.GetAllocation() + Index * sizeof(ElementType), 1);
		return Index;
	}
	SizeType AddDefaulted(SizeType Count)
	{
		const SizeType Index = AddUninitialized(Count);
		DefaultConstructItems<ElementType>((uint8*)AllocatorInstance.GetAllocation() + Index * sizeof(ElementType), Count);
		return Index;
	}

	/**
	 * Add a new item to the end of the array, possibly reallocating the whole
	 * array to fit. The new item will be default-constructed.
	 *
	 * @return A reference to the newly-inserted element.
	 * @see Add_GetRef, AddZeroed_GetRef, AddUnique_GetRef, Insert_GetRef
	 */
	ElementType& AddDefaulted_GetRef()
	{
		const SizeType Index = AddUninitialized();
		ElementType* Ptr = GetData() + Index;
		DefaultConstructItems<ElementType>(Ptr, 1);
		return *Ptr;
	}

#if !UE_DEPRECATE_MUTABLE_TOBJECTPTR
	/** Mutable implicit conversion operator to container of compatible element type. */
	template <
		typename AliasElementType = ElementType
		UE_REQUIRES(TIsContainerElementTypeReinterpretable_V<AliasElementType>)
	>
	operator TArray<typename TContainerElementTypeCompatibility<AliasElementType>::ReinterpretType, AllocatorType>& ()
	{
		using ElementCompat = TContainerElementTypeCompatibility<ElementType>;
		ElementCompat::ReinterpretRangeContiguous(begin(), end(), Num());
		return *reinterpret_cast<TArray<typename ElementCompat::ReinterpretType>*>(this);
	}
#endif
  
	/** Immutable implicit conversion operator to constant container of compatible element type. */
	template <
		typename AliasElementType = ElementType
		UE_REQUIRES(TIsContainerElementTypeReinterpretable_V<AliasElementType>)
	>
	operator const TArray<typename TContainerElementTypeCompatibility<AliasElementType>::ReinterpretType, AllocatorType>& () const
	{
		using ElementCompat = TContainerElementTypeCompatibility<ElementType>;
		ElementCompat::ReinterpretRangeContiguous(begin(), end(), Num());
		return *reinterpret_cast<const TArray<typename ElementCompat::ReinterpretType>*>(this);
	}

	/**
	 * Move assignment operator.
	 * Compatible element type version.
	 *
	 * @param Other Array to assign and move from.
	 */
	template <
		typename AliasElementType = ElementType
		UE_REQUIRES(TIsContainerElementTypeCopyable_V<AliasElementType>)
	>
	TArray& operator=(TArray<typename TContainerElementTypeCompatibility<ElementType>::CopyFromOtherType, AllocatorType>&& Other)
	{
		TContainerElementTypeCompatibility<ElementType>::CopyingFromOtherType();
		DestructItems(GetData(), ArrayNum);
		MoveOrCopy(*this, Other, ArrayMax);
		return *this; 
	}

	/**
	 * Assignment operator. First deletes all currently contained elements
	 * and then copies from other array.
	 * Compatible element type version.
	 *
	 * @param Other The source array to assign from.
	 */
	template <
		typename OtherAllocator,
		typename AliasElementType = ElementType
		UE_REQUIRES(TIsContainerElementTypeCopyable_V<AliasElementType>)
	>
	TArray& operator=(const TArray<typename TContainerElementTypeCompatibility<ElementType>::CopyFromOtherType, OtherAllocator>& Other)
	{
		TContainerElementTypeCompatibility<ElementType>::CopyingFromOtherType();
		DestructItems(GetData(), ArrayNum);
		CopyToEmpty(Other.GetData(), Other.Num(), ArrayMax);
		return *this;
	}

	/**
	 * Inserts given elements into the array at given location.
	 * Compatible element type version.
	 *
	 * @param Items Array of elements to insert.
	 * @param InIndex Tells where to insert the new elements.
	 * @returns Location at which the item was inserted.
	 */
	template <
		typename OtherAllocator,
		typename AliasElementType = ElementType
		UE_REQUIRES(TIsContainerElementTypeCopyable_V<AliasElementType>)
	>
	SizeType Insert(const TArray<typename TContainerElementTypeCompatibility<ElementType>::CopyFromOtherType, OtherAllocator>& Items, const SizeType InIndex)
	{
		TContainerElementTypeCompatibility<ElementType>::CopyingFromOtherType();

		auto NumNewElements = Items.Num();

		InsertUninitializedImpl(InIndex, NumNewElements);
		ConstructItems<ElementType>(GetData() + InIndex, Items.GetData(), NumNewElements);

		return InIndex;
	}

	/**
	 * Inserts given elements into the array at given location.
	 * Compatible element type version.
	 *
	 * @param Items Array of elements to insert.
	 * @param InIndex Tells where to insert the new elements.
	 * @returns Location at which the item was inserted.
	 */
	template <
		typename OtherAllocator,
		typename AliasElementType = ElementType
		UE_REQUIRES(TIsContainerElementTypeCopyable_V<AliasElementType>)
	>
	SizeType Insert(TArray<typename TContainerElementTypeCompatibility<ElementType>::CopyFromOtherType, OtherAllocator>&& Items, const SizeType InIndex)
	{
		check((const void*)this != (const void*)&Items);
		TContainerElementTypeCompatibility<ElementType>::CopyingFromOtherType();

		auto NumNewElements = Items.Num();

		InsertUninitializedImpl(InIndex, NumNewElements);
		RelocateConstructItems<ElementType>(GetData() + InIndex, Items.GetData(), NumNewElements);
		Items.ArrayNum = 0;

		Items.SlackTrackerNumChanged();

		return InIndex;
	}

	/**
	 * Adds a raw array of elements to the end of the TArray.
	 * Compatible element type version.
	 *
	 * @param Ptr   A pointer to an array of elements to add.
	 * @param Count The number of elements to insert from Ptr.
	 * @see Add, Insert
	 */
	template <
		typename AliasElementType = ElementType
		UE_REQUIRES(TIsContainerElementTypeCopyable_V<AliasElementType>)
	>
	void Append(const typename TContainerElementTypeCompatibility<ElementType>::CopyFromOtherType* Ptr, SizeType Count)
	{
		check(Ptr != nullptr || Count == 0);
		TContainerElementTypeCompatibility<ElementType>::CopyingFromOtherType();

		SizeType Pos = AddUninitialized(Count);
		ConstructItems<ElementType>(GetData() + Pos, Ptr, Count);
	}

private:

	/**
	 * Adds unique element to array if it doesn't exist.
	 *
	 * @param Args Item to add.
	 * @returns Index of the element in the array.
	 */
	template <typename ArgsType>
	SizeType AddUniqueImpl(ArgsType&& Args)
	{
		SizeType Index;
		if (Find(Args, Index))
		{
			return Index;
		}

		return Add(Forward<ArgsType>(Args));
	}

public:

	/**
	 * Adds unique element to array if it doesn't exist.
	 *
	 * Move semantics version.
	 *
	 * @param Item Item to add.
	 * @returns Index of the element in the array.
	 * @see Add, AddDefaulted, AddZeroed, Append, Insert
	 */
	FORCEINLINE SizeType AddUnique(ElementType&& Item)
	{
		return AddUniqueImpl(MoveTempIfPossible(Item));
	}

	/**
	 * Adds unique element to array if it doesn't exist.
	 *
	 * @param Item Item to add.
	 * @returns Index of the element in the array.
	 * @see Add, AddDefaulted, AddZeroed, Append, Insert
	 */
	FORCEINLINE SizeType AddUnique(const ElementType& Item)
	{
		return AddUniqueImpl(Item);
	}

	/**
	 * Reserves memory such that the array can contain at least Number elements.
	 *
	 * @param Number The number of elements that the array should be able to contain after allocation.
	 * @see Shrink
	 */
	FORCEINLINE void Reserve(SizeType Number)
	{
		checkSlow(Number >= 0);
		if (Number < 0)
		{
			// Cast to USizeType first to prevent sign extension on negative sizes, producing unusually large values.
			UE::Core::Private::OnInvalidArrayNum((unsigned long long)(USizeType)Number);
		}
		else if (Number > ArrayMax)
		{
			ResizeTo(Number);
		}
	}

	/**
	 * Sets the size of the array, filling it with the given element.
	 *
	 * @param Element The element to fill array with.
	 * @param Number The number of elements that the array should be able to contain after allocation.
	 */
	void Init(const ElementType& Element, SizeType Number)
	{
		Empty(Number);
		for (SizeType Index = 0; Index < Number; ++Index)
		{
			Add(Element);
		}
	}

	/**
	 * Removes the first occurrence of the specified item in the array,
	 * maintaining order but not indices.
	 *
	 * @param Item The item to remove.
	 * @returns The number of items removed. For RemoveSingleItem, this is always either 0 or 1.
	 * @see Add, Insert, Remove, RemoveAll, RemoveAllSwap
	 */
	SizeType RemoveSingle(const ElementType& Item)
	{
		SizeType Index = Find(Item);
		if (Index == INDEX_NONE)
		{
			return 0;
		}

		auto* RemovePtr = GetData() + Index;

		// Destruct items that match the specified Item.
		DestructItems(RemovePtr, 1);
		RelocateConstructItems<ElementType>(RemovePtr, RemovePtr + 1, ArrayNum - (Index + 1));

		// Update the array count
		--ArrayNum;

		SlackTrackerNumChanged();

		// Removed one item
		return 1;
	}

	/**
	 * Removes as many instances of Item as there are in the array, maintaining
	 * order but not indices.
	 *
	 * @param Item Item to remove from array.
	 * @returns Number of removed elements.
	 * @see Add, Insert, RemoveAll, RemoveAllSwap, RemoveSingle, RemoveSwap
	 */
	SizeType Remove(const ElementType& Item)
	{
		CheckAddress(&Item);

		// Element is non-const to preserve compatibility with existing code with a non-const operator==() member function
		return RemoveAll([&Item](ElementType& Element) { return Element == Item; });
	}

	/**
	 * Remove all instances that match the predicate, maintaining order but not indices
	 * Optimized to work with runs of matches/non-matches
	 *
	 * @param Predicate Predicate class instance
	 * @returns Number of removed elements.
	 * @see Add, Insert, RemoveAllSwap, RemoveSingle, RemoveSwap
	 */
	template <class PREDICATE_CLASS>
	SizeType RemoveAll(const PREDICATE_CLASS& Predicate)
	{
		const SizeType OriginalNum = ArrayNum;
		if (!OriginalNum)
		{
			return 0; // nothing to do, loop assumes one item so need to deal with this edge case here
		}

		ElementType* Data = GetData();

		SizeType WriteIndex = 0;
		SizeType ReadIndex = 0;
		bool bNotMatch = !::Invoke(Predicate, Data[ReadIndex]); // use a ! to guarantee it can't be anything other than zero or one
		do
		{
			SizeType RunStartIndex = ReadIndex++;
			while (ReadIndex < OriginalNum && bNotMatch == !::Invoke(Predicate, Data[ReadIndex]))
			{
				ReadIndex++;
			}
			SizeType RunLength = ReadIndex - RunStartIndex;
			checkSlow(RunLength > 0);
			if (bNotMatch)
			{
				// this was a non-matching run, we need to move it
				if (WriteIndex != RunStartIndex)
				{
					RelocateConstructItems<ElementType>(Data + WriteIndex, Data + RunStartIndex, RunLength);
				}
				WriteIndex += RunLength;
			}
			else
			{
				// this was a matching run, delete it
				DestructItems(Data + RunStartIndex, RunLength);
			}
			bNotMatch = !bNotMatch;
		} while (ReadIndex < OriginalNum);

		ArrayNum = WriteIndex;

		SlackTrackerNumChanged();

		return OriginalNum - ArrayNum;
	}

	/**
	 * Remove all instances that match the predicate
	 *
	 * @param Predicate Predicate class instance
	 * @param AllowShrinking Tell if this function can shrink the memory in-use if suitable.
	 * @see Remove, RemoveSingle, RemoveSingleSwap, RemoveSwap
	 */
	template <class PREDICATE_CLASS>
	SizeType RemoveAllSwap(const PREDICATE_CLASS& Predicate, EAllowShrinking AllowShrinking = EAllowShrinking::Yes)
	{
		bool bRemoved = false;
		const SizeType OriginalNum = ArrayNum;
		for (SizeType ItemIndex = 0; ItemIndex < Num();)
		{
			if (::Invoke(Predicate, (*this)[ItemIndex]))
			{
				bRemoved = true;
				RemoveAtSwap(ItemIndex, 1, EAllowShrinking::No);
			}
			else
			{
				++ItemIndex;
			}
		}

		if (bRemoved && AllowShrinking == EAllowShrinking::Yes)
		{
			ResizeShrink();
		}

		return OriginalNum - ArrayNum;
	}
	template <class PREDICATE_CLASS>
	UE_ALLOWSHRINKING_BOOL_DEPRECATED("RemoveAllSwap")
	FORCEINLINE SizeType RemoveAllSwap(const PREDICATE_CLASS& Predicate, bool bAllowShrinking)
	{
		return RemoveAllSwap(Predicate, bAllowShrinking ? EAllowShrinking::Yes : EAllowShrinking::No);
	}

	/**
	 * Removes the first occurrence of the specified item in the array. This version is much more efficient
	 * O(Count) instead of O(ArrayNum), but does not preserve the order
	 *
	 * @param Item The item to remove
	 * @param AllowShrinking Tell if this function can shrink the memory in-use if suitable.
	 *
	 * @returns The number of items removed. For RemoveSingleItem, this is always either 0 or 1.
	 * @see Add, Insert, Remove, RemoveAll, RemoveAllSwap, RemoveSwap
	 */
	SizeType RemoveSingleSwap(const ElementType& Item, EAllowShrinking AllowShrinking = EAllowShrinking::Yes)
	{
		SizeType Index = Find(Item);
		if (Index == INDEX_NONE)
		{
			return 0;
		}

		RemoveAtSwap(Index, 1, AllowShrinking);

		// Removed one item
		return 1;
	}
	UE_ALLOWSHRINKING_BOOL_DEPRECATED("RemoveSingleSwap")
	FORCEINLINE SizeType RemoveSingleSwap(const ElementType& Item, bool bAllowShrinking)
	{
		return RemoveSingleSwap(Item, bAllowShrinking ? EAllowShrinking::Yes : EAllowShrinking::No);
	}

	/**
	 * Removes all instances of a given item from the array.
	 *
	 * This version is much more efficient, because it uses RemoveAtSwap
	 * internally which is O(Count) instead of RemoveAt which is O(ArrayNum),
	 * but does not preserve the order.
	 *
	 * @param Item The item to remove
	 * @param AllowShrinking Tell if this function can shrink the memory in-use if suitable.
	 *
	 * @returns Number of elements removed.
	 * @see Add, Insert, Remove, RemoveAll, RemoveAllSwap
	 */
	SizeType RemoveSwap(const ElementType& Item, EAllowShrinking AllowShrinking = EAllowShrinking::Yes)
	{
		CheckAddress(&Item);

		const SizeType OriginalNum = ArrayNum;
		bool bRemoved = false;
		for (SizeType Index = 0; Index < ArrayNum; Index++)
		{
			if ((*this)[Index] == Item)
			{
				bRemoved = true;
				RemoveAtSwap(Index--, 1, EAllowShrinking::No);
			}
		}

		if (bRemoved && AllowShrinking == EAllowShrinking::Yes)
		{
			ResizeShrink();
		}

		return OriginalNum - ArrayNum;
	}
	UE_ALLOWSHRINKING_BOOL_DEPRECATED("RemoveSwap")
	FORCEINLINE SizeType RemoveSwap(const ElementType& Item, bool bAllowShrinking)
	{
		return RemoveSwap(Item, bAllowShrinking ? EAllowShrinking::Yes : EAllowShrinking::No);
	}

	/**
	 * Element-wise array memory swap.
	 *
	 * @param FirstIndexToSwap Position of the first element to swap.
	 * @param SecondIndexToSwap Position of the second element to swap.
	 */
	FORCEINLINE void SwapMemory(SizeType FirstIndexToSwap, SizeType SecondIndexToSwap)
	{
		::Swap(
			*(ElementType*)((uint8*)AllocatorInstance.GetAllocation() + (sizeof(ElementType)*FirstIndexToSwap)),
			*(ElementType*)((uint8*)AllocatorInstance.GetAllocation() + (sizeof(ElementType)*SecondIndexToSwap))
		);
	}

	/**
	 * Element-wise array element swap.
	 *
	 * This version is doing more sanity checks than SwapMemory.
	 *
	 * @param FirstIndexToSwap Position of the first element to swap.
	 * @param SecondIndexToSwap Position of the second element to swap.
	 */
	FORCEINLINE void Swap(SizeType FirstIndexToSwap, SizeType SecondIndexToSwap)
	{
		check((FirstIndexToSwap >= 0) && (SecondIndexToSwap >= 0));
		check((ArrayNum > FirstIndexToSwap) && (ArrayNum > SecondIndexToSwap));
		if (FirstIndexToSwap != SecondIndexToSwap)
		{
			SwapMemory(FirstIndexToSwap, SecondIndexToSwap);
		}
	}

	/**
	 * Searches for the first entry of the specified type, will only work with
	 * TArray<UObject*>. Optionally return the item's index, and can specify
	 * the start index.
	 *
	 * @param Item (Optional output) If it's not null, then it will point to
	 *             the found element. Untouched if element hasn't been found.
	 * @param ItemIndex (Optional output) If it's not null, then it will be set
	 *             to the position of found element in the array. Untouched if
	 *             element hasn't been found.
	 * @param StartIndex (Optional) Index in array at which the function should
	 *             start to look for element.
	 * @returns True if element was found. False otherwise.
	 */
	template<typename SearchType>
	bool FindItemByClass(SearchType **Item = nullptr, SizeType *ItemIndex = nullptr, SizeType StartIndex = 0) const
	{
		UClass* SearchClass = SearchType::StaticClass();
		for (SizeType Idx = StartIndex; Idx < ArrayNum; Idx++)
		{
			if ((*this)[Idx] != nullptr && (*this)[Idx]->IsA(SearchClass))
			{
				if (Item != nullptr)
				{
					*Item = (SearchType*)((*this)[Idx]);
				}
				if (ItemIndex != nullptr)
				{
					*ItemIndex = Idx;
				}
				return true;
			}
		}
		return false;
	}

	// Iterators
	typedef TIndexedContainerIterator<      TArray,       ElementType, SizeType> TIterator;
	typedef TIndexedContainerIterator<const TArray, const ElementType, SizeType> TConstIterator;

	/**
	 * Creates an iterator for the contents of this array
	 *
	 * @returns The iterator.
	 */
	TIterator CreateIterator()
	{
		return TIterator(*this);
	}

	/**
	 * Creates a const iterator for the contents of this array
	 *
	 * @returns The const iterator.
	 */
	TConstIterator CreateConstIterator() const
	{
		return TConstIterator(*this);
	}

	#if TARRAY_RANGED_FOR_CHECKS
		typedef TCheckedPointerIterator<      ElementType, SizeType, false> RangedForIteratorType;
		typedef TCheckedPointerIterator<const ElementType, SizeType, false> RangedForConstIteratorType;
		typedef TCheckedPointerIterator<      ElementType, SizeType, true>  RangedForReverseIteratorType;
		typedef TCheckedPointerIterator<const ElementType, SizeType, true>  RangedForConstReverseIteratorType;
	#else
		typedef                               ElementType* RangedForIteratorType;
		typedef                         const ElementType* RangedForConstIteratorType;
		typedef TReversePointerIterator<      ElementType> RangedForReverseIteratorType;
		typedef TReversePointerIterator<const ElementType> RangedForConstReverseIteratorType;
	#endif

public:

	/**
	 * DO NOT USE DIRECTLY
	 * STL-like iterators to enable range-based for loop support.
	 */
	#if TARRAY_RANGED_FOR_CHECKS
		FORCEINLINE RangedForIteratorType             begin ()       { return RangedForIteratorType            (ArrayNum, GetData()); }
		FORCEINLINE RangedForConstIteratorType        begin () const { return RangedForConstIteratorType       (ArrayNum, GetData()); }
		FORCEINLINE RangedForIteratorType             end   ()       { return RangedForIteratorType            (ArrayNum, GetData() + Num()); }
		FORCEINLINE RangedForConstIteratorType        end   () const { return RangedForConstIteratorType       (ArrayNum, GetData() + Num()); }
		FORCEINLINE RangedForReverseIteratorType      rbegin()       { return RangedForReverseIteratorType     (ArrayNum, GetData() + Num()); }
		FORCEINLINE RangedForConstReverseIteratorType rbegin() const { return RangedForConstReverseIteratorType(ArrayNum, GetData() + Num()); }
		FORCEINLINE RangedForReverseIteratorType      rend  ()       { return RangedForReverseIteratorType     (ArrayNum, GetData()); }
		FORCEINLINE RangedForConstReverseIteratorType rend  () const { return RangedForConstReverseIteratorType(ArrayNum, GetData()); }
	#else
		FORCEINLINE RangedForIteratorType             begin ()       { return                                   GetData(); }
		FORCEINLINE RangedForConstIteratorType        begin () const { return                                   GetData(); }
		FORCEINLINE RangedForIteratorType             end   ()       { return                                   GetData() + Num(); }
		FORCEINLINE RangedForConstIteratorType        end   () const { return                                   GetData() + Num(); }
		FORCEINLINE RangedForReverseIteratorType      rbegin()       { return RangedForReverseIteratorType     (GetData() + Num()); }
		FORCEINLINE RangedForConstReverseIteratorType rbegin() const { return RangedForConstReverseIteratorType(GetData() + Num()); }
		FORCEINLINE RangedForReverseIteratorType      rend  ()       { return RangedForReverseIteratorType     (GetData()); }
		FORCEINLINE RangedForConstReverseIteratorType rend  () const { return RangedForConstReverseIteratorType(GetData()); }
	#endif

public:

	/**
	 * Sorts the array assuming < operator is defined for the item type.
	 *
	 * @note: If your array contains raw pointers, they will be automatically dereferenced during sorting.
	 *        Therefore, your array will be sorted by the values being pointed to, rather than the pointers' values.
	 *        If this is not desirable, please use Algo::Sort(MyArray) directly instead.
	 *        The auto-dereferencing behavior does not occur with smart pointers.
	 */
	void Sort()
	{
		Algo::Sort(*this, TDereferenceWrapper<ElementType, TLess<>>(TLess<>()));
	}

	/**
	 * Sorts the array using user define predicate class.
	 *
	 * @param Predicate Predicate class instance.
	 *
	 * @note: If your array contains raw pointers, they will be automatically dereferenced during sorting.
	 *        Therefore, your predicate will be passed references rather than pointers.
	 *        If this is not desirable, please use Algo::Sort(MyArray, Predicate) directly instead.
	 *        The auto-dereferencing behavior does not occur with smart pointers.
	 */
	template <class PREDICATE_CLASS>
	void Sort(const PREDICATE_CLASS& Predicate)
	{
		TDereferenceWrapper<ElementType, PREDICATE_CLASS> PredicateWrapper(Predicate);
		Algo::Sort(*this, PredicateWrapper);
	}

	/**
	 * Stable sorts the array assuming < operator is defined for the item type.
	 *
	 * Stable sort is slower than non-stable algorithm.
	 *
	 * @note: If your array contains raw pointers, they will be automatically dereferenced during sorting.
	 *        Therefore, your array will be sorted by the values being pointed to, rather than the pointers' values.
	 *        If this is not desirable, please use Algo::StableSort(MyArray) directly instead.
	 *        The auto-dereferencing behavior does not occur with smart pointers.
	 */
	void StableSort()
	{
		Algo::StableSort(*this, TDereferenceWrapper<ElementType, TLess<>>(TLess<>()));
	}

	/**
	 * Stable sorts the array using user defined predicate class.
	 *
	 * Stable sort is slower than non-stable algorithm.
	 *
	 * @param Predicate Predicate class instance
	 *
	 * @note: If your array contains raw pointers, they will be automatically dereferenced during sorting.
	 *        Therefore, your predicate will be passed references rather than pointers.
	 *        If this is not desirable, please use Algo::StableSort(MyArray, Predicate) directly instead.
	 *        The auto-dereferencing behavior does not occur with smart pointers.
	 */
	template <class PREDICATE_CLASS>
	void StableSort(const PREDICATE_CLASS& Predicate)
	{
		TDereferenceWrapper<ElementType, PREDICATE_CLASS> PredicateWrapper(Predicate);
		Algo::StableSort(*this, PredicateWrapper);
	}

#if defined(_MSC_VER) && !defined(__clang__)	// Relies on MSVC-specific lazy template instantiation to support arrays of incomplete types
private:
	/**
	 * Helper function that can be used inside the debuggers watch window to debug TArrays. E.g. "*Class->Defaults.DebugGet(5)". 
	 *
	 * @param Index Position to get.
	 * @returns Reference to the element at given position.
	 */
	FORCENOINLINE const ElementType& DebugGet(SizeType Index) const
	{
		return GetData()[Index];
	}
#endif

private:
	void AllocatorResizeAllocation(SizeType CurrentArrayNum, SizeType NewArrayMax)
	{
		if constexpr (TAllocatorTraits<AllocatorType>::SupportsElementAlignment)
		{
			AllocatorInstance.ResizeAllocation(CurrentArrayNum, NewArrayMax, sizeof(ElementType), alignof(ElementType));
		}
		else
		{
			AllocatorInstance.ResizeAllocation(CurrentArrayNum, NewArrayMax, sizeof(ElementType));
		}
		SlackTrackerNumChanged();
	}

	SizeType AllocatorCalculateSlackShrink(SizeType CurrentArrayNum, SizeType NewArrayMax)
	{
		if constexpr (TAllocatorTraits<AllocatorType>::SupportsElementAlignment)
		{
			return AllocatorInstance.CalculateSlackShrink(CurrentArrayNum, NewArrayMax, sizeof(ElementType), alignof(ElementType));
		}
		else
		{
			return AllocatorInstance.CalculateSlackShrink(CurrentArrayNum, NewArrayMax, sizeof(ElementType));
		}
	}

	SizeType AllocatorCalculateSlackGrow(SizeType CurrentArrayNum, SizeType NewArrayMax)
	{
		if constexpr (TAllocatorTraits<AllocatorType>::SupportsElementAlignment)
		{
			return AllocatorInstance.CalculateSlackGrow(CurrentArrayNum, NewArrayMax, sizeof(ElementType), alignof(ElementType));
		}
		else
		{
			return AllocatorInstance.CalculateSlackGrow(CurrentArrayNum, NewArrayMax, sizeof(ElementType));
		}
	}

	SizeType AllocatorCalculateSlackReserve(SizeType NewArrayMax)
	{
		if constexpr (TAllocatorTraits<AllocatorType>::SupportsElementAlignment)
		{
			return AllocatorInstance.CalculateSlackReserve(NewArrayMax, sizeof(ElementType), alignof(ElementType));
		}
		else
		{
			return AllocatorInstance.CalculateSlackReserve(NewArrayMax, sizeof(ElementType));
		}
	}

	FORCENOINLINE void ResizeGrow(SizeType OldNum)
	{
		SizeType LocalArrayNum = ArrayNum;

		// This should only happen when we've underflowed or overflowed SizeType in the caller
		if (LocalArrayNum < OldNum)
		{
			// Cast to USizeType first to prevent sign extension on negative sizes, producing unusually large values.
			UE::Core::Private::OnInvalidArrayNum((unsigned long long)(USizeType)LocalArrayNum);
		}
		ArrayMax = AllocatorCalculateSlackGrow(LocalArrayNum, ArrayMax);
		AllocatorResizeAllocation(OldNum, ArrayMax);
	}
	FORCENOINLINE void ResizeShrink()
	{
		const SizeType NewArrayMax = AllocatorCalculateSlackShrink(ArrayNum, ArrayMax);
		if (NewArrayMax != ArrayMax)
		{
			ArrayMax = NewArrayMax;
			check(ArrayMax >= ArrayNum);
			AllocatorResizeAllocation(ArrayNum, ArrayMax);
		}
	}
	FORCENOINLINE void ResizeTo(SizeType NewMax)
	{
		if (NewMax)
		{
			NewMax = AllocatorCalculateSlackReserve(NewMax);
		}
		if (NewMax != ArrayMax)
		{
			ArrayMax = NewMax;
			AllocatorResizeAllocation(ArrayNum, ArrayMax);
		}
	}
	FORCENOINLINE void ResizeForCopy(SizeType NewMax, SizeType PrevMax)
	{
		if (NewMax)
		{
			NewMax = AllocatorCalculateSlackReserve(NewMax);
		}
		if (NewMax > PrevMax)
		{
			AllocatorResizeAllocation(0, NewMax);
			ArrayMax = NewMax;
		}
		else
		{
			ArrayMax = PrevMax;
		}
	}


	/**
	 * Copies data from one array into this array. Uses the fast path if the
	 * data in question does not need a constructor.
	 *
	 * @param Source The source array to copy
	 * @param PrevMax The previous allocated size
	 */
	template <typename OtherElementType, typename OtherSizeType>
	void CopyToEmpty(const OtherElementType* OtherData, OtherSizeType OtherNum, SizeType PrevMax)
	{
		SizeType NewNum = (SizeType)OtherNum;
		checkf((OtherSizeType)NewNum == OtherNum, TEXT("Invalid number of elements to add to this array type: %lld"), (long long)NewNum);

		ArrayNum = NewNum;
		if (OtherNum || PrevMax)
		{
			ResizeForCopy(NewNum, PrevMax);
			ConstructItems<ElementType>(GetData(), OtherData, OtherNum);
		}
		else
		{
			ArrayMax = AllocatorInstance.GetInitialCapacity();
		}

		SlackTrackerNumChanged();
	}

	/**
	 * Copies data from one array into this array. Uses the fast path if the
	 * data in question does not need a constructor.
	 *
	 * @param Source The source array to copy
	 * @param PrevMax The previous allocated size
	 * @param ExtraSlack Additional amount of memory to allocate at
	 *                   the end of the buffer. Counted in elements.
	 */
	template <typename OtherElementType, typename OtherSizeType>
	void CopyToEmptyWithSlack(const OtherElementType* OtherData, OtherSizeType OtherNum, SizeType PrevMax, SizeType ExtraSlack)
	{
		SizeType NewNum = (SizeType)OtherNum;
		checkf((OtherSizeType)NewNum == OtherNum, TEXT("Invalid number of elements to add to this array type: %lld"), (long long)NewNum);

		ArrayNum = NewNum;
		if (OtherNum || ExtraSlack || PrevMax)
		{
			USizeType NewMax = NewNum + ExtraSlack;

			// This should only happen when we've underflowed or overflowed SizeType
			if ((SizeType)NewMax < NewNum)
			{
				UE::Core::Private::OnInvalidArrayNum((unsigned long long)NewMax);
			}

			ResizeForCopy(NewNum + ExtraSlack, PrevMax);
			ConstructItems<ElementType>(GetData(), OtherData, OtherNum);
		}
		else
		{
			ArrayMax = AllocatorInstance.GetInitialCapacity();
		}

		SlackTrackerNumChanged();
	}

protected:

	template<typename ElementType, typename AllocatorType>
	friend class TIndirectArray;

	ElementAllocatorType AllocatorInstance;
	SizeType             ArrayNum;
	SizeType             ArrayMax;

public:
	void WriteMemoryImage(FMemoryImageWriter& Writer) const
	{
		if constexpr (TAllocatorTraits<AllocatorType>::SupportsFreezeMemoryImage && THasTypeLayout<ElementType>::Value)
		{
			this->AllocatorInstance.WriteMemoryImage(Writer, StaticGetTypeLayoutDesc<ElementType>(), this->ArrayNum);
			Writer.WriteBytes(this->ArrayNum);
			Writer.WriteBytes(this->ArrayNum);
		}
		else
		{
			// Writing non-freezable TArray is only supported for 64-bit target for now
			// Would need complete layout macros for all allocator types in order to properly write (empty) 32bit versions
			check(Writer.Is64BitTarget());
			Writer.WriteBytes(TArray());
		}
	}

	void CopyUnfrozen(const FMemoryUnfreezeContent& Context, void* Dst) const
	{
		if constexpr (TAllocatorTraits<AllocatorType>::SupportsFreezeMemoryImage && THasTypeLayout<ElementType>::Value)
		{
			TArray* DstArray = new(Dst) TArray();
			DstArray->SetNumZeroed(this->ArrayNum);
			this->AllocatorInstance.CopyUnfrozen(Context, StaticGetTypeLayoutDesc<ElementType>(), this->ArrayNum, DstArray->GetData());
		}
		else
		{
			new(Dst) TArray();
		}
	}

	static void AppendHash(const FPlatformTypeLayoutParameters& LayoutParams, FSHA1& Hasher)
	{
		if constexpr (TAllocatorTraits<AllocatorType>::SupportsFreezeMemoryImage && THasTypeLayout<ElementType>::Value)
		{
			Freeze::AppendHash(StaticGetTypeLayoutDesc<ElementType>(), LayoutParams, Hasher);
		}
	}

	void ToString(const FPlatformTypeLayoutParameters& LayoutParams, FMemoryToStringContext& OutContext) const
	{
		if constexpr (TAllocatorTraits<AllocatorType>::SupportsFreezeMemoryImage && THasTypeLayout<ElementType>::Value)
		{
			this->AllocatorInstance.ToString(StaticGetTypeLayoutDesc<ElementType>(), this->ArrayNum, this->ArrayMax, LayoutParams, OutContext);
		}
	}

	/**
	* Implicit heaps
	*/
public:
	/** 
	 * Builds an implicit heap from the array.
	 *
	 * @param Predicate Predicate class instance.
	 *
	 * @note: If your array contains raw pointers, they will be automatically dereferenced during heapification.
	 *        Therefore, your predicate will be passed references rather than pointers.
	 *        The auto-dereferencing behavior does not occur with smart pointers.
	 */
	template <class PREDICATE_CLASS>
	FORCEINLINE void Heapify(const PREDICATE_CLASS& Predicate)
	{
		TDereferenceWrapper<ElementType, PREDICATE_CLASS> PredicateWrapper(Predicate);
		Algo::Heapify(*this, PredicateWrapper);
	}

	/**
	 * Builds an implicit heap from the array. Assumes < operator is defined
	 * for the template type.
	 *
	 * @note: If your array contains raw pointers, they will be automatically dereferenced during heapification.
	 *        Therefore, your array will be heapified by the values being pointed to, rather than the pointers' values.
	 *        The auto-dereferencing behavior does not occur with smart pointers.
	 */
	void Heapify()
	{
		Heapify(TLess<ElementType>());
	}

	/** 
	 * Adds a new element to the heap.
	 *
	 * @param InItem Item to be added.
	 * @param Predicate Predicate class instance.
	 * @return The index of the new element.
	 *
	 * @note: If your array contains raw pointers, they will be automatically dereferenced during heapification.
	 *        Therefore, your predicate will be passed references rather than pointers.
	 *        The auto-dereferencing behavior does not occur with smart pointers.
	 */
	template <class PREDICATE_CLASS>
	SizeType HeapPush(ElementType&& InItem, const PREDICATE_CLASS& Predicate)
	{
		// Add at the end, then sift up
		Add(MoveTempIfPossible(InItem));
		TDereferenceWrapper<ElementType, PREDICATE_CLASS> PredicateWrapper(Predicate);
		SizeType Result = AlgoImpl::HeapSiftUp(GetData(), (SizeType)0, Num() - 1, FIdentityFunctor(), PredicateWrapper);

		return Result;
	}

	/** 
	 * Adds a new element to the heap.
	 *
	 * @param InItem Item to be added.
	 * @param Predicate Predicate class instance.
	 * @return The index of the new element.
	 *
	 * @note: If your array contains raw pointers, they will be automatically dereferenced during heapification.
	 *        Therefore, your predicate will be passed references rather than pointers.
	 *        The auto-dereferencing behavior does not occur with smart pointers.
	 */
	template <class PREDICATE_CLASS>
	SizeType HeapPush(const ElementType& InItem, const PREDICATE_CLASS& Predicate)
	{
		// Add at the end, then sift up
		Add(InItem);
		TDereferenceWrapper<ElementType, PREDICATE_CLASS> PredicateWrapper(Predicate);
		SizeType Result = AlgoImpl::HeapSiftUp(GetData(), (SizeType)0, Num() - 1, FIdentityFunctor(), PredicateWrapper);

		return Result;
	}

	/** 
	 * Adds a new element to the heap. Assumes < operator is defined for the
	 * template type.
	 *
	 * @param InItem Item to be added.
	 * @return The index of the new element.
	 *
	 * @note: If your array contains raw pointers, they will be automatically dereferenced during heapification.
	 *        Therefore, your array will be heapified by the values being pointed to, rather than the pointers' values.
	 *        The auto-dereferencing behavior does not occur with smart pointers.
	 */
	SizeType HeapPush(ElementType&& InItem)
	{
		return HeapPush(MoveTempIfPossible(InItem), TLess<ElementType>());
	}

	/** 
	 * Adds a new element to the heap. Assumes < operator is defined for the
	 * template type.
	 *
	 * @param InItem Item to be added.
	 * @return The index of the new element.
	 *
	 * @note: If your array contains raw pointers, they will be automatically dereferenced during heapification.
	 *        Therefore, your array will be heapified by the values being pointed to, rather than the pointers' values.
	 *        The auto-dereferencing behavior does not occur with smart pointers.
	 */
	SizeType HeapPush(const ElementType& InItem)
	{
		return HeapPush(InItem, TLess<ElementType>());
	}

	/** 
	 * Removes the top element from the heap.
	 *
	 * @param OutItem The removed item.
	 * @param Predicate Predicate class instance.
	 * @param AllowShrinking Tell if this function can shrink the memory in-use if suitable.
	 *
	 * @note: If your array contains raw pointers, they will be automatically dereferenced during heapification.
	 *        Therefore, your predicate will be passed references rather than pointers.
	 *        The auto-dereferencing behavior does not occur with smart pointers.
	 */
	template <class PREDICATE_CLASS>
	void HeapPop(ElementType& OutItem, const PREDICATE_CLASS& Predicate, EAllowShrinking AllowShrinking = EAllowShrinking::Yes)
	{
		OutItem = MoveTemp((*this)[0]);
		RemoveAtSwap(0, 1, AllowShrinking);

		TDereferenceWrapper< ElementType, PREDICATE_CLASS> PredicateWrapper(Predicate);
		AlgoImpl::HeapSiftDown(GetData(), (SizeType)0, Num(), FIdentityFunctor(), PredicateWrapper);
	}
	template <class PREDICATE_CLASS>
	UE_ALLOWSHRINKING_BOOL_DEPRECATED("HeapPop")
	FORCEINLINE void HeapPop(ElementType& OutItem, const PREDICATE_CLASS& Predicate, bool bAllowShrinking)
	{
		HeapPop(OutItem, Predicate, bAllowShrinking ? EAllowShrinking::Yes : EAllowShrinking::No);
	}

	/** 
	 * Removes the top element from the heap. Assumes < operator is defined for
	 * the template type.
	 *
	 * @param OutItem The removed item.
	 * @param AllowShrinking (Optional) Tells if this call can shrink the array allocation if suitable after the pop. Default is yes.
	 *
	 * @note: If your array contains raw pointers, they will be automatically dereferenced during heapification.
	 *        Therefore, your array will be heapified by the values being pointed to, rather than the pointers' values.
	 *        The auto-dereferencing behavior does not occur with smart pointers.
	 */
	void HeapPop(ElementType& OutItem, EAllowShrinking AllowShrinking = EAllowShrinking::Yes)
	{
		HeapPop(OutItem, TLess<ElementType>(), AllowShrinking);
	}
	UE_ALLOWSHRINKING_BOOL_DEPRECATED("HeapPop")
	FORCEINLINE void HeapPop(ElementType& OutItem, bool bAllowShrinking)
	{
		HeapPop(OutItem, bAllowShrinking ? EAllowShrinking::Yes : EAllowShrinking::No);
	}

	/**
	 * Verifies the heap.
	 *
	 * @param Predicate Predicate class instance.
	 */
	template <class PREDICATE_CLASS>
	void VerifyHeap(const PREDICATE_CLASS& Predicate)
	{
		check(Algo::IsHeap(*this, Predicate));
	}

	/** 
	 * Removes the top element from the heap.
	 *
	 * @param Predicate Predicate class instance.
	 * @param AllowShrinking (Optional) Tells if this call can shrink the array allocation if suitable after the discard. Default is yes.
	 *
	 * @note: If your array contains raw pointers, they will be automatically dereferenced during heapification.
	 *        Therefore, your predicate will be passed references rather than pointers.
	 *        The auto-dereferencing behavior does not occur with smart pointers.
	 */
	template <class PREDICATE_CLASS>
	void HeapPopDiscard(const PREDICATE_CLASS& Predicate, EAllowShrinking AllowShrinking = EAllowShrinking::Yes)
	{
		RemoveAtSwap(0, 1, AllowShrinking);
		TDereferenceWrapper< ElementType, PREDICATE_CLASS> PredicateWrapper(Predicate);
		AlgoImpl::HeapSiftDown(GetData(), (SizeType)0, Num(), FIdentityFunctor(), PredicateWrapper);
	}
	template <class PREDICATE_CLASS>
	UE_ALLOWSHRINKING_BOOL_DEPRECATED("HeapPopDiscard")
	FORCEINLINE void HeapPopDiscard(const PREDICATE_CLASS& Predicate, bool bAllowShrinking)
	{
		HeapPopDiscard(Predicate, bAllowShrinking ? EAllowShrinking::Yes : EAllowShrinking::No);
	}

	/** 
	 * Removes the top element from the heap. Assumes < operator is defined for the template type.
	 *
	 * @param AllowShrinking (Optional) Tells if this call can shrink the array
	 *		allocation if suitable after the discard. Default is yes.
	 *
	 * @note: If your array contains raw pointers, they will be automatically dereferenced during heapification.
	 *        Therefore, your array will be heapified by the values being pointed to, rather than the pointers' values.
	 *        The auto-dereferencing behavior does not occur with smart pointers.
	 */
	void HeapPopDiscard(EAllowShrinking AllowShrinking = EAllowShrinking::Yes)
	{
		HeapPopDiscard(TLess<ElementType>(), AllowShrinking);
	}
	UE_ALLOWSHRINKING_BOOL_DEPRECATED("HeapPopDiscard")
	FORCEINLINE void HeapPopDiscard(bool bAllowShrinking)
	{
		HeapPopDiscard(TLess<ElementType>(), bAllowShrinking ? EAllowShrinking::Yes : EAllowShrinking::No);
	}

	/** 
	 * Returns the top element from the heap (does not remove the element).
	 *
	 * Const version.
	 *
	 * @returns The reference to the top element from the heap.
	 */
	const ElementType& HeapTop() const
	{
		return (*this)[0];
	}

	/** 
	 * Returns the top element from the heap (does not remove the element).
	 *
	 * @returns The reference to the top element from the heap.
	 */
	ElementType& HeapTop()
	{
		return (*this)[0];
	}

	/**
	 * Removes an element from the heap.
	 *
	 * @param Index Position at which to remove item.
	 * @param Predicate Predicate class instance.
	 * @param AllowShrinking (Optional) Tells if this call can shrink the array allocation
	 *		if suitable after the remove (default = yes).
	 *
	 * @note: If your array contains raw pointers, they will be automatically dereferenced during heapification.
	 *        Therefore, your predicate will be passed references rather than pointers.
	 *        The auto-dereferencing behavior does not occur with smart pointers.
	 */
	template <class PREDICATE_CLASS>
	void HeapRemoveAt(SizeType Index, const PREDICATE_CLASS& Predicate, EAllowShrinking AllowShrinking = EAllowShrinking::Yes)
	{
		RemoveAtSwap(Index, 1, AllowShrinking);

		TDereferenceWrapper< ElementType, PREDICATE_CLASS> PredicateWrapper(Predicate);
		AlgoImpl::HeapSiftDown(GetData(), Index, Num(), FIdentityFunctor(), PredicateWrapper);
		AlgoImpl::HeapSiftUp(GetData(), (SizeType)0, FPlatformMath::Min(Index, Num() - 1), FIdentityFunctor(), PredicateWrapper);
	}
	template <class PREDICATE_CLASS>
	UE_ALLOWSHRINKING_BOOL_DEPRECATED("HeapRemoveAt")
	void HeapRemoveAt(SizeType Index, const PREDICATE_CLASS& Predicate, bool bAllowShrinking)
	{
		HeapRemoveAt(Index, Predicate, bAllowShrinking ? EAllowShrinking::Yes : EAllowShrinking::No);
	}

	/**
	 * Removes an element from the heap. Assumes < operator is defined for the template type.
	 *
	 * @param Index Position at which to remove item.
	 * @param AllowShrinking (Optional) Tells if this call can shrink the array allocation
	 *		if suitable after the remove (default = yes).
	 *
	 * @note: If your array contains raw pointers, they will be automatically dereferenced during heapification.
	 *        Therefore, your array will be heapified by the values being pointed to, rather than the pointers' values.
	 *        The auto-dereferencing behavior does not occur with smart pointers.
	 */
	void HeapRemoveAt(SizeType Index, EAllowShrinking AllowShrinking = EAllowShrinking::Yes)
	{
		HeapRemoveAt(Index, TLess< ElementType >(), AllowShrinking);
	}
	UE_ALLOWSHRINKING_BOOL_DEPRECATED("HeapRemoveAt")
	FORCEINLINE void HeapRemoveAt(SizeType Index, bool bAllowShrinking)
	{
		HeapRemoveAt(Index, bAllowShrinking ? EAllowShrinking::Yes : EAllowShrinking::No);
	}

	/**
	 * Performs heap sort on the array.
	 *
	 * @param Predicate Predicate class instance.
	 *
	 * @note: If your array contains raw pointers, they will be automatically dereferenced during heapification.
	 *        Therefore, your predicate will be passed references rather than pointers.
	 *        The auto-dereferencing behavior does not occur with smart pointers.
	 */
	template <class PREDICATE_CLASS>
	void HeapSort(const PREDICATE_CLASS& Predicate)
	{
		TDereferenceWrapper<ElementType, PREDICATE_CLASS> PredicateWrapper(Predicate);
		Algo::HeapSort(*this, PredicateWrapper);
	}

	/**
	 * Performs heap sort on the array. Assumes < operator is defined for the
	 * template type.
	 *
	 * @note: If your array contains raw pointers, they will be automatically dereferenced during heapification.
	 *        Therefore, your array will be heapified by the values being pointed to, rather than the pointers' values.
	 *        The auto-dereferencing behavior does not occur with smart pointers.
	 */
	void HeapSort()
	{
		HeapSort(TLess<ElementType>());
	}

	const ElementAllocatorType& GetAllocatorInstance() const { return AllocatorInstance; }
	ElementAllocatorType& GetAllocatorInstance() { return AllocatorInstance; }

	friend struct TArrayPrivateFriend;
};


namespace Freeze
{
	template<typename T, typename AllocatorType>
	void IntrinsicWriteMemoryImage(FMemoryImageWriter& Writer, const TArray<T, AllocatorType>& Object, const FTypeLayoutDesc&)
	{
		Object.WriteMemoryImage(Writer);
	}

	template<typename T, typename AllocatorType>
	uint32 IntrinsicUnfrozenCopy(const FMemoryUnfreezeContent& Context, const TArray<T, AllocatorType>& Object, void* OutDst)
	{
		Object.CopyUnfrozen(Context, OutDst);
		return sizeof(Object);
	}

	template<typename T, typename AllocatorType>
	uint32 IntrinsicAppendHash(const TArray<T, AllocatorType>* DummyObject, const FTypeLayoutDesc& TypeDesc, const FPlatformTypeLayoutParameters& LayoutParams, FSHA1& Hasher)
	{
		return AppendHashForNameAndSize(TypeDesc.Name, sizeof(TArray<T, AllocatorType>), Hasher);
	}

	template<typename T, typename AllocatorType>
	uint32 IntrinsicGetTargetAlignment(const TArray<T, AllocatorType>* DummyObject, const FTypeLayoutDesc& TypeDesc, const FPlatformTypeLayoutParameters& LayoutParams)
	{
		// Assume alignment of array is drive by pointer
		return FMath::Min(8u, LayoutParams.MaxFieldAlignment);
	}

	template<typename T, typename AllocatorType>
	void IntrinsicToString(const TArray<T, AllocatorType>& Object, const FTypeLayoutDesc& TypeDesc, const FPlatformTypeLayoutParameters& LayoutParams, FMemoryToStringContext& OutContext)
	{
		Object.ToString(LayoutParams, OutContext);
	}
}

DECLARE_TEMPLATE_INTRINSIC_TYPE_LAYOUT((template <typename T, typename AllocatorType>), (TArray<T, AllocatorType>));

template <typename InElementType, typename AllocatorType>
struct TIsZeroConstructType<TArray<InElementType, AllocatorType>>
{
	enum { Value = TAllocatorTraits<AllocatorType>::IsZeroConstruct };
};

template <typename T, typename AllocatorType>
struct TIsContiguousContainer<TArray<T, AllocatorType>>
{
	enum { Value = true };
};

/**
 * Trait which determines whether or not a type is a TArray.
 */
template <typename T> constexpr bool TIsTArray_V = false;

template <typename InElementType, typename InAllocatorType> constexpr bool TIsTArray_V<               TArray<InElementType, InAllocatorType>> = true;
template <typename InElementType, typename InAllocatorType> constexpr bool TIsTArray_V<const          TArray<InElementType, InAllocatorType>> = true;
template <typename InElementType, typename InAllocatorType> constexpr bool TIsTArray_V<      volatile TArray<InElementType, InAllocatorType>> = true;
template <typename InElementType, typename InAllocatorType> constexpr bool TIsTArray_V<const volatile TArray<InElementType, InAllocatorType>> = true;

template <typename T>
struct TIsTArray
{
	enum { Value = TIsTArray_V<T> };
};


//
// Array operator news.
//
template <typename T,typename AllocatorType> void* operator new(size_t Size, TArray<T, AllocatorType>& Array)
{
	check(Size == sizeof(T));
	const auto Index = Array.AddUninitialized();
	return &Array[Index];
}
template <typename T,typename AllocatorType> void* operator new( size_t Size, TArray<T,AllocatorType>& Array, typename TArray<T, AllocatorType>::SizeType Index )
{
	check(Size == sizeof(T));
	Array.InsertUninitialized(Index);
	return &Array[Index];
}

struct TArrayPrivateFriend
{
	/**
	 * Serialization operator.
	 *
	 * @param Ar Archive to serialize the array with.
	 * @param A Array to serialize.
	 * @returns Passing the given archive.
	 */
	template<typename ElementType, typename AllocatorType>
	static FArchive& Serialize(FArchive& Ar, TArray<ElementType, AllocatorType>& A)
	{
		A.CountBytes(Ar);

		// For net archives, limit serialization to 16MB, to protect against excessive allocation
		typedef typename AllocatorType::SizeType SizeType;
		constexpr SizeType MaxNetArraySerialize = (16 * 1024 * 1024) / sizeof(ElementType);
		SizeType SerializeNum = Ar.IsLoading() ? 0 : A.ArrayNum;

		Ar << SerializeNum;

		if (SerializeNum == 0)
		{
			// if we are loading, then we have to reset the size to 0, in case it isn't currently 0
			if (Ar.IsLoading())
			{
				A.Empty();
			}
			return Ar;
		}

		check(SerializeNum >= 0);

		if (Ar.IsError() || SerializeNum < 0 || !ensure(!Ar.IsNetArchive() || SerializeNum <= MaxNetArraySerialize))
		{
			Ar.SetError();
			return Ar;
		}

		// if we don't need to perform per-item serialization, just read it in bulk
		if constexpr (sizeof(ElementType) == 1 || TCanBulkSerialize<ElementType>::Value)
		{
			A.ArrayNum = SerializeNum;

			// Serialize simple bytes which require no construction or destruction.
			if ((A.ArrayNum || A.ArrayMax) && Ar.IsLoading())
			{
				A.ResizeForCopy(A.ArrayNum, A.ArrayMax);
			}

			if constexpr (TIsUECoreVariant<ElementType, double>::Value)
			{
				if (Ar.IsLoading() && Ar.UEVer() < EUnrealEngineObjectUE5Version::LARGE_WORLD_COORDINATES)
				{
					// Per item serialization is required for core variant types loaded from pre LWC archives, to enable conversion from float to double.
					A.Empty(SerializeNum);
					for (SizeType i = 0; i < SerializeNum; i++)
					{
						Ar << A.AddDefaulted_GetRef();
					}
				}
				else
				{
					Ar.Serialize(A.GetData(), A.Num() * sizeof(ElementType));
				}
			}
			else
			{
				Ar.Serialize(A.GetData(), A.Num() * sizeof(ElementType));
			}

		}
		else if (Ar.IsLoading())
		{
			// Required for resetting ArrayNum
			A.Empty(SerializeNum);

			for (SizeType i=0; i<SerializeNum; i++)
			{
				Ar << A.AddDefaulted_GetRef();
			}
		}
		else
		{
			A.ArrayNum = SerializeNum;

			for (SizeType i=0; i<A.ArrayNum; i++)
			{
				Ar << A[i];
			}
		}

		A.SlackTrackerNumChanged();

		return Ar;
	}
};


template<typename ElementType, typename AllocatorType>
FArchive& operator<<(FArchive& Ar, TArray<ElementType, AllocatorType>& A)
{
	return TArrayPrivateFriend::Serialize(Ar, A);
}

/** Returns a unique hash by combining those of each array element. */
template<typename InElementType, typename InAllocatorType>
uint32 GetTypeHash(const TArray<InElementType, InAllocatorType>& A)
{
	uint32 Hash = 0;
	for (const InElementType& V : A)
	{
		Hash = HashCombineFast(Hash, GetTypeHash(V));
	}
	return Hash;
}

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "Templates/IsSigned.h"
#include "Templates/AndOrNot.h"
#include "Templates/IsConstructible.h"
#include "Templates/MakeUnsigned.h"
#endif
