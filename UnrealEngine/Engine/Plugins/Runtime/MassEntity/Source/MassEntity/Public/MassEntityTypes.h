// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StructTypeBitSet.h"
#include "MassProcessingTypes.h"
#include "StructArrayView.h"
#include "Subsystems/Subsystem.h"
#include "MassExternalSubsystemTraits.h"
#include "SharedStruct.h"
#include "MassEntityTypes.generated.h"


MASSENTITY_API DECLARE_LOG_CATEGORY_EXTERN(LogMass, Warning, All);

DECLARE_STATS_GROUP(TEXT("Mass"), STATGROUP_Mass, STATCAT_Advanced);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Mass Total Frame Time"), STAT_Mass_Total, STATGROUP_Mass, MASSENTITY_API);

// This is the base class for all lightweight fragments
USTRUCT()
struct FMassFragment
{
	GENERATED_BODY()

	FMassFragment() {}
};

// This is the base class for types that will only be tested for presence/absence, i.e. Tags.
// Subclasses should never contain any member properties.
USTRUCT()
struct FMassTag
{
	GENERATED_BODY()

	FMassTag() {}
};

USTRUCT()
struct FMassChunkFragment
{
	GENERATED_BODY()

	FMassChunkFragment() {}
};

USTRUCT()
struct FMassSharedFragment
{
	GENERATED_BODY()

	FMassSharedFragment() {}
};

// A handle to a lightweight entity.  An entity is used in conjunction with the FMassEntityManager
// for the current world and can contain lightweight fragments.
USTRUCT()
struct alignas(8) FMassEntityHandle
{
	GENERATED_BODY()

	FMassEntityHandle() = default;
	FMassEntityHandle(const int32 InIndex, const int32 InSerialNumber)
		: Index(InIndex), SerialNumber(InSerialNumber)
	{
	}
	
	UPROPERTY(VisibleAnywhere, Category = "Mass|Debug", Transient)
	int32 Index = 0;
	
	UPROPERTY(VisibleAnywhere, Category = "Mass|Debug", Transient)
	int32 SerialNumber = 0;

	bool operator==(const FMassEntityHandle Other) const
	{
		return Index == Other.Index && SerialNumber == Other.SerialNumber;
	}

	bool operator!=(const FMassEntityHandle Other) const
	{
		return !operator==(Other);
	}

	/** Has meaning only for sorting purposes */
	bool operator<(const FMassEntityHandle Other) const { return Index < Other.Index; }

	/** Note that this function is merely checking if Index and SerialNumber are set. There's no way to validate if 
	 *  these indicate a valid entity in an EntitySubsystem without asking the system. */
	bool IsSet() const
	{
		return Index != 0 && SerialNumber != 0;
	}

	FORCEINLINE bool IsValid() const
	{
		return IsSet();
	}

	void Reset()
	{
		Index = SerialNumber = 0;
	}

	/** Allows the entity handle to be shared anonymously. */
	uint64 AsNumber() const { return *reinterpret_cast<const uint64*>(this); } // Relying on the fact that this struct only stores 2 integers and is aligned correctly.
	/** Reconstruct the entity handle from an anonymously shared integer. */
	static FMassEntityHandle FromNumber(uint64 Value) 
	{ 
		FMassEntityHandle Result;
		*reinterpret_cast<uint64_t*>(&Result) = Value;
		return Result;
	}

	friend uint32 GetTypeHash(const FMassEntityHandle Entity)
	{
		return HashCombine(Entity.Index, Entity.SerialNumber);
	}

	FString DebugGetDescription() const
	{
		return FString::Printf(TEXT("i: %d sn: %d"), Index, SerialNumber);
	}
};

static_assert(sizeof(FMassEntityHandle) == sizeof(uint64), "Expected FMassEntityHandle to be convertable to a 64-bit integer value, so size needs to be 8 bytes.");
static_assert(alignof(FMassEntityHandle) == sizeof(uint64), "Expected FMassEntityHandle to be convertable to a 64-bit integer value, so alignment needs to be 8 bytes.");

