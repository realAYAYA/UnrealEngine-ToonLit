// Copyright Epic Games, Inc. All Rights Reserved.

#include "Systems/MovieSceneStringPropertySystem.h"
#include "Systems/StringChannelEvaluatorSystem.h"
#include "MovieSceneTracksComponentTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneStringPropertySystem)


UMovieSceneStringPropertySystem::UMovieSceneStringPropertySystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	BindToProperty(UE::MovieScene::FMovieSceneTracksComponentTypes::Get()->String);

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		DefineImplicitPrerequisite(UStringChannelEvaluatorSystem::StaticClass(), GetClass());
	}
}

void UMovieSceneStringPropertySystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	Super::OnRun(InPrerequisites, Subsequents);
}

