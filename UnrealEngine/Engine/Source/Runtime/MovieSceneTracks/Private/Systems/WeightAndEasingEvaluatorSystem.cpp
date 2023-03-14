// Copyright Epic Games, Inc. All Rights Reserved.

#include "Systems/WeightAndEasingEvaluatorSystem.h"
#include "Async/TaskGraphInterfaces.h"
#include "Compilation/MovieSceneCompiledDataManager.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/EntityAllocationIterator.h"
#include "EntitySystem/MovieSceneEntitySystemTask.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "Sections/MovieSceneSubSection.h"
#include "Systems/FloatChannelEvaluatorSystem.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneSection.h"

#include "EntitySystem/MovieSceneEvalTimeSystem.h"

#include "Algo/Find.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WeightAndEasingEvaluatorSystem)

DECLARE_CYCLE_STAT(TEXT("MovieScene: Evaluate easing"), MovieSceneEval_EvaluateEasingTask, STATGROUP_MovieSceneECS);

UMovieSceneHierarchicalEasingInstantiatorSystem::UMovieSceneHierarchicalEasingInstantiatorSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
	RelevantComponent = BuiltInComponents->HierarchicalEasingProvider;
	SystemCategories = EEntitySystemCategory::Core;
}

void UMovieSceneHierarchicalEasingInstantiatorSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	static constexpr uint16 INVALID_EASING_CHANNEL = uint16(-1);

	const FBuiltInComponentTypes* const BuiltInComponents = FBuiltInComponentTypes::Get();
	const FInstanceRegistry* const InstanceRegistry = Linker->GetInstanceRegistry();
	UWeightAndEasingEvaluatorSystem* const EvaluatorSystem = Linker->LinkSystem<UWeightAndEasingEvaluatorSystem>();

	// Step 1: Visit any new hierarchical easing providers (i.e. entities created by sub-sections with easing on them)
	//
	// We allocate the hierarchical easing channel for their sub-sequence.
	//
	auto VisitNewEasingProviders = [this, InstanceRegistry, EvaluatorSystem](const FEntityAllocation* Allocation, const FInstanceHandle* InstanceHandles, const FMovieSceneSequenceID* HierarchicalEasingProviders)
	{
		for (int32 Index = 0; Index < Allocation->Num(); ++Index)
		{
			const FMovieSceneSequenceID SequenceID = HierarchicalEasingProviders[Index];
			const FInstanceHandle SubSequenceHandle = InstanceRegistry->FindRelatedInstanceHandle(InstanceHandles[Index], SequenceID);

			// We use instance handles here because sequence IDs by themselves are only unique to a single hierarchy 
			// of sequences. If a root sequence is playing twice at the same time, there will be 2 sequence instances
			// for the same ID...
			// 
			// We allocate a new easing channel on the evaluator system, add this sub-section (provider) to the list of
			// contributors to that channel, and add the channel ID to our own map for step 2 below (this is all done
			// inside AllocateEasingChannel).
			//
			// It could happen that we already had an easing channel for this sub-sequence. This can happen in editor when
			// the user forces a re-import of the sub-section (by resizing it or whatever).
			uint16& EasingChannel = InstanceHandleToEasingChannel.FindOrAdd(SubSequenceHandle, INVALID_EASING_CHANNEL);
			if (EasingChannel == INVALID_EASING_CHANNEL)
			{
				EasingChannel = EvaluatorSystem->AllocateEasingChannel(SubSequenceHandle);
			}
			else
			{
			}
		}
	};

	FEntityTaskBuilder()
		.Read(BuiltInComponents->InstanceHandle)
		.Read(BuiltInComponents->HierarchicalEasingProvider)
		.FilterAll({ BuiltInComponents->Tags.NeedsLink })
		.Iterate_PerAllocation(&Linker->EntityManager, VisitNewEasingProviders);

	// Step 2: Visit any new entities that are inside an eased-in/out sub-sequence.
	//
	// We need to assign them to the appropriate hierarchical easing channel that we created in step 1.
	//
	auto VisitNewEasings = [this, InstanceRegistry](const FEntityAllocation* Allocation, const FInstanceHandle* InstanceHandles, uint16* HierarchicalEasings)
	{
		for (int32 Index = 0; Index < Allocation->Num(); ++Index)
		{
			const FInstanceHandle& InstanceHandle = InstanceHandles[Index];
			const uint16* EasingChannel = InstanceHandleToEasingChannel.Find(InstanceHandle);
			if (ensure(EasingChannel))
			{
				HierarchicalEasings[Index] = *EasingChannel;
			}
		}
	};

	FEntityTaskBuilder()
		.Read(BuiltInComponents->InstanceHandle)
		.Write(BuiltInComponents->HierarchicalEasingChannel)
		.FilterAll({ BuiltInComponents->Tags.NeedsLink })
		.Iterate_PerAllocation(&Linker->EntityManager, VisitNewEasings);

	// Step 3: Visit removed hierarchical easing providers, so we can free up our channels.
	//
	auto VisitRemovedEasingProviders = [this, InstanceRegistry, EvaluatorSystem](const FEntityAllocation* Allocation, const FInstanceHandle* InstanceHandles, const FMovieSceneSequenceID* HierarchicalEasingProviders)
	{
		for (int32 Index = 0; Index < Allocation->Num(); ++Index)
		{
			const FSequenceInstance* RootInstance = &InstanceRegistry->GetInstance(InstanceHandles[Index]);
			if (!RootInstance->IsRootSequence())
			{
				RootInstance = &InstanceRegistry->GetInstance(RootInstance->GetRootInstanceHandle());
			}

			const FMovieSceneSequenceID SubSequenceID = HierarchicalEasingProviders[Index];
			const FInstanceHandle SubSequenceHandle = RootInstance->FindSubInstance(SubSequenceID);

			uint16 OutEasingChannel;
			const bool bRemoved = InstanceHandleToEasingChannel.RemoveAndCopyValue(SubSequenceHandle, OutEasingChannel);
			if (ensure(bRemoved))
			{
				EvaluatorSystem->ReleaseEasingChannel(OutEasingChannel);
			}
		}
	};

	FEntityTaskBuilder()
		.Read(BuiltInComponents->InstanceHandle)
		.Read(BuiltInComponents->HierarchicalEasingProvider)
		.FilterAll({ BuiltInComponents->Tags.NeedsUnlink })
		.FilterNone({ BuiltInComponents->ParentEntity })
		.Iterate_PerAllocation(&Linker->EntityManager, VisitRemovedEasingProviders);

}

