// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Systems/MovieScenePropertySystem.h"
#include "MovieSceneIntegerPropertySystem.generated.h"

UCLASS(MinimalAPI)
class UMovieSceneIntegerPropertySystem : public UMovieScenePropertySystem
{
	GENERATED_BODY()

	UMovieSceneIntegerPropertySystem(const FObjectInitializer& ObjInit);

	MOVIESCENETRACKS_API virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
};
