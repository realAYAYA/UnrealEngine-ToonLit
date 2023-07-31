// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Scene.h"
#include "Evaluation/PersistentEvaluationData.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Sections/MovieSceneCameraShakeSection.h"
#include "MovieSceneCameraShakeTemplate.generated.h"

class UCameraShakeBase;
struct FMovieSceneContext;

/**
 * Custom logic for playing camera shakes inside sequences.
 *
 * Factory methods for these evaluators are registered on the FMovieSceneCameraShakeEvaluatorRegistry. An evaluator is created for
 * each camera shake running in a sequence.
 */
UCLASS()
class MOVIESCENETRACKS_API UMovieSceneCameraShakeEvaluator : public UObject
{
	GENERATED_BODY()

public:
	/** Called when setting up a camera shake to play inside a sequence */
	virtual bool Setup(const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player, UCameraShakeBase* ShakeInstance) { return false; }
	/** Called just before updating the camera shake playing inside a sequence */
	virtual bool Evaluate(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player, UCameraShakeBase* ShakeInstance) { return false; }
};

DECLARE_DELEGATE_RetVal_OneParam(UMovieSceneCameraShakeEvaluator*, FMovieSceneBuildShakeEvaluator, UCameraShakeBase*);

/** Registry for factories of shake evaluators */
struct MOVIESCENETRACKS_API FMovieSceneCameraShakeEvaluatorRegistry
{
public:
	/** Registers a new shake evaluator factory */
	static void RegisterShakeEvaluatorBuilder(const FMovieSceneBuildShakeEvaluator& Builder)
	{
		ShakeEvaluatorBuilders.Add(Builder);
	}

	/**
	 * Builds a shake evaluator for the given shake.
	 *
	 * The first factory method that returns a valid pointer wins.
	 * Most shake types wouldn't have any custom shake evaluator, in which case this
	 * method returns a null pointer.
	 */
	static UMovieSceneCameraShakeEvaluator* BuildShakeEvaluator(UCameraShakeBase* ShakeInstance)
	{
		for (FMovieSceneBuildShakeEvaluator& Builder : ShakeEvaluatorBuilders)
		{
			if (UMovieSceneCameraShakeEvaluator* Evaluator = Builder.Execute(ShakeInstance))
			{
				return Evaluator;
			}
		}
		return nullptr;
	}

private:
	/** List of registered factory methods */
	static TArray<FMovieSceneBuildShakeEvaluator> ShakeEvaluatorBuilders;
};

/** Section template for a camera anim */
USTRUCT()
struct FMovieSceneCameraShakeSectionTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()

	FMovieSceneCameraShakeSectionTemplate();
	FMovieSceneCameraShakeSectionTemplate(const UMovieSceneCameraShakeSection& Section);

private:
	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Initialize(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const override;
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

	/** Source data taken from the section */
	UPROPERTY()
	FMovieSceneCameraShakeSectionData SourceData;

	/** Cached section start time */
	UPROPERTY()
	FFrameNumber SectionStartTime;
};

