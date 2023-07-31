// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Systems/MovieScenePropertySystem.h"
#include "EntitySystem/MovieSceneEntityInstantiatorSystem.h"
#include "EntitySystem/MovieScenePreAnimatedStateSystem.h"
#include "EntitySystem/MovieSceneOverlappingEntityTracker.h"
#include "MovieSceneTracksComponentTypes.h"

#include "MovieSceneComponentTransformSystem.generated.h"


UCLASS(MinimalAPI)
class UMovieSceneComponentTransformSystem : public UMovieScenePropertySystem
{
public:

	GENERATED_BODY()

	UMovieSceneComponentTransformSystem(const FObjectInitializer& ObjInit);

private:

	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
};
