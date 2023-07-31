// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimTypes.h"
#include "Animation/AnimCurveTypes.h"
#include "BonePose.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimNodeMessages.h"
#include "AnimNode_SaveCachedPose.generated.h"

namespace UE { namespace Anim {

// Event that can be subscribed to receive skipped updates when a cached pose is run.
// When a cached pose update call executes the link with the maximum weight, this event receives information about
// the other links with lesser weights
class ENGINE_API FCachedPoseSkippedUpdateHandler : public IGraphMessage
{
	DECLARE_ANIMGRAPH_MESSAGE(FCachedPoseSkippedUpdateHandler);

public:
	FCachedPoseSkippedUpdateHandler(TUniqueFunction<void(TArrayView<const FMessageStack>)> InFunction)
		: Function(MoveTemp(InFunction))
	{}

	// Called when there are Update() calls that were skipped due to pose caching. 
	void OnUpdatesSkipped(TArrayView<const FMessageStack> InSkippedUpdates) { Function(InSkippedUpdates); }

private:
	// Function to call
	TUniqueFunction<void(TArrayView<const FMessageStack>)> Function;
};

}}	// namespace UE::Anim

USTRUCT(BlueprintInternalUseOnly)
struct ENGINE_API FAnimNode_SaveCachedPose : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Links)
	FPoseLink Pose;

	/** Intentionally not exposed, set by AnimBlueprintCompiler */
	UPROPERTY()
	FName CachePoseName;

	float GlobalWeight;

protected:
	FCompactPose CachedPose;
	FBlendedCurve CachedCurve;
	UE::Anim::FStackAttributeContainer CachedAttributes;

	struct FCachedUpdateContext
	{
		FAnimationUpdateContext Context;
		TSharedPtr<FAnimationUpdateSharedContext> SharedContext;
	};

	TArray<FCachedUpdateContext> CachedUpdateContexts;

	FGraphTraversalCounter InitializationCounter;
	FGraphTraversalCounter CachedBonesCounter;
	FGraphTraversalCounter UpdateCounter;
	FGraphTraversalCounter EvaluationCounter;

public:	
	FAnimNode_SaveCachedPose();

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

	void PostGraphUpdate();
};
