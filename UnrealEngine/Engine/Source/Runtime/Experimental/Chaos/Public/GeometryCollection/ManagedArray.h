// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "Containers/Array.h"
#include "GeometryCollection/GeometryCollectionSection.h"
#include "Templates/SharedPointer.h"
#include "Templates/UnrealTemplate.h"
#include "Chaos/ChaosArchive.h"
#include "UObject/DestructionObjectVersion.h"
//
#include "ChaosLog.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/BVHParticles.h"
#include "Math/Vector.h"

struct FManagedArrayCollection;
DEFINE_LOG_CATEGORY_STATIC(UManagedArrayLogging, NoLogging, All);

template <typename T>
void TryBulkSerializeManagedArray(Chaos::FChaosArchive& Ar, TArray<T>& Array)
{
	Ar << Array;
}

//Note: see TArray::BulkSerialize for requirements
inline void TryBulkSerializeManagedArray(Chaos::FChaosArchive& Ar, TArray<FVector3f>& Array)
{
	Array.BulkSerialize(Ar);
}

inline void TryBulkSerializeManagedArray(Chaos::FChaosArchive& Ar, TArray<FGuid>& Array)
{
	Array.BulkSerialize(Ar);
}

inline void TryBulkSerializeManagedArray(Chaos::FChaosArchive& Ar, TArray<FIntVector>& Array)
{
	Array.BulkSerialize(Ar);
}

inline void TryBulkSerializeManagedArray(Chaos::FChaosArchive& Ar, TArray<FVector2f>& Array)
{
	Array.BulkSerialize(Ar);
}

inline void TryBulkSerializeManagedArray(Chaos::FChaosArchive& Ar, TArray<float>& Array)
{
	Array.BulkSerialize(Ar);
}

inline void TryBulkSerializeManagedArray(Chaos::FChaosArchive& Ar, TArray<FQuat4f>& Array)
{
	Array.BulkSerialize(Ar);
}

inline void TryBulkSerializeManagedArray(Chaos::FChaosArchive& Ar, TArray<bool>& Array)
{
	Array.BulkSerialize(Ar);
}

inline void TryBulkSerializeManagedArray(Chaos::FChaosArchive& Ar, TArray<int32>& Array)
{
	Array.BulkSerialize(Ar);
}

inline void TryBulkSerializeManagedArray(Chaos::FChaosArchive& Ar, TArray<uint8>& Array)
{
	Array.BulkSerialize(Ar);
}

inline void TryBulkSerializeManagedArray(Chaos::FChaosArchive& Ar, TArray<FIntVector2>& Array)
{
	Array.BulkSerialize(Ar);
}

inline void TryBulkSerializeManagedArray(Chaos::FChaosArchive& Ar, TArray<FIntVector4>& Array)
{
	Array.BulkSerialize(Ar);
}

/***
*  Managed Array Base
*
*  The ManagedArrayBase allows a common base class for the
*  the template class ManagedArray<T>. (see ManagedArray)
*
*/
class FManagedArrayBase : public FNoncopyable
{
	friend FManagedArrayCollection;
protected:
	/**
	* Protected access to array resizing. Only the managers of the Array
	* are allowed to perform a resize. (see friend list above).
	*/
	virtual void Resize(const int32 Num) {};

	/**
	* Protected access to array reservation. Only the managers of the Array
	* are allowed to perform a reserve. (see friend list above).
	*/
	virtual void Reserve(const int32 Num) {};

	/**
	 * Reorder elements given a new ordering. Sizes must match
	 * @param NewOrder Mapping from indices in the new array -> indices in the old array
	 */
	virtual void Reorder(const TArray<int32>& NewOrder) = 0;

	/** 
	 * Reindex given a lookup table
	 * @param InverseNewOrder Mapping from indices into the old array -> indices in the new array
	 */
	//todo: this should really assert, but material is currently relying on both faces and vertices
	virtual void ReindexFromLookup(const TArray<int32>& InverseNewOrder) { }

	/**
	* Init from a predefined Array
	*/
	virtual void Init(const FManagedArrayBase& ) {};

public:
	FManagedArrayBase()
	{
		ClearDirtyFlag();
	}
	
	virtual ~FManagedArrayBase() {}

	FORCEINLINE void ClearDirtyFlag()
	{
		bIsDirty = false;
	}
	
	FORCEINLINE_DEBUGGABLE void MarkDirty()
	{
		bIsDirty = true;
	}
	
	FORCEINLINE bool IsDirty() const
	{
		return bIsDirty;
	}
	
