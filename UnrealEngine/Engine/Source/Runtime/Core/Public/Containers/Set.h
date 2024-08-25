// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Templates/UnrealTypeTraits.h"
#include "Templates/UnrealTemplate.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/ContainerElementTypeCompatibility.h"
#include "Templates/Sorting.h"
#include "Containers/Array.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/StructBuilder.h"
#include "Templates/Function.h"
#include <initializer_list>
#include "Templates/TypeHash.h"
#include "Containers/SparseArray.h"
#include "Serialization/StructuredArchive.h"
#include "Serialization/MemoryImageWriter.h"
#include "ContainersFwd.h"
#include "Templates/RetainedRef.h"
#include <type_traits>

/**
 * The base KeyFuncs type with some useful definitions for all KeyFuncs; meant to be derived from instead of used directly.
 * bInAllowDuplicateKeys=true is slightly faster because it allows the TSet to skip validating that
 * there isn't already a duplicate entry in the TSet.
  */
template<typename ElementType,typename InKeyType,bool bInAllowDuplicateKeys = false>
struct BaseKeyFuncs
{
	typedef InKeyType KeyType;
	typedef typename TCallTraits<InKeyType>::ParamType KeyInitType;
	typedef typename TCallTraits<ElementType>::ParamType ElementInitType;

	enum { bAllowDuplicateKeys = bInAllowDuplicateKeys };
};

/**
 * A default implementation of the KeyFuncs used by TSet which uses the element as a key.
 */
template<typename ElementType,bool bInAllowDuplicateKeys /*= false*/>
struct DefaultKeyFuncs : BaseKeyFuncs<ElementType,ElementType,bInAllowDuplicateKeys>
{
	typedef typename TTypeTraits<ElementType>::ConstPointerType KeyInitType;
	typedef typename TCallTraits<ElementType>::ParamType ElementInitType;

	/**
	 * @return The key used to index the given element.
	 */
	static FORCEINLINE KeyInitType GetSetKey(ElementInitType Element)
	{
		return Element;
	}

	/**
	 * @return True if the keys match.
	 */
	static FORCEINLINE bool Matches(KeyInitType A, KeyInitType B)
	{
		return A == B;
	}

	/**
	 * @return True if the keys match.
	 */
	template<typename ComparableKey>
	static FORCEINLINE bool Matches(KeyInitType A, ComparableKey B)
	{
		return A == B;
	}

	/** Calculates a hash index for a key. */
	static FORCEINLINE uint32 GetKeyHash(KeyInitType Key)
	{
		return GetTypeHash(Key);
	}

	/** Calculates a hash index for a key. */
	template<typename ComparableKey>
	static FORCEINLINE uint32 GetKeyHash(ComparableKey Key)
	{
		return GetTypeHash(Key);
	}
};

/** This is used to provide type specific behavior for a move which will destroy B. */
/** Should be in UnrealTemplate but isn't for Clang build reasons - will move later */
template<typename T>
FORCEINLINE void MoveByRelocate(T& A, T& B)
{
	// Destruct the previous value of A.
	A.~T();

	// Relocate B into the 'hole' left by the destruction of A, leaving a hole in B instead.
	RelocateConstructItems<T>(&A, &B, 1);
}

template <typename AllocatorType, typename InDerivedType = void>
class TScriptSet;

/** Either NULL or an identifier for an element of a set. */
class FSetElementId
{
public:

	template<typename,typename,typename>
	friend class TSet;

	template <typename, typename>
	friend class TScriptSet;

	/** Default constructor. */
	FORCEINLINE FSetElementId() = default;

	/** @return a boolean value representing whether the id is NULL. */
	FORCEINLINE bool IsValidId() const
	{
		return Index != INDEX_NONE;
	}

	/** Comparison operators. */
	FORCEINLINE friend bool operator==(const FSetElementId& A,const FSetElementId& B)
	{
		return A.Index == B.Index;
	}
	FORCEINLINE friend bool operator!=(const FSetElementId& A, const FSetElementId& B)
	{
		return A.Index != B.Index;
	}

	FORCEINLINE int32 AsInteger() const
	{
		return Index;
	}

	FORCEINLINE static FSetElementId FromInteger(int32 Integer)
	{
		return FSetElementId(Integer);
	}

private:
	/** The index of the element in the set's element array. */
	int32 Index = INDEX_NONE;

	/** Initialization constructor. */
	FORCEINLINE explicit FSetElementId(int32 InIndex)
		: Index(InIndex)
	{
	}
};

// This is just an int32
DECLARE_INTRINSIC_TYPE_LAYOUT(FSetElementId);

template<typename InElementType, bool bTypeLayout>
class TSetElementBase
{
public:
	typedef InElementType ElementType;

	FORCEINLINE TSetElementBase() {}

	/** Initialization constructor. */
	template <
		typename InitType
		UE_REQUIRES(!std::is_same_v<TSetElementBase, std::decay_t<InitType>>)
	>
	explicit FORCEINLINE TSetElementBase(InitType&& InValue)
		: Value(Forward<InitType>(InValue))
	{
	}

	TSetElementBase(TSetElementBase&&) = default;
	TSetElementBase(const TSetElementBase&) = default;
	TSetElementBase& operator=(TSetElementBase&&) = default;
	TSetElementBase& operator=(const TSetElementBase&) = default;

	/** The element's value. */
	ElementType Value;

	/** The id of the next element in the same hash bucket. */
	mutable FSetElementId HashNextId;

	/** The hash bucket that the element is currently linked to. */
	mutable int32 HashIndex;
};

template<typename InElementType>
class TSetElementBase<InElementType, true>
{
	DECLARE_INLINE_TYPE_LAYOUT(TSetElementBase, NonVirtual);
public:
	typedef InElementType ElementType;

	FORCEINLINE TSetElementBase() {}

	/** Initialization constructor. */
	template <
		typename InitType
		UE_REQUIRES(!std::is_same_v<TSetElementBase, std::decay_t<InitType>>)
	>
	explicit FORCEINLINE TSetElementBase(InitType&& InValue)
		: Value(Forward<InitType>(InValue))
	{
	}

	TSetElementBase(TSetElementBase&&) = default;
	TSetElementBase(const TSetElementBase&) = default;
	TSetElementBase& operator=(TSetElementBase&&) = default;
	TSetElementBase& operator=(const TSetElementBase&) = default;

	/** The element's value. */
	LAYOUT_FIELD(ElementType, Value);

	/** The id of the next element in the same hash bucket. */
	LAYOUT_MUTABLE_FIELD(FSetElementId, HashNextId);

	/** The hash bucket that the element is currently linked to. */
	LAYOUT_MUTABLE_FIELD(int32, HashIndex);
};

/** An element in the set. */
template <typename InElementType>
class TSetElement : public TSetElementBase<InElementType, THasTypeLayout<InElementType>::Value>
{
	using Super = TSetElementBase<InElementType, THasTypeLayout<InElementType>::Value>;
public:
	/** Default constructor. */
	FORCEINLINE TSetElement()
	{}

	/** Initialization constructor. */
	template <
		typename InitType
		UE_REQUIRES(!std::is_same_v<TSetElement, std::decay_t<InitType>>)
	>
	explicit FORCEINLINE TSetElement(InitType&& InValue)
		: Super(Forward<InitType>(InValue))
	{
	}

	TSetElement(TSetElement&&) = default;
	TSetElement(const TSetElement&) = default;
	TSetElement& operator=(TSetElement&&) = default;
	TSetElement& operator=(const TSetElement&) = default;

	// Comparison operators
	FORCEINLINE bool operator==(const TSetElement& Other) const
	{
		return this->Value == Other.Value;
	}
	FORCEINLINE bool operator!=(const TSetElement& Other) const
	{
		return this->Value != Other.Value;
	}
};

namespace UE::Core::Private
{
	[[noreturn]] CORE_API void OnInvalidSetNum(unsigned long long NewNum);
}

/**
 * A set with an optional KeyFuncs parameters for customizing how the elements are compared and searched.  
 * E.g. You can specify a mapping from elements to keys if you want to find elements by specifying a subset of 
 * the element type.  It uses a TSparseArray of the elements, and also links the elements into a hash with a 
 * number of buckets proportional to the number of elements.  Addition, removal, and finding are O(1).
 *
 * The ByHash() functions are somewhat dangerous but particularly useful in two scenarios:
 * -- Heterogeneous lookup to avoid creating expensive keys like FString when looking up by const TCHAR*.
 *	  You must ensure the hash is calculated in the same way as ElementType is hashed.
 *    If possible put both ComparableKey and ElementType hash functions next to each other in the same header
 *    to avoid bugs when the ElementType hash function is changed.
 * -- Reducing contention around hash tables protected by a lock. It is often important to incur
 *    the cache misses of reading key data and doing the hashing *before* acquiring the lock.
 *
 **/
template<
	typename InElementType,
	typename KeyFuncs /*= DefaultKeyFuncs<ElementType>*/,
	typename Allocator /*= FDefaultSetAllocator*/
	>
class TSet
{
public:
	typedef InElementType ElementType;
	typedef KeyFuncs    KeyFuncsType;
	typedef Allocator   AllocatorType;

	using SizeType = typename Allocator::SparseArrayAllocator::ElementAllocator::SizeType;

	static_assert(std::is_same_v<SizeType, int32>, "TSet currently only supports 32-bit allocators");

private:
	using USizeType = std::make_unsigned_t<SizeType>;

	template <typename, typename>
	friend class TScriptSet;

	typedef typename KeyFuncs::KeyInitType     KeyInitType;
	typedef typename KeyFuncs::ElementInitType ElementInitType;

	typedef TSetElement<InElementType> SetElementType;

public:
	/** Initialization constructor. */
	FORCEINLINE TSet()
	:	HashSize(0)
	{}

