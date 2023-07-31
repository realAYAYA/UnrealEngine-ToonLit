// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieScene/Parameters/MovieSceneNiagaraParameterSectionTemplate.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "MovieSceneNiagaraFloatParameterSectionTemplate.generated.h"

USTRUCT()
struct FMovieSceneNiagaraFloatParameterSectionTemplate : public FMovieSceneNiagaraParameterSectionTemplate
{
	GENERATED_BODY()

public:
	FMovieSceneNiagaraFloatParameterSectionTemplate();

	FMovieSceneNiagaraFloatParameterSectionTemplate(FNiagaraVariable InParameter, const FMovieSceneFloatChannel& InFloatChannel);

private:
	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }

protected:
	virtual void GetAnimatedParameterValue(FFrameTime InTime, const FNiagaraVariableBase& InTargetParameter, const TArray<uint8>& InCurrentValueData, TArray<uint8>& OutAnimatedValueData) const override;

private:
	UPROPERTY()
	FMovieSceneFloatChannel FloatChannel;
};