	//todo(ocohen): these should all be private with friend access to managed collection
	
	/** Perform a memory move between the two arrays */
	virtual void ExchangeArrays(FManagedArrayBase& Src) = 0;

	/** Remove elements */
	virtual void RemoveElements(const TArray<int32>& SortedDeletionList)
	{
		check(false);
	}

	/** The length of the array.*/
	virtual int32 Num() const 
	{
		return 0; 
	};

	/** The reserved length of the array.*/
	virtual int32 Max() const
	{
		return 0;
	};

	/** Serialization */
	virtual void Serialize(Chaos::FChaosArchive& Ar) 
	{
		check(false);
	}

	/** TypeSize */
	virtual size_t GetTypeSize() const
	{
		return 0;
	}

	/**
	* Reindex - Adjust index dependent elements.  
	*   Offsets is the size of the dependent group;
	*   Final is post resize of dependent group used for bounds checking on remapped indices.
	*/
	virtual void Reindex(const TArray<int32> & Offsets, const int32 & FinalSize, const TArray<int32> & SortedDeletionList, const TSet<int32> & DeletionSet) { }

#if 0 //not needed until per instance serialization
	/** Swap elements*/
	virtual void Swap(int32 Index1, int32 Index2) = 0;
#endif

	/** Empty the array. */
	virtual void Empty()
	{
		check(false);
	}
private:
	bool bIsDirty;
};

template <typename T>
class TManagedArrayBase;

template <typename T>
void InitHelper(TArray<T>& Array, const TManagedArrayBase<T>& NewTypedArray, int32 Size);
template <typename T>
void InitHelper(TArray<TUniquePtr<T>>& Array, const TManagedArrayBase<TUniquePtr<T>>& NewTypedArray, int32 Size);

/***
*  Managed Array
*
*  Restricts clients ability to resize the array external to the containing manager. 
*/
template<class InElementType>
class TManagedArrayBase : public FManagedArrayBase
{

public:

	using ElementType = InElementType;

	/**
	* Constructor (default) Build an empty shared array
	*
	*/	
	FORCEINLINE TManagedArrayBase()
	{}

	/**
	* Constructor (TArray)
	*
	*/
	FORCEINLINE TManagedArrayBase(const TArray<ElementType>& Other)
		: Array(Other)
	{}

	/**
	* Copy Constructor (default)
	*/
	FORCEINLINE TManagedArrayBase(const TManagedArrayBase<ElementType>& Other) = delete;

	/**
	* Move Constructor
	*/
	FORCEINLINE TManagedArrayBase(TManagedArrayBase<ElementType>&& Other)
		: Array(MoveTemp(Other.Array))
	{}
	FORCEINLINE TManagedArrayBase(TArray<ElementType>&& Other)
		: Array(MoveTemp(Other))
	{}

	/**
	* Assignment operator
	*/
	FORCEINLINE TManagedArrayBase& operator=(TManagedArrayBase<ElementType>&& Other)
	{
		return this->operator=(MoveTemp(Other.Array));
	}

	FORCEINLINE TManagedArrayBase& operator=(TArray<ElementType>&& Other)
	{
		// ryan - is it okay to check that the size matches?
		ensureMsgf(Array.Num() == 0 || Array.Num() == Other.Num(), TEXT("TManagedArrayBase<T>::operator=(TArray<T>&&) : Invalid array size."));
		Array = MoveTemp(Other);
		return *this;
	}

	/**
	* Virtual Destructor 
	*
	*/
	virtual ~TManagedArrayBase()
	{}


	virtual void RemoveElements(const TArray<int32>& SortedDeletionList) override
	{
		if (SortedDeletionList.Num() == 0)
		{
			return;
		}

		int32 RangeStart = SortedDeletionList.Last();
		for (int32 ii = SortedDeletionList.Num()-1 ; ii > -1 ; --ii)
		{
			if (ii == 0)
			{
				Array.RemoveAt(SortedDeletionList[0], RangeStart - SortedDeletionList[0] + 1, false);

			}
			else if (SortedDeletionList[ii] != (SortedDeletionList[ii - 1]+1)) // compare this and previous values to make sure the difference is only 1.
			{
				Array.RemoveAt(SortedDeletionList[ii], RangeStart - SortedDeletionList[ii] + 1, false);
				RangeStart = SortedDeletionList[ii-1];
			}
		}

 		Array.Shrink();
	}


