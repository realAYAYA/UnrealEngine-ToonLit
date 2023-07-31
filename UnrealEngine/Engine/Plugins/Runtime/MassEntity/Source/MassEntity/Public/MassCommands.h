// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ProfilingDebugging/CsvProfilerConfig.h"
#include "Misc/MTAccessDetector.h"
#include "MassEntityTypes.h"
#include "MassEntityUtils.h"
#include "MassEntityManager.h"

/**
 * Enum used by MassBatchCommands to declare their "type". This data is later used to group commands so that command 
 * effects are applied in a controllable fashion 
 * Important: if changed make sure to update FMassCommandBuffer::Flush.CommandTypeOrder as well
 */
UENUM()
enum class EMassCommandOperationType : uint8
{
	None,				// default value. Commands marked this way will be always executed last. Programmers are encouraged to instead use one of the meaningful values below.
	Create,				// signifies commands performing entity creation
	Add,				// signifies commands adding fragments or tags to entities
	Remove,				// signifies commands removing fragments or tags from entities
	ChangeComposition,	// signifies commands both adding and removing fragments and/or tags from entities
	Set,				// signifies commands setting values to pre-existing fragments. The fragments might be added if missing,
						// depending on specific command, so this group will always be executed after the Add group
	Destroy,			// signifies commands removing entities
	MAX
};

struct MASSENTITY_API FMassBatchedCommand
{
	virtual ~FMassBatchedCommand() {}

	virtual void Execute(FMassEntityManager& System) const = 0;
	virtual void Reset()
	{
		bHasWork = false;
	}

	bool HasWork() const { return bHasWork; }
	EMassCommandOperationType GetOperationType() const { return OperationType; }

	template<typename T>
	FORCENOINLINE static uint32 GetCommandIndex()
	{
		static const uint32 ThisTypesStaticIndex = CommandsCounter++;
		return ThisTypesStaticIndex;
	}

	virtual SIZE_T GetAllocatedSize() const = 0;

#if CSV_PROFILER || WITH_MASSENTITY_DEBUG
	virtual int32 GetNumOperationsStat() const = 0;
	FName GetFName() const { return DebugName; }
#endif // CSV_PROFILER || WITH_MASSENTITY_DEBUG

protected:
	// @todo note for reviewers - I could use an opinion if having a virtual function per-command would be a more 
	// preferable way of asking commands if there's anything to do.
	bool bHasWork = false;
	EMassCommandOperationType OperationType = EMassCommandOperationType::None;

#if CSV_PROFILER || WITH_MASSENTITY_DEBUG
	FName DebugName;
#endif // CSV_PROFILER || WITH_MASSENTITY_DEBUG

private:
	static std::atomic<uint32> CommandsCounter;
};

struct FMassBatchedEntityCommand : public FMassBatchedCommand
{
	using Super = FMassBatchedCommand;

	void Add(FMassEntityHandle Entity)
	{
		UE_MT_SCOPED_READ_ACCESS(EntitiesAccessDetector);
		TargetEntities.Add(Entity);
		bHasWork = true;
	}

	void Add(TConstArrayView<FMassEntityHandle> Entities)
	{
		UE_MT_SCOPED_READ_ACCESS(EntitiesAccessDetector);
		TargetEntities.Append(Entities.GetData(), Entities.Num());
		bHasWork = true;
	}

protected:
	virtual SIZE_T GetAllocatedSize() const
	{
		return TargetEntities.GetAllocatedSize();
	}

	virtual void Reset() override
	{
		TargetEntities.Reset();
		Super::Reset();
	}

#if CSV_PROFILER || WITH_MASSENTITY_DEBUG
	virtual int32 GetNumOperationsStat() const override { return TargetEntities.Num(); }
#endif // CSV_PROFILER || WITH_MASSENTITY_DEBUG

	UE_MT_DECLARE_RW_ACCESS_DETECTOR(EntitiesAccessDetector); 
	TArray<FMassEntityHandle> TargetEntities;
};

