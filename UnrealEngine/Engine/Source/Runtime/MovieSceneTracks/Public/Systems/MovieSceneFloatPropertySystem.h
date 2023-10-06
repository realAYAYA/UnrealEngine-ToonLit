// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Systems/MovieScenePropertySystem.h"
#include "MovieSceneFloatPropertySystem.generated.h"



UCLASS(MinimalAPI)
class UMovieSceneFloatPropertySystem : public UMovieScenePropertySystem
{
	GENERATED_BODY()

	UMovieSceneFloatPropertySystem(const FObjectInitializer& ObjInit);

	MOVIESCENETRACKS_API virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
};