	/** Copy constructor. */
	FORCEINLINE TSet(const TSet& Copy)
	:	HashSize(0)
	{
		*this = Copy;
	}

	FORCEINLINE explicit TSet(const TArray<ElementType>& InArray)
		: HashSize(0)
	{
		Append(InArray);
	}

	FORCEINLINE explicit TSet(TArray<ElementType>&& InArray)
		: HashSize(0)
	{
		Append(MoveTemp(InArray));
	}

	/** Destructor. */
	FORCEINLINE ~TSet()
	{
		HashSize = 0;
	}

	/** Assignment operator. */
	TSet& operator=(const TSet& Copy)
	{
		if (this != &Copy)
		{
			int32 CopyHashSize = Copy.HashSize;

			DestructItems((FSetElementId*)Hash.GetAllocation(), HashSize);
			Hash.ResizeAllocation(0, CopyHashSize, sizeof(FSetElementId));
			ConstructItems<FSetElementId>(Hash.GetAllocation(), (FSetElementId*)Copy.Hash.GetAllocation(), CopyHashSize);
			HashSize = CopyHashSize;

			Elements = Copy.Elements;
		}
		return *this;
	}

private:
	template <typename SetType>
	static FORCEINLINE void Move(SetType& ToSet, SetType& FromSet)
	{
		ToSet.Elements = (ElementArrayType&&)FromSet.Elements;

		ToSet.Hash.MoveToEmpty(FromSet.Hash);

		ToSet  .HashSize = FromSet.HashSize;
		FromSet.HashSize = 0;
	}

public:
	/** Initializer list constructor. */
	TSet(std::initializer_list<ElementType> InitList)
		: HashSize(0)
	{
		Append(InitList);
	}

	/** Move constructor. */
	TSet(TSet&& Other)
		: HashSize(0)
	{
		this->Move(*this, Other);
	}

	/** Move assignment operator. */
	TSet& operator=(TSet&& Other)
	{
		if (this != &Other)
		{
			this->Move(*this, Other);
		}

		return *this;
	}

	/** Constructor for moving elements from a TSet with a different SetAllocator */
	template<typename OtherAllocator>
	TSet(TSet<ElementType, KeyFuncs, OtherAllocator>&& Other)
		: HashSize(0)
	{
		Append(MoveTemp(Other));
	}

	/** Constructor for copying elements from a TSet with a different SetAllocator */
	template<typename OtherAllocator>
	TSet(const TSet<ElementType, KeyFuncs, OtherAllocator>& Other)
		: HashSize(0)
	{
		Append(Other);
	}

	/** Assignment operator for moving elements from a TSet with a different SetAllocator */
	template<typename OtherAllocator>
	TSet& operator=(TSet<ElementType, KeyFuncs, OtherAllocator>&& Other)
	{
		Reset();
		Append(MoveTemp(Other));
		return *this;
	}

	/** Assignment operator for copying elements from a TSet with a different SetAllocator */
	template<typename OtherAllocator>
	TSet& operator=(const TSet<ElementType, KeyFuncs, OtherAllocator>& Other)
	{
		Reset();
		Append(Other);
		return *this;
	}

	/** Initializer list assignment operator */
	TSet& operator=(std::initializer_list<ElementType> InitList)
	{
		Reset();
		Append(InitList);
		return *this;
	}

	/**
	 * Removes all elements from the set, potentially leaving space allocated for an expected number of elements about to be added.
	 * @param ExpectedNumElements - The number of elements about to be added to the set.
	 */
	void Empty(int32 ExpectedNumElements = 0)
	{
		// Empty the elements array, and reallocate it for the expected number of elements.
		const int32 DesiredHashSize = Allocator::GetNumberOfHashBuckets(ExpectedNumElements);
		const bool ShouldDoRehash = ShouldRehash(ExpectedNumElements, DesiredHashSize, EAllowShrinking::Yes);

		if (!ShouldDoRehash)
		{
			// If the hash was already the desired size, clear the references to the elements that have now been removed.
			UnhashElements();
		}

		Elements.Empty(ExpectedNumElements);

		// Resize the hash to the desired size for the expected number of elements.
		if (ShouldDoRehash)
		{
			HashSize = DesiredHashSize;
			Rehash();
		}
	}

	/** Efficiently empties out the set but preserves all allocations and capacities */
    void Reset()
    {
		if (Num() == 0)
		{
			return;
		}
    	
		// Reset the elements array.
		UnhashElements();
		Elements.Reset();
    }

	/** Shrinks the set's element storage to avoid slack. */
	FORCEINLINE void Shrink()
	{
		Elements.Shrink();
		Relax();
	}

	/** Compacts the allocated elements into a contiguous range. */
	FORCEINLINE void Compact()
	{
		if (Elements.Compact())
		{
			HashSize = Allocator::GetNumberOfHashBuckets(Elements.Num());
			Rehash();
		}
	}

	/** Compacts the allocated elements into a contiguous range. Does not change the iteration order of the elements. */
	FORCEINLINE void CompactStable()
	{
		if (Elements.CompactStable())
		{
			HashSize = Allocator::GetNumberOfHashBuckets(Elements.Num());
			Rehash();
		}
	}

	/** Preallocates enough memory to contain Number elements */
	FORCEINLINE void Reserve(int32 Number)
	{
		// makes sense only when Number > Elements.Num() since TSparseArray::Reserve 
		// does any work only if that's the case
		if ((USizeType)Number > (USizeType)Elements.Num())
		{
			// Trap negative reserves
			if (Number < 0)
			{
				UE::Core::Private::OnInvalidSetNum((unsigned long long)Number);
			}

			// Preallocates memory for array of elements
			Elements.Reserve(Number);

			// Calculate the corresponding hash size for the specified number of elements.
			const int32 NewHashSize = Allocator::GetNumberOfHashBuckets(Number);

			// If the hash hasn't been created yet, or is smaller than the corresponding hash size, rehash
			// to force a preallocation of the hash table
			if(!HashSize || HashSize < NewHashSize)
			{
				HashSize = NewHashSize;
				Rehash();
			}
		}
	}

	/** Relaxes the set's hash to a size strictly bounded by the number of elements in the set. */
	FORCEINLINE void Relax()
	{
		ConditionalRehash(Elements.Num(), EAllowShrinking::Yes);
	}

	/** 
	 * Helper function to return the amount of memory allocated by this container 
	 * Only returns the size of allocations made directly by the container, not the elements themselves.
	 * @return number of bytes allocated by this container
	 */
	FORCEINLINE SIZE_T GetAllocatedSize( void ) const
	{
		return Elements.GetAllocatedSize() + Hash.GetAllocatedSize(HashSize, sizeof(FSetElementId));
	}

	/** Tracks the container's memory use through an archive. */
	FORCEINLINE void CountBytes(FArchive& Ar) const
	{
		Elements.CountBytes(Ar);
		Ar.CountBytes(HashSize * sizeof(int32),HashSize * sizeof(FSetElementId));
	}

	/**
	 * Returns true if the sets is empty and contains no elements. 
	 *
	 * @returns True if the set is empty.
	 * @see Num
	 */
	bool IsEmpty() const
	{
		return Elements.IsEmpty();
	}

	/** @return the number of elements. */
	FORCEINLINE int32 Num() const
	{
		return Elements.Num();
	}

	FORCEINLINE int32 GetMaxIndex() const
	{
		return Elements.GetMaxIndex();
	}

	/**
	 * Checks whether an element id is valid.
	 * @param Id - The element id to check.
	 * @return true if the element identifier refers to a valid element in this set.
	 */
	FORCEINLINE bool IsValidId(FSetElementId Id) const
	{
		SizeType Index = Id.Index;
		return Index != INDEX_NONE &&
			Index >= 0 &&
			Index < Elements.GetMaxIndex() &&
			Elements.IsAllocated(Index);
	}

	/** Accesses the identified element's value. Element must be valid (see @IsValidId). */
	FORCEINLINE ElementType& operator[](FSetElementId Id)
	{
		return Elements[Id.Index].Value;
	}

	/** Accesses the identified element's value. Element must be valid (see @IsValidId). */
	FORCEINLINE const ElementType& operator[](FSetElementId Id) const
	{
		return Elements[Id.Index].Value;
	}

	/** Accesses the identified element's value. Element must be valid (see @IsValidId). */
	[[nodiscard]] FORCEINLINE ElementType& Get(FSetElementId Id)
	{
		return Elements[Id.Index].Value;
	}

	/** Accesses the identified element's value. Element must be valid (see @IsValidId). */
	[[nodiscard]] FORCEINLINE const ElementType& Get(FSetElementId Id) const
	{
		return Elements[Id.Index].Value;
	}

	/**
	 * Adds an element to the set.
	 *
	 * @param	InElement					Element to add to set
	 * @param	bIsAlreadyInSetPtr	[out]	Optional pointer to bool that will be set depending on whether element is already in set
	 * @return	A pointer to the element stored in the set.
	 */
	FORCEINLINE FSetElementId Add(const InElementType&  InElement, bool* bIsAlreadyInSetPtr = nullptr) { return Emplace(                   InElement , bIsAlreadyInSetPtr); }
	FORCEINLINE FSetElementId Add(      InElementType&& InElement, bool* bIsAlreadyInSetPtr = nullptr) { return Emplace(MoveTempIfPossible(InElement), bIsAlreadyInSetPtr); }

