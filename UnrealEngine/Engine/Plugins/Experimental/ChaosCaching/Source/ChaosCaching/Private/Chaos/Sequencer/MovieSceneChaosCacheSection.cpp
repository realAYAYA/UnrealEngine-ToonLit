// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/Sequencer/MovieSceneChaosCacheSection.h"
#include "Chaos/CacheCollection.h"
#include "MovieScene.h"

FMovieSceneChaosCacheParams::FMovieSceneChaosCacheParams() : FMovieSceneBaseCacheParams()
{
	CacheCollection = nullptr;
}

float FMovieSceneChaosCacheParams::GetSequenceLength() const
{
	if(CacheCollection)
	{
		return CacheCollection->GetMaxDuration();
	}
	return 0.f;
}

UMovieSceneChaosCacheSection::UMovieSceneChaosCacheSection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ParamsPtr = &Params;
#if WITH_EDITOR
	InitializePlayRate();
#endif
}




