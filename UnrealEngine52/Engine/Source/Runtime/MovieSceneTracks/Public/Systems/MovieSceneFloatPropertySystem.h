// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Systems/MovieScenePropertySystem.h"
#include "MovieSceneFloatPropertySystem.generated.h"



UCLASS()
class MOVIESCENETRACKS_API UMovieSceneFloatPropertySystem : public UMovieScenePropertySystem
{
	GENERATED_BODY()

	UMovieSceneFloatPropertySystem(const FObjectInitializer& ObjInit);

	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
};
