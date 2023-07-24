// Copyright Epic Games, Inc. All Rights Reserved.

#include "Systems/WeightAndEasingEvaluatorSystem.h"
#include "Async/TaskGraphInterfaces.h"
#include "Compilation/MovieSceneCompiledDataManager.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/EntityAllocationIterator.h"
#include "EntitySystem/MovieSceneEntitySystemTask.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneEntityMutations.h"
#include "EntitySystem/MovieSceneRootInstantiatorSystem.h"
#include "EntitySystem/MovieSceneComponentRegistry.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "Systems/MovieScenePropertyInstantiator.h"
#include "EntitySystem/Interrogation/MovieSceneInterrogatedPropertyInstantiator.h"
#include "Sections/MovieSceneSubSection.h"
#include "Systems/FloatChannelEvaluatorSystem.h"
#include "Systems/DoubleChannelEvaluatorSystem.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneSection.h"

#include "EntitySystem/MovieSceneEvalTimeSystem.h"

#include "Algo/Find.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WeightAndEasingEvaluatorSystem)

DECLARE_CYCLE_STAT(TEXT("MovieScene: Evaluate easing"), MovieSceneEval_EvaluateEasingTask, STATGROUP_MovieSceneECS);
DECLARE_CYCLE_STAT(TEXT("MovieScene: Harvest easing"), MovieSceneEval_HarvestEasingTask, STATGROUP_MovieSceneECS);


namespace UE::MovieScene
{

static constexpr uint16 INVALID_EASING_CHANNEL = uint16(-1);

enum class EHierarchicalSequenceChannelFlags
{
	None = 0,
	SharedWithParent = 1,
};
ENUM_CLASS_FLAGS(EHierarchicalSequenceChannelFlags)

/**
 * Class that accumulates active easing data for a particular root sequence hierarchy
 */
struct FRootSequenceData
{
	explicit FRootSequenceData(FRootInstanceHandle InRootInstanceHandle, const FInstanceRegistry* InInstanceRegistry)
		: InstanceRegistry(InInstanceRegistry)
		, RootHierarchy(nullptr) // defer initializing the hierarchy until we need it
		, RootInstanceHandle(InRootInstanceHandle)
	{}

	/** Retrieve the index of the specified sequence ID if it has already been encountered */
	int32 IndexOf(FMovieSceneSequenceID SequenceID) const
	{
		const int32 Index = Algo::LowerBoundBy(SequenceIDs, SequenceID, &FSequenceIDAndChannel::SequenceID);
		if (SequenceIDs.IsValidIndex(Index) && SequenceIDs[Index].SequenceID == SequenceID)
		{
			return Index;
		}
		return INDEX_NONE;
	}

	/* Retrieve the assigned channel ID for the specified sequence ID. Only valid once ProduceChannels has been called. */
	uint16 GetChannelID(FMovieSceneSequenceID SequenceID) const
	{
		const int32 ChannelIndex = IndexOf(SequenceID);
		if (ChannelIndex != INDEX_NONE)
		{
			return SequenceIDs[ChannelIndex].ChannelID;
		}
		return INVALID_EASING_CHANNEL;
	}

	/** Add the specified sequence ID to our map */
	int32 Add(FMovieSceneSequenceID SequenceID, EHierarchicalSequenceChannelFlags InFlags)
	{
		const int32 Index = Algo::LowerBoundBy(SequenceIDs, SequenceID, &FSequenceIDAndChannel::SequenceID);
		check(!SequenceIDs.IsValidIndex(Index) ||SequenceIDs[Index].SequenceID != SequenceID);

		SequenceIDs.Insert(FSequenceIDAndChannel{ SequenceID, INVALID_EASING_CHANNEL, InFlags }, Index);

		return Index;
	}

	/** Add the specified sequence ID to our map if it resides as a child underneath one that has an active weight */
	bool AddImplicitChild(FMovieSceneSequenceID SequenceID)
	{
		const bool bHasChannel = IndexOf(SequenceID) != INDEX_NONE;
		if (bHasChannel)
		{
			return true;
		}

		if (!RootHierarchy)
		{
			RootHierarchy = InstanceRegistry->GetInstance(RootInstanceHandle).GetPlayer()->GetEvaluationTemplate().GetHierarchy();
		}

		if (ensure(RootHierarchy))
		{
			const FMovieSceneSequenceHierarchyNode* Node = RootHierarchy->FindNode(SequenceID);
			check(Node);

			if (Node->ParentID != MovieSceneSequenceID::Invalid && AddImplicitChild(Node->ParentID))
			{
				Add(SequenceID, EHierarchicalSequenceChannelFlags::SharedWithParent);
				return true;
			}
		}
		return false;
	}