//////////////////////////////////////////////////////////////////////
// Entity destruction

struct FMassCommandDestroyEntities : public FMassBatchedEntityCommand
{
	FMassCommandDestroyEntities()
	{
		OperationType = EMassCommandOperationType::Destroy;
	}

protected:
	virtual void Execute(FMassEntityManager& System) const override
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MassCommandDestroyEntities_Execute);

		TArray<FMassArchetypeEntityCollection> EntityCollectionsToDestroy;
		UE::Mass::Utils::CreateEntityCollections(System, TargetEntities, FMassArchetypeEntityCollection::FoldDuplicates, EntityCollectionsToDestroy);
		for (FMassArchetypeEntityCollection& Collection : EntityCollectionsToDestroy)
		{
			System.BatchDestroyEntityChunks(Collection);
		}
	}
};

//////////////////////////////////////////////////////////////////////
// Simple fragment composition change

template<typename... TTypes>
struct FMassCommandAddFragments : public FMassBatchedEntityCommand
{
	FMassCommandAddFragments()
	{
		UE::Mass::TMultiTypeList<TTypes...>::PopulateBitSet(FragmentsAffected);
		OperationType = EMassCommandOperationType::Add;
	}
protected:
	virtual void Execute(FMassEntityManager& System) const override
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MassCommandAddFragments_Execute);
		TArray<FMassArchetypeEntityCollection> EntityCollections;
		UE::Mass::Utils::CreateEntityCollections(System, TargetEntities, FMassArchetypeEntityCollection::FoldDuplicates, EntityCollections);
		System.BatchChangeFragmentCompositionForEntities(EntityCollections, FragmentsAffected, FMassFragmentBitSet());
	}
	FMassFragmentBitSet FragmentsAffected;
};

template<typename... TTypes>
struct FMassCommandRemoveFragments : public FMassBatchedEntityCommand
{
	FMassCommandRemoveFragments()
	{
		UE::Mass::TMultiTypeList<TTypes...>::PopulateBitSet(FragmentsAffected);
		OperationType = EMassCommandOperationType::Remove;
	}
protected:
	virtual void Execute(FMassEntityManager& System) const override
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MassCommandRemoveFragments_Execute);
		TArray<FMassArchetypeEntityCollection> EntityCollections;
		UE::Mass::Utils::CreateEntityCollections(System, TargetEntities, FMassArchetypeEntityCollection::FoldDuplicates, EntityCollections);
		System.BatchChangeFragmentCompositionForEntities(EntityCollections, FMassFragmentBitSet(), FragmentsAffected);
	}
	FMassFragmentBitSet FragmentsAffected;
};

//////////////////////////////////////////////////////////////////////
// Simple tag composition change

struct FMassCommandChangeTags : public FMassBatchedEntityCommand
{
	using Super = FMassBatchedEntityCommand;

	FMassCommandChangeTags()
	{
		OperationType = EMassCommandOperationType::ChangeComposition;
	}

protected:
	virtual void Execute(FMassEntityManager& System) const override
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MassCommandChangeTags_Execute);
		TArray<FMassArchetypeEntityCollection> EntityCollections;
		UE::Mass::Utils::CreateEntityCollections(System, TargetEntities, FMassArchetypeEntityCollection::FoldDuplicates, EntityCollections);

		System.BatchChangeTagsForEntities(EntityCollections, TagsToAdd, TagsToRemove);
	}

	virtual SIZE_T GetAllocatedSize() const override
	{
		return TagsToAdd.GetAllocatedSize() + TagsToRemove.GetAllocatedSize() + Super::GetAllocatedSize();
	}

	FMassTagBitSet TagsToAdd;
	FMassTagBitSet TagsToRemove;
};

