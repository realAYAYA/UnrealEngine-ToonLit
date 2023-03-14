// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieScene/MovieSceneNiagaraSystemSpawnSection.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneNiagaraSystemSpawnSection)

UMovieSceneNiagaraSystemSpawnSection::UMovieSceneNiagaraSystemSpawnSection()
{
	EvalOptions.CompletionMode = EMovieSceneCompletionMode::RestoreState;
	SectionStartBehavior = ENiagaraSystemSpawnSectionStartBehavior::Activate;
	SectionEvaluateBehavior = ENiagaraSystemSpawnSectionEvaluateBehavior::ActivateIfInactive;
	SectionEndBehavior = ENiagaraSystemSpawnSectionEndBehavior::SetSystemInactive;
	AgeUpdateMode = ENiagaraAgeUpdateMode::TickDeltaTime;
	bAllowScalability = false;
}

ENiagaraSystemSpawnSectionStartBehavior UMovieSceneNiagaraSystemSpawnSection::GetSectionStartBehavior() const
{
	return SectionStartBehavior;
}

ENiagaraSystemSpawnSectionEvaluateBehavior UMovieSceneNiagaraSystemSpawnSection::GetSectionEvaluateBehavior() const
{
	return SectionEvaluateBehavior;
}

ENiagaraSystemSpawnSectionEndBehavior UMovieSceneNiagaraSystemSpawnSection::GetSectionEndBehavior() const
{
	return SectionEndBehavior;
}

ENiagaraAgeUpdateMode UMovieSceneNiagaraSystemSpawnSection::GetAgeUpdateMode() const
{
	return AgeUpdateMode;
}

bool UMovieSceneNiagaraSystemSpawnSection::GetAllowScalability()const
{
	return bAllowScalability;
}
