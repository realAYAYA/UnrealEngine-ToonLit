// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/MovieSceneEntitySystem.h"
#include "DoublePerlinNoiseChannelEvaluatorSystem.generated.h"

/**
 * System that is responsible for evaluating double perlin noise channels.
 */
UCLASS(MinimalAPI)
class UDoublePerlinNoiseChannelEvaluatorSystem : public UMovieSceneEntitySystem
{
public:
	GENERATED_BODY()

	MOVIESCENETRACKS_API UDoublePerlinNoiseChannelEvaluatorSystem(const FObjectInitializer& ObjInit);
	MOVIESCENETRACKS_API virtual void OnSchedulePersistentTasks(UE::MovieScene::IEntitySystemScheduler* TaskScheduler) override;
	MOVIESCENETRACKS_API virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
};