	/**
	* Init from a predefined Array of matching type
	*/
	virtual void Init(const FManagedArrayBase& NewArray) override
	{
		ensureMsgf(NewArray.GetTypeSize() == GetTypeSize(),TEXT("TManagedArrayBase<T>::Init : Invalid array types."));
		const TManagedArrayBase<ElementType> & NewTypedArray = static_cast< const TManagedArrayBase<ElementType>& >(NewArray);
		int32 Size = NewTypedArray.Num();

		Resize(Size);
		InitHelper(Array, NewTypedArray, Size);
	}

	/**
	 * Fill the array with \p Value.
	 */
	void Fill(const ElementType& Value)
	{
		for (int32 Idx = 0; Idx < Array.Num(); ++Idx)
			Array[Idx] = Value;
	}

#if 0
	virtual void Swap(int32 Index1, int32 Index2) override
	{
		Exchange(Array[Index1], Array[Index2]);
	}
#endif

	virtual void ExchangeArrays(FManagedArrayBase& NewArray) override
	{
		//It's up to the caller to make sure that the two arrays are of the same type
		ensureMsgf(NewArray.GetTypeSize() == GetTypeSize(), TEXT("TManagedArrayBase<T>::Exchange : Invalid array types."));
		TManagedArrayBase<ElementType>& NewTypedArray = static_cast<TManagedArrayBase<ElementType>& >(NewArray);

		Exchange(*this, NewTypedArray);
	}

	/**
	* Returning a reference to the element at index.
	*
	* @returns Array element reference
	*/
	FORCEINLINE ElementType & operator[](int Index)
	{
		// @todo : optimization
		// TArray->operator(Index) will perform checks against the 
		// the array. It might be worth implementing the memory
		// management directly on the ManagedArray, to avoid the
		// overhead of the TArray.
		return Array[Index];
	}
	FORCEINLINE const ElementType & operator[](int Index) const
	{
		return Array[Index];
	}

	/**
	* Helper function for returning the internal const array
	*
	* @returns const array of all the elements
	*/
	FORCEINLINE const TArray<ElementType>& GetConstArray()
	{
		return Array;
	}

	FORCEINLINE const TArray<ElementType>& GetConstArray() const
	{
		return Array;
	}
	
	/**
	* Helper function for returning a typed pointer to the first array entry.
	*
	* @returns Pointer to first array entry or nullptr if ArrayMax == 0.
	*/
	FORCEINLINE ElementType* GetData()
	{
		return Array.GetData();
	}

	/**
	* Helper function for returning a typed pointer to the first array entry.
	*
	* @returns Pointer to first array entry or nullptr if ArrayMax == 0.
	*/
	FORCEINLINE const ElementType * GetData() const
	{
		return Array.GetData();
	}

	/**
	* Helper function returning the size of the inner type.
	*
	* @returns Size in bytes of array type.
	*/
	FORCEINLINE size_t GetTypeSize() const override
	{
		return sizeof(ElementType);
	}

	/**
	* Returning the size of the array
	*
	* @returns Array size
	*/
	FORCEINLINE int32 Num() const override
	{
		return Array.Num();
	}

	FORCEINLINE int32 Max() const override
	{
		return Array.Max();
	}

	FORCEINLINE bool Contains(const ElementType& Item) const
	{
		return Array.Contains(Item);
	}

	/**
	* Find first index of the element
	*/
	int32 Find(const ElementType& Item) const
	{
		return Array.Find(Item);
	}

	/**
	* Count the number of entries match \p Item.
	*/
	int32 Count(const ElementType& Item) const
	{
		int32 Num = 0;
		for (int32 Idx = 0; Idx < Array.Num(); ++Idx)
			Num += Array[Idx] == Item ? 1 : 0;
		return Num;
	}

	/**
	* Checks if index is in array range.
	*
	* @param Index Index to check.
	*/
	FORCEINLINE void RangeCheck(int32 Index) const
	{
		checkf((Index >= 0) & (Index < Array.Num()), TEXT("Array index out of bounds: %i from an array of size %i"), Index, Array.Num());
	}

	/**
	* Serialization Support
	*
	* @param Chaos::FChaosArchive& Ar
	*/
	virtual void Serialize(Chaos::FChaosArchive& Ar)
	{		
		Ar.UsingCustomVersion(FDestructionObjectVersion::GUID);
		int Version = 1;
		Ar << Version;
	
		if (Ar.CustomVer(FDestructionObjectVersion::GUID) < FDestructionObjectVersion::BulkSerializeArrays)
		{
			Ar << Array;
		}
		else
		{
			TryBulkSerializeManagedArray(Ar, Array);
		}
	}