	/**
	 * Adds an element to the set if not already present and returns a reference to the added or existing element.
	 *
	 * @param	InElement					Element to add to set
	 * @param	bIsAlreadyInSetPtr	[out]	Optional pointer to bool that will be set depending on whether element is already in set
	 * @return	A reference to the element stored in the set.
	 */
	FORCEINLINE ElementType& FindOrAdd(const InElementType& InElement, bool* bIsAlreadyInSetPtr = nullptr)
	{
		return FindOrAddByHash(KeyFuncs::GetKeyHash(KeyFuncs::GetSetKey(InElement)), InElement, bIsAlreadyInSetPtr);
	}
	FORCEINLINE ElementType& FindOrAdd(InElementType&& InElement, bool* bIsAlreadyInSetPtr = nullptr)
	{
		return FindOrAddByHash(KeyFuncs::GetKeyHash(KeyFuncs::GetSetKey(InElement)), MoveTempIfPossible(InElement), bIsAlreadyInSetPtr);
	}

	/**
	 * Adds an element to the set.
	 *
	 * @see		Class documentation section on ByHash() functions
	 * @param	InElement					Element to add to set
	 * @param	bIsAlreadyInSetPtr	[out]	Optional pointer to bool that will be set depending on whether element is already in set
     * @return  A handle to the element stored in the set
     */
	FORCEINLINE FSetElementId AddByHash(uint32 KeyHash, const InElementType& InElement, bool* bIsAlreadyInSetPtr = nullptr)
	{
		return EmplaceByHash(KeyHash, InElement, bIsAlreadyInSetPtr);
	}
	FORCEINLINE FSetElementId AddByHash(uint32 KeyHash,		 InElementType&& InElement, bool* bIsAlreadyInSetPtr = nullptr)
	{
		return EmplaceByHash(KeyHash, MoveTempIfPossible(InElement), bIsAlreadyInSetPtr);
	}

	/**
	 * Adds an element to the set if not already present and returns a reference to the added or existing element.
	 *
	 * @see		Class documentation section on ByHash() functions
	 * @param	InElement					Element to add to set
	 * @param	bIsAlreadyInSetPtr	[out]	Optional pointer to bool that will be set depending on whether element is already in set
	 * @return  A reference to the element stored in the set
	 */
	template <typename ElementReferenceType>
	ElementType& FindOrAddByHash(uint32 KeyHash, ElementReferenceType&& InElement, bool* bIsAlreadyInSetPtr = nullptr)
	{
		SizeType ExistingIndex = FindIndexByHash(KeyHash, KeyFuncs::GetSetKey(InElement));
		bool bIsAlreadyInSet = ExistingIndex != INDEX_NONE;
		if (bIsAlreadyInSetPtr)
		{
			*bIsAlreadyInSetPtr = bIsAlreadyInSet;
		}
		if (bIsAlreadyInSet)
		{
			return Elements[ExistingIndex].Value;
		}

		// Create a new element.
		FSparseArrayAllocationInfo ElementAllocation = Elements.AddUninitialized();
		SetElementType& Element = *new (ElementAllocation) SetElementType(Forward<ElementReferenceType>(InElement));
		RehashOrLink(KeyHash, Element, ElementAllocation.Index);
		return Element.Value;
	}

private:
	bool TryReplaceExisting(uint32 KeyHash, SetElementType& Element, SizeType& InOutElementIndex, bool* bIsAlreadyInSetPtr)
	{
		bool bIsAlreadyInSet = false;
		if constexpr (!KeyFuncs::bAllowDuplicateKeys)
		{
			// If the set doesn't allow duplicate keys, check for an existing element with the same key as the element being added.

			// Don't bother searching for a duplicate if this is the first element we're adding
			if (Elements.Num() != 1)
			{
				SizeType ExistingIndex = FindIndexByHash(KeyHash, KeyFuncs::GetSetKey(Element.Value));
				bIsAlreadyInSet = ExistingIndex != INDEX_NONE;
				if (bIsAlreadyInSet)
				{
					// If there's an existing element with the same key as the new element, replace the existing element with the new element.
					MoveByRelocate(Elements[ExistingIndex].Value, Element.Value);

					// Then remove the new element.
					Elements.RemoveAtUninitialized(InOutElementIndex);

					// Then point the return value at the replaced element.
					InOutElementIndex = ExistingIndex;
				}
			}
		}
		if (bIsAlreadyInSetPtr)
		{
			*bIsAlreadyInSetPtr = bIsAlreadyInSet;
		}
		return bIsAlreadyInSet;
	}

	FORCEINLINE void RehashOrLink(uint32 KeyHash, SetElementType& Element, SizeType ElementIndex)
	{
		// Check if the hash needs to be resized.
		if (!ConditionalRehash(Elements.Num(), EAllowShrinking::No))
		{
			// If the rehash didn't add the new element to the hash, add it.
			LinkElement(ElementIndex, Element, KeyHash);
		}
	}

public:
	/**
	 * Adds an element to the set.
	 *
	 * @param	Args						The argument(s) to be forwarded to the set element's constructor.
	 * @param	bIsAlreadyInSetPtr	[out]	Optional pointer to bool that will be set depending on whether element is already in set
	 * @return	A handle to the element stored in the set.
	 */
	template <typename ArgsType = ElementType>
	FSetElementId Emplace(ArgsType&& Args, bool* bIsAlreadyInSetPtr = nullptr)
	{
		// Create a new element.
		FSparseArrayAllocationInfo ElementAllocation = Elements.AddUninitialized();
		SetElementType& Element = *new (ElementAllocation) SetElementType(Forward<ArgsType>(Args));

		SizeType NewHashIndex = ElementAllocation.Index;

		uint32 KeyHash = KeyFuncs::GetKeyHash(KeyFuncs::GetSetKey(Element.Value));
		if (!TryReplaceExisting(KeyHash, Element, NewHashIndex, bIsAlreadyInSetPtr))
		{
			RehashOrLink(KeyHash, Element, NewHashIndex);
		}
		return FSetElementId(NewHashIndex);
	}
	
	/**
	 * Adds an element to the set.
	 *
	 * @see		Class documentation section on ByHash() functions
	 * @param	Args						The argument(s) to be forwarded to the set element's constructor.
	 * @param	bIsAlreadyInSetPtr	[out]	Optional pointer to bool that will be set depending on whether element is already in set
	 * @return	A handle to the element stored in the set.
	 */
	template <typename ArgsType = ElementType>
	FSetElementId EmplaceByHash(uint32 KeyHash, ArgsType&& Args, bool* bIsAlreadyInSetPtr = nullptr)
	{
		// Create a new element.
		FSparseArrayAllocationInfo ElementAllocation = Elements.AddUninitialized();
		SetElementType& Element = *new (ElementAllocation) SetElementType(Forward<ArgsType>(Args));

		SizeType NewHashIndex = ElementAllocation.Index;

		if (!TryReplaceExisting(KeyHash, Element, ElementAllocation.Index, bIsAlreadyInSetPtr))
		{
			RehashOrLink(KeyHash, Element, NewHashIndex);
		}
		return FSetElementId(NewHashIndex);
	}

	template<typename ViewSizeType>
	void Append(TArrayView<ElementType, ViewSizeType> InElements)
	{
		Reserve(Elements.Num() + InElements.Num());
		for (const ElementType& Element : InElements)
		{
			Add(Element);
		}
	}

	template<typename ViewSizeType>
	void Append(TArrayView<const ElementType, ViewSizeType> InElements)
	{
		Reserve(Elements.Num() + InElements.Num());
		for (const ElementType& Element : InElements)
		{
			Add(Element);
		}
	}

	template<typename ArrayAllocator>
	void Append(const TArray<ElementType, ArrayAllocator>& InElements)
	{
		Reserve(Elements.Num() + InElements.Num());
		for (const ElementType& Element : InElements)
		{
			Add(Element);
		}
	}

	template<typename ArrayAllocator>
	void Append(TArray<ElementType, ArrayAllocator>&& InElements)
	{
		Reserve(Elements.Num() + InElements.Num());
		for (ElementType& Element : InElements)
		{
			Add(MoveTempIfPossible(Element));
		}
		InElements.Reset();
	}

	/**
	 * Add all items from another set to our set (union without creating a new set)
	 * @param OtherSet - The other set of items to add.
	 */
	template<typename OtherAllocator>
	void Append(const TSet<ElementType, KeyFuncs, OtherAllocator>& OtherSet)
	{
		Reserve(Elements.Num() + OtherSet.Num());
		for (const ElementType& Element : OtherSet)
		{
			Add(Element);
		}
	}

	template<typename OtherAllocator>
	void Append(TSet<ElementType, KeyFuncs, OtherAllocator>&& OtherSet)
	{
		Reserve(Elements.Num() + OtherSet.Num());
		for (ElementType& Element : OtherSet)
		{
			Add(MoveTempIfPossible(Element));
		}
		OtherSet.Reset();
	}