	/**
	 * Produce channel IDs in the specified output data, ordered parent-first
	 */
	void ProduceChannels(TSparseArray<uint16>& OutInstanceIDToChannel, TArray<FHierarchicalEasingChannelData>& OutData)
	{
		const FSequenceInstance& RootInstance = InstanceRegistry->GetInstance(RootInstanceHandle);
		for (int32 Index = 0; Index < SequenceIDs.Num(); ++Index)
		{
			ProduceChannelForIndex(Index, RootInstance, OutInstanceIDToChannel, OutData);
		}
	}

	/**
	 * Produce channels for the specified index within our map, including any of its parents
	 */
	uint16 ProduceChannelForIndex(int32 Index, const FSequenceInstance& RootInstance, TSparseArray<uint16>& OutInstanceIDToChannel, TArray<FHierarchicalEasingChannelData>& OutData)
	{
		const uint16 ExistingChannel = SequenceIDs[Index].ChannelID;
		if (ExistingChannel != INVALID_EASING_CHANNEL)
		{
			return ExistingChannel;
		}

		FSequenceIDAndChannel ChannelData = SequenceIDs[Index];

		if (ChannelData.SequenceID == MovieSceneSequenceID::Root)
		{
			// Add a new channel with no parent
			const uint16 ChannelID = static_cast<uint16>(OutData.Num());
			OutData.Add(FHierarchicalEasingChannelData{ INVALID_EASING_CHANNEL });
			OutInstanceIDToChannel.Insert(RootInstanceHandle.InstanceID, ChannelID);

			SequenceIDs[Index].ChannelID = ChannelID;
			return ChannelID;
		}

		if (!RootHierarchy)
		{
			RootHierarchy = InstanceRegistry->GetInstance(RootInstanceHandle).GetPlayer()->GetEvaluationTemplate().GetHierarchy();
		}

		if (!RootHierarchy)
		{
			return INVALID_EASING_CHANNEL;
		}

		// Set up hierarchy info for non-root sequences
		const FMovieSceneSequenceHierarchyNode* Node    = RootHierarchy->FindNode(ChannelData.SequenceID);
		const FMovieSceneSubSequenceData*       SubData = RootHierarchy->FindSubData(ChannelData.SequenceID);
		if (!Node || !SubData)
		{
			return INVALID_EASING_CHANNEL;
		}

		uint16 ParentChannel = INVALID_EASING_CHANNEL;
		FMovieSceneSequenceID Parent = MovieSceneSequenceID::Invalid;
		while (Node)
		{
			const int32 ParentIndex = IndexOf(Node->ParentID);
			if (ParentIndex != INDEX_NONE)
			{
				ParentChannel = ProduceChannelForIndex(ParentIndex, RootInstance, OutInstanceIDToChannel, OutData);
				break;
			}

			Node = RootHierarchy->FindNode(Node->ParentID);
		}

		// Don't bother creating channels for sequences that don't have an instance
		FInstanceHandle SubInstanceHandle = RootInstance.FindSubInstance(ChannelData.SequenceID);
		if (!SubInstanceHandle.IsValid())
		{
			return ParentChannel;
		}

		if (EnumHasAnyFlags(ChannelData.Flags, EHierarchicalSequenceChannelFlags::SharedWithParent))
		{
			OutInstanceIDToChannel.Insert(SubInstanceHandle.InstanceID, ParentChannel);
			SequenceIDs[Index].ChannelID = ParentChannel;
			return ParentChannel;
		}
		else
		{
			const uint16 ChannelID = static_cast<uint16>(OutData.Num());
			OutData.Add(FHierarchicalEasingChannelData{ ParentChannel });
			OutInstanceIDToChannel.Insert(SubInstanceHandle.InstanceID, ChannelID);
			SequenceIDs[Index].ChannelID = ChannelID;
			return ChannelID;
		}
	}

private:

	struct FSequenceIDAndChannel
	{
		FMovieSceneSequenceID SequenceID;
		uint16 ChannelID;
		EHierarchicalSequenceChannelFlags Flags;
	};