	typedef typename TArray<InElementType>::RangedForIteratorType		RangedForIteratorType;
	typedef typename TArray<InElementType>::RangedForConstIteratorType	RangedForConstIteratorType;

	/**
	* DO NOT USE DIRECTLY
	* STL-like iterators to enable range-based for loop support.
	*/
	FORCEINLINE RangedForIteratorType      begin()			{ return Array.begin(); }
	FORCEINLINE RangedForConstIteratorType begin() const	{ return Array.begin(); }
	FORCEINLINE RangedForIteratorType      end  ()			{ return Array.end(); }
	FORCEINLINE RangedForConstIteratorType end  () const	{ return Array.end(); }

private:
	/**
	* Protected Resize to prevent external resizing of the array
	*
	* @param New array size.
	*/
	void Resize(const int32 Size) 
	{ 
		Array.SetNum(Size,true);
	}

	/**
	* Protected Reserve to prevent external reservation of the array
	*
	* @param New array reservation size.
	*/
	void Reserve(const int32 Size)
	{
		Array.Reserve(Size);
	}

	/**
	* Protected clear to prevent external clearing of the array
	*
	*/
	void Empty()
	{
		Array.Empty();
	}

	void Reorder(const TArray<int32>& NewOrder) override
	{
		const int32 NumElements = Num();
		check(NewOrder.Num() == NumElements);
		TArray<InElementType> NewArray;
		NewArray.AddDefaulted(NumElements);
		for (int32 OriginalIdx = 0; OriginalIdx < NumElements; ++OriginalIdx)
		{
			NewArray[OriginalIdx] = MoveTemp(Array[NewOrder[OriginalIdx]]);
		}
		Exchange(Array, NewArray);
	}

	TArray<InElementType> Array;

};

template <typename T>
void InitHelper(TArray<T>& Array, const TManagedArrayBase<T>& NewTypedArray, int32 Size)
{
	for (int32 Index = 0; Index < Size; Index++)
	{
		Array[Index] = NewTypedArray[Index];
	}
}

template <typename T>
void InitHelper(TArray<TUniquePtr<T>>& Array, const TManagedArrayBase<TUniquePtr<T>>& NewTypedArray, int32 Size)
{
	for (int32 Index = 0; Index < Size; Index++)
	{
		if (NewTypedArray[Index])
		{
			Array[Index].Reset((T*)NewTypedArray[Index]->Copy().Release());
		}
	}
}

//
//
//
#define UNSUPPORTED_UNIQUE_ARRAY_COPIES(TYPE, NAME) \
template<> inline void InitHelper(TArray<TYPE>& Array, const TManagedArrayBase<TYPE>& NewTypedArray, int32 Size) { \
	UE_LOG(LogChaos,Warning, TEXT("Cannot make a copy of unique array of type (%s) within the managed array collection. Regenerate unique pointer attributes if needed."), NAME); \
}

typedef TUniquePtr<Chaos::TGeometryParticle<Chaos::FReal, 3>> LOCAL_MA_UniqueTGeometryParticle;
UNSUPPORTED_UNIQUE_ARRAY_COPIES(LOCAL_MA_UniqueTGeometryParticle, TEXT("Chaos::TGeometryParticle"));

typedef TUniquePtr<Chaos::FBVHParticles, TDefaultDelete<Chaos::FBVHParticles>> LOCAL_MA_UniqueTBVHParticles;
UNSUPPORTED_UNIQUE_ARRAY_COPIES(LOCAL_MA_UniqueTBVHParticles, TEXT("Chaos::FBVHParticles"));

typedef TUniquePtr<TArray<UE::Math::TVector<float>>> LOCAL_MA_UniqueTArrayTVector;
UNSUPPORTED_UNIQUE_ARRAY_COPIES(LOCAL_MA_UniqueTArrayTVector, TEXT("TArray<UE::Math::TVector<float>>"));

template<class InElementType>
class TManagedArray : public TManagedArrayBase<InElementType>
{
public:
	FORCEINLINE TManagedArray()
	{}

	FORCEINLINE TManagedArray(const TArray<InElementType>& Other)
		: TManagedArrayBase<InElementType>(Other)
	{}

	FORCEINLINE TManagedArray(TManagedArray<InElementType>&& Other)
		: TManagedArrayBase<InElementType>(MoveTemp(Other))
	{}

	FORCEINLINE TManagedArray(TArray<InElementType>&& Other)
		: TManagedArrayBase<InElementType>(MoveTemp(Other))
	{}

	FORCEINLINE TManagedArray& operator=(TManagedArray<InElementType>&& Other)
	{
		TManagedArrayBase<InElementType>::operator=(MoveTemp(Other));
		return *this;
	}

