// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Layout/Margin.h"
#include "Systems/MovieScenePropertySystem.h"
#include "MovieSceneMarginPropertySystem.generated.h"

Expose_TNameOf(FMargin);

UCLASS(MinimalAPI)
class UMovieSceneMarginPropertySystem : public UMovieScenePropertySystem
{
public:

	GENERATED_BODY()
	
	UMG_API UMovieSceneMarginPropertySystem(const FObjectInitializer& ObjInit);

	UMG_API virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
};