namespace UE
{
namespace MovieScene
{

struct FEvaluateEasings
{
	UWeightAndEasingEvaluatorSystem* EvaluatorSystem;

	FEvaluateEasings(UWeightAndEasingEvaluatorSystem* InEvaluatorSystem)
		: EvaluatorSystem(InEvaluatorSystem)
	{
		check(EvaluatorSystem);
	}

	void ForEachAllocation(
			const FEntityAllocation* InAllocation, 
			TRead<FFrameTime> Times,
			TReadOptional<FEasingComponentData> OptEasing,
			TReadOptional<double> OptManualWeights,
			TReadOptional<FInstanceHandle> OptInstanceHandles,
			TReadOptional<FMovieSceneSequenceID> OptHierarchicalEasingProviders,
			TWrite<double> Results)
	{
		const int32 Num = InAllocation->Num();

		// Initialize our result array.
		for (int32 Idx = 0; Idx < Num; ++Idx)
		{
			Results[Idx] = 1.f;
		}

		// Compute and add easing weight.
		if (OptEasing)
		{
			for (int32 Idx = 0; Idx < Num; ++Idx)
			{
				const double EasingWeight = OptEasing[Idx].Section->EvaluateEasing(Times[Idx]);
				Results[Idx] *= FMath::Max(EasingWeight, 0.f);
			}
		}

		// Manual weight has already been computed by the float channel evaluator system, so we
		// just need to pick up the result and combine it.
		if (OptManualWeights)
		{
			for (int32 Idx = 0; Idx < Num; ++Idx)
			{
				Results[Idx] *= FMath::Max(OptManualWeights[Idx], 0.f);
			}
		}

		// If this is an allocation for sub-sections that provide some ease-in/out to their child sub-sequence,
		// we store the resulting weight/easing results in the corresponding hierarhical easing channel data.
		// This will let us later apply those values onto all entities in the hierarchy below.
		// Sadly, this goes into random data access.
		//
		// Note that we need to check for instance handles because in interrogation evaluations, there are no
		// instance handles.
		//
		if (OptInstanceHandles && OptHierarchicalEasingProviders)
		{
			const FInstanceRegistry* InstanceRegistry = EvaluatorSystem->GetLinker()->GetInstanceRegistry();

			for (int32 Idx = 0; Idx < Num; ++Idx)
			{
				const FSequenceInstance* RootInstance = &InstanceRegistry->GetInstance(OptInstanceHandles[Idx]);
				if (!RootInstance->IsRootSequence())
				{
					RootInstance = &InstanceRegistry->GetInstance(RootInstance->GetRootInstanceHandle());
				}

				const FInstanceHandle SubSequenceHandle = RootInstance->FindSubInstance(OptHierarchicalEasingProviders[Idx]);
				EvaluatorSystem->SetSubSequenceEasing(SubSequenceHandle, Results[Idx]);
			}
		}
	}
};

struct FAccumulateHierarchicalEasings
{
	TSparseArray<FHierarchicalEasingChannelData>* EasingChannels;
	
