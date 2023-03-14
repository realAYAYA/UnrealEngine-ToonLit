// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Systems/MovieScenePropertySystem.h"
#include "MovieScene3DTransformPropertySystem.generated.h"


UCLASS()
class MOVIESCENETRACKS_API UMovieScene3DTransformPropertySystem : public UMovieScenePropertySystem
{
public:

	GENERATED_BODY()

	UMovieScene3DTransformPropertySystem(const FObjectInitializer& ObjInit);

	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
};
