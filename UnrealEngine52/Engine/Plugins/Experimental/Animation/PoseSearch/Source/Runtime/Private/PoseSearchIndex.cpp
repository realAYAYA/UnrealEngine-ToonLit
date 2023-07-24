// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchIndex.h"
#include "PoseSearchEigenHelper.h"

namespace UE::PoseSearch
{

static inline float CompareFeatureVectors(TConstArrayView<float> A, TConstArrayView<float> B, TConstArrayView<float> WeightsSqrt)
{
	check(A.Num() == B.Num() && A.Num() == WeightsSqrt.Num());

	Eigen::Map<const Eigen::ArrayXf> VA(A.GetData(), A.Num());
	Eigen::Map<const Eigen::ArrayXf> VB(B.GetData(), B.Num());
	Eigen::Map<const Eigen::ArrayXf> VW(WeightsSqrt.GetData(), WeightsSqrt.Num());

	return ((VA - VB) * VW).square().sum();
}

void CompareFeatureVectors(TConstArrayView<float> A, TConstArrayView<float> B, TConstArrayView<float> WeightsSqrt, TArrayView<float> Result)
{
	check(A.Num() == B.Num() && A.Num() == WeightsSqrt.Num() && A.Num() == Result.Num());

	Eigen::Map<const Eigen::ArrayXf> VA(A.GetData(), A.Num());
	Eigen::Map<const Eigen::ArrayXf> VB(B.GetData(), B.Num());
	Eigen::Map<const Eigen::ArrayXf> VW(WeightsSqrt.GetData(), WeightsSqrt.Num());
	Eigen::Map<Eigen::ArrayXf> VR(Result.GetData(), Result.Num());

	VR = ((VA - VB) * VW).square();
}

} // namespace UE::PoseSearch

//////////////////////////////////////////////////////////////////////////
// FPoseSearchBaseIndex
const FPoseSearchIndexAsset& FPoseSearchIndexBase::GetAssetForPose(int32 PoseIdx) const
{
	const int32 AssetIndex = PoseMetadata[PoseIdx].AssetIndex;
	return Assets[AssetIndex];
}

const FPoseSearchIndexAsset* FPoseSearchIndexBase::GetAssetForPoseSafe(int32 PoseIdx) const
{
	if (PoseMetadata.IsValidIndex(PoseIdx))
	{
		const int32 AssetIndex = PoseMetadata[PoseIdx].AssetIndex;
		if (Assets.IsValidIndex(AssetIndex))
		{
			return &Assets[AssetIndex];
		}
	}
	return nullptr;
}

float FPoseSearchIndexBase::GetAssetTime(int32 PoseIdx, float SamplingInterval) const
{
	const FPoseSearchIndexAsset& Asset = GetAssetForPose(PoseIdx);

	if (Asset.Type == ESearchIndexAssetType::Sequence || Asset.Type == ESearchIndexAssetType::AnimComposite)
	{
		const FFloatInterval SamplingRange = Asset.SamplingInterval;

		float AssetTime = FMath::Min(SamplingRange.Min + SamplingInterval * (PoseIdx - Asset.FirstPoseIdx), SamplingRange.Max);
		return AssetTime;
	}

	if (Asset.Type == ESearchIndexAssetType::BlendSpace)
	{
		const FFloatInterval SamplingRange = Asset.SamplingInterval;

		// For BlendSpaces the AssetTime is in the range [0, 1] while the Sampling Range
		// is in real time (seconds)
		float AssetTime = FMath::Min(SamplingRange.Min + SamplingInterval * (PoseIdx - Asset.FirstPoseIdx), SamplingRange.Max) / (Asset.NumPoses * SamplingInterval);
		return AssetTime;
	}
	
	checkNoEntry();
	return -1.0f;
}

bool FPoseSearchIndexBase::IsEmpty() const
{
	const bool bEmpty = Assets.Num() == 0 || NumPoses == 0;
	return bEmpty;
}

void FPoseSearchIndexBase::Reset()
{
	FPoseSearchIndexBase Default;
	*this = Default;
}

FArchive& operator<<(FArchive& Ar, FPoseSearchIndexBase& Index)
{
	int32 NumValues = 0;
	int32 NumAssets = 0;

	if (Ar.IsSaving())
	{
		NumValues = Index.Values.Num();
		NumAssets = Index.Assets.Num();
	}

	Ar << Index.NumPoses;
	Ar << NumValues;
	Ar << NumAssets;
	Ar << Index.OverallFlags;

	if (Ar.IsLoading())
	{
		Index.Values.SetNumUninitialized(NumValues);
		Index.PoseMetadata.SetNumUninitialized(Index.NumPoses);
		Index.Assets.SetNumUninitialized(NumAssets);
	}

	if (Index.Values.Num() > 0)
	{
		Ar.Serialize(&Index.Values[0], Index.Values.Num() * Index.Values.GetTypeSize());
	}

	if (Index.PoseMetadata.Num() > 0)
	{
		Ar.Serialize(&Index.PoseMetadata[0], Index.PoseMetadata.Num() * Index.PoseMetadata.GetTypeSize());
	}

	if (Index.Assets.Num() > 0)
	{
		Ar.Serialize(&Index.Assets[0], Index.Assets.Num() * Index.Assets.GetTypeSize());
	}

	Ar << Index.MinCostAddend;

	return Ar;
}

