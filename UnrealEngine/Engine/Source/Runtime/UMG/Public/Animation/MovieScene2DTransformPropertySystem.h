// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Systems/MovieScenePropertySystem.h"
#include "MovieScene2DTransformPropertySystem.generated.h"

UCLASS(MinimalAPI)
class UMovieScene2DTransformPropertySystem : public UMovieScenePropertySystem
{
public:

	GENERATED_BODY()

	UMG_API UMovieScene2DTransformPropertySystem(const FObjectInitializer& ObjInit);

	UMG_API virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
};