	FAccumulateHierarchicalEasings(TSparseArray<FHierarchicalEasingChannelData>* InEasingChannels)
		: EasingChannels(InEasingChannels)
	{}

	FORCEINLINE TStatId           GetStatId() const    { return GET_STATID(MovieSceneEval_EvaluateEasingTask); }
	static ENamedThreads::Type    GetDesiredThread()   { return ENamedThreads::AnyThread; }
	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		Run();
	}

	void Run()
	{
		for (FHierarchicalEasingChannelData& EasingChannel : *EasingChannels)
		{
			EasingChannel.FinalEasingResult = 1.f;
			for (FHierarchicalEasingChannelContributorData& EasingChannelContributor : EasingChannel.Contributors)
			{
				EasingChannel.FinalEasingResult *= EasingChannelContributor.EasingResult;
			}
		}
	}
};

struct FPropagateHierarchicalEasings
{
	TSparseArray<FHierarchicalEasingChannelData>* EasingChannels;

	FPropagateHierarchicalEasings(TSparseArray<FHierarchicalEasingChannelData>* InEasingChannels)
		: EasingChannels(InEasingChannels)
	{}

	void ForEachAllocation(const FEntityAllocation* Allocation, TRead<uint16> HierarchicalEasingChannels, TWrite<double> WeightAndEasingResults)
	{
		const int32 Num = Allocation->Num();
		for (int32 Index = 0; Index < Num; ++Index)
		{
			const uint16 HierarchicalEasingChannel = HierarchicalEasingChannels[Index];

			if (ensure(EasingChannels->IsValidIndex(HierarchicalEasingChannel)))
			{
				const FHierarchicalEasingChannelData& EasingChannel = (*EasingChannels)[HierarchicalEasingChannel];
				WeightAndEasingResults[Index] *= EasingChannel.FinalEasingResult;
			}
		}
	}
};

} // namespace MovieScene
} // namespace UE

UWeightAndEasingEvaluatorSystem::UWeightAndEasingEvaluatorSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	SystemCategories = UE::MovieScene::EEntitySystemCategory::ChannelEvaluators;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		DefineImplicitPrerequisite(UMovieSceneEvalTimeSystem::StaticClass(), GetClass());
		DefineImplicitPrerequisite(UFloatChannelEvaluatorSystem::StaticClass(), GetClass());
	}
}

bool UWeightAndEasingEvaluatorSystem::IsRelevantImpl(UMovieSceneEntitySystemLinker* InLinker) const
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes* Components = FBuiltInComponentTypes::Get();
	return InLinker->EntityManager.ContainsAnyComponent({ Components->Easing, Components->WeightResult });
}

void UWeightAndEasingEvaluatorSystem::OnUnlink()
{
	if (!ensure(EasingChannels.Num() == 0))
	{
		EasingChannels.Reset();
	}
}

uint16 UWeightAndEasingEvaluatorSystem::AllocateEasingChannel(UE::MovieScene::FInstanceHandle SubSequenceHandle)
{
	using namespace UE::MovieScene;

	// Create a new data for the new channel.
	FHierarchicalEasingChannelData NewEasingChannelData;
	NewEasingChannelData.Contributors.Add({ SubSequenceHandle, 1.f });

	const FSequenceInstance& SubSequenceInstance = Linker->GetInstanceRegistry()->GetInstance(SubSequenceHandle);
	const FMovieSceneRootEvaluationTemplateInstance& RootEvalTemplate = SubSequenceInstance.GetPlayer()->GetEvaluationTemplate();
	TArray<FInstanceHandle> SubSequenceParentage;
	RootEvalTemplate.GetSequenceParentage(SubSequenceHandle, SubSequenceParentage);
	for (const FInstanceHandle& ParentHandle : SubSequenceParentage)
	{
		NewEasingChannelData.Contributors.Add({ ParentHandle, 1.f });
	}

	return EasingChannels.Add(NewEasingChannelData);
}

