// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Class.h"
#include "Containers/BitArray.h"
#include "InstancedStruct.h"
#include "StructUtilsTypes.h"


/**
 * The TStructTypeBitSet holds information on "existence" of subtypes of a given UStruct. The information on 
 * available child-structs is gathered lazily - the internal FStructTracker is assigning a given type a new index the
 * very first time the type is encountered. 
 * To create a specific instantiation of the type you also need to provide a type that will instantiate a static 
 * FStructTracker instance. The supplied macros hide this details away however.
 * 
 * To do declare a bitset type for an arbitrary struct type FFooBar add the following in your header or cpp file:
 * 
 *	DECLARE_STRUCTTYPEBITSET(FMyFooBarBitSet, FFooBar);
 * 
 * where FMyFooBarBitSet is the alias of the type you can use in your code. To have your type exposed to other modules 
 * use DECLARE_STRUCTTYPEBITSET_EXPORTED, like so:
 * 
 *	DECLARE_STRUCTTYPEBITSET_EXPORTED(MYMODULE_API, FMyFooBarBitSet, FFooBar);
 *
 * You also need to instantiate the static FStructTracker added by the DECLARE* macro. You can easily do it by placing 
 * the following in your cpp file (continuing the FFooBar example):
 *
 *	DEFINE_TYPEBITSET(FMyFooBarBitSet);
 * 
 */
struct FStructTracker
{
	int32 FindOrAddStructTypeIndex(const UStruct& InStructType)
	{
		// Get existing index...
		const uint32 Hash = PointerHash(&InStructType);
		FSetElementId ElementId = StructTypeToIndexSet.FindIdByHash(Hash, Hash);

		if (!ElementId.IsValidId())
		{
			// .. or create new one
			ElementId = StructTypeToIndexSet.AddByHash(Hash, Hash);
			checkSlow(ElementId.IsValidId());

			checkSlow(StructTypesList.Num() == ElementId.AsInteger());
			StructTypesList.Add(&InStructType);
		}
		const int32 Index = ElementId.AsInteger();

#if WITH_STRUCTUTILS_DEBUG
		if (Index == DebugStructTypeNamesList.Num())
		{
			DebugStructTypeNamesList.Add(InStructType.GetFName());
			ensure(StructTypeToIndexSet.Num() == DebugStructTypeNamesList.Num());
		}
#endif // WITH_STRUCTUTILS_DEBUG
		return Index;
	}

	const UStruct* GetStructType(const int32 StructTypeIndex) const
	{
		return StructTypesList.IsValidIndex(StructTypeIndex) ? StructTypesList[StructTypeIndex].Get() : nullptr;
	}

	int32 Num() const {	return StructTypeToIndexSet.Num(); }

#if WITH_STRUCTUTILS_DEBUG
	/**
	* @return index identifying given tag or INDEX_NONE if it has never been used/seen before.
	*/
	FName DebugGetStructTypeName(const int32 StructTypeIndex) const
	{
		return DebugStructTypeNamesList.IsValidIndex(StructTypeIndex) ? DebugStructTypeNamesList[StructTypeIndex] : FName();
	}

	template<typename T>
	TConstArrayView<TWeakObjectPtr<const T>> DebugGetAllStructTypes() const 
	{ 
		return MakeArrayView(reinterpret_cast<const TWeakObjectPtr<const T>*>(StructTypesList.GetData()), StructTypesList.Num());
	}

	void DebugResetStructTypeMappingInfo()
	{
		StructTypeToIndexSet.Reset();
		StructTypesList.Reset();
		DebugStructTypeNamesList.Reset();
	}
	TArray<FName, TInlineAllocator<64>> DebugStructTypeNamesList;
#endif // WITH_STRUCTUTILS_DEBUG

	TSet<uint32> StructTypeToIndexSet;
	TArray<TWeakObjectPtr<const UStruct>, TInlineAllocator<64>> StructTypesList;
};

template<typename TBaseStruct, typename TStructTrackerWrapper, typename TUStructType = UScriptStruct>
struct TStructTypeBitSet
{
	using FStructTrackerWrapper = TStructTrackerWrapper;

private:
	struct FBitArrayExt : TBitArray<>
	{
		FBitArrayExt() = default;
		FBitArrayExt(const TBitArray<>& Source) : TBitArray<>(Source)
		{}

