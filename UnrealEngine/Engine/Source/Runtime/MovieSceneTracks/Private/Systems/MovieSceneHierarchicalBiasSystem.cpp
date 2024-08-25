// Copyright Epic Games, Inc. All Rights Reserved.

#include "Systems/MovieSceneHierarchicalBiasSystem.h"

#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/MovieSceneEntitySystemTask.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieScenePreAnimatedStateSystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneHierarchicalBiasSystem)


namespace UE
{
namespace MovieScene
{

struct FEntityGroupSequenceKey
{
	FRootInstanceHandle RootInstance;
	FEntityGroupID GroupID;
	friend uint32 GetTypeHash(const FEntityGroupSequenceKey& In)
	{
		return HashCombine(GetTypeHash(In.RootInstance), GetTypeHash(In.GroupID));
	}
	friend bool operator==(const FEntityGroupSequenceKey& A, const FEntityGroupSequenceKey& B)
	{
		return A.RootInstance == B.RootInstance && A.GroupID == B.GroupID;
	}
};

struct FHierarchicalBiasTask
{
	explicit FHierarchicalBiasTask(UMovieSceneEntitySystemLinker* InLinker)
		: Linker(InLinker)
	{}

	void InitializeGroup(FRootInstanceHandle RootInstanceHandle, FEntityGroupID GroupID)
	{
		MaxBiasByGroup.FindOrAdd(FEntityGroupSequenceKey{ RootInstanceHandle, GroupID }, MIN_int16);
	}

	bool HasAnyWork() const
	{
		return MaxBiasByGroup.Num() != 0;
	}

	void ForEachAllocation(FEntityAllocationIteratorItem Iterator, TRead<FMovieSceneEntityID> EntityIDs, TRead<FRootInstanceHandle> RootInstanceHandles, TRead<FEntityGroupID> GroupIDs, TReadOptional<int16> OptHBiases)
	{
		const int32 Num = Iterator.GetAllocation()->Num();
		const FComponentMask& AllocationType = Iterator.GetAllocationType();
		const bool bIgnoreBias = AllocationType.Contains(FBuiltInComponentTypes::Get()->Tags.IgnoreHierarchicalBias)
			|| AllocationType.Contains(FBuiltInComponentTypes::Get()->HierarchicalBlendTarget);

		if (bIgnoreBias)
		{
			for (int32 Index = 0; Index < Num; ++Index)
			{
				FEntityGroupSequenceKey Key{ RootInstanceHandles[Index], GroupIDs[Index] };
				ActiveContributorsByGroup.Add(Key, EntityIDs[Index]);
			}
		}
		else if (OptHBiases)
		{
			for (int32 Index = 0; Index < Num; ++Index)
			{
				VisitGroup(EntityIDs[Index], RootInstanceHandles[Index], GroupIDs[Index], OptHBiases[Index]);
			}
		}
		else
		{
			for (int32 Index = 0; Index < Num; ++Index)
			{
				VisitGroup(EntityIDs[Index], RootInstanceHandles[Index], GroupIDs[Index], 0);
			}
		}
	}

	void PostTask()
	{
		FBuiltInComponentTypes* Components = FBuiltInComponentTypes::Get();

		for (auto It = ActiveContributorsByGroup.CreateIterator(); It; ++It)
		{
			Linker->EntityManager.RemoveComponent(It.Value(), Components->Tags.Ignored);
		}

		for (auto It = InactiveContributorsByGroup.CreateIterator(); It; ++It)
		{
			Linker->EntityManager.AddComponent(It.Value(), Components->Tags.Ignored);
		}
	}

private:

	void VisitGroup(FMovieSceneEntityID EntityID, FRootInstanceHandle RootInstanceHandle, FEntityGroupID GroupID, int16 HBias)
	{
		FEntityGroupSequenceKey Key{ RootInstanceHandle, GroupID };

		// If this group hasn't changed at all (ie InitializeGroup was not called for it) do nothing
		if (int16* ExistingBias = MaxBiasByGroup.Find(Key))
		{
			if (HBias > *ExistingBias)
			{
				for (auto It = ActiveContributorsByGroup.CreateKeyIterator(Key); It; ++It)
				{
					InactiveContributorsByGroup.Add(Key, It.Value());
					It.RemoveCurrent();
				}

				*ExistingBias = HBias;
				ActiveContributorsByGroup.Add(Key, EntityID);
			}
			else if (HBias == *ExistingBias)
			{
				ActiveContributorsByGroup.Add(Key, EntityID);
			}
			else
			{
				InactiveContributorsByGroup.Add(Key, EntityID);
			}
		}
	}

	TMap<FEntityGroupSequenceKey, int16> MaxBiasByGroup;

	TMultiMap<FEntityGroupSequenceKey, FMovieSceneEntityID> InactiveContributorsByGroup;

	TMultiMap<FEntityGroupSequenceKey, FMovieSceneEntityID> ActiveContributorsByGroup;

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
		DefineComponentConsumer(GetClass(), FBuiltInComponentTypes::Get()->Group);

		// Don't flag things with the Ignore tag (due to hierarchical biases) until all systems have
		// had a chance to take them into account for pre-animated state.
		DefineImplicitPrerequisite(UMovieSceneCachePreAnimatedStateSystem::StaticClass(), GetClass());
	}
}

bool UMovieSceneHierarchicalBiasSystem::IsRelevantImpl(UMovieSceneEntitySystemLinker* InLinker) const
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes* Components = FBuiltInComponentTypes::Get();
	return InLinker->EntityManager.ContainsAllComponents({ Components->Group, Components->HierarchicalBias });
}

void UMovieSceneHierarchicalBiasSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes* Components = FBuiltInComponentTypes::Get();

	FHierarchicalBiasTask Task(Linker);

	// First, add all the groups that have changed to the map
	FEntityTaskBuilder()
	.Read(Components->RootInstanceHandle)
	.Read(Components->Group)
	.FilterAny({ Components->Tags.NeedsLink, Components->Tags.NeedsUnlink })
	.Iterate_PerEntity(&Linker->EntityManager, [&Task](FRootInstanceHandle RootInstanceHandle, FEntityGroupID GroupID)
			{ Task.InitializeGroup(RootInstanceHandle, GroupID); });

	if (Task.HasAnyWork())
	{
		FEntityTaskBuilder()
		.ReadEntityIDs()
		.Read(Components->RootInstanceHandle)
		.Read(Components->Group)
		.ReadOptional(Components->HierarchicalBias)
		.FilterNone({ Components->Tags.NeedsUnlink })
		.RunInline_PerAllocation(&Linker->EntityManager, Task);
	}
}

