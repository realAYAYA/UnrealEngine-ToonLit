// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EntitySystem/MovieSceneEntityIDs.h"
#include "EntitySystem/MovieSceneSequenceInstanceHandle.h"
#include "MovieSceneObjectBindingID.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/MovieSceneCachedEntityFilterResult.h"
#include "EntitySystem/MovieSceneEntityInstantiatorSystem.h"
#include "EntitySystem/MovieSceneEntitySystem.h"
#include "EntitySystem/MovieScenePropertyBinding.h"
#include "Evaluation/MovieSceneEvaluationOperand.h"
#include "UObject/ObjectMacros.h"
#include "Sections/TemplateSequenceSection.h"
#include "TemplateSequenceSystem.generated.h"

UCLASS(MinimalAPI)
class UTemplateSequenceSystem : public UMovieSceneEntitySystem
{
public:
	GENERATED_BODY()

	UTemplateSequenceSystem(const FObjectInitializer& ObjInit);

private:
	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override final;

private:
	UE::MovieScene::FCachedEntityFilterResult_Match ApplicableFilter;
};

UCLASS(MinimalAPI)
class UTemplateSequencePropertyScalingInstantiatorSystem : public UMovieSceneEntitySystem
{
public:

	GENERATED_BODY()

	UTemplateSequencePropertyScalingInstantiatorSystem(const FObjectInitializer& ObjInit);

	bool HasAnyFloatScales() const { return FloatScaleUseCount > 0; }
	bool HasAnyTransformScales() const { return TransformScaleUseCount > 0; }

private:

	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;

private:
	
	using FPropertyScaleEntityIDs = TArray<UE::MovieScene::FMovieSceneEntityID, TInlineAllocator<2>>;
	TMap<UE::MovieScene::FInstanceHandle, FPropertyScaleEntityIDs> PropertyScaledInstances;

	int32 FloatScaleUseCount = 0;
	int32 TransformScaleUseCount = 0;
};

UCLASS(MinimalAPI)
class UTemplateSequencePropertyScalingEvaluatorSystem : public UMovieSceneEntitySystem
{
public:

	GENERATED_BODY()

	UTemplateSequencePropertyScalingEvaluatorSystem(const FObjectInitializer& ObjInit);

	using FPropertyScaleKey = TTuple<UE::MovieScene::FInstanceHandle, FGuid, FName>;
	using FPropertyScaleValue = TTuple<ETemplateSectionPropertyScaleType, float>;

	using FPropertyScaleValueArray = TArray<FPropertyScaleValue, TInlineAllocator<2>>;

	void AddPropertyScale(const FPropertyScaleKey& Key, const FPropertyScaleValue& Value);
	void FindPropertyScales(const FPropertyScaleKey& Key, FPropertyScaleValueArray& OutValues) const;

private:

	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override final;
	
private:

	struct FMultiPropertyScaleValue
	{
		FPropertyScaleValueArray Values;
	};

	TMap<FPropertyScaleKey, FMultiPropertyScaleValue> PropertyScales;
};