//////////////////////////////////////////////////////////////////////////
// FPoseSearchIndex
FPoseSearchIndex::FPoseSearchIndex(const FPoseSearchIndex& Other)
	: FPoseSearchIndexBase(Other)
	, PCAValues(Other.PCAValues)
	, PCAProjectionMatrix(Other.PCAProjectionMatrix)
	, Mean(Other.Mean)
	, WeightsSqrt(Other.WeightsSqrt)
	, KDTree(Other.KDTree)
#if WITH_EDITORONLY_DATA
	, PCAExplainedVariance(Other.PCAExplainedVariance)
	, Deviation(Other.Deviation)
#endif // WITH_EDITORONLY_DATA
{
	check(!PCAValues.IsEmpty() || KDTree.DataSource.PointCount == 0);
	KDTree.DataSource.Data = PCAValues.IsEmpty() ? nullptr : PCAValues.GetData();
}

FPoseSearchIndex& FPoseSearchIndex::operator=(const FPoseSearchIndex& Other)
{
	if (this != &Other)
	{
		this->~FPoseSearchIndex();
		new(this)FPoseSearchIndex(Other);
	}
	return *this;
}

void FPoseSearchIndex::Reset()
{
	FPoseSearchIndex Default;
	*this = Default;
}

TConstArrayView<float> FPoseSearchIndex::GetPoseValues(int32 PoseIdx) const
{
	const int32 SchemaCardinality = WeightsSqrt.Num();
	check(PoseIdx >= 0 && PoseIdx < NumPoses&& SchemaCardinality > 0);
	const int32 ValueOffset = PoseIdx * SchemaCardinality;
	return MakeArrayView(&Values[ValueOffset], SchemaCardinality);
}

TConstArrayView<float> FPoseSearchIndex::GetPoseValuesSafe(int32 PoseIdx) const
{
	if (PoseIdx >= 0 && PoseIdx < NumPoses)
	{
		const int32 SchemaCardinality = WeightsSqrt.Num();
		const int32 ValueOffset = PoseIdx * SchemaCardinality;
		return MakeArrayView(&Values[ValueOffset], SchemaCardinality);
	}
	return TConstArrayView<float>();
}

FPoseSearchCost FPoseSearchIndex::ComparePoses(int32 PoseIdx, EPoseSearchBooleanRequest QueryMirrorRequest, UE::PoseSearch::EPoseComparisonFlags PoseComparisonFlags, float MirrorMismatchCostBias, TConstArrayView<float> QueryValues) const
{
	// base dissimilarity cost representing how the associated PoseIdx differ, in a weighted way, from the query pose (QueryValues)
	const float DissimilarityCost = UE::PoseSearch::CompareFeatureVectors(GetPoseValues(PoseIdx), QueryValues, WeightsSqrt);

	// cost addend associated to a mismatch in mirror state between query and analyzed PoseIdx
	float MirrorMismatchAddend = 0.f;
	if (QueryMirrorRequest != EPoseSearchBooleanRequest::Indifferent)
	{
		const FPoseSearchIndexAsset& IndexAsset = GetAssetForPose(PoseIdx);
		const bool bMirroringMismatch =
			(IndexAsset.bMirrored && QueryMirrorRequest == EPoseSearchBooleanRequest::FalseValue) ||
			(!IndexAsset.bMirrored && QueryMirrorRequest == EPoseSearchBooleanRequest::TrueValue);
		if (bMirroringMismatch)
		{
			MirrorMismatchAddend = MirrorMismatchCostBias;
		}
	}

	const FPoseSearchPoseMetadata& PoseIdxMetadata = PoseMetadata[PoseIdx];

	// cost addend associated to Schema->BaseCostBias or overriden by UAnimNotifyState_PoseSearchModifyCost
	const float NotifyAddend = PoseIdxMetadata.CostAddend;

	// cost addend associated to Schema->ContinuingPoseCostBias or overriden by UAnimNotifyState_PoseSearchOverrideContinuingPoseCostBias
	const float ContinuingPoseCostAddend = EnumHasAnyFlags(PoseComparisonFlags, UE::PoseSearch::EPoseComparisonFlags::ContinuingPose) ? PoseIdxMetadata.ContinuingPoseCostAddend : 0.f;

	return FPoseSearchCost(DissimilarityCost, NotifyAddend, MirrorMismatchAddend, ContinuingPoseCostAddend);
}

FArchive& operator<<(FArchive& Ar, FPoseSearchIndex& Index)
{
	Ar << static_cast<FPoseSearchIndexBase&>(Index);

	int32 NumPCAValues = 0;

	if (Ar.IsSaving())
	{
		NumPCAValues = Index.PCAValues.Num();
	}

	Ar << NumPCAValues;

	if (Ar.IsLoading())
	{
		Index.PCAValues.SetNumUninitialized(NumPCAValues);
	}

	if (Index.PCAValues.Num() > 0)
	{
		Ar.Serialize(&Index.PCAValues[0], Index.PCAValues.Num() * Index.PCAValues.GetTypeSize());
	}

	Ar << Index.WeightsSqrt;
	Ar << Index.Mean;
	Ar << Index.PCAProjectionMatrix;

	Serialize(Ar, Index.KDTree, Index.PCAValues.GetData());

#if WITH_EDITORONLY_DATA
	Ar << Index.PCAExplainedVariance;
	Ar << Index.Deviation;
#endif // WITH_EDITORONLY_DATA

	return Ar;
}