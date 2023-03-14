// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Channels/MovieSceneObjectPathChannel.h"
#include "Evaluation/MovieScenePropertyTemplate.h"
#include "MovieSceneObjectPropertyTemplate.generated.h"

class UMovieSceneObjectPropertyTrack;
class UMovieSceneObjectPropertySection;

USTRUCT()
struct FMovieSceneObjectPropertyTemplate : public FMovieScenePropertySectionTemplate
{
	GENERATED_BODY()

	FMovieSceneObjectPropertyTemplate() {}
	FMovieSceneObjectPropertyTemplate(const UMovieSceneObjectPropertySection& Section, const UMovieSceneObjectPropertyTrack& Track);

private:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void SetupOverrides() override;
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

	UPROPERTY()
	FMovieSceneObjectPathChannel ObjectChannel;
};
