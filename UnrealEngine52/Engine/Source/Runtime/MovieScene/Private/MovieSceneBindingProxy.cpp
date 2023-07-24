// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneBindingProxy.h"
#include "MovieSceneSequence.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneBindingProxy)

UMovieScene* FMovieSceneBindingProxy::GetMovieScene() const
{
	return Sequence ? Sequence->GetMovieScene() : nullptr;
}
