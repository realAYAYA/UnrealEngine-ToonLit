// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PoseSearch/KDTree.h"
#include "PoseSearch/PoseSearchCost.h"
#include "PoseSearch/PoseSearchDefines.h"

namespace UE::PoseSearch
{

POSESEARCH_API void CompareFeatureVectors(TConstArrayView<float> A, TConstArrayView<float> B, TConstArrayView<float> WeightsSqrt, TArrayView<float> Result);
POSESEARCH_API float CompareFeatureVectors(TConstArrayView<float> A, TConstArrayView<float> B);

/**
 * This is kept for each pose in the search index along side the feature vector values and is used to influence the search.
 */
struct FPoseMetadata
{
private:
	// bits 0-30 represent the AssetIndex, bit 31 represents bBlockTransition
	uint32 Data = 0;
	float CostAddend = 0.f;

public:
	void Init(uint32 AssetIndex, bool bBlockTransition, float InCostAddend)
	{
		check((AssetIndex & (1 << 31)) == 0);
		Data = AssetIndex | (bBlockTransition ? 1 << 31 : 0);

		CostAddend = InCostAddend;
	}

	bool IsBlockTransition() const
	{
		return Data & (1 << 31);
	}

	uint32 GetAssetIndex() const
	{
		return Data & ~(1 << 31);
	}

	float GetCostAddend() const
	{
		return CostAddend;
	}

	friend FArchive& operator<<(FArchive& Ar, FPoseMetadata& Metadata);
};

/**
* Information about a source animation asset used by a search index.
* Some source animation entries may generate multiple FSearchIndexAsset entries.
**/
struct FSearchIndexAsset
{
	FSearchIndexAsset() {}

	FSearchIndexAsset(
		int32 InSourceAssetIdx,
		int32 InFirstPoseIdx,
		bool bInMirrored, 
		const FFloatInterval& InSamplingInterval,
		int32 SchemaSampleRate,
		int32 InPermutationIdx,
		FVector InBlendParameters = FVector::Zero())
		: SourceAssetIdx(InSourceAssetIdx)
		, bMirrored(bInMirrored)
		, PermutationIdx(InPermutationIdx)
		, BlendParameters(InBlendParameters)
		, FirstPoseIdx(InFirstPoseIdx)
		, FirstSampleIdx(FMath::CeilToInt(InSamplingInterval.Min * SchemaSampleRate))
		, LastSampleIdx(FMath::FloorToInt(InSamplingInterval.Max * SchemaSampleRate))
	{
		check(SchemaSampleRate > 0);
	}

	// Index of the source asset in search index's container (i.e. UPoseSearchDatabase)
	int32 SourceAssetIdx = INDEX_NONE;
	bool bMirrored = false;
	int32 PermutationIdx = INDEX_NONE;
	FVector BlendParameters = FVector::Zero();
	int32 FirstPoseIdx = INDEX_NONE;
	int32 FirstSampleIdx = INDEX_NONE;
	int32 LastSampleIdx = INDEX_NONE;

	bool IsPoseInRange(int32 PoseIdx) const { return (PoseIdx >= FirstPoseIdx) && (PoseIdx < FirstPoseIdx + GetNumPoses()); }

#if DO_CHECK
	bool IsInitialized() const 
	{
		return 
			SourceAssetIdx != INDEX_NONE &&
			PermutationIdx != INDEX_NONE &&
			FirstPoseIdx != INDEX_NONE &&
			FirstSampleIdx != INDEX_NONE &&
			LastSampleIdx != INDEX_NONE;
	}
#endif // DO_CHECK

	int32 GetBeginSampleIdx() const { return FirstSampleIdx; }
	int32 GetEndSampleIdx() const {	return LastSampleIdx + 1; }
	int32 GetNumPoses() const { return GetEndSampleIdx() - GetBeginSampleIdx(); }

	float GetFirstSampleTime(int32 SchemaSampleRate) const { check(SchemaSampleRate > 0); return FirstSampleIdx / float(SchemaSampleRate); }
	float GetLastSampleTime(int32 SchemaSampleRate) const { check(SchemaSampleRate > 0); return LastSampleIdx / float(SchemaSampleRate); }

	int32 GetPoseIndexFromTime(float Time, bool bIsLooping, int32 SchemaSampleRate) const
	{
		check(IsInitialized());

		const int32 NumPoses = GetNumPoses();
		int32 PoseOffset = FMath::RoundToInt(SchemaSampleRate * Time) - FirstSampleIdx;
		if (bIsLooping)
		{
			if (PoseOffset < 0)
			{
				PoseOffset = (PoseOffset % NumPoses) + NumPoses;
			}
			else if (PoseOffset >= NumPoses)
			{
				PoseOffset = PoseOffset % NumPoses;
			}
			return FirstPoseIdx + PoseOffset;
		}

		if (PoseOffset >= 0 && PoseOffset < NumPoses)
		{
			return FirstPoseIdx + PoseOffset;
		}

		return INDEX_NONE;
	}