	const FInstanceRegistry* InstanceRegistry;
	const FMovieSceneSequenceHierarchy* RootHierarchy;
	TArray<FSequenceIDAndChannel, TInlineAllocator<32>> SequenceIDs;
	FRootInstanceHandle RootInstanceHandle;
};

using FRootSequenceDataMap = TSortedMap<FRootInstanceHandle, FRootSequenceData, TInlineAllocator<8>>;

/**
 * Mutation that adds easing channel components to consumers that have active weights
 */
struct FEasingChannelMutationBase : IMovieSceneConditionalEntityMutation
{
	explicit FEasingChannelMutationBase(UMovieSceneEntitySystemLinker* Linker)
	{
		BuiltInComponents = FBuiltInComponentTypes::Get();
		Factories = &Linker->EntityManager.GetComponents()->Factories;
	}

	/**
	 * Create the new entity type for a group of entities
	 */
	void CreateMutation(FEntityManager* InEntityManager, FComponentMask* InOutEntityComponentTypes) const override
	{
		InOutEntityComponentTypes->Set(BuiltInComponents->HierarchicalEasingChannel);
		Factories->ComputeMutuallyInclusiveComponents(*InOutEntityComponentTypes);
		InOutEntityComponentTypes->Set(BuiltInComponents->Tags.NeedsLink);
	}

	bool HasStaleComponents() const
	{
		return StaleEntities.Num() != 0;
	}

	void RemoveStaleComponents(UMovieSceneEntitySystemLinker* Linker)
	{
		if (StaleEntities.Num())
		{
			FComponentMask ComponentsToRemove;
			ComponentsToRemove.Set(BuiltInComponents->HierarchicalEasingChannel);
			ComponentsToRemove.Set(BuiltInComponents->WeightAndEasingResult);

			for (FMovieSceneEntityID Entity : StaleEntities)
			{
				Linker->EntityManager.RemoveComponents(Entity, ComponentsToRemove);
			}
		}
	}

protected:

	FBuiltInComponentTypes* BuiltInComponents;
	FEntityFactories* Factories;
	mutable TArray<FMovieSceneEntityID> StaleEntities;
};

/**
 * Mutation that adds or assigns easing channels on easing providers
 */
struct FProviderEasingChannelMutation : FEasingChannelMutationBase
{
	explicit FProviderEasingChannelMutation(UMovieSceneEntitySystemLinker* Linker, const FRootSequenceDataMap* InSequenceDataMap)
		: FEasingChannelMutationBase(Linker)
		, SequenceDataMap(InSequenceDataMap)
	{}

	/**
	 * Called on matching allocations to mark specific entities that need mutating.
	 * If the entity exists within a weighted sequence (directly or indirectly), a channel will be created (or referenced) and assigned
	 */
	void MarkAllocation(FEntityAllocation* Allocation, TBitArray<>& OutEntitiesToMutate) const override
	{
		const FMovieSceneEntityID* EntityIDs = Allocation->GetRawEntityIDs();

		TComponentReader<FRootInstanceHandle>   RootInstanceHandles   = Allocation->ReadComponents(BuiltInComponents->RootInstanceHandle);
		TComponentReader<FMovieSceneSequenceID> EasingProviders       = Allocation->ReadComponents(BuiltInComponents->HierarchicalEasingProvider);
		TOptionalComponentWriter<uint16>        ExistingEasingHandles = Allocation->TryWriteComponents(BuiltInComponents->HierarchicalEasingChannel, FEntityAllocationWriteContext::NewAllocation());

		const bool bHasExistingEasing = ExistingEasingHandles.IsValid();

		// Loop through each entity and check whether it has easing.
		// If so, either assign the new easing channel or mark it for mutation.
		// If not, mark it for removal
		for (int32 Index = 0; Index < Allocation->Num(); ++Index)
		{
			const FRootSequenceData* ChannelData = SequenceDataMap->Find(RootInstanceHandles[Index]);
			const uint16             ChannelID   = ChannelData ? ChannelData->GetChannelID(EasingProviders[Index]) : INVALID_EASING_CHANNEL;

			if (ChannelID != INVALID_EASING_CHANNEL)
			{
				if (bHasExistingEasing)
				{
					ExistingEasingHandles[Index] = ChannelID;
				}
				else
				{
					// Mark this entity for mutation
					OutEntitiesToMutate.PadToNum(Index + 1, false);
					OutEntitiesToMutate[Index] = true;
				}
			}
			else if (bHasExistingEasing)
			{
				StaleEntities.Add(EntityIDs[Index]);
			}
		}
	}

