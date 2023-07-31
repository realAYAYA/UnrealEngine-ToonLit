// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/IMovieScenePropertyComponentHandler.h"
#include "Systems/MovieScenePropertySystem.h"
#include "MovieSceneColorPropertySystem.generated.h"

UCLASS()
class MOVIESCENETRACKS_API UMovieSceneColorPropertySystem : public UMovieScenePropertySystem
{
	GENERATED_BODY()

private:
	UMovieSceneColorPropertySystem(const FObjectInitializer& ObjInit);

	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
};
