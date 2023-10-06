// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Systems/MovieScenePropertySystem.h"
#include "MovieSceneTextPropertySystem.generated.h"

UCLASS(MinimalAPI)
class UMovieSceneTextPropertySystem : public UMovieScenePropertySystem
{
	GENERATED_BODY()

public:
	UMovieSceneTextPropertySystem(const FObjectInitializer& InObjectInitializer);

	//~ Begin UMovieSceneEntitySystem
	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
	//~ End UMovieSceneEntitySystem
};
