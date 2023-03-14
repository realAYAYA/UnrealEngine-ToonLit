// Copyright Epic Games, Inc. All Rights Reserved.

#include "Systems/MovieSceneHierarchicalBiasSystem.h"

#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/MovieSceneEntitySystemTask.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneHierarchicalBiasSystem)


namespace UE
{
namespace MovieScene
{

struct FHierarchicalBiasTask
{
	explicit FHierarchicalBiasTask(UMovieSceneEntitySystemLinker* InLinker)
		: Linker(InLinker)
	{}

	void InitializeChannel(FMovieSceneBlendChannelID BlendChannel)
	{
		MaxBiasByChannel.FindOrAdd(BlendChannel, MIN_int16);
	}

	bool HasAnyWork() const
	{
		return MaxBiasByChannel.Num() != 0;
	}

	void ForEachAllocation(const FEntityAllocation* Allocation, TRead<FMovieSceneEntityID> EntityIDs, TRead<FMovieSceneBlendChannelID> BlendChannels, TReadOptional<int16> OptHBiases)
	{
		const int32 Num = Allocation->Num();
		if (OptHBiases)
		{
			for (int32 Index = 0; Index < Num; ++Index)
			{
				VisitChannel(EntityIDs[Index], BlendChannels[Index], OptHBiases[Index]);
			}
		}
		else
		{
			for (int32 Index = 0; Index < Num; ++Index)
			{
				VisitChannel(EntityIDs[Index], BlendChannels[Index], 0);
			}
		}
	}

	void PostTask()
	{
		FBuiltInComponentTypes* Components = FBuiltInComponentTypes::Get();

		for (auto It = ActiveContributorsByChannel.CreateIterator(); It; ++It)
		{
			Linker->EntityManager.RemoveComponent(It.Value(), Components->Tags.Ignored);
		}

		for (auto It = InactiveContributorsByChannel.CreateIterator(); It; ++It)
		{
			Linker->EntityManager.AddComponent(It.Value(), Components->Tags.Ignored);
		}
	}

private:

	void VisitChannel(FMovieSceneEntityID EntityID, FMovieSceneBlendChannelID BlendChannel, int16 HBias)
	{
		// If this channel hasn't changed at all (ie InitializeChannel was not called for it) do nothing
		if (int16* ExistingBias = MaxBiasByChannel.Find(BlendChannel))
		{
			if (HBias > *ExistingBias)
			{
				for (auto It = ActiveContributorsByChannel.CreateKeyIterator(BlendChannel); It; ++It)
				{
					InactiveContributorsByChannel.Add(BlendChannel, It.Value());
					It.RemoveCurrent();
				}

				*ExistingBias = HBias;
				ActiveContributorsByChannel.Add(BlendChannel, EntityID);
			}
			else if (HBias == *ExistingBias)
			{
				ActiveContributorsByChannel.Add(BlendChannel, EntityID);
			}
			else
			{
				InactiveContributorsByChannel.Add(BlendChannel, EntityID);
			}
		}
	}

	TMap<FMovieSceneBlendChannelID, int16> MaxBiasByChannel;

	TMultiMap<FMovieSceneBlendChannelID, FMovieSceneEntityID> InactiveContributorsByChannel;

	TMultiMap<FMovieSceneBlendChannelID, FMovieSceneEntityID> ActiveContributorsByChannel;

	UMovieSceneEntitySystemLinker* Linker;
};

} // namespace MovieScene
} // namespace UE


UMovieSceneHierarchicalBiasSystem::UMovieSceneHierarchicalBiasSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	using namespace UE::MovieScene;

	SystemCategories = EEntitySystemCategory::Core;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		DefineComponentConsumer(GetClass(), FBuiltInComponentTypes::Get()->BlendChannelInput);
	}
}

bool UMovieSceneHierarchicalBiasSystem::IsRelevantImpl(UMovieSceneEntitySystemLinker* InLinker) const
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes* Components = FBuiltInComponentTypes::Get();
	return InLinker->EntityManager.ContainsAllComponents({ Components->BlendChannelInput, Components->HierarchicalBias });
}

void UMovieSceneHierarchicalBiasSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes* Components = FBuiltInComponentTypes::Get();

	FHierarchicalBiasTask Task(Linker);

	// First, add all the channels that have changed to the map
	FEntityTaskBuilder()
	.Read(Components->BlendChannelInput)
	.FilterAny({ Components->Tags.NeedsLink, Components->Tags.NeedsUnlink })
	.Iterate_PerEntity(&Linker->EntityManager, [&Task](FMovieSceneBlendChannelID BlendChannel){ Task.InitializeChannel(BlendChannel); });

	if (Task.HasAnyWork())
	{
		FEntityTaskBuilder()
		.ReadEntityIDs()
		.Read(Components->BlendChannelInput)
		.ReadOptional(Components->HierarchicalBias)
		.FilterNone({ Components->Tags.NeedsUnlink })
		.RunInline_PerAllocation(&Linker->EntityManager, Task);
	}
}