template<typename T>
struct FMassCommandAddTag : public FMassCommandChangeTags
{
	FMassCommandAddTag()
	{
		OperationType = EMassCommandOperationType::Add;
		TagsToAdd.Add<T>();
#if CSV_PROFILER || WITH_MASSENTITY_DEBUG
		DebugName = TEXT("CommandAddTag");
#endif // CSV_PROFILER || WITH_MASSENTITY_DEBUG
	}
};

template<typename T>
struct FMassCommandRemoveTag : public FMassCommandChangeTags
{
	FMassCommandRemoveTag()
	{
		OperationType = EMassCommandOperationType::Remove;
		TagsToRemove.Add<T>();
#if CSV_PROFILER || WITH_MASSENTITY_DEBUG
		DebugName = TEXT("RemoveAddTag");
#endif // CSV_PROFILER || WITH_MASSENTITY_DEBUG
	}
};

template<typename TOld, typename TNew>
struct FMassCommandSwapTags : public FMassCommandChangeTags
{
	FMassCommandSwapTags()
	{
		OperationType = EMassCommandOperationType::ChangeComposition;
		TagsToRemove.Add<TOld>();
		TagsToAdd.Add<TNew>();
#if CSV_PROFILER || WITH_MASSENTITY_DEBUG
		DebugName = TEXT("SwapAddTags");
#endif // CSV_PROFILER || WITH_MASSENTITY_DEBUG
	}
};

//////////////////////////////////////////////////////////////////////
// Struct Instances adding and setting

template<typename... TOthers>
struct FMassCommandAddFragmentInstances : public FMassBatchedEntityCommand
{
	using Super = FMassBatchedEntityCommand;

	FMassCommandAddFragmentInstances()
	{
		OperationType = EMassCommandOperationType::Set;
		UE::Mass::TMultiTypeList<TOthers...>::PopulateBitSet(FragmentsAffected);
#if CSV_PROFILER || WITH_MASSENTITY_DEBUG
		DebugName = TEXT("AddFragmentInstanceList");
#endif // CSV_PROFILER || WITH_MASSENTITY_DEBUG
	}

	void Add(FMassEntityHandle Entity, TOthers... InFragments)
	{
		Super::Add(Entity);
		Fragments.Add(InFragments...);
	}

protected:
	virtual SIZE_T GetAllocatedSize() const override
	{
		return Super::GetAllocatedSize() + Fragments.GetAllocatedSize() + FragmentsAffected.GetAllocatedSize();
	}

	virtual void Execute(FMassEntityManager& System) const override
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MassCommandAddFragmentInstances_Execute);

		TArray<FStructArrayView> GenericMultiArray;
		GenericMultiArray.Reserve(Fragments.GetNumArrays());
		Fragments.GetAsGenericMultiArray(GenericMultiArray);

		TArray<FMassArchetypeEntityCollectionWithPayload> EntityCollections;
		FMassArchetypeEntityCollectionWithPayload::CreateEntityRangesWithPayload(System, TargetEntities, FMassArchetypeEntityCollection::FoldDuplicates
			, FMassGenericPayloadView(GenericMultiArray), EntityCollections);

		System.BatchAddFragmentInstancesForEntities(EntityCollections, FragmentsAffected);
	}

	mutable UE::Mass::TMultiArray<TOthers...> Fragments;
	FMassFragmentBitSet FragmentsAffected;
};

template<typename... TOthers>
struct FMassCommandBuildEntity : public FMassCommandAddFragmentInstances<TOthers...>
{
	FMassCommandBuildEntity()
	{
		Super::OperationType = EMassCommandOperationType::Create;
#if CSV_PROFILER || WITH_MASSENTITY_DEBUG
		Super::DebugName = TEXT("FMassCommandBuildEntity");
#endif // CSV_PROFILER || WITH_MASSENTITY_DEBUG
	}

protected:
	using Super = FMassCommandAddFragmentInstances<TOthers...>;

