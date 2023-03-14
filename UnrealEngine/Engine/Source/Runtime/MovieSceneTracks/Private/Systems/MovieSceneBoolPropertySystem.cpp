// Copyright Epic Games, Inc. All Rights Reserved.

#include "Systems/MovieSceneBoolPropertySystem.h"
#include "Systems/MovieScenePiecewiseBoolBlenderSystem.h"
#include "MovieSceneTracksComponentTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneBoolPropertySystem)

UMovieSceneBoolPropertySystem::UMovieSceneBoolPropertySystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	BindToProperty(UE::MovieScene::FMovieSceneTracksComponentTypes::Get()->Bool);

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		DefineImplicitPrerequisite(UMovieScenePiecewiseBoolBlenderSystem::StaticClass(), GetClass());

		DefineComponentConsumer(GetClass(), UE::MovieScene::FMovieSceneTracksComponentTypes::Get()->Bool.PropertyTag);
	}
}

void UMovieSceneBoolPropertySystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	Super::OnRun(InPrerequisites, Subsequents);
}


