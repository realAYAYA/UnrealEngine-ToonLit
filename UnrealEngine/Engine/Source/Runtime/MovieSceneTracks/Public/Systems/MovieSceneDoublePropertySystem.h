// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Systems/MovieScenePropertySystem.h"
#include "MovieSceneDoublePropertySystem.generated.h"



UCLASS()
class MOVIESCENETRACKS_API UMovieSceneDoublePropertySystem : public UMovieScenePropertySystem
{
	GENERATED_BODY()

	UMovieSceneDoublePropertySystem(const FObjectInitializer& ObjInit);

	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
};
