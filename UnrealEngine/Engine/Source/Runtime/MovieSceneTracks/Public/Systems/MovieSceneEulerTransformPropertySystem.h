// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Systems/MovieScenePropertySystem.h"
#include "MovieSceneEulerTransformPropertySystem.generated.h"


UCLASS(MinimalAPI)
class UMovieSceneEulerTransformPropertySystem : public UMovieScenePropertySystem
{
public:

	GENERATED_BODY()

	MOVIESCENETRACKS_API UMovieSceneEulerTransformPropertySystem(const FObjectInitializer& ObjInit);

	MOVIESCENETRACKS_API virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
};