		FBitArrayExt& operator=(const TBitArray<>& Other)
		{
			*((TBitArray<>*)this) = Other;
			return *this;
		}

		void SetAll(const bool bValue)
		{
			Init(bValue, TStructTrackerWrapper::StructTracker.Num());
		}

		FORCEINLINE bool HasAll(const TBitArray<>& Other) const
		{
			FConstWordIterator ThisIterator(*this);
			FConstWordIterator OtherIterator(Other);

			while (ThisIterator || OtherIterator)
			{
				const uint32 A = ThisIterator ? ThisIterator.GetWord() : 0;
				const uint32 B = OtherIterator ? OtherIterator.GetWord() : 0;
				if ((A & B) != B)
				{
					return false;
				}

				++ThisIterator;
				++OtherIterator;
			}

			return true;
		}

		FORCEINLINE bool HasAny(const TBitArray<>& Other) const
		{
			FConstWordIterator ThisIterator(*this);
			FConstWordIterator OtherIterator(Other);

			while (ThisIterator || OtherIterator)
			{
				const uint32 A = ThisIterator ? ThisIterator.GetWord() : 0;
				const uint32 B = OtherIterator ? OtherIterator.GetWord() : 0;
				if ((A & B) != 0)
				{
					return true;
				}

				++ThisIterator;
				++OtherIterator;
			}

			return false;
		}

		FORCEINLINE bool IsEmpty() const
		{
			FConstWordIterator Iterator(*this);

			while (Iterator && Iterator.GetWord() == 0)
			{
				++Iterator;
			}

			return !Iterator;
		}

		FORCEINLINE void operator-=(const TBitArray<>& Other)
		{
			FWordIterator ThisIterator(*this);
			FConstWordIterator OtherIterator(Other);

			while (ThisIterator && OtherIterator)
			{
				ThisIterator.SetWord(ThisIterator.GetWord() & ~OtherIterator.GetWord());

				++ThisIterator;
				++OtherIterator;
			}
		}

		FORCEINLINE friend uint32 GetTypeHash(const FBitArrayExt& Instance)
		{
			FConstWordIterator Iterator(Instance);
			uint32 Hash = 0;
			uint32 TrailingZeroHash = 0;
			while (Iterator)
			{
				const uint32 Word = Iterator.GetWord();
				if (Word)
				{
					Hash = HashCombine(TrailingZeroHash ? TrailingZeroHash : Hash, Word);
					TrailingZeroHash = 0;
				}
				else // potentially a trailing 0-word
				{
					TrailingZeroHash = HashCombine(TrailingZeroHash ? TrailingZeroHash : Hash, Word);
				}
				++Iterator;
			}
			return Hash;
		}

		void AddAtIndex(const int32 Index)
		{
			PadToNum(Index + 1, false);
			SetBitNoCheck(Index, true);
		}

		void RemoveAtIndex(const int32 Index)
		{
			check(Index >= 0);
			if (Index < Num())
			{
				SetBitNoCheck(Index, false);
			}
			// else, it's already not present
		}

		bool Contains(const int32 Index) const
		{
			check(Index >= 0);
			return (Index < Num()) && (*this)[Index];
		}

	protected:
		/**
		 * duplication of TBitArray::SetBitNoCheck needed since it's private but it's the performant way of setting bits
		 * when we know the index is valid.
		 * @todo ask Core team about exposing that
		 */
		void SetBitNoCheck(const int32 Index, const bool Value)
		{
			uint32& Word = GetData()[Index / NumBitsPerDWORD];
			const uint32 BitOffset = (Index % NumBitsPerDWORD);
			Word = (Word & ~(1 << BitOffset)) | (((uint32)Value) << BitOffset);
		}
	};

public:
	TStructTypeBitSet() = default;

	explicit TStructTypeBitSet(const TUStructType& StructType)
	{
		Add(StructType);
	}

