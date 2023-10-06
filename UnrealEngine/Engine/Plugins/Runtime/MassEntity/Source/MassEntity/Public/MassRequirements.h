// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"
#include "MassRequirements.generated.h"

struct FMassDebugger;
struct FMassArchetypeHandle;
struct FMassExecutionRequirements;
struct FMassRequirementAccessDetector;

UENUM()
enum class EMassFragmentAccess : uint8
{
	/** no binding required */
	None, 

	/** We want to read the data for the fragment */
	ReadOnly,

	/** We want to read and write the data for the fragment */
	ReadWrite,

	MAX
};

UENUM()
enum class EMassFragmentPresence : uint8
{
	/** All of the required fragments must be present */
	All,

	/** One of the required fragments must be present */
	Any,

	/** None of the required fragments can be present */
	None,

	/** If fragment is present we'll use it, but it missing stop processing of a given archetype */
	Optional,

	MAX
};


struct MASSENTITY_API FMassFragmentRequirementDescription
{
	const UScriptStruct* StructType = nullptr;
	EMassFragmentAccess AccessMode = EMassFragmentAccess::None;
	EMassFragmentPresence Presence = EMassFragmentPresence::Optional;

public:
	FMassFragmentRequirementDescription(){}
	FMassFragmentRequirementDescription(const UScriptStruct* InStruct, const EMassFragmentAccess InAccessMode, const EMassFragmentPresence InPresence)
		: StructType(InStruct)
		, AccessMode(InAccessMode)
		, Presence(InPresence)
	{
		check(InStruct);
		checkf((Presence != EMassFragmentPresence::Any && Presence != EMassFragmentPresence::Optional)
			|| AccessMode == EMassFragmentAccess::ReadOnly || AccessMode == EMassFragmentAccess::ReadWrite, TEXT("Only ReadOnly and ReadWrite modes are suppored for optional requirements"));
	}

	bool RequiresBinding() const { return (AccessMode != EMassFragmentAccess::None); }
	bool IsOptional() const { return (Presence == EMassFragmentPresence::Optional || Presence == EMassFragmentPresence::Any); }

	/** these functions are used for sorting. See FScriptStructSortOperator */
	int32 GetStructureSize() const
	{
		return StructType->GetStructureSize();
	}

	FName GetFName() const
	{
		return StructType->GetFName();
	}
};

/**
 *  FMassSubsystemRequirements is a structure that declares runtime subsystem access type given calculations require.
 */
USTRUCT()
struct MASSENTITY_API FMassSubsystemRequirements
{
	GENERATED_BODY()

	friend FMassDebugger;
	friend FMassRequirementAccessDetector;

public:
	template<typename T>
	FMassSubsystemRequirements& AddSubsystemRequirement(const EMassFragmentAccess AccessMode)
	{
		check(AccessMode != EMassFragmentAccess::None && AccessMode != EMassFragmentAccess::MAX);

		switch (AccessMode)
		{
		case EMassFragmentAccess::ReadOnly:
			RequiredConstSubsystems.Add<T>();
			bRequiresGameThreadExecution |= TMassExternalSubsystemTraits<T>::GameThreadOnly;
			break;
		case EMassFragmentAccess::ReadWrite:
			RequiredMutableSubsystems.Add<T>();
			bRequiresGameThreadExecution |= TMassExternalSubsystemTraits<T>::GameThreadOnly;
			break;
		default:
			check(false);
		}

		return *this;
	}

	FMassSubsystemRequirements& AddSubsystemRequirement(const TSubclassOf<USubsystem> SubsystemClass, const EMassFragmentAccess AccessMode, const bool bGameThreadOnly = true)
	{
		check(AccessMode != EMassFragmentAccess::None && AccessMode != EMassFragmentAccess::MAX);

		switch (AccessMode)
		{
		case EMassFragmentAccess::ReadOnly:
			RequiredConstSubsystems.Add(**SubsystemClass);
			bRequiresGameThreadExecution |= bGameThreadOnly;
			break;
		case EMassFragmentAccess::ReadWrite:
			RequiredMutableSubsystems.Add(**SubsystemClass);
			bRequiresGameThreadExecution |= bGameThreadOnly;
			break;
		default:
			check(false);
		}

		return *this;
	}