DECLARE_STRUCTTYPEBITSET_EXPORTED(MASSENTITY_API, FMassFragmentBitSet, FMassFragment);
DECLARE_STRUCTTYPEBITSET_EXPORTED(MASSENTITY_API, FMassTagBitSet, FMassTag);
DECLARE_STRUCTTYPEBITSET_EXPORTED(MASSENTITY_API, FMassChunkFragmentBitSet, FMassChunkFragment);
DECLARE_STRUCTTYPEBITSET_EXPORTED(MASSENTITY_API, FMassSharedFragmentBitSet, FMassSharedFragment);
DECLARE_CLASSTYPEBITSET_EXPORTED(MASSENTITY_API, FMassExternalSubsystemBitSet, USubsystem);

/** The type summarily describing a composition of an entity or an archetype. It contains information on both the
 *  fragments as well as tags */
struct FMassArchetypeCompositionDescriptor
{
	FMassArchetypeCompositionDescriptor() = default;
	FMassArchetypeCompositionDescriptor(const FMassFragmentBitSet& InFragments, const FMassTagBitSet& InTags, const FMassChunkFragmentBitSet& InChunkFragments, const FMassSharedFragmentBitSet& InSharedFragments)
		: Fragments(InFragments)
		, Tags(InTags)
		, ChunkFragments(InChunkFragments)
		, SharedFragments(InSharedFragments)
	{}

	FMassArchetypeCompositionDescriptor(TConstArrayView<const UScriptStruct*> InFragments, const FMassTagBitSet& InTags, const FMassChunkFragmentBitSet& InChunkFragments, const FMassSharedFragmentBitSet& InSharedFragments)
		: FMassArchetypeCompositionDescriptor(FMassFragmentBitSet(InFragments), InTags, InChunkFragments, InSharedFragments)
	{}

	FMassArchetypeCompositionDescriptor(TConstArrayView<FInstancedStruct> InFragmentInstances, const FMassTagBitSet& InTags, const FMassChunkFragmentBitSet& InChunkFragments, const FMassSharedFragmentBitSet& InSharedFragments)
		: FMassArchetypeCompositionDescriptor(FMassFragmentBitSet(InFragmentInstances), InTags, InChunkFragments, InSharedFragments)
	{}

	FMassArchetypeCompositionDescriptor(FMassFragmentBitSet&& InFragments, FMassTagBitSet&& InTags, FMassChunkFragmentBitSet&& InChunkFragments, FMassSharedFragmentBitSet&& InSharedFragments)
		: Fragments(MoveTemp(InFragments))
		, Tags(MoveTemp(InTags))
		, ChunkFragments(MoveTemp(InChunkFragments))
		, SharedFragments(MoveTemp(InSharedFragments))
	{}

	FMassArchetypeCompositionDescriptor(FMassFragmentBitSet&& InFragments)
		: Fragments(MoveTemp(InFragments))
	{}

	FMassArchetypeCompositionDescriptor(FMassTagBitSet&& InTags)
		: Tags(MoveTemp(InTags))
	{}

	void Reset()
	{
		Fragments.Reset();
		Tags.Reset();
		ChunkFragments.Reset();
		SharedFragments.Reset();
	}

	bool IsEquivalent(const FMassArchetypeCompositionDescriptor& OtherDescriptor) const
	{
		return Fragments.IsEquivalent(OtherDescriptor.Fragments) &&
			Tags.IsEquivalent(OtherDescriptor.Tags) &&
			ChunkFragments.IsEquivalent(OtherDescriptor.ChunkFragments) &&
			SharedFragments.IsEquivalent(OtherDescriptor.SharedFragments);
	}

	bool IsEmpty() const 
	{ 
		return Fragments.IsEmpty() &&
			Tags.IsEmpty() &&
			ChunkFragments.IsEmpty() &&
			SharedFragments.IsEmpty();
	}

	bool HasAll(const FMassArchetypeCompositionDescriptor& OtherDescriptor) const
	{
		return Fragments.HasAll(OtherDescriptor.Fragments) &&
			Tags.HasAll(OtherDescriptor.Tags) &&
			ChunkFragments.HasAll(OtherDescriptor.ChunkFragments) &&
			SharedFragments.HasAll(OtherDescriptor.SharedFragments);
	}