	explicit TStructTypeBitSet(std::initializer_list<const TUStructType*> InitList)
	{
		for (const TUStructType* StructType : InitList)
		{
			if (StructType)
			{
				Add(*StructType);
			}
		}
	}

	explicit TStructTypeBitSet(TConstArrayView<const TUStructType*> InitList)
	{
		for (const TUStructType* StructType : InitList)
		{
			if (StructType)
			{
				Add(*StructType);
			}
		}
	}

	/** This flavor of constructor is only available for UScriptStructs */
	template<typename T = TBaseStruct, typename = typename TEnableIf<!TIsDerivedFrom<T, UObject>::IsDerived>::Type>
	explicit TStructTypeBitSet(TConstArrayView<FInstancedStruct> InitList)
	{
		for (const FInstancedStruct& StructInstance : InitList)
		{
			if (StructInstance.GetScriptStruct())
			{
				Add(*StructInstance.GetScriptStruct());
			}
		}
	}

	/** Retrieves indices of bits of given value. */
	struct FIndexIterator
	{
	public:
		explicit FIndexIterator(const FBitArrayExt& BitArray, const bool bInValueToCheck = true)
			: It(BitArray), bValueToCheck(bInValueToCheck)
		{
			if (It && It.GetValue() != bInValueToCheck)
			{
				// will result in either setting IT to the first bInValue, or making bool(It) == false
				++(*this);
			}
		}

		operator bool() const { return bool(It); }

		/** @todo can be optimized to skip whole empty words, similar to TBitArray<>::Find */
		FIndexIterator& operator++() 
		{ 
			while (++It)
			{
				if (It.GetValue() == bValueToCheck)
				{
					break;
				}
			}
			return *this; 
		}

		int32 operator*() const
		{
			return It.GetIndex();
		}

	private:
		TBitArray<>::FConstIterator It;
		const bool bValueToCheck;
	};

private:
	/** 
	 * A private constructor for a creating an instance straight from TBitArrays. 
	 * @Note that this constructor needs to remain private to ensure consistency of stored values with data tracked 
	 * by the TStructTrackerWrapper::StructTracker
	 */
	TStructTypeBitSet(const TBitArray<>& Source)
		: StructTypesBitArray(Source)
	{
	}

	TStructTypeBitSet(const int32 BitToSet)
	{
		StructTypesBitArray.AddAtIndex(BitToSet);
	}

	FORCEINLINE_DEBUGGABLE static const UStruct* GetBaseUStruct()
	{
		static const UStruct* Instance = UE::StructUtils::GetAsUStruct<TBaseStruct>();
		return Instance;
	}

public:

	static int32 GetTypeIndex(const TUStructType& InStructType)
	{
#if WITH_STRUCTUTILS_DEBUG
		ensureMsgf(InStructType.IsChildOf(GetBaseUStruct())
			, TEXT("Creating index for '%s' while it doesn't derive from the expected struct type %s")
			, *InStructType.GetPathName(), *GetBaseUStruct()->GetName());
#endif // WITH_STRUCTUTILS_DEBUG

		return TStructTrackerWrapper::StructTracker.FindOrAddStructTypeIndex(InStructType);
	}

	template<typename T>
	static int32 GetTypeIndex()
	{
		static_assert(TIsDerivedFrom<T, TBaseStruct>::IsDerived, "Given struct type doesn't match the expected base struct type.");
		static const int32 TypeIndex = GetTypeIndex(*UE::StructUtils::GetAsUStruct<T>());
		return TypeIndex;
	}
	
	template<typename T>
	static const TStructTypeBitSet& GetTypeBitSet()
	{
		static_assert(TIsDerivedFrom<T, TBaseStruct>::IsDerived, "Given struct type doesn't match the expected base struct type.");
		static const TStructTypeBitSet TypeBitSet(GetTypeIndex<T>());
		return TypeBitSet;
	}

	static const TUStructType* GetTypeAtIndex(const int32 Index)
	{
		return Cast<const TUStructType>(TStructTrackerWrapper::StructTracker.GetStructType(Index));
	}
	
