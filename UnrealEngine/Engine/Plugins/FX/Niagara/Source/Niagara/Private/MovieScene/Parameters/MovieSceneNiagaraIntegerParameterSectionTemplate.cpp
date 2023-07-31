// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneNiagaraIntegerParameterSectionTemplate.h"
#include "NiagaraComponent.h"
#include "NiagaraTypes.h"
#include "IMovieScenePlayer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneNiagaraIntegerParameterSectionTemplate)

FMovieSceneNiagaraIntegerParameterSectionTemplate::FMovieSceneNiagaraIntegerParameterSectionTemplate()
{
}

FMovieSceneNiagaraIntegerParameterSectionTemplate::FMovieSceneNiagaraIntegerParameterSectionTemplate(FNiagaraVariable InParameter, const FMovieSceneIntegerChannel& InIntegerChannel)
	: FMovieSceneNiagaraParameterSectionTemplate(InParameter)
	, IntegerChannel(InIntegerChannel)
{
}

void FMovieSceneNiagaraIntegerParameterSectionTemplate::GetAnimatedParameterValue(FFrameTime InTime, const FNiagaraVariableBase& InTargetParameter, const TArray<uint8>& InCurrentValueData, TArray<uint8>& OutAnimatedValueData) const
{
	FNiagaraInt32 const* CurrentValue = (FNiagaraInt32 const*)InCurrentValueData.GetData();
	FNiagaraInt32 AnimatedValue = *CurrentValue;

	IntegerChannel.Evaluate(InTime, AnimatedValue.Value);

	OutAnimatedValueData.AddUninitialized(sizeof(FNiagaraInt32));
	FMemory::Memcpy(OutAnimatedValueData.GetData(), (uint8*)&AnimatedValue, sizeof(FNiagaraInt32));
}