	void Append(std::initializer_list<ElementType> InitList)
	{
		Reserve(Elements.Num() + (int32)InitList.size());
		for (const ElementType& Element : InitList)
		{
			Add(Element);
		}
	}

private:
	void RemoveByIndex(SizeType ElementIndex)
	{
		checkf(Elements.IsValidIndex(ElementIndex), TEXT("Invalid ElementIndex passed to TSet::RemoveByIndex"));

		const SetElementType& ElementBeingRemoved = Elements[ElementIndex];

		// Remove the element from the hash.
		FSetElementId* HashPtr              = Hash.GetAllocation();
		SizeType*      NextElementIndexIter = &HashPtr[ElementBeingRemoved.HashIndex].Index;
		for (;;)
		{
			SizeType NextElementIndex = *NextElementIndexIter;
			checkf(NextElementIndex != INDEX_NONE, TEXT("Corrupt hash"));

			if (NextElementIndex == ElementIndex)
			{
				*NextElementIndexIter = ElementBeingRemoved.HashNextId.Index;
				break;
			}

			NextElementIndexIter = &Elements[NextElementIndex].HashNextId.Index;
		}

		// Remove the element from the elements array.
		Elements.RemoveAt(ElementIndex);
	}

public:
	/**
	 * Removes an element from the set.
	 * @param Element - A pointer to the element in the set, as returned by Add or Find.
	 */
	void Remove(FSetElementId ElementId)
	{
		RemoveByIndex(ElementId.Index);
	}

private:
	/**
	 * Finds an element with a pre-calculated hash and a key that can be compared to KeyType
	 * @see	Class documentation section on ByHash() functions
	 * @return The element id that matches the key and hash or an invalid element id
	 */
	template <typename ComparableKey>
	SizeType FindIndexByHash(uint32 KeyHash, const ComparableKey& Key) const
	{
		if (Elements.Num() == 0)
		{
			return INDEX_NONE;
		}

		FSetElementId* HashPtr      = Hash.GetAllocation();
		SizeType       ElementIndex = HashPtr[KeyHash & (HashSize - 1)].Index;
		for (;;)
		{
			if (ElementIndex == INDEX_NONE)
			{
				return INDEX_NONE;
			}

			if (KeyFuncs::Matches(KeyFuncs::GetSetKey(Elements[ElementIndex].Value), Key))
			{
				// Return the first match, regardless of whether the set has multiple matches for the key or not.
				return ElementIndex;
			}

			ElementIndex = Elements[ElementIndex].HashNextId.Index;
		}
	}

public:
	/**
	 * Finds an element with the given key in the set.
	 * @param Key - The key to search for.
	 * @return The id of the set element matching the given key, or the NULL id if none matches.
	 */
	FSetElementId FindId(KeyInitType Key) const
	{
		return FSetElementId(FindIndexByHash(KeyFuncs::GetKeyHash(Key), Key));
	}

	/**
	 * Finds an element with a pre-calculated hash and a key that can be compared to KeyType
	 * @see	Class documentation section on ByHash() functions
	 * @return The element id that matches the key and hash or an invalid element id
	 */
	template<typename ComparableKey>
	FSetElementId FindIdByHash(uint32 KeyHash, const ComparableKey& Key) const
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		// Disable deprecations warnings to stop warnings being thrown by our check macro.
		checkSlow(KeyHash == KeyFuncs::GetKeyHash(Key));
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		return FSetElementId(FindIndexByHash(KeyHash, Key));
	}

	/**
	 * Finds an element with the given key in the set.
	 * @param Key - The key to search for.
	 * @return A pointer to an element with the given key.  If no element in the set has the given key, this will return NULL.
	 */
	FORCEINLINE ElementType* Find(KeyInitType Key)
	{
		SizeType ElementIndex = FindIndexByHash(KeyFuncs::GetKeyHash(Key), Key);
		if (ElementIndex != INDEX_NONE)
		{
			return &Elements[ElementIndex].Value;
		}
		else
		{
			return nullptr;
		}
	}
	
	/**
	 * Finds an element with the given key in the set.
	 * @param Key - The key to search for.
	 * @return A const pointer to an element with the given key.  If no element in the set has the given key, this will return NULL.
	 */
	FORCEINLINE const ElementType* Find(KeyInitType Key) const
	{
		return const_cast<TSet*>(this)->Find(Key);
	}

	/**
	 * Finds an element with a pre-calculated hash and a key that can be compared to KeyType.
	 * @see	Class documentation section on ByHash() functions
	 * @return A pointer to the contained element or nullptr.
	 */
	template<typename ComparableKey>
	ElementType* FindByHash(uint32 KeyHash, const ComparableKey& Key)
	{
		SizeType ElementIndex = FindIndexByHash(KeyHash, Key);
		if (ElementIndex != INDEX_NONE)
		{
			return &Elements[ElementIndex].Value;
		}
		else
		{
			return nullptr;
		}
	}

	template<typename ComparableKey>
	const ElementType* FindByHash(uint32 KeyHash, const ComparableKey& Key) const
	{
		return const_cast<TSet*>(this)->FindByHash(KeyHash, Key);
	}

private:
	template<typename ComparableKey>
	FORCEINLINE int32 RemoveImpl(uint32 KeyHash, const ComparableKey& Key)
	{
		int32 NumRemovedElements = 0;

		FSetElementId* NextElementId = &GetTypedHash(KeyHash);
		while (NextElementId->IsValidId())
		{
			SetElementType& Element = Elements[NextElementId->Index];

			if (KeyFuncs::Matches(KeyFuncs::GetSetKey(Element.Value), Key))
			{
				// This element matches the key, remove it from the set.  Note that Remove sets *NextElementId to point to the next
				// element after the removed element in the hash bucket.
				RemoveByIndex(NextElementId->Index);
				NumRemovedElements++;

				if constexpr (!KeyFuncs::bAllowDuplicateKeys)
				{
					// If the hash disallows duplicate keys, we're done removing after the first matched key.
					break;
				}
			}
			else
			{
				NextElementId = &Element.HashNextId;
			}
		}

		return NumRemovedElements;
	}