	static uint32 CalculateHash(const FMassFragmentBitSet& InFragments, const FMassTagBitSet& InTags, const FMassChunkFragmentBitSet& InChunkFragments, const FMassSharedFragmentBitSet& InSharedFragmentBitSet)
	{
		const uint32 FragmentsHash = GetTypeHash(InFragments);
		const uint32 TagsHash = GetTypeHash(InTags);
		const uint32 ChunkFragmentsHash = GetTypeHash(InChunkFragments);
		const uint32 SharedFragmentsHash = GetTypeHash(InSharedFragmentBitSet);
		return HashCombine(HashCombine(HashCombine(FragmentsHash, TagsHash), ChunkFragmentsHash), SharedFragmentsHash);
	}	

	uint32 CalculateHash() const 
	{
		return CalculateHash(Fragments, Tags, ChunkFragments, SharedFragments);
	}

	int32 CountStoredTypes() const
	{
		return Fragments.CountStoredTypes() + Tags.CountStoredTypes() + ChunkFragments.CountStoredTypes() + SharedFragments.CountStoredTypes();
	}

	void DebugOutputDescription(FOutputDevice& Ar) const
	{
#if WITH_MASSENTITY_DEBUG
		const bool bAutoLineEnd = Ar.GetAutoEmitLineTerminator();
		Ar.SetAutoEmitLineTerminator(false);

		Ar.Logf(TEXT("Fragments:\n"));
		Fragments.DebugGetStringDesc(Ar);
		Ar.Logf(TEXT("Tags:\n"));
		Tags.DebugGetStringDesc(Ar);
		Ar.Logf(TEXT("ChunkFragments:\n"));
		ChunkFragments.DebugGetStringDesc(Ar);

		Ar.SetAutoEmitLineTerminator(bAutoLineEnd);
#endif // WITH_MASSENTITY_DEBUG

	}

	FMassFragmentBitSet Fragments;
	FMassTagBitSet Tags;
	FMassChunkFragmentBitSet ChunkFragments;
	FMassSharedFragmentBitSet SharedFragments;
};

struct MASSENTITY_API FMassArchetypeSharedFragmentValues
{
	static constexpr uint32 EmptyInstanceHash = 0;

	FMassArchetypeSharedFragmentValues() = default;
	FMassArchetypeSharedFragmentValues(const FMassArchetypeSharedFragmentValues& OtherFragmentValues) = default;
	FMassArchetypeSharedFragmentValues(FMassArchetypeSharedFragmentValues&& OtherFragmentValues) = default;
	FMassArchetypeSharedFragmentValues& operator=(const FMassArchetypeSharedFragmentValues& OtherFragmentValues) = default;
	FMassArchetypeSharedFragmentValues& operator=(FMassArchetypeSharedFragmentValues&& OtherFragmentValues) = default;

	FORCEINLINE bool HasExactFragmentTypesMatch(const FMassSharedFragmentBitSet& InSharedFragmentBitSet) const
	{
		return SharedFragmentBitSet == InSharedFragmentBitSet;
	}

	FORCEINLINE bool HasAllRequiredFragmentTypes(const FMassSharedFragmentBitSet& InSharedFragmentBitSet) const
	{
		return SharedFragmentBitSet.HasAll(InSharedFragmentBitSet);
	}

	FORCEINLINE bool IsEquivalent(const FMassArchetypeSharedFragmentValues& OtherSharedFragmentValues) const
	{
		return GetTypeHash(*this) == GetTypeHash(OtherSharedFragmentValues);
	}

	FORCEINLINE FConstSharedStruct& AddConstSharedFragment(const FConstSharedStruct& Fragment)
	{
		DirtyHashCache();
		check(Fragment.GetScriptStruct());
		SharedFragmentBitSet.Add(*Fragment.GetScriptStruct());
		return ConstSharedFragments.Add_GetRef(Fragment);
	}

	FORCEINLINE FSharedStruct AddSharedFragment(const FSharedStruct& Fragment)
	{
		DirtyHashCache();
		check(Fragment.GetScriptStruct());
		SharedFragmentBitSet.Add(*Fragment.GetScriptStruct());
		return SharedFragments.Add_GetRef(Fragment);
	}

