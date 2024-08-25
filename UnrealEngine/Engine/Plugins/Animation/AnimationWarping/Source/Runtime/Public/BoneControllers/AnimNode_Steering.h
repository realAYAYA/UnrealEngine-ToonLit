// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BoneControllers/BoneControllerTypes.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "Kismet/KismetMathLibrary.h"
#include "AnimNode_Steering.generated.h"

struct FAnimationInitializeContext;
struct FComponentSpacePoseContext;
struct FNodeDebugData;

// add procedural delta to the root motion attribute 
USTRUCT(BlueprintInternalUseOnly)
struct ANIMATIONWARPINGRUNTIME_API FAnimNode_Steering : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

	// The Orientation to steer towards
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Evaluation, meta=(PinShownByDefault))
	FQuat TargetOrientation = FQuat::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Filtering)
	bool bEnableTargetSmoothing = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Filtering)
	float SmoothTargetStiffness = 300;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Filtering)
	float SmoothTargetDamping = 1;
	
	// The number of seconds in the future before we should reach the TargetOrientation when play animations with no root motion rotation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Evaluation, meta=(PinShownByDefault))
	float ProceduralTargetTime = 0.2f;
	
	// Deprected old/unused parameter, to avoid breaking data
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Evaluation, meta=(PinShownByDefault))
	float TargetTime = 0.2f;
	
	// The number of seconds in the future before we should reach the TargetOrientation when playing animations with root motion rotation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Evaluation, meta=(PinShownByDefault))
	float AnimatedTargetTime = 0.2f;

	// If less than this number of degrees of rotation is found over TargetTime seconds in the CurrentAnimAsset, then rotation will be added linearly to reach the TargetOrientation
	// Otherwise the root motion will be scaled to reach the TargetOrientation over TargetTime seconds
	UPROPERTY(EditAnywhere, DisplayName=RootMotionAngleThreshold, Category=Evaluation)
	float RootMotionThreshold = 1.0f;

	// below this movement speed (based on the root motion in the animation) disable steering
	UPROPERTY(EditAnywhere, Category=Evaluation)
	float DisableSteeringBelowSpeed = 1.0f;
	
	// Animation Asset for incorporating root motion data. If CurrentAnimAsset is set, and the animation has root motion rotation within the TargetTime, then those rotations will be scaled to reach the TargetOrientation
	UPROPERTY(EditAnywhere, Transient, BlueprintReadWrite, Category=Evaluation, meta=(PinShownByDefault))
	TObjectPtr<UAnimationAsset> CurrentAnimAsset;
	
	// Current playback time in seconds of the CurrentAnimAsset
	UPROPERTY(EditAnywhere, Transient, BlueprintReadWrite, Category=Evaluation, meta=(PinShownByDefault))
	float CurrentAnimAssetTime = 0.f;

	// FAnimNodeBase interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	// End of FAnimNodeBase interface
	
	// FAnimNode_SkeletalControlBase interface
	virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override { return true; }
	// End of FAnimNode_SkeletalControlBase interface

private:

	bool bResetFilter = true;

	FQuat FilteredTarget = FQuat::Identity;
	FQuaternionSpringState TargetSmoothingState;
	
	FTransform RootBoneTransform;
};