	void Reset();

	const FMassExternalSubsystemBitSet& GetRequiredConstSubsystems() const { return RequiredConstSubsystems; }
	const FMassExternalSubsystemBitSet& GetRequiredMutableSubsystems() const { return RequiredMutableSubsystems; }
	bool IsEmpty() const { return RequiredConstSubsystems.IsEmpty() && RequiredMutableSubsystems.IsEmpty(); }

	bool DoesRequireGameThreadExecution() const { return bRequiresGameThreadExecution; }
	void ExportRequirements(FMassExecutionRequirements& OutRequirements) const;

protected:
	FMassExternalSubsystemBitSet RequiredConstSubsystems;
	FMassExternalSubsystemBitSet RequiredMutableSubsystems;

private:
	bool bRequiresGameThreadExecution = false;
};

/** 
 *  FMassFragmentRequirements is a structure that describes properties required of an archetype that's a subject of calculations.
 */
USTRUCT()
struct MASSENTITY_API FMassFragmentRequirements
{
	GENERATED_BODY()

	friend FMassDebugger;
	friend FMassRequirementAccessDetector;

public:
	FMassFragmentRequirements(){}
	FMassFragmentRequirements(std::initializer_list<UScriptStruct*> InitList);
	FMassFragmentRequirements(TConstArrayView<const UScriptStruct*> InitList);

	FMassFragmentRequirements& AddRequirement(const UScriptStruct* FragmentType, const EMassFragmentAccess AccessMode, const EMassFragmentPresence Presence = EMassFragmentPresence::All)
	{
		checkf(FragmentRequirements.FindByPredicate([FragmentType](const FMassFragmentRequirementDescription& Item){ return Item.StructType == FragmentType; }) == nullptr
			, TEXT("Duplicated requirements are not supported. %s already present"), *GetNameSafe(FragmentType));
		
		if (Presence != EMassFragmentPresence::None)
		{
			FragmentRequirements.Emplace(FragmentType, AccessMode, Presence);
		}

		switch (Presence)
		{
		case EMassFragmentPresence::All:
			RequiredAllFragments.Add(*FragmentType);
			break;
		case EMassFragmentPresence::Any:
			RequiredAnyFragments.Add(*FragmentType);
			break;
		case EMassFragmentPresence::Optional:
			RequiredOptionalFragments.Add(*FragmentType);
			break;
		case EMassFragmentPresence::None:
			RequiredNoneFragments.Add(*FragmentType);
			break;
		}
		// force recaching the next time this query is used or the following CacheArchetypes call.
		IncrementChangeCounter();
		return *this;
	}

	/** FMassFragmentRequirements ref returned for chaining */
	template<typename T>
	FMassFragmentRequirements& AddRequirement(const EMassFragmentAccess AccessMode, const EMassFragmentPresence Presence = EMassFragmentPresence::All)
	{
		checkf(FragmentRequirements.FindByPredicate([](const FMassFragmentRequirementDescription& Item) { return Item.StructType == T::StaticStruct(); }) == nullptr
			, TEXT("Duplicated requirements are not supported. %s already present"), *T::StaticStruct()->GetName());

		static_assert(TIsDerivedFrom<T, FMassFragment>::IsDerived, "Given struct doesn't represent a valid fragment type. Make sure to inherit from FMassFragment or one of its child-types.");
		
		if (Presence != EMassFragmentPresence::None)
		{
			FragmentRequirements.Emplace(T::StaticStruct(), AccessMode, Presence);
		}
		
		switch (Presence)
		{
		case EMassFragmentPresence::All:
			RequiredAllFragments.Add<T>();
			break;
		case EMassFragmentPresence::Any:
			RequiredAnyFragments.Add<T>();
			break;
		case EMassFragmentPresence::Optional:
			RequiredOptionalFragments.Add<T>();
			break;
		case EMassFragmentPresence::None:
			RequiredNoneFragments.Add<T>();
			break;
		}
		// force recaching the next time this query is used or the following CacheArchetypes call.
		IncrementChangeCounter();
		return *this;
	}