	virtual void Execute(FMassEntityManager& System) const override
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MassCommandBuildEntity_Execute);

		TArray<FStructArrayView> GenericMultiArray;
		GenericMultiArray.Reserve(Super::Fragments.GetNumArrays());
		Super::Fragments.GetAsGenericMultiArray(GenericMultiArray);

		TArray<FMassArchetypeEntityCollectionWithPayload> EntityCollections;
		FMassArchetypeEntityCollectionWithPayload::CreateEntityRangesWithPayload(System, Super::TargetEntities, FMassArchetypeEntityCollection::FoldDuplicates
			, FMassGenericPayloadView(GenericMultiArray), EntityCollections);

		check(EntityCollections.Num() <= 1);
		if (EntityCollections.Num())
		{
			System.BatchBuildEntities(EntityCollections[0], Super::FragmentsAffected, FMassArchetypeSharedFragmentValues());
		}
	}
};

/** 
 * Note: that TSharedFragmentValues is always expected to be FMassArchetypeSharedFragmentValues, but is declared as 
 *	template's param to maintain uniform command adding interface via FMassCommandBuffer.PushCommand. 
 *	PushCommands received all input params in one `typename...` list and as such cannot be easily split up to reason about.
 */
template<typename TSharedFragmentValues, typename... TOthers>
struct FMassCommandBuildEntityWithSharedFragments : public FMassBatchedCommand
{
	using Super = FMassBatchedCommand;

	FMassCommandBuildEntityWithSharedFragments()
	{
		OperationType = EMassCommandOperationType::Create;
		UE::Mass::TMultiTypeList<TOthers...>::PopulateBitSet(FragmentsAffected);
#if CSV_PROFILER || WITH_MASSENTITY_DEBUG
		DebugName = TEXT("FMassCommandBuildEntityWithSharedFragments");
#endif // CSV_PROFILER || WITH_MASSENTITY_DEBUG
	}

	void Add(FMassEntityHandle Entity, FMassArchetypeSharedFragmentValues&& InSharedFragments, TOthers... InFragments)
	{
		InSharedFragments.Sort();

		// Compute hash before adding to the map since evaluation order is not guaranteed
		// and MoveTemp will invalidate InSharedFragments
		const uint32 Hash = GetTypeHash(InSharedFragments);

		FPerSharedFragmentsHashData& Instance = Data.FindOrAdd(Hash, MoveTemp(InSharedFragments));
		Instance.Fragments.Add(InFragments...);
		Instance.TargetEntities.Add(Entity);

		bHasWork = true;
	}

protected:
	virtual SIZE_T GetAllocatedSize() const override
	{
		SIZE_T TotalSize = 0;
		for (const auto& KeyValue : Data)
		{
			TotalSize += KeyValue.Value.GetAllocatedSize();
		}
		TotalSize += Data.GetAllocatedSize();
		TotalSize += FragmentsAffected.GetAllocatedSize();
		return TotalSize;
	}

	virtual void Execute(FMassEntityManager& System) const 
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MassCommandBuildEntityWithSharedFragments_Execute);

		constexpr int FragmentTypesCount = UE::Mass::TMultiTypeList<TOthers...>::Ordinal + 1;
		TArray<FStructArrayView> GenericMultiArray;
		GenericMultiArray.Reserve(FragmentTypesCount);

		for (auto It : Data)
		{			
			It.Value.Fragments.GetAsGenericMultiArray(GenericMultiArray);

			TArray<FMassArchetypeEntityCollectionWithPayload> EntityCollections;
			FMassArchetypeEntityCollectionWithPayload::CreateEntityRangesWithPayload(System, It.Value.TargetEntities, FMassArchetypeEntityCollection::FoldDuplicates
				, FMassGenericPayloadView(GenericMultiArray), EntityCollections);
			checkf(EntityCollections.Num() <= 1, TEXT("We expect TargetEntities to only contain archetype-less entities, ones that need to be \'build\'"));

			if (EntityCollections.Num())
			{
				System.BatchBuildEntities(EntityCollections[0], FragmentsAffected, It.Value.SharedFragmentValues);
			}

			GenericMultiArray.Reset();
		}
	}

	virtual void Reset() override
	{
		Data.Reset();
		Super::Reset();
	}