void UWeightAndEasingEvaluatorSystem::ReleaseEasingChannel(uint16 EasingChannelID)
{
	if (ensure(EasingChannels.IsValidIndex(EasingChannelID)))
	{
		EasingChannels.RemoveAt(EasingChannelID);
	}
}

void UWeightAndEasingEvaluatorSystem::SetSubSequenceEasing(UE::MovieScene::FInstanceHandle SubSequenceHandle, double EasingResult)
{
	using namespace UE::MovieScene;

	// The given sub-sequence has been assigned the given easing value. We can copy that value everywhere this
	// sub-sequence is used in a channel, i.e. for the channel of the sub-sequence itself, but also for the channels
	// of any children sub-sequences under that sub-sequence.
	for (FHierarchicalEasingChannelData& EasingChannel : EasingChannels)
	{
		for (FHierarchicalEasingChannelContributorData& EasingChannelContributor : EasingChannel.Contributors)
		{
			if (EasingChannelContributor.SubSequenceHandle == SubSequenceHandle)
			{
				EasingChannelContributor.EasingResult = EasingResult;
				break;
			}
		}
	}
}

void UWeightAndEasingEvaluatorSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes* Components = FBuiltInComponentTypes::Get();
	FInstanceRegistry* InstanceRegistry = Linker->GetInstanceRegistry();

	// Step 1: Compute all the easings and weights of all entities that have any.
	//
	FGraphEventRef EvalTask = FEntityTaskBuilder()
		// We need the eval time to evaluate easing curves.
		.Read(Components->EvalTime)
		.ReadOptional(Components->Easing)
		// We may need to multiply easing and manual weight together.
		.ReadOptional(Components->WeightResult)
		// For hierarchical easing we need the following 2 components... InstanceHandle is optional
		// because in interrogation evaluations, there are not instance handles.
		.ReadOptional(Components->InstanceHandle)
		.ReadOptional(Components->HierarchicalEasingProvider)
		// We will write the result to a separate component.
		.Write(Components->WeightAndEasingResult)
		.SetStat(GET_STATID(MovieSceneEval_EvaluateEasingTask))
		.Dispatch_PerAllocation<FEvaluateEasings>(&Linker->EntityManager, InPrerequisites, &Subsequents, this);

	// If we have no hierarchical easing, there's only one step... otherwise, we have more work to do.
	
	if (EasingChannels.Num() > 0)
	{
		// Step 2: Gather and compute sub-sequences' hierarchical easing results.
		//
		// Now, some of the entities we processed above happen to be representing sub-sections which contain entire sub-sequences.
		// We need to take their weight/easing result and propagate it to all the entities in these sub-sequences, and keep
		// propagating that down the hierarchy.
		//
		FSystemTaskPrerequisites PropagatePrereqs;

		if (Linker->EntityManager.GetThreadingModel() == EEntityThreadingModel::NoThreading)
		{
			FAccumulateHierarchicalEasings(&EasingChannels).Run();
		}
		else
		{
			FGraphEventArray AccumulatePrereqs { EvalTask };
			FGraphEventRef AccumulateTask = TGraphTask<FAccumulateHierarchicalEasings>::CreateTask(&AccumulatePrereqs, Linker->EntityManager.GetDispatchThread())
				.ConstructAndDispatchWhenReady(&EasingChannels);

			PropagatePrereqs.AddMasterTask(AccumulateTask);
		}

		// Step 3: Apply hierarchical easing results to all entities inside affected sub-sequences.
		//
		FEntityTaskBuilder()
			.Read(Components->HierarchicalEasingChannel)
			.Write(Components->WeightAndEasingResult)
			.SetStat(GET_STATID(MovieSceneEval_EvaluateEasingTask))
			.Dispatch_PerAllocation<FPropagateHierarchicalEasings>(&Linker->EntityManager, PropagatePrereqs, &Subsequents, &EasingChannels);
	}
}