public:
	/**
	 * Removes all elements from the set matching the specified key.
	 * @param Key - The key to match elements against.
	 * @return The number of elements removed.
	 */
	int32 Remove(KeyInitType Key)
	{
		if (Elements.Num())
		{
			return RemoveImpl(KeyFuncs::GetKeyHash(Key), Key);
		}

		return 0;
	}

	/**
	 * Removes all elements from the set matching the specified key.
	 *
	 * @see		Class documentation section on ByHash() functions
	 * @param	Key - The key to match elements against.
	 * @return	The number of elements removed.
	 */
	template<typename ComparableKey>
	int32 RemoveByHash(uint32 KeyHash, const ComparableKey& Key)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		// Disable deprecations warnings to stop warnings being thrown by our check macro.
		checkSlow(KeyHash == KeyFuncs::GetKeyHash(Key));
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		if (Elements.Num())
		{
			return RemoveImpl(KeyHash, Key);
		}

		return 0;
	}

	/**
	 * Checks if the element contains an element with the given key.
	 * @param Key - The key to check for.
	 * @return true if the set contains an element with the given key.
	 */
	FORCEINLINE bool Contains(KeyInitType Key) const
	{
		return FindIndexByHash(KeyFuncs::GetKeyHash(Key), Key) != INDEX_NONE;
	}

	/**
	 * Checks if the element contains an element with the given key.
	 *
	 * @see	Class documentation section on ByHash() functions
	 */
	template<typename ComparableKey>
	FORCEINLINE bool ContainsByHash(uint32 KeyHash, const ComparableKey& Key) const
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		// Disable deprecations warnings to stop warnings being thrown by our check macro.
		checkSlow(KeyHash == KeyFuncs::GetKeyHash(Key));
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		return FindIndexByHash(KeyHash, Key) != INDEX_NONE;
	}

	/**
	 * Sorts the set's elements using the provided comparison class.
	 */
	template <typename PREDICATE_CLASS>
	void Sort( const PREDICATE_CLASS& Predicate )
	{
		// Sort the elements according to the provided comparison class.
		Elements.Sort( FElementCompareClass< PREDICATE_CLASS >( Predicate ) );

		// Rehash.
		Rehash();
	}

	/**
	 * Stable sorts the set's elements using the provided comparison class.
	 */
	template <typename PREDICATE_CLASS>
	void StableSort(const PREDICATE_CLASS& Predicate)
	{
		// Sort the elements according to the provided comparison class.
		Elements.StableSort(FElementCompareClass< PREDICATE_CLASS >(Predicate));

		// Rehash.
		Rehash();
	}

	/**
	* Sort the free element list so that subsequent additions will occur in the lowest available
	* TSparseArray index resulting in tighter packing without moving any existing items. Also useful for
	* some types of determinism. @see TSparseArray::SortFreeList() for more info.
	*/
	void SortFreeList()
	{
		Elements.SortFreeList();
	}

	/**
	 * Describes the set's contents through an output device.
	 * @param Ar - The output device to describe the set's contents through.
	 */
	void Dump(FOutputDevice& Ar)
	{
		Ar.Logf( TEXT("TSet: %i elements, %i hash slots"), Elements.Num(), HashSize );
		for (int32 HashIndex = 0, LocalHashSize = HashSize; HashIndex < LocalHashSize; ++HashIndex)
		{
			// Count the number of elements in this hash bucket.
			int32 NumElementsInBucket = 0;
			for(FSetElementId ElementId = GetTypedHash(HashIndex);
				ElementId.IsValidId();
				ElementId = Elements[ElementId.Index].HashNextId)
			{
				NumElementsInBucket++;
			}

			Ar.Logf(TEXT("   Hash[%i] = %i"),HashIndex,NumElementsInBucket);
		}
	}

	bool VerifyHashElementsKey(KeyInitType Key)
	{
		bool bResult=true;
		if (Elements.Num())
		{
			// iterate over all elements for the hash entry of the given key 
			// and verify that the ids are valid
			FSetElementId ElementId = GetTypedHash(KeyFuncs::GetKeyHash(Key));
			while( ElementId.IsValidId() )
			{
				if( !IsValidId(ElementId) )
				{
					bResult=false;
					break;
				}
				ElementId = Elements[ElementId.Index].HashNextId;
			}
		}
		return bResult;
	}

	void DumpHashElements(FOutputDevice& Ar)
	{
		for (int32 HashIndex = 0, LocalHashSize = HashSize; HashIndex < LocalHashSize; ++HashIndex)
		{
			Ar.Logf(TEXT("   Hash[%i]"),HashIndex);

			// iterate over all elements for the all hash entries 
			// and dump info for all elements
			FSetElementId ElementId = GetTypedHash(HashIndex);
			while( ElementId.IsValidId() )
			{
				if( !IsValidId(ElementId) )
				{
					Ar.Logf(TEXT("		!!INVALID!! ElementId = %d"),ElementId.Index);
				}
				else
				{
					Ar.Logf(TEXT("		VALID ElementId = %d"),ElementId.Index);
				}
				ElementId = Elements[ElementId].HashNextId;
			}
		}
	}

	/** @return the intersection of two sets. (A AND B)*/
	TSet Intersect(const TSet& OtherSet) const
	{
		const bool bOtherSmaller = (Num() > OtherSet.Num());
		const TSet& A = (bOtherSmaller ? OtherSet : *this);
		const TSet& B = (bOtherSmaller ? *this : OtherSet);

		TSet Result;
		Result.Reserve(A.Num()); // Worst case is everything in smaller is in larger

		for(TConstIterator SetIt(A);SetIt;++SetIt)
		{
			if(B.Contains(KeyFuncs::GetSetKey(*SetIt)))
			{
				Result.Add(*SetIt);
			}
		}
		return Result;
	}

	/** @return the union of two sets. (A OR B)*/
	TSet Union(const TSet& OtherSet) const
	{
		TSet Result;
		Result.Reserve(Num() + OtherSet.Num()); // Worst case is 2 totally unique Sets

		for(TConstIterator SetIt(*this);SetIt;++SetIt)
		{
			Result.Add(*SetIt);
		}
		for(TConstIterator SetIt(OtherSet);SetIt;++SetIt)
		{
			Result.Add(*SetIt);
		}
		return Result;
	}

	/** @return the complement of two sets. (A not in B where A is this and B is Other)*/
	TSet Difference(const TSet& OtherSet) const
	{
		TSet Result;
		Result.Reserve(Num()); // Worst case is no elements of this are in Other

		for(TConstIterator SetIt(*this);SetIt;++SetIt)
		{
			if(!OtherSet.Contains(KeyFuncs::GetSetKey(*SetIt)))
			{
				Result.Add(*SetIt);
			}
		}
		return Result;
	}

	/**
	 * Determine whether the specified set is entirely included within this set
	 * 
	 * @param OtherSet	Set to check
	 * 
	 * @return True if the other set is entirely included in this set, false if it is not
	 */
	bool Includes(const TSet<ElementType,KeyFuncs,Allocator>& OtherSet) const
	{
		bool bIncludesSet = true;
		if (OtherSet.Num() <= Num())
		{
			for(TConstIterator OtherSetIt(OtherSet); OtherSetIt; ++OtherSetIt)
			{
				if (!Contains(KeyFuncs::GetSetKey(*OtherSetIt)))
				{
					bIncludesSet = false;
					break;
				}
			}
		}
		else
		{
			// Not possible to include if it is bigger than us
			bIncludesSet = false;
		}
		return bIncludesSet;
	}

	/** @return a TArray of the elements */
	TArray<ElementType> Array() const
	{
		TArray<ElementType> Result;
		Result.Reserve(Num());
		for(TConstIterator SetIt(*this);SetIt;++SetIt)
		{
			Result.Add(*SetIt);
		}
		return Result;
	}

	/**
	 * Checks that the specified address is not part of an element within the container.  Used for implementations
	 * to check that reference arguments aren't going to be invalidated by possible reallocation.
	 *
	 * @param Addr The address to check.
	 */
	FORCEINLINE void CheckAddress(const ElementType* Addr) const
	{
		Elements.CheckAddress(Addr);
	}

	/**
	 * Move assignment operator.
	 * Compatible element type version.
	 *
	 * @param Other Set to assign and move from.
	 */
	template <
		typename OtherKeyFuncs,
		typename AliasElementType = ElementType
		UE_REQUIRES(TIsContainerElementTypeCopyable_V<AliasElementType>)
	>
	TSet& operator=(TSet<typename TContainerElementTypeCompatibility<ElementType>::CopyFromOtherType, OtherKeyFuncs, Allocator>&& Other)
	{
		TContainerElementTypeCompatibility<ElementType>::CopyingFromOtherType();
		Reset();
		Append(MoveTemp(Other));
		return *this;
	}

	/**
	 * Assignment operator. First deletes all currently contained elements
	 * and then copies from other set.
	 * Compatible element type version.
	 *
	 * @param Other The source set to assign from.
	 */
	template <
		typename OtherKeyFuncs,
		typename OtherAllocator,
		typename AliasElementType = ElementType
		UE_REQUIRES(TIsContainerElementTypeCopyable_V<AliasElementType>)
	>
	TSet& operator=(const TSet<typename TContainerElementTypeCompatibility<ElementType>::CopyFromOtherType, OtherKeyFuncs, OtherAllocator>& Other)
	{
		TContainerElementTypeCompatibility<ElementType>::CopyingFromOtherType();
		Reset();
		Append(Other);
		return *this;
	}

	/**
	 * Add all items from another set to our set (union without creating a new set)
	 * Compatible element type version.
	 * @param OtherSet - The other set of items to add.
	 */
	template <
		typename OtherKeyFuncs,
		typename OtherAllocator,
		typename AliasElementType = ElementType
		UE_REQUIRES(TIsContainerElementTypeCopyable_V<AliasElementType>)
	>
	void Append(const TSet<typename TContainerElementTypeCompatibility<ElementType>::CopyFromOtherType, OtherKeyFuncs, OtherAllocator>& OtherSet)
	{
		TContainerElementTypeCompatibility<ElementType>::CopyingFromOtherType();
		Reserve(Elements.Num() + OtherSet.Num());
		for (const ElementType& Element : OtherSet)
		{
			Add(Element);
		}
	}

	/**
	 * Add all items from another set to our set (union without creating a new set)
	 * Compatible element type version.
	 * @param OtherSet - The other set of items to add.
	 */
	template <
		typename OtherKeyFuncs,
		typename AliasElementType = ElementType
		UE_REQUIRES(TIsContainerElementTypeCopyable_V<AliasElementType>)
	>
	void Append(TSet<typename TContainerElementTypeCompatibility<ElementType>::CopyFromOtherType, OtherKeyFuncs, Allocator>&& OtherSet)
	{
		TContainerElementTypeCompatibility<ElementType>::CopyingFromOtherType();
		Reserve(Elements.Num() + OtherSet.Num());
		for (ElementType& Element : OtherSet)
		{
			Add(MoveTempIfPossible(Element));
		}
		OtherSet.Reset();
	}

private:
	/** Extracts the element value from the set's element structure and passes it to the user provided comparison class. */
	template <typename PREDICATE_CLASS>
	class FElementCompareClass
	{
		TDereferenceWrapper< ElementType, PREDICATE_CLASS > Predicate;

	public:
		FORCEINLINE FElementCompareClass( const PREDICATE_CLASS& InPredicate )
			: Predicate( InPredicate )
		{}

		FORCEINLINE bool operator()( const SetElementType& A,const SetElementType& B ) const
		{
			return Predicate( A.Value, B.Value );
		}
	};

	using ElementArrayType = TSparseArray<SetElementType, typename Allocator::SparseArrayAllocator>;
	using HashType         = typename Allocator::HashAllocator::template ForElementType<FSetElementId>;

	ElementArrayType Elements;

	mutable HashType Hash;
	mutable int32	 HashSize;

public:
	void WriteMemoryImage(FMemoryImageWriter& Writer) const
	{
		checkf(!Writer.Is32BitTarget(), TEXT("TSet does not currently support freezing for 32bits"));
		if constexpr (TAllocatorTraits<Allocator>::SupportsFreezeMemoryImage && THasTypeLayout<InElementType>::Value)
		{
			this->Elements.WriteMemoryImage(Writer);
			this->Hash.WriteMemoryImage(Writer, StaticGetTypeLayoutDesc<FSetElementId>(), this->HashSize);
			Writer.WriteBytes(this->HashSize);
		}
		else
		{
			Writer.WriteBytes(TSet());
		}
	}

	void CopyUnfrozen(const FMemoryUnfreezeContent& Context, void* Dst) const
	{
		if constexpr (TAllocatorTraits<Allocator>::SupportsFreezeMemoryImage && THasTypeLayout<InElementType>::Value)
		{
			TSet* DstObject = static_cast<TSet*>(Dst);
			this->Elements.CopyUnfrozen(Context, &DstObject->Elements);

			new(&DstObject->Hash) HashType();
			DstObject->Hash.ResizeAllocation(0, this->HashSize, sizeof(FSetElementId));
			FMemory::Memcpy(DstObject->Hash.GetAllocation(), this->Hash.GetAllocation(), sizeof(FSetElementId) * this->HashSize);
			DstObject->HashSize = this->HashSize;
		}
		else
		{
			new(Dst) TSet();
		}
	}

	static void AppendHash(const FPlatformTypeLayoutParameters& LayoutParams, FSHA1& Hasher)
	{
		ElementArrayType::AppendHash(LayoutParams, Hasher);
	}

