// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/MovieSceneEntitySystem.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "MovieSceneBaseValueEvaluatorSystem.generated.h"

class UObject;

/**
 * System that is responsible for evaluating base values, for "additive from base" blending.
 */
UCLASS()
class MOVIESCENETRACKS_API UMovieSceneBaseValueEvaluatorSystem : public UMovieSceneEntitySystem
{
public:

	GENERATED_BODY()

	UMovieSceneBaseValueEvaluatorSystem(const FObjectInitializer& ObjInit);

	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
};