#if CSV_PROFILER || WITH_MASSENTITY_DEBUG
	virtual int32 GetNumOperationsStat() const override
	{
		int32 TotalCount = 0;
		for (const auto& KeyValue : Data)
		{
			TotalCount += KeyValue.Value.TargetEntities.Num();
		}
		return TotalCount;
	}
#endif // CSV_PROFILER || WITH_MASSENTITY_DEBUG

	FMassFragmentBitSet FragmentsAffected;

	struct FPerSharedFragmentsHashData
	{
		FPerSharedFragmentsHashData(FMassArchetypeSharedFragmentValues&& InSharedFragmentValues)
			: SharedFragmentValues(MoveTemp(InSharedFragmentValues))
		{	
		}

		SIZE_T GetAllocatedSize() const
		{
			return TargetEntities.GetAllocatedSize() + Fragments.GetAllocatedSize() + SharedFragmentValues.GetAllocatedSize();
		}

		TArray<FMassEntityHandle> TargetEntities;
		mutable UE::Mass::TMultiArray<TOthers...> Fragments;
		FMassArchetypeSharedFragmentValues SharedFragmentValues;
	};

	TMap<uint32, FPerSharedFragmentsHashData> Data;
};

//////////////////////////////////////////////////////////////////////
// Commands that really can't know the types at compile time

template<EMassCommandOperationType OpType>
struct FMassDeferredCommand : public FMassBatchedCommand
{
	using Super = FMassBatchedCommand;
	using FExecFunction = TFunction<void(FMassEntityManager& System)>;

	FMassDeferredCommand()
	{
		OperationType = OpType;
#if CSV_PROFILER || WITH_MASSENTITY_DEBUG
		DebugName = TEXT("BatchedDeferredCommand");
#endif // CSV_PROFILER || WITH_MASSENTITY_DEBUG
	};

	void Add(FExecFunction&& ExecFunction)
	{
		DeferredFunctions.Add(MoveTemp(ExecFunction));
		bHasWork = true;
	}

	void Add(const FExecFunction& ExecFunction)
	{
		DeferredFunctions.Add(ExecFunction);
		bHasWork = true;
	}

protected:
	virtual SIZE_T GetAllocatedSize() const
	{
		return DeferredFunctions.GetAllocatedSize();
	}

	virtual void Execute(FMassEntityManager& System) const override
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MassDeferredCommand_Execute);

		for (const FExecFunction& ExecFunction : DeferredFunctions)
		{
			ExecFunction(System);
		}
	}

	virtual void Reset() override
	{
		DeferredFunctions.Reset();
		Super::Reset();
	}

#if CSV_PROFILER || WITH_MASSENTITY_DEBUG
	virtual int32 GetNumOperationsStat() const override
	{
		return DeferredFunctions.Num();
	}
#endif // CSV_PROFILER || WITH_MASSENTITY_DEBUG

	TArray<FExecFunction> DeferredFunctions;
};

using FMassDeferredCreateCommand = FMassDeferredCommand<EMassCommandOperationType::Create>;
using FMassDeferredAddCommand = FMassDeferredCommand<EMassCommandOperationType::Add>;
using FMassDeferredRemoveCommand = FMassDeferredCommand<EMassCommandOperationType::Remove>;
using FMassDeferredChangeCompositionCommand = FMassDeferredCommand<EMassCommandOperationType::ChangeComposition>;
using FMassDeferredSetCommand = FMassDeferredCommand<EMassCommandOperationType::Set>;
using FMassDeferredDestroyCommand = FMassDeferredCommand<EMassCommandOperationType::Destroy>;
