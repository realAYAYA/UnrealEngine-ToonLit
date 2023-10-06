// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTextPropertySystem.h"
#include "TextChannelEvaluatorSystem.h"
#include "TextComponentTypes.h"

UMovieSceneTextPropertySystem::UMovieSceneTextPropertySystem(const FObjectInitializer& InObjectInitializer)
	: Super(InObjectInitializer)
{
	BindToProperty(UE::MovieScene::FTextComponentTypes::Get()->Text);
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		DefineImplicitPrerequisite(UTextChannelEvaluatorSystem::StaticClass(), GetClass());
	}
}

void UMovieSceneTextPropertySystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	Super::OnRun(InPrerequisites, Subsequents);
}