	/**
	 * Initialize a range of new entities with their easing channels
	 */
	void InitializeEntities(const FEntityRange& EntityRange, const FComponentMask& AllocationType) const override
	{
		TComponentReader<FRootInstanceHandle>   RootInstanceHandles = EntityRange.Allocation->ReadComponents(BuiltInComponents->RootInstanceHandle);
		TComponentReader<FMovieSceneSequenceID> EasingProviders     = EntityRange.Allocation->ReadComponents(BuiltInComponents->HierarchicalEasingProvider);
		TComponentWriter<uint16>                EasingChannels      = EntityRange.Allocation->WriteComponents(BuiltInComponents->HierarchicalEasingChannel, FEntityAllocationWriteContext::NewAllocation());

		for (int32 Index = 0; Index < EntityRange.Num; ++Index)
		{
			const int32 Offset = EntityRange.ComponentStartOffset + Index;

			const FRootSequenceData& ChannelData = SequenceDataMap->FindChecked(RootInstanceHandles[Offset]);
			EasingChannels[Offset] = ChannelData.GetChannelID(EasingProviders[Offset]);
		}
	}

private:

	const FRootSequenceDataMap* SequenceDataMap;
};

/**
 * Mutation that adds easing channel components to consumers that have active weights
 */
struct FConsumerEasingChannelMutation : FEasingChannelMutationBase
{
	explicit FConsumerEasingChannelMutation(UMovieSceneEntitySystemLinker* Linker, TSparseArray<uint16>* InInstanceIDToChannel)
		: FEasingChannelMutationBase(Linker)
		, InstanceIDToChannel(InInstanceIDToChannel)
	{}

	/**
	 * Called on matching allocations to mark specific entities that need mutating.
	 * If the entity exists within a weighted sequence (directly or indirectly), a channel will be created (or referenced) and assigned
	 */
	void MarkAllocation(FEntityAllocation* Allocation, TBitArray<>& OutEntitiesToMutate) const override
	{
		const FMovieSceneEntityID* EntityIDs = Allocation->GetRawEntityIDs();

		TComponentReader<FInstanceHandle> InstanceHandles       = Allocation->ReadComponents(BuiltInComponents->InstanceHandle);
		TOptionalComponentWriter<uint16>  ExistingEasingHandles = Allocation->TryWriteComponents(BuiltInComponents->HierarchicalEasingChannel, FEntityAllocationWriteContext::NewAllocation());

		const bool bHasExistingEasing = ExistingEasingHandles.IsValid();

		// Loop through each entity and check whether it has easing.
		// If so, either assign the new easing channel or mark it for mutation.
		// If not, mark it for removal
		for (int32 Index = 0; Index < Allocation->Num(); ++Index)
		{
			if (InstanceIDToChannel->IsValidIndex(InstanceHandles[Index].InstanceID))
			{
				if (bHasExistingEasing)
				{
					ExistingEasingHandles[Index] = (*InstanceIDToChannel)[InstanceHandles[Index].InstanceID];
				}
				else
				{
					// Mark this entity for mutation
					OutEntitiesToMutate.PadToNum(Index + 1, false);
					OutEntitiesToMutate[Index] = true;
				}
			}
			else if (bHasExistingEasing)
			{
				StaleEntities.Add(EntityIDs[Index]);
			}
		}
	}

	/**
	 * Initialize a range of new entities with their easing channels
	 */
	void InitializeEntities(const FEntityRange& EntityRange, const FComponentMask& AllocationType) const override
	{
		FEntityAllocationWriteContext NewAllocation = FEntityAllocationWriteContext::NewAllocation();

		TComponentWriter<uint16>              EasingChannels      = EntityRange.Allocation->WriteComponents(BuiltInComponents->HierarchicalEasingChannel, NewAllocation);
		TComponentReader<FInstanceHandle>     InstanceHandles     = EntityRange.Allocation->ReadComponents(BuiltInComponents->InstanceHandle);

		for (int32 Index = 0; Index < EntityRange.Num; ++Index)
		{
			const int32 Offset = EntityRange.ComponentStartOffset + Index;
			EasingChannels[Offset] = (*InstanceIDToChannel)[InstanceHandles[Offset].InstanceID];
		}
	}

private:

