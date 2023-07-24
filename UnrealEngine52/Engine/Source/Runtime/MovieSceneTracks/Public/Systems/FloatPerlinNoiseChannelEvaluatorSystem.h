// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/MovieSceneEntitySystem.h"
#include "FloatPerlinNoiseChannelEvaluatorSystem.generated.h"

/**
 * System that is responsible for evaluating double perlin noise channels.
 */
UCLASS()
class MOVIESCENETRACKS_API UFloatPerlinNoiseChannelEvaluatorSystem : public UMovieSceneEntitySystem
{
public:
	GENERATED_BODY()

	UFloatPerlinNoiseChannelEvaluatorSystem(const FObjectInitializer& ObjInit);
	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
};