	FORCEINLINE TManagedArray(const TManagedArray<InElementType>& Other) = delete;

	virtual ~TManagedArray()
	{}
};

template<>
class TManagedArray<int32> : public TManagedArrayBase<int32>
{
public:
    using TManagedArrayBase<int32>::Num;

	FORCEINLINE TManagedArray()
	{}

	FORCEINLINE TManagedArray(const TArray<int32>& Other)
		: TManagedArrayBase<int32>(Other)
	{}

	FORCEINLINE TManagedArray(const TManagedArray<int32>& Other) = delete;
	FORCEINLINE TManagedArray(TManagedArray<int32>&& Other) = default;
	FORCEINLINE TManagedArray(TArray<int32>&& Other)
		: TManagedArrayBase<int32>(MoveTemp(Other))
	{}
	FORCEINLINE TManagedArray& operator=(TManagedArray<int32>&& Other) = default;

	virtual ~TManagedArray()
	{}

	virtual void Reindex(const TArray<int32> & Offsets, const int32 & FinalSize, const TArray<int32> & SortedDeletionList, const TSet<int32>& DeletionSet) override
	{
		UE_LOG(UManagedArrayLogging, Log, TEXT("TManagedArray<int32>[%p]::Reindex()"),this);

		int32 ArraySize = Num(), MaskSize = Offsets.Num();
		for (int32 Index = 0; Index < ArraySize; Index++)
		{
			int32 RemapVal = this->operator[](Index);
			if (0 <= RemapVal)
			{
				ensure(RemapVal < MaskSize);
				if (DeletionSet.Contains(this->operator[](Index)))
				{
					this->operator[](Index) = INDEX_NONE;
				}
				else
				{
					this->operator[](Index) -= Offsets[RemapVal];
				}
				ensure(-1 <= this->operator[](Index));
			}
		}
	}

	virtual void ReindexFromLookup(const TArray<int32>& InverseNewOrder) override
	{
		const int32 ArraySize = Num();
		for (int32 Index = 0; Index < ArraySize; ++Index)
		{
			int32& Mapping = this->operator[](Index);
			if (Mapping >= 0)
			{
				Mapping = InverseNewOrder[Mapping];
			}
		}
	}
};


template<>
class TManagedArray<TSet<int32>> : public TManagedArrayBase<TSet<int32>>
{
public:
	using TManagedArrayBase<TSet<int32>>::Num;

	FORCEINLINE TManagedArray()
	{}

	FORCEINLINE TManagedArray(const TArray<TSet<int32>>& Other)
		: TManagedArrayBase< TSet<int32> >(Other)
	{}

	FORCEINLINE TManagedArray(const TManagedArray<TSet<int32>>& Other) = delete;
	FORCEINLINE TManagedArray(TManagedArray<TSet<int32>>&& Other) = default;
	FORCEINLINE TManagedArray(TArray<TSet<int32>>&& Other)
		: TManagedArrayBase<TSet<int32>>(MoveTemp(Other))
	{}
	FORCEINLINE TManagedArray& operator=(TManagedArray<TSet<int32>>&& Other) = default;

	virtual ~TManagedArray()
	{}
	
	virtual void Reindex(const TArray<int32> & Offsets, const int32 & FinalSize, const TArray<int32> & SortedDeletionList, const TSet<int32>& DeletionSet) override
	{
		UE_LOG(UManagedArrayLogging, Log, TEXT("TManagedArray<TArray<int32>>[%p]::Reindex()"), this);
		
		int32 ArraySize = Num(), MaskSize = Offsets.Num();

		for (int32 Index = 0; Index < ArraySize; Index++)
		{
			TSet<int32>& NewSet = this->operator[](Index);
			
			for (int32 Del : SortedDeletionList)
			{
				NewSet.Remove(Del);
			}

			TSet<int32> OldSet = this->operator[](Index);	//need a copy since we're modifying the entries in the set (can't edit in place because value desyncs from hash)
			NewSet.Reset();	//maybe we should remove 

			for (int32 StaleEntry : OldSet)
			{
				const int32 NewEntry = StaleEntry - Offsets[StaleEntry];
				NewSet.Add(NewEntry);
			}
		}
	}