	TSparseArray<uint16>* InstanceIDToChannel;
};

/**
 * Reset WeightAndEasingResult components before accumulation
 */
struct FResetFinalWeightResults
{
	static void ForEachEntity(double& Result)
	{
		Result = 1.f;
	}
};

/**
 * Evaluate section easing into EasingResult components
 */
struct FEvaluateEasings
{
	static void ForEachEntity(FFrameTime EvalTime, const FEasingComponentData& Easing, double& Result)
	{
		const double EasingWeight = Easing.Section->EvaluateEasing(EvalTime);
		Result = FMath::Max(EasingWeight, 0.f);
	}
};

/**
 * Accumulate EasingResult and WeightResult components into WeightAndEasingResult components
 */
struct FAccumulateManualWeights
{
	static void ForEachAllocation(const FEntityAllocation* Allocation, const TReadOneOrMoreOf<double, double>& Results, double* OutAccumulatedResults)
	{
		using NumericType = TDecay<decltype(OutAccumulatedResults)>::Type;
		
		const int32 Num = Allocation->Num();

		const double* WeightResults = Results.Get<0>();
		const double* EasingResults = Results.Get<1>();

		check(WeightResults || EasingResults);

		// Have to do math
		if (WeightResults && EasingResults)
		{
			for (int32 Index = 0; Index < Num; ++Index)
			{
				OutAccumulatedResults[Index] = WeightResults[Index] * EasingResults[Index];
			}
		}
		else
		{
			FMemory::Memcpy(OutAccumulatedResults, WeightResults ? WeightResults : EasingResults, Num * sizeof(NumericType));
		}
	}
};

/**
 * Harvest hierarchical WeightAndEasingResult components from HierarchicalEasingProvider components into a linear buffer for propagation to consumers
 */
struct FHarvestHierarchicalEasings
{
	TArrayView<FHierarchicalEasingChannelData> ComputationData;

	FHarvestHierarchicalEasings(TArray<FHierarchicalEasingChannelData>* InComputationData)
		: ComputationData(*InComputationData)
	{
		check(InComputationData);
	}

	// Before the task runs, initialize the results array
	void PreTask()
	{
		for (FHierarchicalEasingChannelData& Data : ComputationData)
		{
			Data.FinalResult = 1.0;
		}
	}

	// Accumulate all entities that contribute to the channel
	void ForEachEntity(double Result, uint16 EasingChannel)
	{
		ComputationData[EasingChannel].FinalResult *= Result;
	}

	// Multiply hierarchical weights with sub sequences
	void PostTask()
	{
		// Move forward through the results array, multiplying with parents
		// This is possible because the results array is already sorted by depth
		for (int32 Index = 0; Index < ComputationData.Num(); ++Index)
		{
			FHierarchicalEasingChannelData ChannelData = ComputationData[Index];
			if (ChannelData.ParentChannel != uint16(-1))
			{
				// The parent result has already been multiplied by all its parent weights by this point
				ComputationData[Index].FinalResult *= ComputationData[ChannelData.ParentChannel].FinalResult;
			}
		}
	}
};

/**
 * Propagate hierarchical weight values to WeightAndEasingResult components on consumers
 */
struct FPropagateHierarchicalEasings
{
	TArrayView<const FHierarchicalEasingChannelData> ComputationData;

	FPropagateHierarchicalEasings(TArrayView<const FHierarchicalEasingChannelData> InComputationData)
		: ComputationData(InComputationData)
	{}

	void ForEachAllocation(const FEntityAllocation* Allocation, TRead<uint16> HierarchicalEasingChannels, TWrite<double> WeightAndEasingResults)
	{
		const int32 Num = Allocation->Num();
		for (int32 Index = 0; Index < Num; ++Index)
		{
			const uint16 HierarchicalEasingChannel = HierarchicalEasingChannels[Index];
			if (ensure(ComputationData.IsValidIndex(HierarchicalEasingChannel)))
			{
				WeightAndEasingResults[Index] *= ComputationData[HierarchicalEasingChannel].FinalResult;
			}
		}
	}
};


} // namespace UE::MovieScene