	float GetTimeFromPoseIndex(int32 PoseIdx, int32 SchemaSampleRate) const
	{
		check(SchemaSampleRate > 0);

		const int32 PoseOffset = PoseIdx - FirstPoseIdx;
		check(PoseOffset >= 0 && PoseOffset < GetNumPoses());

		const float Time = (FirstSampleIdx + PoseOffset) / float(SchemaSampleRate);
		return Time;
	}

	friend FArchive& operator<<(FArchive& Ar, FSearchIndexAsset& IndexAsset);
};

struct FSearchStats
{
	float AverageSpeed = 0.f;
	float MaxSpeed = 0.f;
	float AverageAcceleration = 0.f;
	float MaxAcceleration = 0.f;
	friend FArchive& operator<<(FArchive& Ar, FSearchStats& Stats);
};

/**
* case class for FSearchIndex. building block used to gather data for data mining and calculate weights, pca, kdtree stuff
*/
struct FSearchIndexBase
{
	TAlignedArray<float> Values;
	TAlignedArray<FPoseMetadata> PoseMetadata;
	bool bAnyBlockTransition = false;
	TAlignedArray<FSearchIndexAsset> Assets;

	// minimum of the database metadata CostAddend: it represents the minimum cost of any search for the associated database (we'll skip the search in case the search result total cost is already less than MinCostAddend)
	float MinCostAddend = -MAX_FLT;

	// @todo: this property should be editor only
	FSearchStats Stats;

	int32 GetNumPoses() const { return PoseMetadata.Num(); }
	bool IsValidPoseIndex(int32 PoseIdx) const { return PoseIdx < GetNumPoses(); }
	bool IsEmpty() const;

	const FSearchIndexAsset& GetAssetForPose(int32 PoseIdx) const;
	POSESEARCH_API const FSearchIndexAsset* GetAssetForPoseSafe(int32 PoseIdx) const;

	void Reset();
	
	friend FArchive& operator<<(FArchive& Ar, FSearchIndexBase& Index);
};

/**
* A search index for animation poses. The structure of the search index is determined by its UPoseSearchSchema.
* May represent a single animation (see UPoseSearchSequenceMetaData) or a collection (see UPoseSearchDatabase).
*/
struct FSearchIndex : public FSearchIndexBase
{
	// we store weights square roots to reduce numerical errors when CompareFeatureVectors 
	// ((VA - VB) * VW).square().sum()
	// instead of
	// ((VA - VB).square() * VW).sum()
	// since (VA - VB).square() could lead to big numbers, and VW being multiplied by the variance of the dataset
	TAlignedArray<float> WeightsSqrt;
	TAlignedArray<float> PCAValues;
	TAlignedArray<float> PCAProjectionMatrix;
	TAlignedArray<float> Mean;

	FKDTree KDTree;

	// @todo: this property should be editor only
	float PCAExplainedVariance = 0.f;

	FSearchIndex() = default;
	~FSearchIndex() = default;
	FSearchIndex(const FSearchIndex& Other); // custom copy constructor to deal with the KDTree DataSrc
	FSearchIndex(FSearchIndex&& Other) = delete;
	FSearchIndex& operator=(const FSearchIndex& Other); // custom equal operator to deal with the KDTree DataSrc
	FSearchIndex& operator=(FSearchIndex&& Other) = delete;

	void Reset();
	TConstArrayView<float> GetPoseValues(int32 PoseIdx) const;
	TConstArrayView<float> GetReconstructedPoseValues(int32 PoseIdx, TArrayView<float> BufferUsedForReconstruction) const;
	POSESEARCH_API TConstArrayView<float> PCAProject(TConstArrayView<float> PoseValues, TArrayView<float> BufferUsedForProjection) const;

	POSESEARCH_API TArray<float> GetPoseValuesSafe(int32 PoseIdx) const;
	POSESEARCH_API TConstArrayView<float> GetPCAPoseValues(int32 PoseIdx) const;
	POSESEARCH_API FPoseSearchCost ComparePoses(int32 PoseIdx, float ContinuingPoseCostBias, TConstArrayView<float> PoseValues, TConstArrayView<float> QueryValues) const;
	POSESEARCH_API FPoseSearchCost CompareAlignedPoses(int32 PoseIdx, float ContinuingPoseCostBias, TConstArrayView<float> PoseValues, TConstArrayView<float> QueryValues) const;

	friend FArchive& operator<<(FArchive& Ar, FSearchIndex& Index);
};

} // namespace UE::PoseSearch