	template<typename T>
	FORCEINLINE void Add()
	{
		static_assert(TIsDerivedFrom<T, TBaseStruct>::IsDerived, "Given struct type doesn't match the expected base struct type.");
		const int32 StructTypeIndex = GetTypeIndex<T>();
		StructTypesBitArray.AddAtIndex(StructTypeIndex);
	}

	template<typename T>
	FORCEINLINE void Remove()
	{
		static_assert(TIsDerivedFrom<T, TBaseStruct>::IsDerived, "Given struct type doesn't match the expected base struct type.");
		const int32 StructTypeIndex = GetTypeIndex<T>();
		StructTypesBitArray.RemoveAtIndex(StructTypeIndex);
	}

	FORCEINLINE void Remove(const TStructTypeBitSet& Other)
	{
		StructTypesBitArray -= Other.StructTypesBitArray;
	}

	template<typename T>
	FORCEINLINE bool Contains() const
	{
		static_assert(TIsDerivedFrom<T, TBaseStruct>::IsDerived, "Given struct type doesn't match the expected base struct type.");
		const int32 StructTypeIndex = GetTypeIndex<T>();
		return StructTypesBitArray.Contains(StructTypeIndex);
	}

	void SetAll(const bool bValue = true)
	{
		StructTypesBitArray.SetAll(bValue);
	}

	void Add(const TUStructType& InStructType)
	{
#if WITH_STRUCTUTILS_DEBUG
		ensureMsgf(InStructType.IsChildOf(GetBaseUStruct())
				, TEXT("Registering '%s' with FStructTracker while it doesn't derive from the expected struct type %s")
				, *InStructType.GetPathName(), *GetBaseUStruct()->GetName());
#endif // WITH_STRUCTUTILS_DEBUG

		const int32 StructTypeIndex = TStructTrackerWrapper::StructTracker.FindOrAddStructTypeIndex(InStructType);
		StructTypesBitArray.AddAtIndex(StructTypeIndex);
	}

	void Remove(const TUStructType& InStructType)
	{
#if WITH_STRUCTUTILS_DEBUG
		ensureMsgf(InStructType.IsChildOf(GetBaseUStruct())
				, TEXT("Registering '%s' with FStructTracker while it doesn't derive from the expected struct type %s")
				, *InStructType.GetPathName(), *GetBaseUStruct()->GetName());
#endif // WITH_STRUCTUTILS_DEBUG

		const int32 StructTypeIndex = TStructTrackerWrapper::StructTracker.FindOrAddStructTypeIndex(InStructType);
		StructTypesBitArray.RemoveAtIndex(StructTypeIndex);
	}

	void Reset() { StructTypesBitArray.Reset(); }

	bool Contains(const TUStructType& InStructType) const
	{
#if WITH_STRUCTUTILS_DEBUG
		ensureMsgf(InStructType.IsChildOf(GetBaseUStruct())
				, TEXT("Registering '%s' with FStructTracker while it doesn't derive from the expected struct type %s")
				, *InStructType.GetPathName(), *GetBaseUStruct()->GetName());
#endif // WITH_STRUCTUTILS_DEBUG

		const int32 StructTypeIndex = TStructTrackerWrapper::StructTracker.FindOrAddStructTypeIndex(InStructType);
		return StructTypesBitArray.Contains(StructTypeIndex);
	}

	FORCEINLINE TStructTypeBitSet operator+(const TStructTypeBitSet& Other) const
	{
		TStructTypeBitSet Result;
		Result.StructTypesBitArray = TBitArray<>::BitwiseOR(StructTypesBitArray, Other.StructTypesBitArray, EBitwiseOperatorFlags::MaxSize);
		return MoveTemp(Result);
	}

	FORCEINLINE void operator+=(const TStructTypeBitSet& Other)
	{
		StructTypesBitArray = TBitArray<>::BitwiseOR(StructTypesBitArray, Other.StructTypesBitArray, EBitwiseOperatorFlags::MaxSize);
	}

	FORCEINLINE void operator-=(const TStructTypeBitSet& Other)
	{
		StructTypesBitArray -= Other.StructTypesBitArray;
	}

	FORCEINLINE TStructTypeBitSet operator+(const TUStructType& NewElement) const
	{
		TStructTypeBitSet Result = *this;
		Result.Add(NewElement);
		return MoveTemp(Result);
	}