	void AddTagRequirement(const UScriptStruct& TagType, const EMassFragmentPresence Presence)
	{
		checkfSlow(int(Presence) < int(EMassFragmentPresence::Optional), TEXT("Optional and MAX presence are not valid calues for AddTagRequirement"));
		switch (Presence)
		{
		case EMassFragmentPresence::All:
			RequiredAllTags.Add(TagType);
			break;
		case EMassFragmentPresence::Any:
			RequiredAnyTags.Add(TagType);
			break;
		case EMassFragmentPresence::None:
			RequiredNoneTags.Add(TagType);
			break;
		}
		IncrementChangeCounter();
	}

	template<typename T>
	FMassFragmentRequirements& AddTagRequirement(const EMassFragmentPresence Presence)
	{
		checkfSlow(int(Presence) < int(EMassFragmentPresence::Optional), TEXT("Optional and MAX presence are not valid calues for AddTagRequirement"));
		static_assert(TIsDerivedFrom<T, FMassTag>::IsDerived, "Given struct doesn't represent a valid tag type. Make sure to inherit from FMassFragment or one of its child-types.");
		switch (Presence)
		{
			case EMassFragmentPresence::All:
				RequiredAllTags.Add<T>();
				break;
			case EMassFragmentPresence::Any:
				RequiredAnyTags.Add<T>();
				break;
			case EMassFragmentPresence::None:
				RequiredNoneTags.Add<T>();
				break;
		}
		IncrementChangeCounter();
		return *this;
	}

	/** actual implementation in specializations */
	template<EMassFragmentPresence Presence> 
	FMassFragmentRequirements& AddTagRequirements(const FMassTagBitSet& TagBitSet)
	{
		static_assert(Presence == EMassFragmentPresence::None || Presence == EMassFragmentPresence::All || Presence == EMassFragmentPresence::Any
			, "The only valid values for AddTagRequirements are All, Any and None");
		return *this;
	}

	template<typename T>
	FMassFragmentRequirements& AddChunkRequirement(const EMassFragmentAccess AccessMode, const EMassFragmentPresence Presence = EMassFragmentPresence::All)
	{
		static_assert(TIsDerivedFrom<T, FMassChunkFragment>::IsDerived, "Given struct doesn't represent a valid chunk fragment type. Make sure to inherit from FMassChunkFragment or one of its child-types.");
		checkf(ChunkFragmentRequirements.FindByPredicate([](const FMassFragmentRequirementDescription& Item) { return Item.StructType == T::StaticStruct(); }) == nullptr
			, TEXT("Duplicated requirements are not supported. %s already present"), *T::StaticStruct()->GetName());
		checkfSlow(Presence != EMassFragmentPresence::Any, TEXT("\'Any\' is not a valid Presence value for AddChunkRequirement."));

		switch (Presence)
		{
			case EMassFragmentPresence::All:
				RequiredAllChunkFragments.Add<T>();
				ChunkFragmentRequirements.Emplace(T::StaticStruct(), AccessMode, Presence);
				break;
			case EMassFragmentPresence::Optional:
				RequiredOptionalChunkFragments.Add<T>();
				ChunkFragmentRequirements.Emplace(T::StaticStruct(), AccessMode, Presence);
				break;
			case EMassFragmentPresence::None:
				RequiredNoneChunkFragments.Add<T>();
				break;
		}
		IncrementChangeCounter();
		return *this;
	}