UMovieSceneHierarchicalEasingInstantiatorSystem::UMovieSceneHierarchicalEasingInstantiatorSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
	SystemCategories = EEntitySystemCategory::Core;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		// Has to come after bound object and root instantiation
		DefineImplicitPrerequisite(UMovieSceneRootInstantiatorSystem::StaticClass(), GetClass());
		DefineComponentConsumer(GetClass(), BuiltInComponents->BoundObject);

		// Has to come before property instantiation so that initial values get created correctly
		DefineImplicitPrerequisite(GetClass(), UMovieScenePropertyInstantiatorSystem::StaticClass());
		DefineImplicitPrerequisite(GetClass(), UMovieSceneInterrogatedPropertyInstantiatorSystem::StaticClass());
	}
}

bool UMovieSceneHierarchicalEasingInstantiatorSystem::IsRelevantImpl(UMovieSceneEntitySystemLinker* InLinker) const
{
	using namespace UE::MovieScene;

	return CachedInstanceIDToChannel.Num() || InLinker->EntityManager.ContainsComponent(FBuiltInComponentTypes::Get()->HierarchicalEasingProvider);
}

void UMovieSceneHierarchicalEasingInstantiatorSystem::OnLink()
{
	EvaluatorSystem = Linker->LinkSystem<UWeightAndEasingEvaluatorSystem>();
	// Keep the evaluator system alive as long as we are alive
	Linker->SystemGraph.AddReference(this, EvaluatorSystem);
}

void UMovieSceneHierarchicalEasingInstantiatorSystem::OnUnlink()
{
	CachedInstanceIDToChannel.Empty();
}

