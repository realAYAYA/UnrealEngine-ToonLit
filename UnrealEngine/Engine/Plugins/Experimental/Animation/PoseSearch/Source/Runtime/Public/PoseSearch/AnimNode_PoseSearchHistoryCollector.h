// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PoseSearch/PoseSearch.h"
#include "Animation/AnimNodeBase.h"
#include "AnimNode_PoseSearchHistoryCollector.generated.h"

USTRUCT(BlueprintInternalUseOnly)
struct POSESEARCH_API FAnimNode_PoseSearchHistoryCollector : public FAnimNode_Base
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Links)
	FPoseLink Source;

	// The maximum amount of poses that can be stored
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(ClampMin="0"))
	int32 PoseCount = 128;
	
	// The time horizon for how long a pose will be stored in seconds
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(ClampMin="0"))
	float PoseDuration = 4.f;

	// If this node should compute velocities using the playing animation root motion, or 
	// the actual movement of the component in the world. While using root motion can 
	// be more stable, not all input animations support it, and it may produce bad results 
	// if the current movement in the world does not match closely the animation data.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault))
	bool bUseRootMotion = true;

public:
	FAnimNode_PoseSearchHistoryCollector() { }

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context);
	virtual void GatherDebugData(FNodeDebugData& DebugData);
	// End of FAnimNode_Base interface

	UE::PoseSearch::FPoseHistory& GetPoseHistory() { return PoseHistory; }
	const UE::PoseSearch::FPoseHistory& GetPoseHistory() const { return PoseHistory; }

protected:

	UE::PoseSearch::FPoseHistory PoseHistory;
};