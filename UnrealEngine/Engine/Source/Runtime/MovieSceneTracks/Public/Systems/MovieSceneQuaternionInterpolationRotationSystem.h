// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/MovieSceneEntitySystem.h"
#include "MovieSceneQuaternionInterpolationRotationSystem.generated.h"


UCLASS()
class MOVIESCENETRACKS_API UMovieSceneQuaternionInterpolationRotationSystem : public UMovieSceneEntitySystem
{
public:

	GENERATED_BODY()

	UMovieSceneQuaternionInterpolationRotationSystem(const FObjectInitializer& ObjInit);

private:
	virtual bool IsRelevantImpl(UMovieSceneEntitySystemLinker* InLinker) const override;
	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
};

