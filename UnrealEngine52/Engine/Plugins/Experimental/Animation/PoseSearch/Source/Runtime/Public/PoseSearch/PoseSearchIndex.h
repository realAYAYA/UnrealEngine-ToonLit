// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PoseSearch/KDTree.h"
#include "PoseSearch/PoseSearchCost.h"
#include "PoseSearchIndex.generated.h"

namespace UE::PoseSearch
{

POSESEARCH_API void CompareFeatureVectors(TConstArrayView<float> A, TConstArrayView<float> B, TConstArrayView<float> WeightsSqrt, TArrayView<float> Result);

enum class EPoseComparisonFlags : int32
{
	None = 0,
	ContinuingPose = 1 << 0,
};
ENUM_CLASS_FLAGS(EPoseComparisonFlags);

} // namespace UE::PoseSearch

UENUM()
enum class ESearchIndexAssetType : int32
{
	Invalid,
	Sequence,
	BlendSpace,
	AnimComposite,
};

UENUM()
enum class EPoseSearchBooleanRequest : uint8
{
	FalseValue,
	TrueValue,
	Indifferent, // if this is used, there will be no cost difference between true and false results

	Num UMETA(Hidden),
	Invalid = Num UMETA(Hidden)
};

UENUM()
enum class EPoseSearchPoseFlags : uint32
{
	None = 0,

	// Don't return this pose as a search result
	BlockTransition = 1 << 0,
};
ENUM_CLASS_FLAGS(EPoseSearchPoseFlags);

/**
 * This is kept for each pose in the search index along side the feature vector values and is used to influence the search.
 */
USTRUCT()
struct POSESEARCH_API FPoseSearchPoseMetadata
{
	GENERATED_BODY()

	UPROPERTY(meta = (NeverInHash))
	EPoseSearchPoseFlags Flags = EPoseSearchPoseFlags::None;

	// @todo: consider float16
	UPROPERTY(meta = (NeverInHash))
	float CostAddend = 0.0f;

	// @todo: consider float16
	UPROPERTY(meta = (NeverInHash))
	float ContinuingPoseCostAddend = 0.0f;

	// @todo: consider int16
	UPROPERTY(meta = (NeverInHash))
	int32 AssetIndex = INDEX_NONE;
};

/**
* Information about a source animation asset used by a search index.
* Some source animation entries may generate multiple FPoseSearchIndexAsset entries.
**/
USTRUCT()
struct POSESEARCH_API FPoseSearchIndexAsset
{
	GENERATED_BODY()
public:
	FPoseSearchIndexAsset() {}

	FPoseSearchIndexAsset(
		ESearchIndexAssetType InType,
		int32 InSourceAssetIdx, 
		bool bInMirrored, 
		const FFloatInterval& InSamplingInterval,
		FVector InBlendParameters = FVector::Zero())
		: Type(InType)
		, SourceAssetIdx(InSourceAssetIdx)
		, bMirrored(bInMirrored)
		, BlendParameters(InBlendParameters)
		, SamplingInterval(InSamplingInterval) {}

	// Default to Sequence for now for backward compatibility but
	// at some point we might want to change this to Invalid.
	UPROPERTY(meta = (NeverInHash))
	ESearchIndexAssetType Type = ESearchIndexAssetType::Sequence;

	// Index of the source asset in search index's container (i.e. UPoseSearchDatabase)
	UPROPERTY(meta = (NeverInHash))
	int32 SourceAssetIdx = INDEX_NONE;

	UPROPERTY(meta = (NeverInHash))
	bool bMirrored = false;

	UPROPERTY(meta = (NeverInHash))
	FVector BlendParameters = FVector::Zero();

	UPROPERTY(meta = (NeverInHash))
	FFloatInterval SamplingInterval;

	UPROPERTY(meta = (NeverInHash))
	int32 FirstPoseIdx = INDEX_NONE;

	UPROPERTY(meta = (NeverInHash))
	int32 NumPoses = 0;

	bool IsPoseInRange(int32 PoseIdx) const { return (PoseIdx >= FirstPoseIdx) && (PoseIdx < FirstPoseIdx + NumPoses); }
};


