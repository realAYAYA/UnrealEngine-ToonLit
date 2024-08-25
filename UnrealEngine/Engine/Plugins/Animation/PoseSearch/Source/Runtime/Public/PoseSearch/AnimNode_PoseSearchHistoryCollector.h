// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimNodeBase.h"
#include "PoseSearch/PoseSearchHistory.h"
#include "PoseSearch/PoseSearchTrajectoryTypes.h"
#include "AnimNode_PoseSearchHistoryCollector.generated.h"

USTRUCT(BlueprintInternalUseOnly)
struct POSESEARCH_API FAnimNode_PoseSearchHistoryCollector_Base : public FAnimNode_Base
{
	GENERATED_BODY()

public:
	
	// The maximum amount of poses that can be stored
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (ClampMin="2"))
	int32 PoseCount = 2;
	
	// how often in seconds poses are collected (if 0, it will collect every update)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (ClampMin="0"))
	float SamplingInterval = 0.04f;

	UPROPERTY(EditAnywhere, Category = Settings)
	TArray<FBoneReference> CollectedBones;

	// if true, the pose history will be initialized with a ref pose at the location and orientation of the AnimInstance.
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bInitializeWithRefPose = false;

	// Reset the pose history if it has become relevant to the graph after not being updated on previous frames.
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bResetOnBecomingRelevant = true;

	// if true pose scales will be cached, otherwise implied to be unitary scales
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bStoreScales = false;

	// time in seconds to recover to the reference skeleton root bone from any eventual root bone modification. if zero the behaviour will be disabled (Experimental)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Experimental, meta = (ClampMin="0"))
	float RootBoneRecoveryTime = 0.f;

	// Update Counter for detecting being relevant
	FGraphTraversalCounter UpdateCounter;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = Debug)
	FLinearColor DebugColor = FLinearColor::Red;
#endif // WITH_EDITORONLY_DATA

	// if true Trajectory the pose history node will generate the trajectory using the TrajectoryData parameters instead of relying on the input Trajectory (Experimental)
	UPROPERTY(EditAnywhere, Category = Experimental)
	bool bGenerateTrajectory = false;

	// input Trajectory samples for pose search queries in Motion Matching. These are expected to be in the world space of the SkeletalMeshComponent.
	// the trajectory sample with AccumulatedSeconds equals to zero (Trajectory.Samples[i].AccumulatedSeconds) is the sample of the previous frame of simulation (since MM works by matching the previous character pose)
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PinShownByDefault, EditCondition="!bGenerateTrajectory", EditConditionHides))
	FPoseSearchQueryTrajectory Trajectory;

	// Input Trajectory velocity will be multiplied by TrajectorySpeedMultiplier: values below 1 will result in selecting animation slower than requested from the original Trajectory
	UPROPERTY(EditAnywhere, Category = Experimental, meta = (PinHiddenByDefault, ClampMin="0", EditCondition="!bGenerateTrajectory", EditConditionHides))
	float TrajectorySpeedMultiplier = 1.f;

	// if bGenerateTrajectory is true, this is the number of trajectory past (collected) samples
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Experimental, meta = (ClampMin = "2", EditCondition = "bGenerateTrajectory", EditConditionHides))
	int32 TrajectoryHistoryCount = 10;

	// if bGenerateTrajectory is true, this is the number of trajectory future (prediction) samples
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Experimental, meta = (ClampMin="2", EditCondition="bGenerateTrajectory", EditConditionHides))
	int32 TrajectoryPredictionCount = 8;

	// if bGenerateTrajectory is true, this is the sampling interval between trajectory future (prediction) samples
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Experimental, meta = (ClampMin="0.001", EditCondition="bGenerateTrajectory", EditConditionHides))
	float PredictionSamplingInterval = 0.4f;

	// if bGenerateTrajectory is true, TrajectoryData contains the tuning parameters to generate the trajectory
	UPROPERTY(EditAnywhere, Category = Experimental, meta=(EditCondition="bGenerateTrajectory", EditConditionHides))
	FPoseSearchTrajectoryData TrajectoryData;

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual bool HasPreUpdate() const { return true; }
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;
	// End of FAnimNode_Base interface

	const UE::PoseSearch::FPoseHistory& GetPoseHistory() const { return PoseHistory; }
	UE::PoseSearch::FPoseHistory& GetPoseHistory() { return PoseHistory; }

protected:
	UE::PoseSearch::FPoseHistory PoseHistory;
};

USTRUCT(BlueprintInternalUseOnly)
struct POSESEARCH_API FAnimNode_PoseSearchHistoryCollector : public FAnimNode_PoseSearchHistoryCollector_Base
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Links, meta = (DisplayPriority = 0))
	FPoseLink Source;

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface
};

USTRUCT(BlueprintInternalUseOnly)
struct POSESEARCH_API FAnimNode_PoseSearchComponentSpaceHistoryCollector : public FAnimNode_PoseSearchHistoryCollector_Base
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Links, meta = (DisplayPriority = 0))
	FComponentSpacePoseLink Source;

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void EvaluateComponentSpace_AnyThread(FComponentSpacePoseContext& Output) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface
};