	FORCEINLINE const TArray<FConstSharedStruct>& GetConstSharedFragments() const
	{
		return ConstSharedFragments;
	}

	FORCEINLINE TArray<FSharedStruct>& GetMutableSharedFragments()
	{
		return SharedFragments;
	}
	
	FORCEINLINE const TArray<FSharedStruct>& GetSharedFragments() const
	{
		return SharedFragments;
	}

	FORCEINLINE void DirtyHashCache()
	{
		HashCache = UINT32_MAX;
		bSorted = false;
	}

	FORCEINLINE void CacheHash() const
	{
		if (HashCache == UINT32_MAX)
		{
			HashCache = CalculateHash();
		}
	}

	friend FORCEINLINE uint32 GetTypeHash(const FMassArchetypeSharedFragmentValues& SharedFragmentValues)
	{
		SharedFragmentValues.CacheHash();
		return SharedFragmentValues.HashCache;
	}

	uint32 CalculateHash() const;

	SIZE_T GetAllocatedSize() const
	{
		return ConstSharedFragments.GetAllocatedSize() + SharedFragments.GetAllocatedSize();
	}

	void Sort()
	{
		if(!bSorted)
		{
			ConstSharedFragments.Sort(FStructTypeSortOperator());
			SharedFragments.Sort(FStructTypeSortOperator());
			bSorted = true;
		}
	}

	bool IsSorted() const { return bSorted; }

protected:
	mutable uint32 HashCache = UINT32_MAX;
	mutable bool bSorted = true; // When no element in the array, consider already sorted
	
	FMassSharedFragmentBitSet SharedFragmentBitSet;
	TArray<FConstSharedStruct> ConstSharedFragments;
	TArray<FSharedStruct> SharedFragments;
};

UENUM()
enum class EMassObservedOperation : uint8
{
	Add,
	Remove,
	// @todo Keeping this here as a indication of design intent. For now we handle entity destruction like removal, but 
	// there might be computationally expensive cases where we might want to avoid for soon-to-be-dead entities. 
	// Destroy,
	// @todo another planned supported operation type
	// Touch,
	MAX
};

enum class EMassExecutionContextType : uint8
{
	Local,
	Processor,
	MAX
};

/** 
 * Note that this is a view and is valid only as long as the source data is valid. Used when flushing mass commands to
 * wrap different kinds of data into a uniform package so that it can be passed over to a common interface.
 */
struct FMassGenericPayloadView
{
	FMassGenericPayloadView() = default;
	FMassGenericPayloadView(TArray<FStructArrayView>&SourceData)
		: Content(SourceData)
	{}
	FMassGenericPayloadView(TArrayView<FStructArrayView> SourceData)
		: Content(SourceData)
	{}

	int32 Num() const { return Content.Num(); }

	void Reset()
	{
		Content = TArrayView<FStructArrayView>();
	}

	FORCEINLINE void Swap(const int32 A, const int32 B)
	{
		for (FStructArrayView& View : Content)
		{
			View.Swap(A, B);
		}
	}

	/** Moves NumToMove elements to the back of the viewed collection. */
	void SwapElementsToEnd(int32 StartIndex, int32 NumToMove);

	TArrayView<FStructArrayView> Content;
};

/**
 * Used to indicate a specific slice of a preexisting FMassGenericPayloadView, it's essentially an access pattern
 * Note: accessing content generates copies of FStructArrayViews stored (still cheap, those are just views). 
 */
struct FMassGenericPayloadViewSlice
{
	FMassGenericPayloadViewSlice() = default;
	FMassGenericPayloadViewSlice(const FMassGenericPayloadView& InSource, const int32 InStartIndex, const int32 InCount)
		: Source(InSource), StartIndex(InStartIndex), Count(InCount)
	{
	}

	FStructArrayView operator[](const int32 Index) const
	{
		return Source.Content[Index].Slice(StartIndex, Count);
	}

	/** @return the number of "layers" (i.e. number of original arrays) this payload has been built from */
	int32 Num() const 
	{
		return Source.Num();
	}

	bool IsEmpty() const
	{
		return !(Source.Num() > 0 && Count > 0);
	}

private:
	FMassGenericPayloadView Source;
	const int32 StartIndex = 0;
	const int32 Count = 0;
};