/**
* case class for FPoseSearchIndex. building block used to gather data for data mining and calculate weights, pca, kdtree stuff
*/
USTRUCT()
struct POSESEARCH_API FPoseSearchIndexBase
{
	GENERATED_BODY()

	UPROPERTY(Category = Info, VisibleAnywhere, meta = (NeverInHash))
	int32 NumPoses = 0;

	UPROPERTY(meta = (NeverInHash))
	TArray<float> Values;
	
	UPROPERTY(meta = (NeverInHash))
	TArray<FPoseSearchPoseMetadata> PoseMetadata;

	UPROPERTY(meta = (NeverInHash))
	EPoseSearchPoseFlags OverallFlags = EPoseSearchPoseFlags::None;

	UPROPERTY(meta = (NeverInHash))
	TArray<FPoseSearchIndexAsset> Assets;

	// minimum of the database metadata CostAddend: it represents the minimum cost of any search for the associated database (we'll skip the search in case the search result total cost is already less than MinCostAddend)
	UPROPERTY(Category = Info, VisibleAnywhere, meta = (NeverInHash))
	float MinCostAddend = -MAX_FLT;

	bool IsValidPoseIndex(int32 PoseIdx) const { return PoseIdx < NumPoses; }
	bool IsEmpty() const;

	const FPoseSearchIndexAsset& GetAssetForPose(int32 PoseIdx) const;
	const FPoseSearchIndexAsset* GetAssetForPoseSafe(int32 PoseIdx) const;

	float GetAssetTime(int32 PoseIdx, float SamplingInterval) const;

	void Reset();
	
	friend FArchive& operator<<(FArchive& Ar, FPoseSearchIndexBase& Index);
};

/**
* A search index for animation poses. The structure of the search index is determined by its UPoseSearchSchema.
* May represent a single animation (see UPoseSearchSequenceMetaData) or a collection (see UPoseSearchDatabase).
*/
USTRUCT()
struct POSESEARCH_API FPoseSearchIndex : public FPoseSearchIndexBase
{
	GENERATED_BODY()

	UPROPERTY(meta = (NeverInHash))
	TArray<float> PCAValues;

	UPROPERTY(Category = Info, VisibleAnywhere, meta = (NeverInHash))
	TArray<float> PCAProjectionMatrix;

	UPROPERTY(Category = Info, VisibleAnywhere, meta = (NeverInHash))
	TArray<float> Mean;

	// we store weights square roots to reduce numerical errors when CompareFeatureVectors 
	// ((VA - VB) * VW).square().sum()
	// instead of
	// ((VA - VB).square() * VW).sum()
	// since (VA - VB).square() could lead to big numbers, and VW being multiplied by the variance of the dataset
	UPROPERTY(Category = Info, VisibleAnywhere, meta = (NeverInHash))
	TArray<float> WeightsSqrt;

	UE::PoseSearch::FKDTree KDTree;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Category = Info, VisibleAnywhere, meta = (NeverInHash))
	float PCAExplainedVariance = 0.f;

	UPROPERTY(Category = Info, VisibleAnywhere, meta = (NeverInHash))
	TArray<float> Deviation;
#endif // WITH_EDITORONLY_DATA

	FPoseSearchIndex() = default;
	~FPoseSearchIndex() = default;
	FPoseSearchIndex(const FPoseSearchIndex& Other); // custom copy constructor to deal with the KDTree DataSrc
	FPoseSearchIndex(FPoseSearchIndex&& Other) = delete;
	FPoseSearchIndex& operator=(const FPoseSearchIndex& Other); // custom equal operator to deal with the KDTree DataSrc
	FPoseSearchIndex& operator=(FPoseSearchIndex&& Other) = delete;

	void Reset();
	TConstArrayView<float> GetPoseValues(int32 PoseIdx) const;
	TConstArrayView<float> GetPoseValuesSafe(int32 PoseIdx) const;
	FPoseSearchCost ComparePoses(int32 PoseIdx, EPoseSearchBooleanRequest QueryMirrorRequest, UE::PoseSearch::EPoseComparisonFlags PoseComparisonFlags, float MirrorMismatchCostBias, TConstArrayView<float> QueryValues) const;

	friend FArchive& operator<<(FArchive& Ar, FPoseSearchIndex& Index);
};