	FORCEINLINE TStructTypeBitSet operator-(const TUStructType& NewElement) const
	{
		TStructTypeBitSet Result = *this;
		Result.Remove(NewElement);
		return MoveTemp(Result);
	}

	FORCEINLINE TStructTypeBitSet operator-(const TStructTypeBitSet& Other) const
	{
		TStructTypeBitSet Result = *this;
		Result -= Other;
		return MoveTemp(Result);
	}

	FORCEINLINE TStructTypeBitSet operator&(const TStructTypeBitSet& Other) const
	{
		return TStructTypeBitSet(TBitArray<>::BitwiseAND(StructTypesBitArray, Other.StructTypesBitArray, EBitwiseOperatorFlags::MinSize));
	}

	FORCEINLINE TStructTypeBitSet operator|(const TStructTypeBitSet& Other) const
	{
		return TStructTypeBitSet(TBitArray<>::BitwiseOR(StructTypesBitArray, Other.StructTypesBitArray, EBitwiseOperatorFlags::MaxSize));
	}

	FORCEINLINE TStructTypeBitSet GetOverlap(const TStructTypeBitSet& Other) const
	{
		return *this & Other;
	}

	FORCEINLINE bool IsEquivalent(const TStructTypeBitSet& Other) const
	{
		return StructTypesBitArray.CompareSetBits(Other.StructTypesBitArray, /*bMissingBitValue=*/false);
	}

	FORCEINLINE bool HasAll(const TStructTypeBitSet& Other) const
	{
		return StructTypesBitArray.HasAll(Other.StructTypesBitArray);
	}

	FORCEINLINE bool HasAny(const TStructTypeBitSet& Other) const
	{
		return StructTypesBitArray.HasAny(Other.StructTypesBitArray);
	}

	FORCEINLINE bool HasNone(const TStructTypeBitSet& Other) const
	{
		return !StructTypesBitArray.HasAny(Other.StructTypesBitArray);
	}

	bool IsEmpty() const 
	{ 
		return StructTypesBitArray.IsEmpty();
	}

	FORCEINLINE bool IsBitSet(const int32 BitIndex) const
	{
		return StructTypesBitArray.Contains(BitIndex);
	}

	FIndexIterator GetIndexIterator(const bool bValueToCheck = true) const
	{
		return FIndexIterator(StructTypesBitArray, bValueToCheck);
	}

	int32 CountStoredTypes() const
	{
		return StructTypesBitArray.CountSetBits();
	}

	static int32 GetMaxNum() 
	{		
		return TStructTrackerWrapper::StructTracker.Num();
	}

	FORCEINLINE bool operator==(const TStructTypeBitSet& Other) const { return StructTypesBitArray == Other.StructTypesBitArray; }
	FORCEINLINE bool operator!=(const TStructTypeBitSet& Other) const { return !(*this == Other); }

	/**
	 * note that this function is slow(ish) due to the FStructTracker utilizing WeakObjectPtrs to store types. 
	 * @todo To be improved.
	 */
	template<typename TOutStructType, typename Allocator>
	void ExportTypes(TArray<const TOutStructType*, Allocator>& OutTypes) const
	{
		TBitArray<>::FConstIterator It(StructTypesBitArray);
		while (It)
		{
			if (It.GetValue())
			{
				OutTypes.Add(Cast<TOutStructType>(TStructTrackerWrapper::StructTracker.GetStructType(It.GetIndex())));
			}
			++It;
		}
	}

	SIZE_T GetAllocatedSize() const
	{
		return StructTypesBitArray.GetAllocatedSize();
	}

	FString DebugGetStringDesc() const
	{
#if WITH_STRUCTUTILS_DEBUG
		FStringOutputDevice Ar;
		DebugGetStringDesc(Ar);
		return static_cast<FString>(Ar);
#else
		return TEXT("DEBUG INFO COMPILED OUT");
#endif //WITH_STRUCTUTILS_DEBUG
	}

#if WITH_STRUCTUTILS_DEBUG
	void DebugGetStringDesc(FOutputDevice& Ar) const
	{
		for (int32 Index = 0; Index < StructTypesBitArray.Num(); ++Index)
		{
			if (StructTypesBitArray[Index])
			{
				Ar.Logf(TEXT("%s, "), *TStructTrackerWrapper::StructTracker.DebugGetStructTypeName(Index).ToString());
			}
		}
	}