namespace UE::Mass
{
	/**
	 * A statically-typed list of of related types. Used mainly to differentiate type collections at compile-type as well as
	 * efficiently produce TStructTypeBitSet representing given collection.
	 */
	template<typename T, typename... TOthers>
	struct TMultiTypeList : TMultiTypeList<TOthers...>
	{
		using Super = TMultiTypeList<TOthers...>;
		using FType = std::remove_const_t<typename TRemoveReference<T>::Type>;
		enum
		{
			Ordinal = Super::Ordinal + 1
		};

		template<typename TBitSetType>
		constexpr static void PopulateBitSet(TBitSetType& OutBitSet)
		{
			Super::PopulateBitSet(OutBitSet);
			OutBitSet += TBitSetType::template GetTypeBitSet<FType>();
		}
	};
		
	/** Single-type specialization of TMultiTypeList. */
	template<typename T>
	struct TMultiTypeList<T>
	{
		using FType = std::remove_const_t<typename TRemoveReference<T>::Type>;
		enum
		{
			Ordinal = 0
		};

		template<typename TBitSetType>
		constexpr static void PopulateBitSet(TBitSetType& OutBitSet)
		{
			OutBitSet += TBitSetType::template GetTypeBitSet<FType>();
		}
	};

	/** 
	 * The type hosts a statically-typed collection of TArrays, where each TArray is strongly-typed (i.e. it contains 
	 * instances of given structs rather than structs wrapped up in FInstancedStruct). This type lets us do batched 
	 * fragment values setting by simply copying data rather than setting per-instance. 
	 */
	template<typename T, typename... TOthers>
	struct TMultiArray : TMultiArray<TOthers...>
	{
		using FType = std::remove_const_t<typename TRemoveReference<T>::Type>;
		using Super = TMultiArray<TOthers...>;

		enum
		{
			Ordinal = Super::Ordinal + 1
		};

		SIZE_T GetAllocatedSize() const
		{
			return FragmentInstances.GetAllocatedSize() + Super::GetAllocatedSize();
		}

		int GetNumArrays() const { return Ordinal + 1; }

		void Add(const FType& Item, TOthers... Rest)
		{
			FragmentInstances.Add(Item);
			Super::Add(Rest...);
		}

		void GetAsGenericMultiArray(TArray<FStructArrayView>& A) /*const*/
		{
			Super::GetAsGenericMultiArray(A);
			A.Add(FStructArrayView(FragmentInstances));
		}

		void GetheredAffectedFragments(FMassFragmentBitSet& OutBitSet) const
		{
			Super::GetheredAffectedFragments(OutBitSet);
			OutBitSet += FMassFragmentBitSet::GetTypeBitSet<FType>();
		}

		void Reset()
		{
			Super::Reset();
			FragmentInstances.Reset();
		}

		TArray<FType> FragmentInstances;
	};

	/**TMultiArray simple-type specialization */
	template<typename T>
	struct TMultiArray<T>
	{
		using FType = std::remove_const_t<typename TRemoveReference<T>::Type>;
		enum { Ordinal = 0 };

		SIZE_T GetAllocatedSize() const
		{
			return FragmentInstances.GetAllocatedSize();
		}

		int GetNumArrays() const { return Ordinal + 1; }

		void Add(const FType& Item) { FragmentInstances.Add(Item); }

		void GetAsGenericMultiArray(TArray<FStructArrayView>& A) /*const*/
		{
			A.Add(FStructArrayView(FragmentInstances));
		}

		void GetheredAffectedFragments(FMassFragmentBitSet& OutBitSet) const
		{
			OutBitSet += FMassFragmentBitSet::GetTypeBitSet<FType>();
		}

		void Reset()
		{
			FragmentInstances.Reset();
		}

		TArray<FType> FragmentInstances;
	};

} // UE::Mass


struct FMassArchetypeCreationParams
{
	/** Created archetype will have chunks of this size. 0 denotes "use default" (see UE::Mass::ChunkSize) */
	int32 ChunkMemorySize = 0;

	/** Name to identify the archetype while debugging*/
	FName DebugName;
};