	virtual void ReindexFromLookup(const TArray<int32> & InverseNewOrder) override
	{

		int32 ArraySize = Num();
		
		for (int32 Index = 0; Index < ArraySize; Index++)
		{
			TSet<int32>& NewSet = this->operator[](Index);
			TSet<int32> OldSet = this->operator[](Index);	//need a copy since we're modifying the entries in the set
			NewSet.Reset();	//maybe we should remove 

			for (int32 StaleEntry : OldSet)
			{
				const int32 NewEntry = StaleEntry >= 0 ? InverseNewOrder[StaleEntry] : StaleEntry;	//only remap if valid
				NewSet.Add(NewEntry);
			}
		}
	}
};

template<>
class TManagedArray<FIntVector> : public TManagedArrayBase<FIntVector>
{
public:
    using TManagedArrayBase<FIntVector>::Num;

	FORCEINLINE TManagedArray()
	{}

	FORCEINLINE TManagedArray(const TArray<FIntVector>& Other)
		: TManagedArrayBase<FIntVector>(Other)
	{}

	FORCEINLINE TManagedArray(const TManagedArray<FIntVector>& Other) = delete;
	FORCEINLINE TManagedArray(TManagedArray<FIntVector>&& Other) = default;
	FORCEINLINE TManagedArray(TArray<FIntVector>&& Other)
		: TManagedArrayBase<FIntVector>(MoveTemp(Other))
	{}
	FORCEINLINE TManagedArray& operator=(TManagedArray<FIntVector>&& Other) = default;

	virtual ~TManagedArray()
	{}

	virtual void Reindex(const TArray<int32> & Offsets, const int32 & FinalSize, const TArray<int32> & SortedDeletionList, const TSet<int32>& DeletionSet) override
	{
		UE_LOG(UManagedArrayLogging, Log, TEXT("TManagedArray<FIntVector>[%p]::Reindex()"), this);
		int32 ArraySize = Num(), MaskSize = Offsets.Num();
		for (int32 Index = 0; Index < ArraySize; Index++)
		{
			const FIntVector & RemapVal = this->operator[](Index);
			for (int i = 0; i < 3; i++)
			{
				if (0 <= RemapVal[i])
				{
					ensure(RemapVal[i] < MaskSize);
					if (DeletionSet.Contains(this->operator[](Index)[i]))
					{
						this->operator[](Index)[i] = INDEX_NONE;
					}
					else
					{
						this->operator[](Index)[i] -= Offsets[RemapVal[i]];
					}
					ensure(-1 <= this->operator[](Index)[i] && this->operator[](Index)[i] <= FinalSize);
				}
			}
		}
	}

	virtual void ReindexFromLookup(const TArray<int32> & InverseNewOrder) override
	{
		int32 ArraySize = Num();
		for (int32 Index = 0; Index < ArraySize; Index++)
		{
			FIntVector& RemapVal = this->operator[](Index);
			for (int i = 0; i < 3; i++)
			{
				if (RemapVal[i] >= 0)
				{
					RemapVal[i] = InverseNewOrder[RemapVal[i]];
				}
			}
		}
	}
};

template<>
class TManagedArray<FIntVector2> : public TManagedArrayBase<FIntVector2>
{
public:
	using TManagedArrayBase<FIntVector2>::Num;

	FORCEINLINE TManagedArray()
	{}

	FORCEINLINE TManagedArray(const TArray<FIntVector2>& Other)
		: TManagedArrayBase<FIntVector2>(Other)
	{}

	FORCEINLINE TManagedArray(const TManagedArray<FIntVector2>& Other) = delete;
	FORCEINLINE TManagedArray(TManagedArray<FIntVector2>&& Other) = default;
	FORCEINLINE TManagedArray(TArray<FIntVector2>&& Other)
		: TManagedArrayBase<FIntVector2>(MoveTemp(Other))
	{}
	FORCEINLINE TManagedArray& operator=(TManagedArray<FIntVector2>&& Other) = default;

	virtual ~TManagedArray()
	{}

	virtual void Reindex(const TArray<int32>& Offsets, const int32& FinalSize, const TArray<int32>& SortedDeletionList, const TSet<int32>& DeletionSet) override
	{
		UE_LOG(UManagedArrayLogging, Log, TEXT("TManagedArray<FIntVector>[%p]::Reindex()"), this);
		int32 ArraySize = Num(), MaskSize = Offsets.Num();
		for (int32 Index = 0; Index < ArraySize; Index++)
		{
			const FIntVector2& RemapVal = this->operator[](Index);
			for (int i = 0; i < 2; i++)
			{
				if (0 <= RemapVal[i])
				{
					ensure(RemapVal[i] < MaskSize);
					if (DeletionSet.Contains(this->operator[](Index)[i]))
					{
						this->operator[](Index)[i] = INDEX_NONE;
					}
					else
					{
						this->operator[](Index)[i] -= Offsets[RemapVal[i]];
					}
					ensure(-1 <= this->operator[](Index)[i] && this->operator[](Index)[i] <= FinalSize);
				}
			}
		}
	}

