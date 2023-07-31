// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Systems/MovieScenePropertySystem.h"
#include "MovieSceneBytePropertySystem.generated.h"

UCLASS()
class MOVIESCENETRACKS_API UMovieSceneBytePropertySystem : public UMovieScenePropertySystem
{
	GENERATED_BODY()

	UMovieSceneBytePropertySystem(const FObjectInitializer& ObjInit);

	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
};