private:

	FORCEINLINE FSetElementId& GetTypedHash(int32 HashIndex) const
	{
		return ((FSetElementId*)Hash.GetAllocation())[HashIndex & (HashSize - 1)];
	}

	/** Links an added element to the hash chain. */
	FORCEINLINE void LinkElement(SizeType ElementIndex, const SetElementType& Element, uint32 KeyHash) const
	{
		// Compute the hash bucket the element goes in.
		Element.HashIndex = KeyHash & (HashSize - 1);

		// Link the element into the hash bucket.
		Element.HashNextId.Index = GetTypedHash(Element.HashIndex).Index;
		GetTypedHash(Element.HashIndex).Index = ElementIndex;
	}

	/** Hashes and links an added element to the hash chain. */
	FORCEINLINE void HashElement(SizeType ElementIndex, const SetElementType& Element) const
	{
		LinkElement(ElementIndex, Element, KeyFuncs::GetKeyHash(KeyFuncs::GetSetKey(Element.Value)));
	}

	/** Reset hash buckets to invalid */
	void UnhashElements()
	{
		FSetElementId* HashPtr = Hash.GetAllocation();

		// Check if it should be faster to clear the hash by going through elements instead of resetting the whole hash
		if (Num() < (HashSize / 4))
		{
			// Faster path: only reset hash buckets to invalid for elements in the hash
			for (const SetElementType& Element: Elements)
			{
				HashPtr[Element.HashIndex].Index = INDEX_NONE;
			}
		}
		else
		{
			/** Reset the whole hash to invalid */
			for (int32 I = 0; I < HashSize; ++I)
			{
				HashPtr[I].Index = INDEX_NONE;
			}
		}
	}

	/**
	 * Checks if the hash has an appropriate number of buckets, and if it should be resized.
	 * @param NumHashedElements - The number of elements to size the hash for.
	 * @param DesiredHashSize - Desired size if we should rehash.
	 * @param AllowShrinking - If the hash is allowed to shrink.
	 * @return true if the set should berehashed.
	 */
	FORCEINLINE bool ShouldRehash(int32 NumHashedElements, int32 DesiredHashSize, EAllowShrinking AllowShrinking) const
	{
		// If the hash hasn't been created yet, or is smaller than the desired hash size, rehash.
		// If shrinking is allowed and the hash is bigger than the desired hash size, rehash.
		return ((NumHashedElements > 0 && HashSize < DesiredHashSize) || (AllowShrinking == EAllowShrinking::Yes && HashSize > DesiredHashSize));
	}

	/**
	 * Checks if the hash has an appropriate number of buckets, and if not resizes it.
	 * @param NumHashedElements - The number of elements to size the hash for.
	 * @param AllowShrinking - If the hash is allowed to shrink.
	 * @return true if the set was rehashed.
	 */
	bool ConditionalRehash(int32 NumHashedElements, EAllowShrinking AllowShrinking) const
	{
		// Calculate the desired hash size for the specified number of elements.
		const int32 DesiredHashSize = Allocator::GetNumberOfHashBuckets(NumHashedElements);

		if (ShouldRehash(NumHashedElements, DesiredHashSize, AllowShrinking))
		{
			HashSize = DesiredHashSize;
			Rehash();
			return true;
		}

		return false;
	}

	/** Resizes the hash. */
	void Rehash() const
	{
		// Free the old hash.
		Hash.ResizeAllocation(0,0,sizeof(FSetElementId));

		int32 LocalHashSize = HashSize;
		if (LocalHashSize)
		{
			// Allocate the new hash.
			checkSlow(FMath::IsPowerOfTwo(HashSize));
			Hash.ResizeAllocation(0, LocalHashSize, sizeof(FSetElementId));
			for (int32 HashIndex = 0; HashIndex < LocalHashSize; ++HashIndex)
			{
				GetTypedHash(HashIndex) = FSetElementId();
			}

			// Add the existing elements to the new hash.
			for(typename ElementArrayType::TConstIterator ElementIt(Elements);ElementIt;++ElementIt)
			{
				HashElement(ElementIt.GetIndex(), *ElementIt);
			}
		}
	}

	/** The base type of whole set iterators. */
	template<bool bConst, bool bRangedFor = false>
	class TBaseIterator
	{
	private:
		friend class TSet;

		typedef std::conditional_t<bConst,const ElementType,ElementType> ItElementType;

	public:
		typedef std::conditional_t<
			bConst,
			std::conditional_t<bRangedFor, typename ElementArrayType::TRangedForConstIterator, typename ElementArrayType::TConstIterator>,
			std::conditional_t<bRangedFor, typename ElementArrayType::TRangedForIterator,      typename ElementArrayType::TIterator     >
		> ElementItType;

		FORCEINLINE TBaseIterator(const ElementItType& InElementIt)
			: ElementIt(InElementIt)
		{
		}

		/** Advances the iterator to the next element. */
		FORCEINLINE TBaseIterator& operator++()
		{
			++ElementIt;
			return *this;
		}

		/** conversion to "bool" returning true if the iterator is valid. */
		FORCEINLINE explicit operator bool() const
		{ 
			return !!ElementIt; 
		}
		/** inverse of the "bool" operator */
		FORCEINLINE bool operator !() const 
		{
			return !(bool)*this;
		}

		// Accessors.
		FORCEINLINE FSetElementId GetId() const
		{
			return FSetElementId(ElementIt.GetIndex());
		}
		FORCEINLINE ItElementType* operator->() const
		{
			return &ElementIt->Value;
		}
		FORCEINLINE ItElementType& operator*() const
		{
			return ElementIt->Value;
		}

		FORCEINLINE bool operator==(const TBaseIterator& Rhs) const { return ElementIt == Rhs.ElementIt; }
		FORCEINLINE bool operator!=(const TBaseIterator& Rhs) const { return ElementIt != Rhs.ElementIt; }

		ElementItType ElementIt;
	};

	/** The base type of whole set iterators. */
	template<bool bConst>
	class TBaseKeyIterator
	{
	private:
		typedef std::conditional_t<bConst, const TSet, TSet> SetType;
		typedef std::conditional_t<bConst,const ElementType,ElementType> ItElementType;
		typedef typename TTypeTraits<typename KeyFuncs::KeyType>::ConstPointerType ReferenceOrValueType;

	public:
		using KeyArgumentType =
			std::conditional_t<
				std::is_reference_v<ReferenceOrValueType>,
				TRetainedRef<std::remove_reference_t<ReferenceOrValueType>>,
				KeyInitType
			>;

		/** Initialization constructor. */
		FORCEINLINE TBaseKeyIterator(SetType& InSet, KeyArgumentType InKey)
			: Set  (InSet)
			, Key  (InKey) //-V1041
			, Index(INDEX_NONE)
		{
			// The set's hash needs to be initialized to find the elements with the specified key.
			Set.ConditionalRehash(Set.Elements.Num(), EAllowShrinking::No);
			if (Set.HashSize)
			{
				NextIndex = Set.GetTypedHash(KeyFuncs::GetKeyHash(Key)).Index;
				++(*this);
			}
			else
			{
				NextIndex = INDEX_NONE;
			}
		}

		/** Advances the iterator to the next element. */
		FORCEINLINE TBaseKeyIterator& operator++()
		{
			Index = NextIndex;

			while (Index != INDEX_NONE)
			{
				NextIndex = Set.Elements[Index].HashNextId.Index;
				checkSlow(Index != NextIndex);

				if (KeyFuncs::Matches(KeyFuncs::GetSetKey(Set.Elements[Index].Value),Key))
				{
					break;
				}

				Index = NextIndex;
			}
			return *this;
		}

		/** conversion to "bool" returning true if the iterator is valid. */
		FORCEINLINE explicit operator bool() const
		{ 
			return Index != INDEX_NONE;
		}
		/** inverse of the "bool" operator */
		FORCEINLINE bool operator !() const 
		{
			return !(bool)*this;
		}

		// Accessors.
		[[nodiscard]] FORCEINLINE FSetElementId GetId() const
		{
			return FSetElementId(Index);
		}
		FORCEINLINE ItElementType* operator->() const
		{
			return &Set.Elements[Index].Value;
		}
		FORCEINLINE ItElementType& operator*() const
		{
			return Set.Elements[Index].Value;
		}

	protected:
		SetType& Set;
		ReferenceOrValueType Key;
		SizeType Index;
		SizeType NextIndex;
	};

public:

	/** Used to iterate over the elements of a const TSet. */
	class TConstIterator : public TBaseIterator<true>
	{
		friend class TSet;

	public:
		FORCEINLINE TConstIterator(const TSet& InSet)
			: TBaseIterator<true>(InSet.Elements.begin())
		{
		}
	};

	/** Used to iterate over the elements of a TSet. */
	class TIterator : public TBaseIterator<false>
	{
		friend class TSet;

	public:
		FORCEINLINE TIterator(TSet& InSet)
			: TBaseIterator<false>(InSet.Elements.begin())
			, Set                 (InSet)
		{
		}

		/** Removes the current element from the set. */
		FORCEINLINE void RemoveCurrent()
		{
			Set.RemoveByIndex(TBaseIterator<false>::ElementIt.GetIndex());
		}

	private:
		TSet& Set;
	};

	using TRangedForConstIterator = TBaseIterator<true, true>;
	using TRangedForIterator      = TBaseIterator<false, true>;

	/** Used to iterate over the elements of a const TSet. */
	class TConstKeyIterator : public TBaseKeyIterator<true>
	{
	private:
		using Super = TBaseKeyIterator<true>;

	public:
		using KeyArgumentType = typename Super::KeyArgumentType;

		FORCEINLINE TConstKeyIterator(const TSet& InSet, KeyArgumentType InKey)
			: Super(InSet, InKey)
		{
		}
	};

	/** Used to iterate over the elements of a TSet. */
	class TKeyIterator : public TBaseKeyIterator<false>
	{
	private:
		using Super = TBaseKeyIterator<false>;

	public:
		using KeyArgumentType = typename Super::KeyArgumentType;

		FORCEINLINE TKeyIterator(TSet& InSet, KeyArgumentType InKey)
			: Super(InSet, InKey)
		{
		}

		/** Removes the current element from the set. */
		FORCEINLINE void RemoveCurrent()
		{
			this->Set.RemoveByIndex(TBaseKeyIterator<false>::Index);
			TBaseKeyIterator<false>::Index = INDEX_NONE;
		}
	};

	/** Creates an iterator for the contents of this set */
	FORCEINLINE TIterator CreateIterator()
	{
		return TIterator(*this);
	}

	/** Creates a const iterator for the contents of this set */
	FORCEINLINE TConstIterator CreateConstIterator() const
	{
		return TConstIterator(*this);
	}

	friend struct TSetPrivateFriend;

public:
	/**
	 * DO NOT USE DIRECTLY
	 * STL-like iterators to enable range-based for loop support.
	 */
	FORCEINLINE TRangedForIterator      begin()       { return TRangedForIterator     (Elements.begin()); }
	FORCEINLINE TRangedForConstIterator begin() const { return TRangedForConstIterator(Elements.begin()); }
	FORCEINLINE TRangedForIterator      end()         { return TRangedForIterator     (Elements.end());   }
	FORCEINLINE TRangedForConstIterator end() const   { return TRangedForConstIterator(Elements.end());   }
};

