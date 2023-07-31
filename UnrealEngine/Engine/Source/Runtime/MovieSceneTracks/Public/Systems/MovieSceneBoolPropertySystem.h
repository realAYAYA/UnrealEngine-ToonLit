// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Systems/MovieScenePropertySystem.h"
#include "MovieSceneBoolPropertySystem.generated.h"



UCLASS()
class MOVIESCENETRACKS_API UMovieSceneBoolPropertySystem : public UMovieScenePropertySystem
{
	GENERATED_BODY()

	UMovieSceneBoolPropertySystem(const FObjectInitializer& ObjInit);

	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
};

