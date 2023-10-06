// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Systems/MovieScenePropertySystem.h"
#include "MovieSceneStringPropertySystem.generated.h"



UCLASS(MinimalAPI)
class UMovieSceneStringPropertySystem : public UMovieScenePropertySystem
{
	GENERATED_BODY()

	UMovieSceneStringPropertySystem(const FObjectInitializer& ObjInit);

	MOVIESCENETRACKS_API virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
};
