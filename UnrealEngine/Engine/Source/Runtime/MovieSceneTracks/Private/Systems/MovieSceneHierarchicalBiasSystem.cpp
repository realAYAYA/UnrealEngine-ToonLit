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

struct FBlendChannelSequenceKey
{
	FRootInstanceHandle RootInstance;
	FMovieSceneBlendChannelID BlendChannelID;
	friend uint32 GetTypeHash(const FBlendChannelSequenceKey& In)
	{
		return HashCombine(GetTypeHash(In.RootInstance), GetTypeHash(In.BlendChannelID));
	}
	friend bool operator==(const FBlendChannelSequenceKey& A, const FBlendChannelSequenceKey& B)
	{
		return A.RootInstance == B.RootInstance && A.BlendChannelID == B.BlendChannelID;
	}
};

struct FHierarchicalBiasTask
{
	explicit FHierarchicalBiasTask(UMovieSceneEntitySystemLinker* InLinker)
		: Linker(InLinker)
	{}

	void InitializeChannel(FRootInstanceHandle RootInstanceHandle, FMovieSceneBlendChannelID BlendChannel)
	{
		MaxBiasByChannel.FindOrAdd(FBlendChannelSequenceKey{ RootInstanceHandle, BlendChannel }, MIN_int16);
	}

	bool HasAnyWork() const
	{
		return MaxBiasByChannel.Num() != 0;
	}

	void ForEachAllocation(FEntityAllocationIteratorItem Iterator, TRead<FMovieSceneEntityID> EntityIDs, TRead<FRootInstanceHandle> RootInstanceHandles, TRead<FMovieSceneBlendChannelID> BlendChannels, TReadOptional<int16> OptHBiases)
	{
		const int32 Num = Iterator.GetAllocation()->Num();
		const FComponentMask& AllocationType = Iterator.GetAllocationType();
		const bool bIgnoreBias = AllocationType.Contains(FBuiltInComponentTypes::Get()->Tags.IgnoreHierarchicalBias)
			|| AllocationType.Contains(FBuiltInComponentTypes::Get()->HierarchicalBlendTarget);

		if (bIgnoreBias)
		{
			for (int32 Index = 0; Index < Num; ++Index)
			{
				FBlendChannelSequenceKey Key{ RootInstanceHandles[Index], BlendChannels[Index] };
				ActiveContributorsByChannel.Add(Key, EntityIDs[Index]);
			}
		}
		else if (OptHBiases)
		{
			for (int32 Index = 0; Index < Num; ++Index)
			{
				VisitChannel(EntityIDs[Index], RootInstanceHandles[Index], BlendChannels[Index], OptHBiases[Index]);
			}
		}
		else
		{
			for (int32 Index = 0; Index < Num; ++Index)
			{
				VisitChannel(EntityIDs[Index], RootInstanceHandles[Index], BlendChannels[Index], 0);
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

	void VisitChannel(FMovieSceneEntityID EntityID, FRootInstanceHandle RootInstanceHandle, FMovieSceneBlendChannelID BlendChannel, int16 HBias)
	{
		FBlendChannelSequenceKey Key{ RootInstanceHandle, BlendChannel };

		// If this channel hasn't changed at all (ie InitializeChannel was not called for it) do nothing
		if (int16* ExistingBias = MaxBiasByChannel.Find(Key))
		{
			if (HBias > *ExistingBias)
			{
				for (auto It = ActiveContributorsByChannel.CreateKeyIterator(Key); It; ++It)
				{
					InactiveContributorsByChannel.Add(Key, It.Value());
					It.RemoveCurrent();
				}

				*ExistingBias = HBias;
				ActiveContributorsByChannel.Add(Key, EntityID);
			}
			else if (HBias == *ExistingBias)
			{
				ActiveContributorsByChannel.Add(Key, EntityID);
			}
			else
			{
				InactiveContributorsByChannel.Add(Key, EntityID);
			}
		}
	}

	TMap<FBlendChannelSequenceKey, int16> MaxBiasByChannel;

	TMultiMap<FBlendChannelSequenceKey, FMovieSceneEntityID> InactiveContributorsByChannel;

	TMultiMap<FBlendChannelSequenceKey, FMovieSceneEntityID> ActiveContributorsByChannel;

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
	.Read(Components->RootInstanceHandle)
	.Read(Components->BlendChannelInput)
	.FilterAny({ Components->Tags.NeedsLink, Components->Tags.NeedsUnlink })
	.Iterate_PerEntity(&Linker->EntityManager, [&Task](FRootInstanceHandle RootInstanceHandle, FMovieSceneBlendChannelID BlendChannel){ Task.InitializeChannel(RootInstanceHandle, BlendChannel); });

	if (Task.HasAnyWork())
	{
		FEntityTaskBuilder()
		.ReadEntityIDs()
		.Read(Components->RootInstanceHandle)
		.Read(Components->BlendChannelInput)
		.ReadOptional(Components->HierarchicalBias)
		.FilterNone({ Components->Tags.NeedsUnlink })
		.RunInline_PerAllocation(&Linker->EntityManager, Task);
	}
}

