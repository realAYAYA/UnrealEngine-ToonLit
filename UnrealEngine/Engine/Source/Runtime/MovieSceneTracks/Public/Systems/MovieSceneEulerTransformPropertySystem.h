// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Systems/MovieScenePropertySystem.h"
#include "MovieSceneEulerTransformPropertySystem.generated.h"


UCLASS()
class MOVIESCENETRACKS_API UMovieSceneEulerTransformPropertySystem : public UMovieScenePropertySystem
{
public:

	GENERATED_BODY()

	UMovieSceneEulerTransformPropertySystem(const FObjectInitializer& ObjInit);

	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
};
