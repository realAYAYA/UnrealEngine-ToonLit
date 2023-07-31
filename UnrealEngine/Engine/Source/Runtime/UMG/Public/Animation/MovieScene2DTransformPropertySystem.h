// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Systems/MovieScenePropertySystem.h"
#include "MovieScene2DTransformPropertySystem.generated.h"

UCLASS()
class UMG_API UMovieScene2DTransformPropertySystem : public UMovieScenePropertySystem
{
public:

	GENERATED_BODY()

	UMovieScene2DTransformPropertySystem(const FObjectInitializer& ObjInit);

	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
};
