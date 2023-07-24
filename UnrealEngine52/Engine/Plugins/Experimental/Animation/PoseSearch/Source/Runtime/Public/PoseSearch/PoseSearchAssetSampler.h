// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BonePose.h"
#include "PoseSearchAssetSampler.generated.h"

struct FAnimationPoseData;
struct FAnimExtractContext;
class UAnimationAsset;
class UAnimNotifyState_PoseSearchBase;
class UBlendSpace;
class UMirrorDataTable;

USTRUCT()
struct POSESEARCH_API FPoseSearchExtrapolationParameters
{
	GENERATED_BODY()

	// If the angular root motion speed in degrees is below this value, it will be treated as zero.
	UPROPERTY(EditAnywhere, Category = "Settings")
	float AngularSpeedThreshold = 1.0f;

	// If the root motion linear speed is below this value, it will be treated as zero.
	UPROPERTY(EditAnywhere, Category = "Settings")
	float LinearSpeedThreshold = 1.0f;

	// Time from sequence start/end used to extrapolate the trajectory.
	UPROPERTY(EditAnywhere, Category = "Settings")
	float SampleTime = 0.05f;
};

namespace UE::PoseSearch
{

struct POSESEARCH_API FAssetSamplingContext
{
	// Time delta used for computing pose derivatives
	static constexpr float FiniteDelta = 1 / 60.0f;

	// Mirror data table pointer copied from Schema for convenience
	TObjectPtr<const UMirrorDataTable> MirrorDataTable;

	// Compact pose format of Mirror Bone Map
	TCustomBoneIndexArray<FCompactPoseBoneIndex, FCompactPoseBoneIndex> CompactPoseMirrorBones;

	// Pre-calculated component space rotations of reference pose, which allows mirror to work with any joint orientation
	// Only initialized and used when a mirroring table is specified
	TCustomBoneIndexArray<FQuat, FCompactPoseBoneIndex> ComponentSpaceRefRotations;

	void Init(const UMirrorDataTable* InMirrorDataTable, const FBoneContainer& BoneContainer);
	FTransform MirrorTransform(const FTransform& Transform) const;
};

/**
 * Helper interface for sampling data from animation assets
 */
class POSESEARCH_API IAssetSampler
{
public:
	virtual ~IAssetSampler() {};

	virtual float GetPlayLength() const = 0;
	virtual bool IsLoopable() const = 0;

	// Gets the time associated with a particular root distance traveled
	virtual float GetTimeFromRootDistance(float Distance) const = 0;

	// Gets the total root distance traveled 
	virtual float GetTotalRootDistance() const = 0;

	// Gets the final root transformation at the end of the asset's playback time
	virtual FTransform GetTotalRootTransform() const = 0;

	// Extracts pose for this asset for a given context
	virtual void ExtractPose(const FAnimExtractContext& ExtractionCtx, FAnimationPoseData& OutAnimPoseData) const = 0;

	// Extracts the accumulated root distance at the given time, using the extremities of the sequence to extrapolate 
	// beyond the sequence limits when Time is less than zero or greater than the sequence length
	virtual float ExtractRootDistance(float Time) const = 0;

	// Extracts root transform at the given time, using the extremities of the sequence to extrapolate beyond the 
	// sequence limits when Time is less than zero or greater than the sequence length.
	virtual FTransform ExtractRootTransform(float Time) const = 0;

	// Extracts notify states inheriting from UAnimNotifyState_PoseSearchBase present in the sequence at Time.
	// The function does not empty NotifyStates before adding new notifies!
	virtual void ExtractPoseSearchNotifyStates(float Time, TArray<UAnimNotifyState_PoseSearchBase*>& NotifyStates) const = 0;

	virtual const UAnimationAsset* GetAsset() const = 0;

	virtual void Process() = 0;
};

// Sampler working with UAnimSequenceBase so it can be used for UAnimSequence as well as UAnimComposite.
struct POSESEARCH_API FSequenceBaseSampler : public IAssetSampler
{
public:
	struct FInput
	{
		TWeakObjectPtr<const UAnimSequenceBase> SequenceBase;
		int32 RootDistanceSamplingRate = 60;
		FPoseSearchExtrapolationParameters ExtrapolationParameters;
	} Input;

	void Init(const FInput& Input);
	virtual void Process() override;

	virtual float GetPlayLength() const override;
	virtual bool IsLoopable() const override;

	virtual float GetTimeFromRootDistance(float Distance) const override;

	virtual float GetTotalRootDistance() const override { return TotalRootDistance; }
	virtual FTransform GetTotalRootTransform() const override { return TotalRootTransform; }

	virtual void ExtractPose(const FAnimExtractContext& ExtractionCtx, FAnimationPoseData& OutAnimPoseData) const override;
	virtual float ExtractRootDistance(float Time) const override;
	virtual FTransform ExtractRootTransform(float Time) const override;
	virtual void ExtractPoseSearchNotifyStates(float Time, TArray<class UAnimNotifyState_PoseSearchBase*>& NotifyStates) const override;
	virtual const UAnimationAsset* GetAsset() const override;

private:
	float TotalRootDistance = 0.0f;
	FTransform TotalRootTransform = FTransform::Identity;
	TArray<float> AccumulatedRootDistance;

	void ProcessRootDistance();
};

struct POSESEARCH_API FBlendSpaceSampler : public IAssetSampler
{
public:
	struct FInput
	{
		FBoneContainer BoneContainer;
		TWeakObjectPtr<const UBlendSpace> BlendSpace;
		int32 RootDistanceSamplingRate = 60;
		int32 RootTransformSamplingRate = 60;
		FPoseSearchExtrapolationParameters ExtrapolationParameters;
		FVector BlendParameters;
	} Input;

	void Init(const FInput& Input);
	virtual void Process() override;

	virtual float GetPlayLength() const override { return PlayLength; }
	virtual bool IsLoopable() const override;

	virtual float GetTimeFromRootDistance(float Distance) const override;

	virtual float GetTotalRootDistance() const override { return TotalRootDistance; }
	virtual FTransform GetTotalRootTransform() const override { return TotalRootTransform; }

	virtual void ExtractPose(const FAnimExtractContext& ExtractionCtx, FAnimationPoseData& OutAnimPoseData) const override;
	virtual float ExtractRootDistance(float Time) const override;
	virtual FTransform ExtractRootTransform(float Time) const override;
	virtual void ExtractPoseSearchNotifyStates(float Time, TArray<class UAnimNotifyState_PoseSearchBase*>& NotifyStates) const override;

	virtual const UAnimationAsset* GetAsset() const override;

private:
	float PlayLength = 0.0f;
	float TotalRootDistance = 0.0f;
	FTransform TotalRootTransform = FTransform::Identity;
	TArray<float> AccumulatedRootDistance;
	TArray<FTransform> AccumulatedRootTransform;

	void ProcessPlayLength();
	void ProcessRootDistance();
	void ProcessRootTransform();

	// Extracts the pre-computed blend space root transform. ProcessRootTransform must be run first.
	FTransform ExtractBlendSpaceRootTrackTransform(float Time) const;
	FTransform ExtractBlendSpaceRootMotion(float StartTime, float DeltaTime, bool bAllowLooping) const;
	FTransform ExtractBlendSpaceRootMotionFromRange(float StartTrackPosition, float EndTrackPosition) const;
};

} // namespace UE::PoseSearch
