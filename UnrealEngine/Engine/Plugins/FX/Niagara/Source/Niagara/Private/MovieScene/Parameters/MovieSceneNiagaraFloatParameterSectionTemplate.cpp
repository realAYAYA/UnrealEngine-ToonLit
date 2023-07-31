// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneNiagaraFloatParameterSectionTemplate.h"
#include "NiagaraComponent.h"
#include "NiagaraTypes.h"
#include "IMovieScenePlayer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneNiagaraFloatParameterSectionTemplate)

FMovieSceneNiagaraFloatParameterSectionTemplate::FMovieSceneNiagaraFloatParameterSectionTemplate()
{
}

FMovieSceneNiagaraFloatParameterSectionTemplate::FMovieSceneNiagaraFloatParameterSectionTemplate(FNiagaraVariable InParameter, const FMovieSceneFloatChannel& InFloatChannel)
	: FMovieSceneNiagaraParameterSectionTemplate(InParameter)
	, FloatChannel(InFloatChannel)
{
}

void FMovieSceneNiagaraFloatParameterSectionTemplate::GetAnimatedParameterValue(FFrameTime InTime, const FNiagaraVariableBase& InTargetParameter, const TArray<uint8>& InCurrentValueData, TArray<uint8>& OutAnimatedValueData) const
{
	FNiagaraFloat const* CurrentValue = (FNiagaraFloat const*)InCurrentValueData.GetData();
	FNiagaraFloat AnimatedValue = *CurrentValue;

	FloatChannel.Evaluate(InTime, AnimatedValue.Value);

	OutAnimatedValueData.AddUninitialized(sizeof(FNiagaraFloat));
	FMemory::Memcpy(OutAnimatedValueData.GetData(), (uint8*)&AnimatedValue, sizeof(FNiagaraFloat));
}