namespace Freeze
{
	template<typename ElementType, typename KeyFuncs, typename Allocator>
	void IntrinsicWriteMemoryImage(FMemoryImageWriter& Writer, const TSet<ElementType, KeyFuncs, Allocator>& Object, const FTypeLayoutDesc&)
	{
		Object.WriteMemoryImage(Writer);
	}

	template<typename ElementType, typename KeyFuncs, typename Allocator>
	uint32 IntrinsicUnfrozenCopy(const FMemoryUnfreezeContent& Context, const TSet<ElementType, KeyFuncs, Allocator>& Object, void* OutDst)
	{
		Object.CopyUnfrozen(Context, OutDst);
		return sizeof(Object);
	}

	template<typename ElementType, typename KeyFuncs, typename Allocator>
	uint32 IntrinsicAppendHash(const TSet<ElementType, KeyFuncs, Allocator>* DummyObject, const FTypeLayoutDesc& TypeDesc, const FPlatformTypeLayoutParameters& LayoutParams, FSHA1& Hasher)
	{
		TSet<ElementType, KeyFuncs, Allocator>::AppendHash(LayoutParams, Hasher);
		return DefaultAppendHash(TypeDesc, LayoutParams, Hasher);
	}
}

DECLARE_TEMPLATE_INTRINSIC_TYPE_LAYOUT((template <typename ElementType, typename KeyFuncs, typename Allocator>), (TSet<ElementType, KeyFuncs, Allocator>));

struct FScriptSetLayout
{
	// int32 ElementOffset = 0; // always at zero offset from the TSetElement - not stored here
	int32 HashNextIdOffset;
	int32 HashIndexOffset;
	int32 Size;

	FScriptSparseArrayLayout SparseArrayLayout;
};

// Untyped set type for accessing TSet data, like FScriptArray for TArray.
// Must have the same memory representation as a TSet.
template <typename Allocator, typename InDerivedType>
class TScriptSet
{
	using DerivedType = std::conditional_t<std::is_void_v<InDerivedType>, TScriptSet, InDerivedType>;

public:
	static FScriptSetLayout GetScriptLayout(int32 ElementSize, int32 ElementAlignment)
	{
		FScriptSetLayout Result;

		// TSetElement<TPair<Key, Value>>
		FStructBuilder SetElementStruct;
		int32 ElementOffset      = SetElementStruct.AddMember(ElementSize,           ElementAlignment);
		Result.HashNextIdOffset  = SetElementStruct.AddMember(sizeof(FSetElementId), alignof(FSetElementId));
		Result.HashIndexOffset   = SetElementStruct.AddMember(sizeof(int32),         alignof(int32));
		Result.Size              = SetElementStruct.GetSize();
		Result.SparseArrayLayout = FScriptSparseArray::GetScriptLayout(SetElementStruct.GetSize(), SetElementStruct.GetAlignment());

		checkf(ElementOffset == 0, TEXT("The element inside the TSetElement is expected to be at the start of the struct"));

		return Result;
	}

	TScriptSet()
		: HashSize(0)
	{
	}

	bool IsValidIndex(int32 Index) const
	{
		return Elements.IsValidIndex(Index);
	}

	bool IsEmpty() const
	{
		return Elements.IsEmpty();
	}

	int32 Num() const
	{
		return Elements.Num();
	}

	int32 GetMaxIndex() const
	{
		return Elements.GetMaxIndex();
	}

	void* GetData(int32 Index, const FScriptSetLayout& Layout)
	{
		return Elements.GetData(Index, Layout.SparseArrayLayout);
	}

	const void* GetData(int32 Index, const FScriptSetLayout& Layout) const
	{
		return Elements.GetData(Index, Layout.SparseArrayLayout);
	}

	void MoveAssign(DerivedType& Other, const FScriptSetLayout& Layout)
	{
		checkSlow(this != &Other);
		Empty(0, Layout);
		Elements.MoveAssign(Other.Elements, Layout.SparseArrayLayout);
		Hash.MoveToEmpty(Other.Hash);
		HashSize = Other.HashSize; Other.HashSize = 0;
	}

	void Empty(int32 Slack, const FScriptSetLayout& Layout)
	{
		// Empty the elements array, and reallocate it for the expected number of elements.
		Elements.Empty(Slack, Layout.SparseArrayLayout);

		// Calculate the desired hash size for the specified number of elements.
		const int32 DesiredHashSize = Allocator::GetNumberOfHashBuckets(Slack);

		// If the hash hasn't been created yet, or is smaller than the desired hash size, rehash.
		if (Slack != 0 && (HashSize == 0 || HashSize != DesiredHashSize))
		{
			HashSize = DesiredHashSize;

			// Free the old hash.
			Hash.ResizeAllocation(0, HashSize, sizeof(FSetElementId));
		}

		FSetElementId* HashPtr = Hash.GetAllocation();
		for (int32 I = 0; I < HashSize; ++I)
		{
			HashPtr[I].Index = INDEX_NONE;
		}
	}

	void RemoveAt(int32 Index, const FScriptSetLayout& Layout)
	{
		check(IsValidIndex(Index));

		void* ElementBeingRemoved = Elements.GetData(Index, Layout.SparseArrayLayout);

		// Remove the element from the hash.
		for (FSetElementId* NextElementId = &GetTypedHash(GetHashIndexRef(ElementBeingRemoved, Layout)); NextElementId->IsValidId(); NextElementId = &GetHashNextIdRef(Elements.GetData(NextElementId->Index, Layout.SparseArrayLayout), Layout))
		{
			if (NextElementId->Index == Index)
			{
				*NextElementId = GetHashNextIdRef(ElementBeingRemoved, Layout);
				break;
			}
		}

		// Remove the element from the elements array.
		Elements.RemoveAtUninitialized(Layout.SparseArrayLayout, Index);
	}

	/**
	 * Adds an uninitialized object to the set.
	 * The set will need rehashing at some point after this call to make it valid.
	 *
	 * @return  The index of the added element.
	 */
	int32 AddUninitialized(const FScriptSetLayout& Layout)
	{
		int32 Result = Elements.AddUninitialized(Layout.SparseArrayLayout);
		return Result;
	}

	void Rehash(const FScriptSetLayout& Layout, TFunctionRef<uint32 (const void*)> GetKeyHash)
	{
		// Free the old hash.
		Hash.ResizeAllocation(0,0,sizeof(FSetElementId));

		HashSize = Allocator::GetNumberOfHashBuckets(Elements.Num());
		if (HashSize)
		{
			// Allocate the new hash.
			checkSlow(FMath::IsPowerOfTwo(HashSize));
			Hash.ResizeAllocation(0, HashSize, sizeof(FSetElementId));
			for (int32 HashIndex = 0; HashIndex < HashSize; ++HashIndex)
			{
				GetTypedHash(HashIndex) = FSetElementId();
			}

			// Add the existing elements to the new hash.
			int32 Index = 0;
			int32 Count = Elements.Num();
			while (Count)
			{
				if (Elements.IsValidIndex(Index))
				{
					FSetElementId ElementId(Index);

					void* Element = (uint8*)Elements.GetData(Index, Layout.SparseArrayLayout);

					// Compute the hash bucket the element goes in.
					uint32 KeyHash = GetKeyHash(Element);
					int32  HashIndex   = KeyHash & (HashSize - 1);
					GetHashIndexRef(Element, Layout) = KeyHash & (HashSize - 1);

					// Link the element into the hash bucket.
					GetHashNextIdRef(Element, Layout) = GetTypedHash(HashIndex);
					GetTypedHash(HashIndex) = ElementId;

					--Count;
				}

				++Index;
			}
		}
	}

private:
	int32 FindIndexImpl(const void* Element, const FScriptSetLayout& Layout, uint32 KeyHash, TFunctionRef<bool (const void*, const void*)> EqualityFn)
	{
		const int32  HashIndex = KeyHash & (HashSize - 1);

		uint8* CurrentElement = nullptr;
		for (FSetElementId ElementId = GetTypedHash(HashIndex);
			ElementId.IsValidId();
			ElementId = GetHashNextIdRef(CurrentElement, Layout))
		{
			CurrentElement = (uint8*)Elements.GetData(ElementId.Index, Layout.SparseArrayLayout);
			if (EqualityFn(Element, CurrentElement))
			{
				return ElementId.Index;
			}
		}
	
		return INDEX_NONE;
	}

public:
	int32 FindIndex(const void* Element, const FScriptSetLayout& Layout, TFunctionRef<uint32 (const void*)> GetKeyHash, TFunctionRef<bool (const void*, const void*)> EqualityFn)
	{
		if (Elements.Num())
		{
			return FindIndexImpl(Element, Layout, GetKeyHash(Element), EqualityFn);
		}

		return INDEX_NONE;
	}


	int32 FindIndexByHash(const void* Element, const FScriptSetLayout& Layout, uint32 KeyHash, TFunctionRef<bool (const void*, const void*)> EqualityFn)
	{
		if (Elements.Num())
		{
			return FindIndexImpl(Element, Layout, KeyHash, EqualityFn);
		}

		return INDEX_NONE;
	}

	int32 FindOrAdd(const void* Element, const FScriptSetLayout& Layout, TFunctionRef<uint32(const void*)> GetKeyHash, TFunctionRef<bool(const void*, const void*)> EqualityFn, TFunctionRef<void(void*)> ConstructFn)
	{
		uint32 KeyHash = GetKeyHash(Element);
		int32 OldElementIndex = FindIndexByHash(Element, Layout, KeyHash, EqualityFn);
		if (OldElementIndex != INDEX_NONE)
		{
			return OldElementIndex;
		}

		return AddNewElement(Layout, GetKeyHash, KeyHash, ConstructFn);
	}

