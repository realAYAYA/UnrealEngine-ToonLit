// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Evaluation/MovieSceneAnimTypeID.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Sections/MovieSceneCameraShakeSourceTriggerSection.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneCameraShakeSourceTriggerTemplate.generated.h"

class UMovieSceneCameraShakeSourceTriggerSection;

USTRUCT()
struct MOVIESCENETRACKS_API FMovieSceneCameraShakeSourceTriggerSectionTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()
	
	FMovieSceneCameraShakeSourceTriggerSectionTemplate() {}
	FMovieSceneCameraShakeSourceTriggerSectionTemplate(const UMovieSceneCameraShakeSourceTriggerSection& Section);

	static FMovieSceneAnimTypeID GetAnimTypeID();
	
private:
	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void SetupOverrides() override;
	virtual void Setup(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const override;
	virtual void EvaluateSwept(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const TRange<FFrameNumber>& SweptRange, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;
	virtual void TearDown(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const override;

private:
	UPROPERTY()
	TArray<FFrameNumber> TriggerTimes;

	UPROPERTY()
	TArray<FMovieSceneCameraShakeSourceTrigger> TriggerValues;
};

