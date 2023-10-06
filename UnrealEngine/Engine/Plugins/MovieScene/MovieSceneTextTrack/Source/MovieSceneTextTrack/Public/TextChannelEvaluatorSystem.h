// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/MovieSceneEntitySystem.h"
#include "TextChannelEvaluatorSystem.generated.h"

/** System responsible for evaluating Text channels */
UCLASS(MinimalAPI)
class UTextChannelEvaluatorSystem : public UMovieSceneEntitySystem
{
	GENERATED_BODY()

public:
	UTextChannelEvaluatorSystem(const FObjectInitializer& InObjectInitializer);

	//~ Begin UMovieSceneEntitySystem
	virtual void OnSchedulePersistentTasks(UE::MovieScene::IEntitySystemScheduler* TaskScheduler) override;
	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
	//~ End UMovieSceneEntitySystem
};