	void DebugGetIndividualNames(TArray<FName>& OutFNames) const
	{
		for (int32 Index = 0; Index < StructTypesBitArray.Num(); ++Index)
		{
			if (StructTypesBitArray[Index])
			{
				OutFNames.Add(TStructTrackerWrapper::StructTracker.DebugGetStructTypeName(Index));
			}
		}
	}

	static FName DebugGetStructTypeName(const int32 StructTypeIndex)
	{
		return TStructTrackerWrapper::StructTracker.DebugGetStructTypeName(StructTypeIndex);
	}

	static TConstArrayView<TWeakObjectPtr<const TUStructType>> DebugGetAllStructTypes()
	{
		return TStructTrackerWrapper::StructTracker.template DebugGetAllStructTypes<TUStructType>();
	}

	/**
	 * Resets all the information gathered on the tags. Calling this results in invalidating all previously created
	 * FStructTypeCollection instances. Used only for debugging and unit/functional testing.
	 */
	static void DebugResetStructTypeMappingInfo()
	{
		TStructTrackerWrapper::StructTracker.DebugResetStructTypeMappingInfo();
	}

protected:
	// unittesting purposes only
	const TBitArray<>& DebugGetStructTypesBitArray() const { return StructTypesBitArray; }
	TBitArray<>& DebugGetMutableStructTypesBitArray() { return StructTypesBitArray; }
#endif // WITH_STRUCTUTILS_DEBUG

public:
	FORCEINLINE friend uint32 GetTypeHash(const TStructTypeBitSet& Instance)
	{
		const uint32 StoredTypeHash = PointerHash(GetBaseUStruct());
		const uint32 BitArrayHash = GetTypeHash(Instance.StructTypesBitArray);
		return HashCombine(StoredTypeHash, BitArrayHash);
	}

private:
	FBitArrayExt StructTypesBitArray;
};

/** 
 * We're declaring StructTracker this way rather than being a static member variable of TStructTypeBitSet to avoid linking
 * issues. We're run into all sorts of issues depending on compiler and whether strict mode was on, and this is the best
 * way we could come up with to solve it. Thankfully the user doesn't need to even know about this class's existence
 * as long as they are using the macros below.
 */
#define _DECLARE_TYPEBITSET_IMPL(EXPORTED_API, ContainerTypeName, BaseType, BaseUStructType) struct ContainerTypeName##StructTracker \
	{ \
		EXPORTED_API static FStructTracker StructTracker; \
	}; \
	template struct EXPORTED_API TStructTypeBitSet<BaseType, ContainerTypeName##StructTracker, BaseUStructType>; \
	using ContainerTypeName = TStructTypeBitSet<BaseType, ContainerTypeName##StructTracker, BaseUStructType>

#define DECLARE_STRUCTTYPEBITSET_EXPORTED(EXPORTED_API, ContainerTypeName, BaseStructType) _DECLARE_TYPEBITSET_IMPL(EXPORTED_API, ContainerTypeName, BaseStructType, UScriptStruct)
#define DECLARE_STRUCTTYPEBITSET(ContainerTypeName, BaseStructType) _DECLARE_TYPEBITSET_IMPL(, ContainerTypeName, BaseStructType, UScriptStruct)
#define DECLARE_CLASSTYPEBITSET_EXPORTED(EXPORTED_API, ContainerTypeName, BaseStructType) _DECLARE_TYPEBITSET_IMPL(EXPORTED_API, ContainerTypeName, BaseStructType, UClass)
#define DECLARE_CLASSTYPEBITSET(ContainerTypeName, BaseStructType) _DECLARE_TYPEBITSET_IMPL(, ContainerTypeName, BaseStructType, UClass)

#define DEFINE_TYPEBITSET(ContainerTypeName) \
	FStructTracker ContainerTypeName##StructTracker::StructTracker;
