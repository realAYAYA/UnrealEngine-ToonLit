// Copyright Epic Games, Inc. All Rights Reserved.

#include "Niagara/Sequencer/MovieSceneNiagaraCacheSection.h"

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

UMovieSceneNiagaraCacheSection::UMovieSceneNiagaraCacheSection()
{
	EvalOptions.CompletionMode = EMovieSceneCompletionMode::RestoreState;
}

#if WITH_EDITOR
void UMovieSceneNiagaraCacheSection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property &&
		PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UMovieSceneNiagaraCacheSection, Params) &&
		PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(FMovieSceneNiagaraCacheParams, SimCache))
	{
		if (TOptional<TRange<FFrameNumber>> AutoSizeRange = GetAutoSizeRange(); AutoSizeRange.IsSet())
		{
			SetRange(AutoSizeRange.GetValue());
		}
		bCacheOutOfDate = false;
	}

	if (PropertyChangedEvent.Property &&
		PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UMovieSceneNiagaraCacheSection, Params) &&
		PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(FMovieSceneNiagaraCacheParams, CacheParameters))
	{
		bCacheOutOfDate = true;
	}
}
#endif

UMovieSceneNiagaraCacheSection::UMovieSceneNiagaraCacheSection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ParamsPtr = &Params;
#if WITH_EDITOR
	InitializePlayRate();
#endif
}




