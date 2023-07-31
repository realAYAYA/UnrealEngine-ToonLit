// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Systems/MovieScenePropertySystem.h"
#include "MovieSceneIntegerPropertySystem.generated.h"

UCLASS()
class MOVIESCENETRACKS_API UMovieSceneIntegerPropertySystem : public UMovieScenePropertySystem
{
	GENERATED_BODY()

	UMovieSceneIntegerPropertySystem(const FObjectInitializer& ObjInit);

	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
};
