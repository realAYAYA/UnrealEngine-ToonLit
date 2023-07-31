// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Systems/MovieScenePropertySystem.h"
#include "MovieSceneEnumPropertySystem.generated.h"

UCLASS()
class MOVIESCENETRACKS_API UMovieSceneEnumPropertySystem : public UMovieScenePropertySystem
{
	GENERATED_BODY()

	UMovieSceneEnumPropertySystem(const FObjectInitializer& ObjInit);

	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
};
