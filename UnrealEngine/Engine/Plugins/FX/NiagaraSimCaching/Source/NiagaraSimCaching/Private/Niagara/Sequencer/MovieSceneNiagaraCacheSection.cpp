// Copyright Epic Games, Inc. All Rights Reserved.

#include "Niagara/Sequencer/MovieSceneNiagaraCacheSection.h"
#include "MovieScene.h"

FMovieSceneNiagaraCacheParams::FMovieSceneNiagaraCacheParams() : FMovieSceneBaseCacheParams()
{
	SimCache = nullptr;
}

float FMovieSceneNiagaraCacheParams::GetSequenceLength() const
{
	if (SimCache)
	{
		return SimCache->GetDurationSeconds();
	}
	return 0.f;
}

UMovieSceneNiagaraCacheSection::UMovieSceneNiagaraCacheSection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ParamsPtr = &Params;
#if WITH_EDITOR
	InitializePlayRate();
#endif
}




