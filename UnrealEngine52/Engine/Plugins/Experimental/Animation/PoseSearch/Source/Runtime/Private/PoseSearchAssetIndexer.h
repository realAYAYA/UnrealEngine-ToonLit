// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "Animation/AnimationPoseData.h"
#include "Animation/AnimCurveTypes.h"
#include "Animation/AttributesRuntime.h"
#include "BonePose.h"
#include "PoseSearch/PoseSearchAssetIndexer.h"
#include "PoseSearch/PoseSearchIndex.h"

namespace UE::PoseSearch
{
struct FAssetIndexingContext;

class FAssetIndexer : public IAssetIndexer
{
public:

	struct FOutput
	{
		int32 FirstIndexedSample = 0;
		int32 LastIndexedSample = 0;
		int32 NumIndexedPoses = 0;

		TArray<float> FeatureVectorTable;
		TArray<FPoseSearchPoseMetadata> PoseMetadata;
		TBitArray<> AllFeaturesNotAdded;
	} Output;

	void Reset();
	void Init(const FAssetIndexingContext& IndexingContext, const FBoneContainer& InBoneContainer);
	bool Process();

public: // IAssetIndexer

	const FAssetIndexingContext& GetIndexingContext() const override { return IndexingContext; }
	FSampleInfo GetSampleInfo(float SampleTime) const override;
	FSampleInfo GetSampleInfoRelative(float SampleTime, const FSampleInfo& Origin) const override;
	FTransform MirrorTransform(const FTransform& Transform) const override;
	FTransform GetTransformAndCacheResults(float SampleTime, float OriginTime, int8 SchemaBoneIdx, bool& Clamped) override;

private:
	FPoseSearchPoseMetadata GetMetadata(int32 SampleIdx) const;

	struct CachedEntry
	{
		float SampleTime;
		float OriginTime;
		bool Clamped;

		// @todo: minimize the Entry memory footprint
		FTransform RootTransform;
		FCompactPose Pose;
		FCSPose<FCompactPose> ComponentSpacePose;
		FBlendedCurve UnusedCurve;
		UE::Anim::FStackAttributeContainer UnusedAtrribute;
		FAnimationPoseData AnimPoseData = { Pose, UnusedCurve, UnusedAtrribute };
	};

	FBoneContainer BoneContainer;
	FAssetIndexingContext IndexingContext;
	TArray<CachedEntry> CachedEntries;
};

} // namespace UE::PoseSearch

#endif // WITH_EDITOR