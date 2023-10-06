// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/MovieSceneEntityInstantiatorSystem.h"
#include "MovieSceneInitialValueSystem.generated.h"


/**
 * System responsible for initializing initial values for all property types
 * Will handle the presence of an FInitialValueCache extension on the linker
 */
UCLASS(MinimalAPI)
class UMovieSceneInitialValueSystem
	: public UMovieSceneEntityInstantiatorSystem
{
public:

	GENERATED_BODY()

	MOVIESCENETRACKS_API UMovieSceneInitialValueSystem(const FObjectInitializer& ObjInit);

private:

	MOVIESCENETRACKS_API virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
	MOVIESCENETRACKS_API virtual bool IsRelevantImpl(UMovieSceneEntitySystemLinker* InLinker) const override;
	MOVIESCENETRACKS_API virtual void OnLink() override;
	MOVIESCENETRACKS_API virtual void OnUnlink() override;
};
