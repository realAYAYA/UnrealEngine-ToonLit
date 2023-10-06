// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Systems/MovieScenePropertySystem.h"
#include "MovieSceneBoolPropertySystem.generated.h"



UCLASS(MinimalAPI)
class UMovieSceneBoolPropertySystem : public UMovieScenePropertySystem
{
	GENERATED_BODY()

	UMovieSceneBoolPropertySystem(const FObjectInitializer& ObjInit);

	MOVIESCENETRACKS_API virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
};

