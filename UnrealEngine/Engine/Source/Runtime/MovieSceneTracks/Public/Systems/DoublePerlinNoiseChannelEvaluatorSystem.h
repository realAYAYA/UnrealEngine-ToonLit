// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/MovieSceneEntitySystem.h"
#include "DoublePerlinNoiseChannelEvaluatorSystem.generated.h"

/**
 * System that is responsible for evaluating double perlin noise channels.
 */
UCLASS()
class MOVIESCENETRACKS_API UDoublePerlinNoiseChannelEvaluatorSystem : public UMovieSceneEntitySystem
{
public:
	GENERATED_BODY()

	UDoublePerlinNoiseChannelEvaluatorSystem(const FObjectInitializer& ObjInit);
	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
};