	virtual void ReindexFromLookup(const TArray<int32>& InverseNewOrder) override
	{
		int32 ArraySize = Num();
		for (int32 Index = 0; Index < ArraySize; Index++)
		{
			FIntVector2& RemapVal = this->operator[](Index);
			for (int i = 0; i < 2; i++)
			{
				if (RemapVal[i] >= 0)
				{
					RemapVal[i] = InverseNewOrder[RemapVal[i]];
				}
			}
		}
	}
};

template<>
class TManagedArray<TArray<FIntVector2>> : public TManagedArrayBase<TArray<FIntVector2>>
{
public:
	using TManagedArrayBase<TArray<FIntVector2>>::Num;

	FORCEINLINE TManagedArray()
	{}

	FORCEINLINE TManagedArray(const TArray<TArray<FIntVector2>>& Other)
		: TManagedArrayBase<TArray<FIntVector2>>(Other)
	{}

	FORCEINLINE TManagedArray(const TManagedArray<TArray<FIntVector2>>& Other) = delete;
	FORCEINLINE TManagedArray(TManagedArray<TArray<FIntVector2>>&& Other) = default;
	FORCEINLINE TManagedArray(TArray<TArray<FIntVector2>>&& Other)
		: TManagedArrayBase<TArray<FIntVector2>>(MoveTemp(Other))
	{}
	FORCEINLINE TManagedArray& operator=(TManagedArray<TArray<FIntVector2>>&& Other) = default;

	virtual ~TManagedArray()
	{}

	virtual void Reindex(const TArray<int32>& Offsets, const int32& FinalSize, const TArray<int32>& SortedDeletionList, const TSet<int32>& DeletionSet) override
	{
		UE_LOG(UManagedArrayLogging, Log, TEXT("TManagedArray<FIntVector>[%p]::Reindex()"), this);
		int32 ArraySize = Num(), MaskSize = Offsets.Num();
		for (int32 Index = 0; Index < ArraySize; Index++)
		{
			const TArray<FIntVector2>& RemapValArray = this->operator[](Index);
			for (int32 ArrayIndex = 0; ArrayIndex < RemapValArray.Num(); ArrayIndex++)
			{
				const FIntVector2& RemapVal = RemapValArray[ArrayIndex];
				for (int i = 0; i < 2; i++)
				{
					if (0 <= RemapVal[i])
					{
						ensure(RemapVal[i] < MaskSize);
						if (DeletionSet.Contains(this->operator[](Index)[ArrayIndex][i]))
						{
							this->operator[](Index)[ArrayIndex][i] = INDEX_NONE;
						}
						else
						{
							this->operator[](Index)[ArrayIndex][i] -= Offsets[RemapVal[i]];
						}
						ensure(-1 <= this->operator[](Index)[ArrayIndex][i] && this->operator[](Index)[ArrayIndex][i] <= FinalSize);
					}
				}
			}
		}
	}

	virtual void ReindexFromLookup(const TArray<int32>& InverseNewOrder) override
	{
		int32 ArraySize = Num();
		for (int32 Index = 0; Index < ArraySize; Index++)
		{
			TArray<FIntVector2>& RemapValArray = this->operator[](Index);
			for (int32 ArrayIndex = 0; ArrayIndex < RemapValArray.Num(); ArrayIndex++)
			{
				FIntVector2& RemapVal = RemapValArray[ArrayIndex];
				for (int i = 0; i < 2; i++)
				{
					if (RemapVal[i] >= 0)
					{
						RemapVal[i] = InverseNewOrder[RemapVal[i]];
					}
				}
			}
		}
	}
};

template<>
class TManagedArray<FIntVector4> : public TManagedArrayBase<FIntVector4>
{
public:
	using TManagedArrayBase<FIntVector4>::Num;

	FORCEINLINE TManagedArray()
	{}

	FORCEINLINE TManagedArray(const TArray<FIntVector4>& Other)
		: TManagedArrayBase<FIntVector4>(Other)
	{}

	FORCEINLINE TManagedArray(const TManagedArray<FIntVector4>& Other) = delete;
	FORCEINLINE TManagedArray(TManagedArray<FIntVector4>&& Other) = default;
	FORCEINLINE TManagedArray(TArray<FIntVector4>&& Other)
		: TManagedArrayBase<FIntVector4>(MoveTemp(Other))
	{}
	FORCEINLINE TManagedArray& operator=(TManagedArray<FIntVector4>&& Other) = default;

