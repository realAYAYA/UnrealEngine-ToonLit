// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BonePose.h"

struct FAnimationPoseData;
struct FAnimExtractContext;
class UAnimationAsset;
class UAnimNotifyState_PoseSearchBase;

namespace UE::PoseSearch
{

/**
 * Helper for sampling data from animation assets
 */
struct POSESEARCH_API FAnimationAssetSampler
{
	FAnimationAssetSampler(TObjectPtr<const UAnimationAsset> InAnimationAsset = nullptr, const FTransform& InRootTransformOrigin = FTransform::Identity, const FVector& InBlendParameters = FVector::ZeroVector, int32 InRootTransformSamplingRate = 30);
	void Init(TObjectPtr<const UAnimationAsset> InAnimationAsset, const FTransform& InRootTransformOrigin = FTransform::Identity, const FVector& InBlendParameters = FVector::ZeroVector, int32 InRootTransformSamplingRate = 30);

	bool IsInitialized() const;
	float GetPlayLength() const;
	float ToRealTime(float NormalizedTime) const;
	float ToNormalizedTime(float RealTime) const;
	bool IsLoopable() const;

	// Gets the final root transformation at the end of the asset's playback time
	FTransform GetTotalRootTransform() const;

	// Extracts pose for this asset for a given context
	void ExtractPose(const FAnimExtractContext& ExtractionCtx, FAnimationPoseData& OutAnimPoseData) const;

	// Extracts pose for this asset at a given Time
	void ExtractPose(float Time, FCompactPose& OutPose) const;

	// Extracts root transform at the given time, using the extremities of the sequence to extrapolate beyond the 
	// sequence limits when Time is less than zero or greater than the sequence length.
	FTransform ExtractRootTransform(float Time) const;

	// Extracts notify states inheriting from UAnimNotifyState_PoseSearchBase present in the sequence at Time.
	// The function does not empty NotifyStates before adding new notifies!
	void ExtractPoseSearchNotifyStates(float Time, TFunction<bool(UAnimNotifyState_PoseSearchBase*)> ProcessPoseSearchBase) const;
	
	TConstArrayView<FAnimNotifyEvent> GetAllAnimNotifyEvents() const;

	const UAnimationAsset* GetAsset() const;

	void Process();

	static float GetPlayLength(const UAnimationAsset* AnimAsset, const FVector& BlendParameters = FVector::ZeroVector);

protected:
	TWeakObjectPtr<const UAnimationAsset> AnimationAssetPtr;
	FTransform RootTransformOrigin = FTransform::Identity;

	// members used to sample blend spaces only!
	FVector BlendParameters = FVector::ZeroVector;
	int32 RootTransformSamplingRate = 30;
	float CachedPlayLength = 0.f;
	TArray<FTransform> AccumulatedRootTransform;

	const float ExtrapolationSampleTime = 1.f / 30.f;
	const float ExtractionInterval = 1.0f / 120.0f;
};

} // namespace UE::PoseSearch

