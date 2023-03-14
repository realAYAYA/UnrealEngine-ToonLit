// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Evaluation/MovieSceneBaseCacheTemplate.h"
#include "Niagara/Sequencer/MovieSceneNiagaraCacheSection.h"
#include "MovieSceneNiagaraCacheTemplate.generated.h"

USTRUCT()
struct FMovieSceneNiagaraCacheSectionTemplateParameters : public FMovieSceneBaseCacheSectionTemplateParameters
{
	GENERATED_BODY()

	FMovieSceneNiagaraCacheSectionTemplateParameters() {}

	FMovieSceneNiagaraCacheSectionTemplateParameters(const FMovieSceneNiagaraCacheParams& InNiagaraCacheParams, FFrameNumber InSectionStartTime, FFrameNumber InSectionEndTime)
		: FMovieSceneBaseCacheSectionTemplateParameters(InSectionStartTime, InSectionEndTime), NiagaraCacheParams(InNiagaraCacheParams)
 
	{}

	UPROPERTY()
	FMovieSceneNiagaraCacheParams NiagaraCacheParams;
};

USTRUCT()
struct FMovieSceneNiagaraCacheSectionTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()

	FMovieSceneNiagaraCacheSectionTemplate() {}
	FMovieSceneNiagaraCacheSectionTemplate(const UMovieSceneNiagaraCacheSection& Section);

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

	UPROPERTY()
	FMovieSceneNiagaraCacheSectionTemplateParameters Params;
};