void UMovieSceneHierarchicalEasingInstantiatorSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	const FBuiltInComponentTypes* const BuiltInComponents = FBuiltInComponentTypes::Get();
	const FInstanceRegistry* const InstanceRegistry = Linker->GetInstanceRegistry();

	// Update if we have any NeedsLink or NeedsUnlink providers (that contribute to hierachical weights)
	FEntityComponentFilter ChangedProviderFilter;
	ChangedProviderFilter.All({ BuiltInComponents->HierarchicalEasingProvider });
	ChangedProviderFilter.Any({ BuiltInComponents->Tags.NeedsLink, BuiltInComponents->Tags.NeedsUnlink });

	// Or any NeedsLink sub instances (that might need to receieve a parent weight)
	FEntityComponentFilter NewSubInstanceFilter;
	NewSubInstanceFilter.All({ BuiltInComponents->Tags.SubInstance, BuiltInComponents->Tags.NeedsLink });

	// Or any NeedsUnlink sub instances that have an easing channel already assigned
	FEntityComponentFilter ExpiringSubInstanceFilter;
	ExpiringSubInstanceFilter.All({ BuiltInComponents->Tags.SubInstance, BuiltInComponents->HierarchicalEasingChannel, BuiltInComponents->Tags.NeedsUnlink });

	// Check if we have any new or expiring easing providers
	if (Linker->EntityManager.Contains(ChangedProviderFilter) || Linker->EntityManager.Contains(NewSubInstanceFilter) || Linker->EntityManager.Contains(ExpiringSubInstanceFilter))
	{
		FRootSequenceDataMap SequenceDataMap;

		// Called for all easing providers to add them to our accumulation map
		auto VisitEasingProvider = [InstanceRegistry, &SequenceDataMap](FMovieSceneEntityID EntityID, FRootInstanceHandle RootInstance, FMovieSceneSequenceID ProviderID)
		{
			if (InstanceRegistry->IsHandleValid(RootInstance))
			{
				// Make the entry for our root instance
				FRootSequenceData* RootSequenceData = SequenceDataMap.Find(RootInstance);
				if (!RootSequenceData)
				{
					RootSequenceData = &SequenceDataMap.Emplace(RootInstance, FRootSequenceData(RootInstance, InstanceRegistry));
				}

				// Add the sequence ID to the channel map if it doesn't already exist
				if (RootSequenceData->IndexOf(ProviderID) == INDEX_NONE)
				{
					RootSequenceData->Add(ProviderID, EHierarchicalSequenceChannelFlags::None);
				}
			}
		};

		// Called for all sub instances to add them to our accumulation map if they reside within a parent that is blended
		auto VisitSubInstance = [InstanceRegistry, &SequenceDataMap](FMovieSceneEntityID EntityID, FRootInstanceHandle RootInstance, FMovieSceneSequenceID SequenceID)
		{
			if (SequenceID != MovieSceneSequenceID::Root && InstanceRegistry->IsHandleValid(RootInstance))
			{
				if (FRootSequenceData* RootSequenceData = SequenceDataMap.Find(RootInstance))
				{
					RootSequenceData->AddImplicitChild(SequenceID);
				}
			}
		};

		// Step 1 : Iterate all easing providers and add their SequenceIDs to the map
		// 
		FEntityTaskBuilder()
		.ReadEntityIDs()
		.Read(BuiltInComponents->RootInstanceHandle)
		.Read(BuiltInComponents->HierarchicalEasingProvider)
		.FilterNone({ BuiltInComponents->Tags.NeedsUnlink })
		.Iterate_PerEntity(&Linker->EntityManager, VisitEasingProvider);

		// Step 2 : Iterate all sub instances and see if they exist within one of the previously collected easing providers
		// 
		FEntityTaskBuilder()
		.ReadEntityIDs()
		.Read(BuiltInComponents->RootInstanceHandle)
		.Read(BuiltInComponents->SequenceID)
		.FilterAll({ BuiltInComponents->Tags.SubInstance })
		.FilterNone({ BuiltInComponents->Tags.NeedsUnlink })
		.Iterate_PerEntity(&Linker->EntityManager, VisitSubInstance);

		// Step 3: Produce parent-first channel IDs for anything we collected, then pass on the buffer to the evaluator system
		//
		{
			TArray<FHierarchicalEasingChannelData> AllChannels;
			CachedInstanceIDToChannel.Empty();

			for (TPair<FRootInstanceHandle, FRootSequenceData>& Pair : SequenceDataMap)
			{
				Pair.Value.ProduceChannels(CachedInstanceIDToChannel, AllChannels);
			}
			EvaluatorSystem->SetComputationBuffer(MoveTemp(AllChannels));
		}

		// Step 4: Assign channel IDs to easing providers, potentially adding or removing invalid ones
		//
		{
			FEntityComponentFilter ProviderFilter;
			ProviderFilter.All({ BuiltInComponents->RootInstanceHandle, BuiltInComponents->HierarchicalEasingProvider });
			ProviderFilter.None({ BuiltInComponents->Tags.NeedsUnlink });

			FProviderEasingChannelMutation ProviderMutation(Linker, &SequenceDataMap);
			Linker->EntityManager.MutateConditional(ProviderFilter, ProviderMutation);

			ProviderMutation.RemoveStaleComponents(Linker);
		}

		// Step 5: Assign easing channels for anything else that is left and remove stale ones
		//
		{
			FEntityComponentFilter NewFilter;
			NewFilter.All({ BuiltInComponents->InstanceHandle });
			NewFilter.None({ BuiltInComponents->Tags.NeedsUnlink, BuiltInComponents->HierarchicalEasingProvider });

			FConsumerEasingChannelMutation ConsumerMutation(Linker, &CachedInstanceIDToChannel);
			Linker->EntityManager.MutateConditional(NewFilter, ConsumerMutation);

			ConsumerMutation.RemoveStaleComponents(Linker);
		}
	}
	else
	{
		// Just add easing channels to NeedsLink entities
		FEntityComponentFilter Filter;
		Filter.All({ BuiltInComponents->InstanceHandle, BuiltInComponents->Tags.NeedsLink });
		Filter.None({ BuiltInComponents->HierarchicalEasingProvider });

		FConsumerEasingChannelMutation ConsumerMutation(Linker, &CachedInstanceIDToChannel);
		Linker->EntityManager.MutateConditional(Filter, ConsumerMutation);

		ensure(!ConsumerMutation.HasStaleComponents());
		ConsumerMutation.RemoveStaleComponents(Linker);
	}
}

UWeightAndEasingEvaluatorSystem::UWeightAndEasingEvaluatorSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	using namespace UE::MovieScene;

	SystemCategories = UE::MovieScene::EEntitySystemCategory::ChannelEvaluators;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		const FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();

		DefineComponentConsumer(GetClass(), BuiltInComponents->WeightResult);

		DefineImplicitPrerequisite(UMovieSceneEvalTimeSystem::StaticClass(), GetClass());
		DefineComponentProducer(GetClass(), BuiltInComponents->EasingResult);
		DefineComponentProducer(GetClass(), BuiltInComponents->WeightAndEasingResult);
	}
}

