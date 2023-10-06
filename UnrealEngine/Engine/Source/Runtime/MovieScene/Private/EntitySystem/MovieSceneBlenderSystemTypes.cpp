// Copyright Epic Games, Inc. All Rights Reserved.

#include "EntitySystem/MovieSceneBlenderSystemTypes.h"
#include "EntitySystem/MovieSceneBlenderSystem.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"

FMovieSceneBlenderSystemID FMovieSceneBlenderSystemID::Invalid;

TSubclassOf<UMovieSceneBlenderSystem> FMovieSceneBlendChannelID::GetSystemClass() const
{
	return UMovieSceneBlenderSystem::GetBlenderSystemClass(SystemID);
}

UMovieSceneBlenderSystem* FMovieSceneBlendChannelID::FindSystem(const UMovieSceneEntitySystemLinker* Linker) const
{
	TSubclassOf<UMovieSceneBlenderSystem> SystemClass = GetSystemClass();
	return CastChecked<UMovieSceneBlenderSystem>(Linker->FindSystem(SystemClass));
}

