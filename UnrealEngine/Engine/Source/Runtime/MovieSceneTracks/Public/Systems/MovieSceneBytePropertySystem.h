// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Systems/MovieScenePropertySystem.h"
#include "MovieSceneBytePropertySystem.generated.h"

UCLASS(MinimalAPI)
class UMovieSceneBytePropertySystem : public UMovieScenePropertySystem
{
	GENERATED_BODY()

	UMovieSceneBytePropertySystem(const FObjectInitializer& ObjInit);

	MOVIESCENETRACKS_API virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
};
