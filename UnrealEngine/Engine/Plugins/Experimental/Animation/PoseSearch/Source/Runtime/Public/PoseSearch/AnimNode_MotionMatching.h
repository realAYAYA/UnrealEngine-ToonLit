// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimNode_AssetPlayerBase.h"
#include "Animation/MotionTrajectoryTypes.h"
#include "PoseSearch/AnimNode_BlendStack.h"
#include "PoseSearch/PoseSearch.h"
#include "PoseSearch/PoseSearchLibrary.h"

#include "AnimNode_MotionMatching.generated.h"

USTRUCT(BlueprintInternalUseOnly)
struct POSESEARCH_API FAnimNode_MotionMatching : public FAnimNode_AssetPlayerBase
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPoseLink Source;

	// Collection of animations for motion matching
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(PinShownByDefault))
	TObjectPtr<const UPoseSearchSearchableAsset> Searchable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinShownByDefault))
	FGameplayTagContainer ActiveTagsContainer;

	// Motion trajectory samples for pose search queries
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(PinShownByDefault))
	FTrajectorySampleRange Trajectory;

	// Settings for the core motion matching algorithm evaluation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(PinHiddenByDefault))
	FMotionMatchingSettings Settings;

	// Reset the motion matching state if we have become relevant to the graph
	// after not being ticked on the previous frame(s)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault))
	bool bResetOnBecomingRelevant = true;

	// if true the continuing pose will be invalidated
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault))
	bool bForceInterrupt = false;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category=Debug, meta = (PinShownByDefault))
	bool bDebugDraw = false;

	UPROPERTY(EditAnywhere, Category=Debug, meta = (PinShownByDefault))
	bool bDebugDrawQuery = true;

	UPROPERTY(EditAnywhere, Category=Debug, meta = (PinShownByDefault))
	bool bDebugDrawMatch = true;
#endif

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual bool HasPreUpdate() const override;
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;
	// End of FAnimNode_Base interface

private:

	FAnimNode_BlendStack BlendStackNode;

	// Encapsulated motion matching algorithm and internal state
	FMotionMatchingState MotionMatchingState;

	// Update Counter for detecting being relevant
	FGraphTraversalCounter UpdateCounter;

	// FAnimNode_AssetPlayerBase
protected:
	// FAnimNode_AssetPlayerBase interface
	virtual float GetAccumulatedTime() const override;
	virtual UAnimationAsset* GetAnimAsset() const override;
	virtual void UpdateAssetPlayer(const FAnimationUpdateContext& Context) override;
	virtual float GetCurrentAssetLength() const override;
	virtual float GetCurrentAssetTime() const override;
	virtual float GetCurrentAssetTimePlayRateAdjusted() const override;
	virtual bool GetIgnoreForRelevancyTest() const override;
	virtual bool SetIgnoreForRelevancyTest(bool bInIgnoreForRelevancyTest) override;
	// End of FAnimNode_AssetPlayerBase interface

private:

#if WITH_EDITORONLY_DATA
	// If true, "Relevant anim" nodes that look for the highest weighted animation in a state will ignore this node
	UPROPERTY(EditAnywhere, Category=Relevancy, meta=(FoldProperty, PinHiddenByDefault))
	bool bIgnoreForRelevancyTest = false;

	// Whether this node was evaluated last frame
	bool bWasEvaluated = false;
#endif // WITH_EDITORONLY_DATA
};