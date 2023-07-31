// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Evaluation/MovieSceneBaseCacheTemplate.h"
#include "Chaos/Sequencer/MovieSceneChaosCacheSection.h"
#include "MovieSceneChaosCacheTemplate.generated.h"

USTRUCT()
struct FMovieSceneChaosCacheSectionTemplateParameters : public FMovieSceneBaseCacheSectionTemplateParameters
{
	GENERATED_BODY()

	FMovieSceneChaosCacheSectionTemplateParameters() {}

	FMovieSceneChaosCacheSectionTemplateParameters(const FMovieSceneChaosCacheParams& InChaosCacheParams, FFrameNumber InSectionStartTime, FFrameNumber InSectionEndTime)
		: FMovieSceneBaseCacheSectionTemplateParameters(InSectionStartTime, InSectionEndTime), ChaosCacheParams(InChaosCacheParams)
 
	{}

	UPROPERTY()
	FMovieSceneChaosCacheParams ChaosCacheParams;
};

USTRUCT()
struct FMovieSceneChaosCacheSectionTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()

	FMovieSceneChaosCacheSectionTemplate() {}
	FMovieSceneChaosCacheSectionTemplate(const UMovieSceneChaosCacheSection& Section);

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

	UPROPERTY()
	FMovieSceneChaosCacheSectionTemplateParameters Params;
};
