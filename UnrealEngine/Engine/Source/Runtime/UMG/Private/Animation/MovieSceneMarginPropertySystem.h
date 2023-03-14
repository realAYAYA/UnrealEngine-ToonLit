// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Layout/Margin.h"
#include "Systems/MovieScenePropertySystem.h"
#include "MovieSceneMarginPropertySystem.generated.h"

Expose_TNameOf(FMargin);

UCLASS()
class UMG_API UMovieSceneMarginPropertySystem : public UMovieScenePropertySystem
{
public:

	GENERATED_BODY()
	
	UMovieSceneMarginPropertySystem(const FObjectInitializer& ObjInit);

	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
};

