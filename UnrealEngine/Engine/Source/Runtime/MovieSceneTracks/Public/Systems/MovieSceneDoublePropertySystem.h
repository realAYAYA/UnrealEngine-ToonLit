// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Systems/MovieScenePropertySystem.h"
#include "MovieSceneDoublePropertySystem.generated.h"



UCLASS(MinimalAPI)
class UMovieSceneDoublePropertySystem : public UMovieScenePropertySystem
{
	GENERATED_BODY()

	UMovieSceneDoublePropertySystem(const FObjectInitializer& ObjInit);

	MOVIESCENETRACKS_API virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
};