	void Add(const void* Element, const FScriptSetLayout& Layout, TFunctionRef<uint32(const void*)> GetKeyHash, TFunctionRef<bool(const void*, const void*)> EqualityFn, TFunctionRef<void(void*)> ConstructFn, TFunctionRef<void(void*)> DestructFn)
	{
		uint32 KeyHash = GetKeyHash(Element);
		int32 OldElementIndex = FindIndexByHash(Element, Layout, KeyHash, EqualityFn);
		if (OldElementIndex != INDEX_NONE)
		{
			void* ElementPtr = Elements.GetData(OldElementIndex, Layout.SparseArrayLayout);

			DestructFn(ElementPtr);
			ConstructFn(ElementPtr);

			// We don't update the hash because we don't need to - the new element
			// should have the same hash, but let's just check.
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			// Disable deprecations warnings to stop warnings being thrown by our check macro.
			checkSlow(KeyHash == GetKeyHash(ElementPtr));
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}
		else
		{
			AddNewElement(Layout, GetKeyHash, KeyHash, ConstructFn);
		}
	}

private:
	int32 AddNewElement(const FScriptSetLayout& Layout, TFunctionRef<uint32(const void*)> GetKeyHash, uint32 KeyHash, TFunctionRef<void(void*)> ConstructFn)
	{
		int32 NewElementIndex = Elements.AddUninitialized(Layout.SparseArrayLayout);

		void* ElementPtr = Elements.GetData(NewElementIndex, Layout.SparseArrayLayout);
		ConstructFn(ElementPtr);

		const int32 DesiredHashSize = FDefaultSetAllocator::GetNumberOfHashBuckets(Num());
		if (!HashSize || HashSize < DesiredHashSize)
		{
			// rehash, this will link in our new element if needed:
			Rehash(Layout, GetKeyHash);
		}
		else
		{
			// link the new element into the set:
			int32 HashIndex = KeyHash & (HashSize - 1);
			FSetElementId& TypedHash = GetTypedHash(HashIndex);
			GetHashIndexRef(ElementPtr, Layout) = HashIndex;
			GetHashNextIdRef(ElementPtr, Layout) = TypedHash;
			TypedHash = FSetElementId(NewElementIndex);
		}

		return NewElementIndex;
	}

	typedef TScriptSparseArray<typename Allocator::SparseArrayAllocator> ElementArrayType;
	typedef typename Allocator::HashAllocator::template ForElementType<FSetElementId> HashType;

	ElementArrayType Elements;
	mutable HashType Hash;
	mutable int32    HashSize;

	FORCEINLINE FSetElementId& GetTypedHash(int32 HashIndex) const
	{
		return ((FSetElementId*)Hash.GetAllocation())[HashIndex & (HashSize - 1)];
	}

	static FSetElementId& GetHashNextIdRef(const void* Element, const FScriptSetLayout& Layout)
	{
		return *(FSetElementId*)((uint8*)Element + Layout.HashNextIdOffset);
	}

	static int32& GetHashIndexRef(const void* Element, const FScriptSetLayout& Layout)
	{
		return *(int32*)((uint8*)Element + Layout.HashIndexOffset);
	}

	// This function isn't intended to be called, just to be compiled to validate the correctness of the type.
	static void CheckConstraints()
	{
		typedef TScriptSet  ScriptType;
		typedef TSet<int32> RealType;

		// Check that the class footprint is the same
		static_assert(sizeof (ScriptType) == sizeof (RealType), "TScriptSet's size doesn't match TSet");
		static_assert(alignof(ScriptType) == alignof(RealType), "TScriptSet's alignment doesn't match TSet");

		// Check member sizes
		static_assert(sizeof(DeclVal<ScriptType>().Elements) == sizeof(DeclVal<RealType>().Elements), "TScriptSet's Elements member size does not match TSet's");
		static_assert(sizeof(DeclVal<ScriptType>().Hash)     == sizeof(DeclVal<RealType>().Hash),     "TScriptSet's Hash member size does not match TSet's");
		static_assert(sizeof(DeclVal<ScriptType>().HashSize) == sizeof(DeclVal<RealType>().HashSize), "TScriptSet's HashSize member size does not match TSet's");

		// Check member offsets
		static_assert(STRUCT_OFFSET(ScriptType, Elements) == STRUCT_OFFSET(RealType, Elements), "TScriptSet's Elements member offset does not match TSet's");
		static_assert(STRUCT_OFFSET(ScriptType, Hash)     == STRUCT_OFFSET(RealType, Hash),     "TScriptSet's Hash member offset does not match TSet's");
		static_assert(STRUCT_OFFSET(ScriptType, HashSize) == STRUCT_OFFSET(RealType, HashSize), "TScriptSet's FirstFreeIndex member offset does not match TSet's");
	}

public:
	// These should really be private, because they shouldn't be called, but there's a bunch of code
	// that needs to be fixed first.
	TScriptSet(const TScriptSet&) { check(false); }
	void operator=(const TScriptSet&) { check(false); }
};

template <typename AllocatorType, typename InDerivedType>
struct TIsZeroConstructType<TScriptSet<AllocatorType, InDerivedType>>
{
	enum { Value = true };
};

/**
 * Traits class which determines whether or not a type is a TSet.
 */
template <typename T> struct TIsTSet { enum { Value = false }; };

template <typename ElementType, typename KeyFuncs, typename Allocator> struct TIsTSet<               TSet<ElementType, KeyFuncs, Allocator>> { enum { Value = true }; };
template <typename ElementType, typename KeyFuncs, typename Allocator> struct TIsTSet<const          TSet<ElementType, KeyFuncs, Allocator>> { enum { Value = true }; };
template <typename ElementType, typename KeyFuncs, typename Allocator> struct TIsTSet<      volatile TSet<ElementType, KeyFuncs, Allocator>> { enum { Value = true }; };
template <typename ElementType, typename KeyFuncs, typename Allocator> struct TIsTSet<const volatile TSet<ElementType, KeyFuncs, Allocator>> { enum { Value = true }; };

class FScriptSet : public TScriptSet<FDefaultSetAllocator, FScriptSet>
{
	using Super = TScriptSet<FDefaultSetAllocator, FScriptSet>;

public:
	using Super::Super;
};

struct TSetPrivateFriend
{
	/** Serializer. */
	template<typename ElementType, typename KeyFuncs,typename Allocator>
	static FArchive& Serialize(FArchive& Ar,TSet<ElementType, KeyFuncs, Allocator>& Set)
	{
		// Load the set's new elements.
		Ar << Set.Elements;

		if(Ar.IsLoading() || Ar.IsModifyingWeakAndStrongReferences())
		{
			// Free the old hash.
			Set.Hash.ResizeAllocation(0,0,sizeof(FSetElementId));
			Set.HashSize = 0;

			// Hash the newly loaded elements.
			Set.ConditionalRehash(Set.Elements.Num(), EAllowShrinking::No);
		}

		return Ar;
	}

	/** Structured archive serializer. */
	template<typename ElementType, typename KeyFuncs,typename Allocator>
 	static void SerializeStructured(FStructuredArchive::FSlot Slot, TSet<ElementType, KeyFuncs, Allocator>& Set)
 	{
		Slot << Set.Elements;

		if (Slot.GetUnderlyingArchive().IsLoading() || Slot.GetUnderlyingArchive().IsModifyingWeakAndStrongReferences())
		{
			// Free the old hash.
			Set.Hash.ResizeAllocation(0, 0, sizeof(FSetElementId));
			Set.HashSize = 0;

			// Hash the newly loaded elements.
			Set.ConditionalRehash(Set.Elements.Num(), EAllowShrinking::No);
		}
 	}

	// Legacy comparison operators.  Note that these also test whether the set's elements were added in the same order!
	template<typename ElementType, typename KeyFuncs,typename Allocator>
	static bool LegacyCompareEqual(const TSet<ElementType, KeyFuncs, Allocator>& A, const TSet<ElementType, KeyFuncs, Allocator>& B)
	{
		return A.Elements == B.Elements;
	}
};

/** Serializer. */
template <typename ElementType>
FORCEINLINE FArchive& operator<<(FArchive& Ar, TSetElement<ElementType>& Element)
{
	return Ar << Element.Value;
}

/** Structured archive serializer. */
template <typename ElementType>
FORCEINLINE void operator<<(FStructuredArchive::FSlot& Ar, TSetElement<ElementType>& Element)
{
	Ar << Element.Value;
}

/** Serializer. */
template<typename ElementType, typename KeyFuncs,typename Allocator>
FArchive& operator<<(FArchive& Ar, TSet<ElementType, KeyFuncs, Allocator>& Set)
{
	return TSetPrivateFriend::Serialize(Ar, Set);
}

/** Structured archive serializer. */
template<typename ElementType, typename KeyFuncs,typename Allocator>
void operator<<(FStructuredArchive::FSlot& Ar, TSet<ElementType, KeyFuncs, Allocator>& Set)
{
	TSetPrivateFriend::SerializeStructured(Ar, Set);
}

// Legacy comparison operators.  Note that these also test whether the set's elements were added in the same order!
template<typename ElementType, typename KeyFuncs,typename Allocator>
bool LegacyCompareEqual(const TSet<ElementType, KeyFuncs, Allocator>& A,const TSet<ElementType, KeyFuncs, Allocator>& B)
{
	return TSetPrivateFriend::LegacyCompareEqual(A, B);
}
template<typename ElementType, typename KeyFuncs,typename Allocator>
bool LegacyCompareNotEqual(const TSet<ElementType, KeyFuncs, Allocator>& A,const TSet<ElementType, KeyFuncs, Allocator>& B)
{
	return !TSetPrivateFriend::LegacyCompareEqual(A, B);
}

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "Templates/Decay.h"
#endif
