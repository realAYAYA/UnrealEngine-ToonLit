// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "BonePose.h"
#include "CoreMinimal.h"
#include "PoseSearch/PoseSearchDefines.h"
#include "PoseSearch/PoseSearchFeatureChannel.h"
#include "PoseSearch/PoseSearchSchema.h"

namespace UE::PoseSearch
{

struct FAnimationAssetSampler;
struct FAssetSamplingContext;
struct FSearchIndexAsset;
struct FPoseMetadata;

class FAssetIndexer
{
public:
	struct FStats
	{
		int32 NumAccumulatedSamples = 0;
		float AccumulatedSpeed = 0.f;
		float MaxSpeed = 0.f;
		float AccumulatedAcceleration = 0.f;
		float MaxAcceleration = 0.f;
	};

	FAssetIndexer(const FBoneContainer& InBoneContainer, const FSearchIndexAsset& InSearchIndexAsset, 
		const FAssetSamplingContext& InSamplingContext, const UPoseSearchSchema& InSchema, const FAnimationAssetSampler& InAssetSampler);
	void AssignWorkingData(TArrayView<float> InOutFeatureVectorTable, TArrayView<FPoseMetadata> InOutPoseMetadata);
	void Process(int32 AssetIdx);
	const FStats& GetStats() const { return Stats; }

	FQuat GetSampleRotation(float SampleTimeOffset, int32 SampleIdx, int8 SchemaSampleBoneIdx = RootSchemaBoneIdx, int8 SchemaOriginBoneIdx = RootSchemaBoneIdx, EPermutationTimeType PermutationTimeType = EPermutationTimeType::UseSampleTime);
	FVector GetSamplePosition(float SampleTimeOffset, int32 SampleIdx, int8 SchemaSampleBoneIdx = RootSchemaBoneIdx, int8 SchemaOriginBoneIdx = RootSchemaBoneIdx, EPermutationTimeType PermutationTimeType = EPermutationTimeType::UseSampleTime);
	FVector GetSampleVelocity(float SampleTimeOffset, int32 SampleIdx, int8 SchemaSampleBoneIdx = RootSchemaBoneIdx, int8 SchemaOriginBoneIdx = RootSchemaBoneIdx, bool bUseCharacterSpaceVelocities = true, EPermutationTimeType PermutationTimeType = EPermutationTimeType::UseSampleTime);

	int32 GetBeginSampleIdx() const;
	int32 GetEndSampleIdx() const;
	int32 GetNumIndexedPoses() const;
	
	TArrayView<float> GetPoseVector(int32 SampleIdx) const;
	const UPoseSearchSchema* GetSchema() const;
	float CalculatePermutationTimeOffset() const;
	float CalculateSampleTime(int32 SampleIdx) const;

private:
	int32 GetVectorIdx(int32 SampleIdx) const;
	FTransform GetTransform(float SampleTime, bool& bClamped, int8 SchemaBoneIdx = RootSchemaBoneIdx);
	FTransform GetComponentSpaceTransform(float SampleTime, bool& bClamped, int8 SchemaBoneIdx = RootSchemaBoneIdx);
	FVector GetSamplePositionInternal(float SampleTime, float OriginTime, bool& bClamped, int8 SchemaSampleBoneIdx, int8 SchemaOriginBoneIdx);

	struct FSampleInfo
	{
		FTransform RootTransform;
		float ClipTime = 0.f;
		bool bClamped = false;
	};

	struct CachedEntry
	{
		float SampleTime;
		bool bClamped;

		FTransform RootTransform;
		FCSPose<FCompactPose> ComponentSpacePose;
	};

	FSampleInfo GetSampleInfo(float SampleTime) const;
	FTransform MirrorTransform(const FTransform& Transform) const;
	CachedEntry& GetEntry(float SampleTime);
	FTransform CalculateComponentSpaceTransform(CachedEntry& Entry, int8 SchemaBoneIdx);
	void ComputeStats();

	FBoneContainer BoneContainer;
	TMap<float, CachedEntry> CachedEntries;
	const FSearchIndexAsset& SearchIndexAsset;
	const FAssetSamplingContext& SamplingContext;
	const UPoseSearchSchema& Schema;
	const FAnimationAssetSampler& AssetSampler;
	
	TArrayView<float> FeatureVectorTable;
	TArrayView<FPoseMetadata> PoseMetadata;

	FStats Stats;
};

} // namespace UE::PoseSearch

#endif // WITH_EDITOR
