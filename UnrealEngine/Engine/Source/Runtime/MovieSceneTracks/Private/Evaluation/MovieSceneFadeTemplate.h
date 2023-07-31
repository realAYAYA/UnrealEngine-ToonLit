// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Evaluation/MovieSceneTrackImplementation.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Sections/MovieSceneFadeSection.h"

#include "MovieSceneFadeTemplate.generated.h"

USTRUCT()
struct FMovieSceneFadeSectionTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()
	
	FMovieSceneFadeSectionTemplate() : FadeColor(ForceInitToZero), bFadeAudio(false) {}
	FMovieSceneFadeSectionTemplate(const UMovieSceneFadeSection& Section);

private:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

	UPROPERTY()
	FMovieSceneFloatChannel FadeCurve;

	UPROPERTY()
	FLinearColor FadeColor;

	UPROPERTY()
	uint32 bFadeAudio:1;
};
