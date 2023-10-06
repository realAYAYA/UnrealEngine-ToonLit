// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "MovieScene.h"
#include "MovieSceneDynamicBinding.h"
#include "MovieScenePossessable.h"
#include "MovieSceneSpawnable.h"

#include "MovieSceneDynamicBindingUtils.generated.h"

/**
 * A utility class for managing dynamic binding endpoints.
 */
struct MOVIESCENETOOLS_API FMovieSceneDynamicBindingUtils
{
	/**
	 * Set an endpoint on the given dynamic binding.
	 */
	static void SetEndpoint(UMovieScene* MovieScene, FMovieSceneDynamicBinding* DynamicBinding, UK2Node* NewEndpoint);

	/**
	 * Ensures that the dynamic binding blueprint extension has been added to the given sequence's director blueprint.
	 */
	static void EnsureBlueprintExtensionCreated(UMovieSceneSequence* MovieSceneSequence, UBlueprint* Blueprint);

	/**
	 * Utility function for iterating all dynamic bindings in a sequence.
	 */
	template<typename Callback>
	static void IterateDynamicBindings(UMovieScene* InMovieScene, Callback&& InCallback)
	{
		for (int32 Index = 0, PossessableCount = InMovieScene->GetPossessableCount(); Index < PossessableCount; ++Index)
		{
			FMovieScenePossessable& Possessable = InMovieScene->GetPossessable(Index);
			FMovieSceneDynamicBinding& DynamicBinding = Possessable.DynamicBinding;
			InCallback(Possessable.GetGuid(), DynamicBinding);
		}

		for (int32 Index = 0, SpawnableCount = InMovieScene->GetSpawnableCount(); Index < SpawnableCount; ++Index)
		{
			FMovieSceneSpawnable& Spawnable = InMovieScene->GetSpawnable(Index);
			FMovieSceneDynamicBinding& DynamicBinding = Spawnable.DynamicBinding;
			InCallback(Spawnable.GetGuid(), DynamicBinding);
		}
	}

	/**
	 * Utility function for gathering all dynamic bindings in a sequence into a container.
	 */
	static void GatherDynamicBindings(UMovieScene* InMovieScene, TArray<FMovieSceneDynamicBinding*>& OutDynamicBindings)
	{
		IterateDynamicBindings(InMovieScene, [&](const FGuid&, FMovieSceneDynamicBinding& Item)
			{
				OutDynamicBindings.Add(&Item);
			});
	}
};

/**
 * Dummy class, used for easily getting a valid UFunction that helps prepare blueprint function graphs.
 */
UCLASS()
class MOVIESCENETOOLS_API UMovieSceneDynamicBindingEndpointUtil : public UObject
{
	GENERATED_BODY()

	UFUNCTION()
	FMovieSceneDynamicBindingResolveResult SampleResolveBinding() { return FMovieSceneDynamicBindingResolveResult(); }
};