	virtual ~TManagedArray()
	{}

	virtual void Reindex(const TArray<int32>& Offsets, const int32& FinalSize, const TArray<int32>& SortedDeletionList, const TSet<int32>& DeletionSet) override
	{
		UE_LOG(UManagedArrayLogging, Log, TEXT("TManagedArray<FIntVector>[%p]::Reindex()"), this);
		int32 ArraySize = Num(), MaskSize = Offsets.Num();
		for (int32 Index = 0; Index < ArraySize; Index++)
		{
			const FIntVector4& RemapVal = this->operator[](Index);
			for (int i = 0; i < 4; i++)
			{
				if (0 <= RemapVal[i])
				{
					ensure(RemapVal[i] < MaskSize);
					if (DeletionSet.Contains(this->operator[](Index)[i]))
					{
						this->operator[](Index)[i] = INDEX_NONE;
					}
					else
					{
						this->operator[](Index)[i] -= Offsets[RemapVal[i]];
					}
					ensure(-1 <= this->operator[](Index)[i] && this->operator[](Index)[i] <= FinalSize);
				}
			}
		}
	}

	virtual void ReindexFromLookup(const TArray<int32>& InverseNewOrder) override
	{
		int32 ArraySize = Num();
		for (int32 Index = 0; Index < ArraySize; Index++)
		{
			FIntVector4& RemapVal = this->operator[](Index);
			for (int i = 0; i < 4; i++)
			{
				if (RemapVal[i] >= 0)
				{
					RemapVal[i] = InverseNewOrder[RemapVal[i]];
				}
			}
		}
	}
};

template<>
class TManagedArray<TArray<int32>> : public TManagedArrayBase<TArray<int32>>
{
public:
	using TManagedArrayBase<TArray<int32>>::Num;

	FORCEINLINE TManagedArray()
	{}

	FORCEINLINE TManagedArray(const TArray<TArray<int32>>& Other)
		: TManagedArrayBase<TArray<int32>>(Other)
	{}

	FORCEINLINE TManagedArray(const TManagedArray<TArray<int32>>& Other) = delete;
	FORCEINLINE TManagedArray(TManagedArray<TArray<int32>>&& Other) = default;
	FORCEINLINE TManagedArray(TArray<TArray<int32>>&& Other)
		: TManagedArrayBase<TArray<int32>>(MoveTemp(Other))
	{}
	FORCEINLINE TManagedArray& operator=(TManagedArray<TArray<int32>>&& Other) = default;

	virtual ~TManagedArray()
	{}

	virtual void Reindex(const TArray<int32>& Offsets, const int32& FinalSize, const TArray<int32>& SortedDeletionList, const TSet<int32>& DeletionSet) override
	{
		UE_LOG(UManagedArrayLogging, Log, TEXT("TManagedArray<FIntVector>[%p]::Reindex()"), this);
		int32 ArraySize = Num(), MaskSize = Offsets.Num();
		for (int32 Index = 0; Index < ArraySize; Index++)
		{
			const TArray<int32>& RemapValArray = this->operator[](Index);
			for (int32 ArrayIndex = 0; ArrayIndex < RemapValArray.Num(); ArrayIndex++)
			{
				if (0 <= RemapValArray[ArrayIndex])
				{
					ensure(RemapValArray[ArrayIndex] < MaskSize);
					if (DeletionSet.Contains(this->operator[](Index)[ArrayIndex]))
					{
						this->operator[](Index)[ArrayIndex] = INDEX_NONE;
					}
					else
					{
						this->operator[](Index)[ArrayIndex] -= Offsets[RemapValArray[ArrayIndex]];
					}
					ensure(-1 <= this->operator[](Index)[ArrayIndex] && this->operator[](Index)[ArrayIndex] <= FinalSize);
				}
			}
		}
	}

	virtual void ReindexFromLookup(const TArray<int32>& InverseNewOrder) override
	{
		int32 ArraySize = Num();
		for (int32 Index = 0; Index < ArraySize; Index++)
		{
			TArray<int32>& RemapValArray = this->operator[](Index);
			for (int32 ArrayIndex = 0; ArrayIndex < RemapValArray.Num(); ArrayIndex++)
			{
				if (RemapValArray[ArrayIndex] >= 0)
				{
					RemapValArray[ArrayIndex] = InverseNewOrder[RemapValArray[ArrayIndex]];
				}
			}
		}
	}
};
