// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/MovieSceneEntitySystem.h"
#include "EntitySystem/MovieSceneBlenderSystem.h"
#include "EntitySystem/MovieSceneDecompositionQuery.h"
#include "EntitySystem/MovieSceneCachedEntityFilterResult.h"
#include "MovieScenePiecewiseIntegerBlenderSystem.generated.h"

namespace UE
{
namespace MovieScene
{

	/** Blend result struct that stores the cumulative sum of pre-weighted values, alongside the total weight */
	struct FIntegerBlendResult
	{
		/** Cumulative sum of blend values pre-multiplied with each value's weight. */
		int32 Total = 0;
		/** Cumulative sum of weights. */
		double Weight = 0.f;
	};

	/** Struct that maintains accumulation buffers for each blend type */
	struct FIntegerAccumulationBuffers
	{
		bool IsEmpty() const;

		void Reset();

		TArray<FIntegerBlendResult> Absolute;
		TArray<FIntegerBlendResult> Relative;
		TArray<FIntegerBlendResult> Additive;
		TArray<FIntegerBlendResult> AdditiveFromBase;
	};

} // namespace MovieScene
} // namespace UE


UCLASS()
class MOVIESCENETRACKS_API UMovieScenePiecewiseIntegerBlenderSystem : public UMovieSceneBlenderSystem
{
public:
	GENERATED_BODY()

	UMovieScenePiecewiseIntegerBlenderSystem(const FObjectInitializer& ObjInit);

	using FMovieSceneEntityID  = UE::MovieScene::FMovieSceneEntityID;
	using FComponentTypeID     = UE::MovieScene::FComponentTypeID;

	virtual void OnLink() override;
	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;

private:

	void ReinitializeAccumulationBuffers();
	void ZeroAccumulationBuffers();

private:

	UE::MovieScene::FIntegerAccumulationBuffers AccumulationBuffers;

	UE::MovieScene::FCachedEntityManagerState ChannelRelevancyCache;
};

