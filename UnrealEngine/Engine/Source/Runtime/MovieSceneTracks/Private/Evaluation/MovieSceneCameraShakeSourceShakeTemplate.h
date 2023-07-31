// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Evaluation/PersistentEvaluationData.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Sections/MovieSceneCameraShakeSourceShakeSection.h"
#include "MovieSceneCameraShakeSourceShakeTemplate.generated.h"

USTRUCT()
struct FMovieSceneCameraShakeSourceShakeSectionTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()

	FMovieSceneCameraShakeSourceShakeSectionTemplate();
	FMovieSceneCameraShakeSourceShakeSectionTemplate(const UMovieSceneCameraShakeSourceShakeSection& Section);

private:
	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void SetupOverrides() override;
	virtual void Setup(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const override;
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;
	virtual void TearDown(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const override;

	/** Source data taken from the section */
	UPROPERTY()
	FMovieSceneCameraShakeSectionData SourceData;

	/** Cached section start time */
	UPROPERTY()
	FFrameNumber SectionStartTime;

	/** Cached section end time */
	UPROPERTY()
	FFrameNumber SectionEndTime;
};