	template<typename T>
	FMassFragmentRequirements& AddConstSharedRequirement(const EMassFragmentPresence Presence = EMassFragmentPresence::All)
	{
		static_assert(TIsDerivedFrom<T, FMassSharedFragment>::IsDerived, "Given struct doesn't represent a valid shared fragment type. Make sure to inherit from FMassSharedFragment or one of its child-types.");
		checkf(ConstSharedFragmentRequirements.FindByPredicate([](const FMassFragmentRequirementDescription& Item) { return Item.StructType == T::StaticStruct(); }) == nullptr
			, TEXT("Duplicated requirements are not supported. %s already present"), *T::StaticStruct()->GetName());
		checkfSlow(Presence != EMassFragmentPresence::Any, TEXT("\'Any\' is not a valid Presence value for AddSharedRequirement."));

		switch (Presence)
		{
		case EMassFragmentPresence::All:
			RequiredAllSharedFragments.Add<T>();
			ConstSharedFragmentRequirements.Emplace(T::StaticStruct(), EMassFragmentAccess::ReadOnly, Presence);
			break;
		case EMassFragmentPresence::Optional:
			RequiredOptionalSharedFragments.Add<T>();
			ConstSharedFragmentRequirements.Emplace(T::StaticStruct(), EMassFragmentAccess::ReadOnly, Presence);
			break;
		case EMassFragmentPresence::None:
			RequiredNoneSharedFragments.Add<T>();
			break;
		}
		IncrementChangeCounter();
		return *this;
	}

	template<typename T>
	FMassFragmentRequirements& AddSharedRequirement(const EMassFragmentAccess AccessMode, const EMassFragmentPresence Presence = EMassFragmentPresence::All)
	{
		static_assert(TIsDerivedFrom<T, FMassSharedFragment>::IsDerived, "Given struct doesn't represent a valid shared fragment type. Make sure to inherit from FMassSharedFragment or one of its child-types.");
		checkf(SharedFragmentRequirements.FindByPredicate([](const FMassFragmentRequirementDescription& Item) { return Item.StructType == T::StaticStruct(); }) == nullptr
			, TEXT("Duplicated requirements are not supported. %s already present"), *T::StaticStruct()->GetName());
		checkfSlow(Presence != EMassFragmentPresence::Any, TEXT("\'Any\' is not a valid Presence value for AddSharedRequirement."));

		switch (Presence)
		{
		case EMassFragmentPresence::All:
			RequiredAllSharedFragments.Add<T>();
			SharedFragmentRequirements.Emplace(T::StaticStruct(), AccessMode, Presence);
			if (AccessMode == EMassFragmentAccess::ReadWrite)
			{
				bRequiresGameThreadExecution |= TMassSharedFragmentTraits<T>::GameThreadOnly;
			}
			break;
		case EMassFragmentPresence::Optional:
			RequiredOptionalSharedFragments.Add<T>();
			SharedFragmentRequirements.Emplace(T::StaticStruct(), AccessMode, Presence);
			if (AccessMode == EMassFragmentAccess::ReadWrite)
			{
				bRequiresGameThreadExecution |= TMassSharedFragmentTraits<T>::GameThreadOnly;
			}
			break;
		case EMassFragmentPresence::None:
			RequiredNoneSharedFragments.Add<T>();
			break;
		}
		IncrementChangeCounter();
		return *this;
	}

	void Reset();

	/** The function validates requirements we make for queries. See the FMassFragmentRequirements struct description for details.
	 *  Note that this function is non-trivial and end users are not expected to need to use it. 
	 *  @return whether this query's requirements follow the rules. */
	bool CheckValidity() const;