bool UWeightAndEasingEvaluatorSystem::IsRelevantImpl(UMovieSceneEntitySystemLinker* InLinker) const
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes* Components = FBuiltInComponentTypes::Get();
	return InLinker->EntityManager.ContainsAnyComponent({ Components->WeightAndEasingResult });
}

void UWeightAndEasingEvaluatorSystem::OnLink()
{
	PreAllocatedComputationData.Empty();
}

void UWeightAndEasingEvaluatorSystem::OnUnlink()
{
	PreAllocatedComputationData.Empty();
}

void UWeightAndEasingEvaluatorSystem::SetComputationBuffer(TArray<UE::MovieScene::FHierarchicalEasingChannelData>&& InPreAllocatedComputationData)
{
	PreAllocatedComputationData = MoveTemp(InPreAllocatedComputationData);
}

void UWeightAndEasingEvaluatorSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes* Components = FBuiltInComponentTypes::Get();
	FInstanceRegistry* InstanceRegistry = Linker->GetInstanceRegistry();

	// No hierarchical weighting, just reset everything to 1.0
	FGraphEventRef ResetWeights = FEntityTaskBuilder()
	.Write(Components->WeightAndEasingResult)
	.SetStat(GET_STATID(MovieSceneEval_EvaluateEasingTask))
	.Dispatch_PerEntity<FResetFinalWeightResults>(&Linker->EntityManager, InPrerequisites, &Subsequents);

	FSystemTaskPrerequisites ResetWeightsDependencies {InPrerequisites};
	ResetWeightsDependencies.AddComponentTask(Components->WeightAndEasingResult, ResetWeights);

	// Step 1: Evaluate section easing and manual weights in parallel
	//
	FGraphEventRef EvaluateEasing = FEntityTaskBuilder()
	.Read(Components->EvalTime)
	.Read(Components->Easing)
	.Write(Components->EasingResult)
	.SetStat(GET_STATID(MovieSceneEval_EvaluateEasingTask))
	.Dispatch_PerEntity<FEvaluateEasings>(&Linker->EntityManager, ResetWeightsDependencies, &Subsequents);

	FGraphEventRef AccumulateManualWeights = FEntityTaskBuilder()
	.ReadOneOrMoreOf(Components->WeightResult, Components->EasingResult)
	.Write(Components->WeightAndEasingResult)
	.SetStat(GET_STATID(MovieSceneEval_EvaluateEasingTask))
	.Dispatch_PerAllocation<FAccumulateManualWeights>(&Linker->EntityManager, ResetWeightsDependencies, &Subsequents);

	// If we have hierarchical easing, we initialize all the weights to their hierarchical defaults
	if (PreAllocatedComputationData.Num() > 0)
	{
		FSystemTaskPrerequisites HarvestPrereqs {InPrerequisites};
		HarvestPrereqs.AddComponentTask(Components->WeightAndEasingResult, EvaluateEasing);
		HarvestPrereqs.AddComponentTask(Components->WeightAndEasingResult, AccumulateManualWeights);

		// Step 2: Harvest any hierarchical results from providers
		//
		FGraphEventRef HarvestTask = FEntityTaskBuilder()
		.Read(Components->WeightAndEasingResult)
		.Read(Components->HierarchicalEasingChannel)
		.FilterAll({ Components->HierarchicalEasingProvider })  // Only harvest results from entities that are providing results
		.SetStat(GET_STATID(MovieSceneEval_HarvestEasingTask))
		.Dispatch_PerEntity<FHarvestHierarchicalEasings>(&Linker->EntityManager, HarvestPrereqs, nullptr, &PreAllocatedComputationData);

		FSystemTaskPrerequisites PropagatePrereqs {InPrerequisites};
		PropagatePrereqs.AddRootTask(HarvestTask);

		// Step 3: Apply hierarchical easing results to all entities inside affected sub-sequences.
		//
		FEntityTaskBuilder()
		.Read(Components->HierarchicalEasingChannel)
		.Write(Components->WeightAndEasingResult)
		.FilterNone({ Components->HierarchicalEasingProvider }) // Do not propagate hierarchical weights onto providers!
		.SetStat(GET_STATID(MovieSceneEval_EvaluateEasingTask))
		.Dispatch_PerAllocation<FPropagateHierarchicalEasings>(&Linker->EntityManager, PropagatePrereqs, &Subsequents,
			PreAllocatedComputationData);
	}
}