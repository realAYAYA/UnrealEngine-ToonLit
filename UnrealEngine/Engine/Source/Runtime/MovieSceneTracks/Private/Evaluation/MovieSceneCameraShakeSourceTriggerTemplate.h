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
struct FMovieSceneCameraShakeSourceTriggerSectionTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()
	
	FMovieSceneCameraShakeSourceTriggerSectionTemplate() {}
	MOVIESCENETRACKS_API FMovieSceneCameraShakeSourceTriggerSectionTemplate(const UMovieSceneCameraShakeSourceTriggerSection& Section);

	static MOVIESCENETRACKS_API FMovieSceneAnimTypeID GetAnimTypeID();
	
private:
	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	MOVIESCENETRACKS_API virtual void SetupOverrides() override;
	MOVIESCENETRACKS_API virtual void Setup(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const override;
	MOVIESCENETRACKS_API virtual void EvaluateSwept(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const TRange<FFrameNumber>& SweptRange, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;
	MOVIESCENETRACKS_API virtual void TearDown(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const override;

private:
	UPROPERTY()
	TArray<FFrameNumber> TriggerTimes;

	UPROPERTY()
	TArray<FMovieSceneCameraShakeSourceTrigger> TriggerValues;
};