	TConstArrayView<FMassFragmentRequirementDescription> GetFragmentRequirements() const { return FragmentRequirements; }
	TConstArrayView<FMassFragmentRequirementDescription> GetChunkFragmentRequirements() const { return ChunkFragmentRequirements; }
	TConstArrayView<FMassFragmentRequirementDescription> GetConstSharedFragmentRequirements() const { return ConstSharedFragmentRequirements; }
	TConstArrayView<FMassFragmentRequirementDescription> GetSharedFragmentRequirements() const { return SharedFragmentRequirements; }
	const FMassFragmentBitSet& GetRequiredAllFragments() const { return RequiredAllFragments; }
	const FMassFragmentBitSet& GetRequiredAnyFragments() const { return RequiredAnyFragments; }
	const FMassFragmentBitSet& GetRequiredOptionalFragments() const { return RequiredOptionalFragments; }
	const FMassFragmentBitSet& GetRequiredNoneFragments() const { return RequiredNoneFragments; }
	const FMassTagBitSet& GetRequiredAllTags() const { return RequiredAllTags; }
	const FMassTagBitSet& GetRequiredAnyTags() const { return RequiredAnyTags; }
	const FMassTagBitSet& GetRequiredNoneTags() const { return RequiredNoneTags; }
	const FMassChunkFragmentBitSet& GetRequiredAllChunkFragments() const { return RequiredAllChunkFragments; }
	const FMassChunkFragmentBitSet& GetRequiredOptionalChunkFragments() const { return RequiredOptionalChunkFragments; }
	const FMassChunkFragmentBitSet& GetRequiredNoneChunkFragments() const { return RequiredNoneChunkFragments; }
	const FMassSharedFragmentBitSet& GetRequiredAllSharedFragments() const { return RequiredAllSharedFragments; }
	const FMassSharedFragmentBitSet& GetRequiredOptionalSharedFragments() const { return RequiredOptionalSharedFragments; }
	const FMassSharedFragmentBitSet& GetRequiredNoneSharedFragments() const { return RequiredNoneSharedFragments; }

	bool IsEmpty() const;

	bool DoesArchetypeMatchRequirements(const FMassArchetypeHandle& ArchetypeHandle) const;
	bool DoesRequireGameThreadExecution() const { return bRequiresGameThreadExecution; }
	void ExportRequirements(FMassExecutionRequirements& OutRequirements) const;

protected:
	void SortRequirements();

	FORCEINLINE void IncrementChangeCounter() { ++IncrementalChangesCount; }

protected:
	friend FMassRequirementAccessDetector;

	TArray<FMassFragmentRequirementDescription> FragmentRequirements;
	TArray<FMassFragmentRequirementDescription> ChunkFragmentRequirements;
	TArray<FMassFragmentRequirementDescription> ConstSharedFragmentRequirements;
	TArray<FMassFragmentRequirementDescription> SharedFragmentRequirements;
	FMassTagBitSet RequiredAllTags;
	FMassTagBitSet RequiredAnyTags;
	FMassTagBitSet RequiredNoneTags;
	FMassFragmentBitSet RequiredAllFragments;
	FMassFragmentBitSet RequiredAnyFragments;
	FMassFragmentBitSet RequiredOptionalFragments;
	FMassFragmentBitSet RequiredNoneFragments;
	FMassChunkFragmentBitSet RequiredAllChunkFragments;
	FMassChunkFragmentBitSet RequiredOptionalChunkFragments;
	FMassChunkFragmentBitSet RequiredNoneChunkFragments;
	FMassSharedFragmentBitSet RequiredAllSharedFragments;
	FMassSharedFragmentBitSet RequiredOptionalSharedFragments;
	FMassSharedFragmentBitSet RequiredNoneSharedFragments;

	uint32 IncrementalChangesCount = 0;

private:
	bool bRequiresGameThreadExecution = false;
};

template<>
FORCEINLINE FMassFragmentRequirements& FMassFragmentRequirements::AddTagRequirements<EMassFragmentPresence::All>(const FMassTagBitSet& TagBitSet)
{
	RequiredAllTags += TagBitSet;
	// force recaching the next time this query is used or the following CacheArchetypes call.
	IncrementChangeCounter();
	return *this;
}

template<>
FORCEINLINE FMassFragmentRequirements& FMassFragmentRequirements::AddTagRequirements<EMassFragmentPresence::Any>(const FMassTagBitSet& TagBitSet)
{
	RequiredAnyTags += TagBitSet;
	// force recaching the next time this query is used or the following CacheArchetypes call.
	IncrementChangeCounter();
	return *this;
}

template<>
FORCEINLINE FMassFragmentRequirements& FMassFragmentRequirements::AddTagRequirements<EMassFragmentPresence::None>(const FMassTagBitSet& TagBitSet)
{
	RequiredNoneTags += TagBitSet;
	// force recaching the next time this query is used or the following CacheArchetypes call.
	IncrementChangeCounter();
	return *this;
}
