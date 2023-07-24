// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/SortedMap.h"
#include "Containers/SparseArray.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/EntityAllocationIterator.h"
#include "EntitySystem/MovieSceneEntityIDs.h"
#include "EntitySystem/MovieSceneEntityInstantiatorSystem.h"
#include "EntitySystem/MovieSceneEntitySystem.h"
#include "EntitySystem/MovieSceneSequenceInstanceHandle.h"
#include "HAL/Platform.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "WeightAndEasingEvaluatorSystem.generated.h"

class UMovieSceneEntitySystemLinker;
class UMovieSceneEvalTimeSystem;
class UWeightAndEasingEvaluatorSystem;
class UObject;
struct FMovieSceneSubSequenceData;

namespace UE::MovieScene
{

struct FEasingMutationBase;
struct FAddEasingChannelToProviderMutation;
struct FAddEasingChannelToConsumerMutation;

/** Computation data used for accumulating hierarchical weights for sub sequences */
struct FHierarchicalEasingChannelData
{
	/** Our parent's computation data within PreAllocatedComputationData. Must only access if bResultsNeedResort is false. */
	uint16 ParentChannel = uint16(-1);
	/** The final result of this easing channel, accumulated with all parents */
	double FinalResult = 1.0;
};

}  // namespace UE::MovieScene

/**
 * System that creates hierarchical easing channels for any newly introduced HierarchicalEasingProvider components
 */
UCLASS()
class MOVIESCENETRACKS_API UMovieSceneHierarchicalEasingInstantiatorSystem : public UMovieSceneEntityInstantiatorSystem
{
public:

	struct FHierarchicalInstanceData
	{
		int32 HierarchicalDepth = -1;
		uint16 StrongRefCount = 0;
		uint16 WeakRefCount = 0;
		uint16 EasingChannelID = uint16(-1);
	};

	GENERATED_BODY()

	UMovieSceneHierarchicalEasingInstantiatorSystem(const FObjectInitializer& ObjInit);

private:

	virtual bool IsRelevantImpl(UMovieSceneEntitySystemLinker* InLinker) const;
	virtual void OnLink() override;
	virtual void OnUnlink() override;
	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;

private:

	/** Map between a sub-sequence handle and the easing channel affecting it */
	TSparseArray<uint16> CachedInstanceIDToChannel;

	UPROPERTY()
	TObjectPtr<UWeightAndEasingEvaluatorSystem> EvaluatorSystem;
};

/**
 * System that combines manual weights and easings and propagates them to entities with matching EasingChannelID components
 */
UCLASS()
class MOVIESCENETRACKS_API UWeightAndEasingEvaluatorSystem : public UMovieSceneEntitySystem
{
public:

	GENERATED_BODY()

	UWeightAndEasingEvaluatorSystem(const FObjectInitializer& ObjInit);

	/**
	 * Called from the instantiator system once channel IDs have been calculated
	 */
	void SetComputationBuffer(TArray<UE::MovieScene::FHierarchicalEasingChannelData>&& PreAllocatedComputationData);

private:

	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
	virtual bool IsRelevantImpl(UMovieSceneEntitySystemLinker* InLinker) const override;
	virtual void OnLink() override final;
	virtual void OnUnlink() override final;

private:

	/** Unstable array of preallocated storage for computing easing results sorted by hierarchical depth. */
	TArray<UE::MovieScene::FHierarchicalEasingChannelData> PreAllocatedComputationData;
};

