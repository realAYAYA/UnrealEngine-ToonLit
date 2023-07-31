// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundAttenuation.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "MovieSceneAudioTemplate.generated.h"

class UAudioComponent;
class UMovieSceneAudioSection;
class USoundBase;

USTRUCT()
struct FMovieSceneAudioSectionTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()

	FMovieSceneAudioSectionTemplate();
	FMovieSceneAudioSectionTemplate(const UMovieSceneAudioSection& Section);

	UPROPERTY()
	TObjectPtr<const UMovieSceneAudioSection> AudioSection;

private:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;
};
