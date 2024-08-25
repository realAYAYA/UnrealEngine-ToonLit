// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchIndex.h"
#include "PoseSearchEigenHelper.h"

namespace UE::PoseSearch
{

static FORCEINLINE float CompareFeatureVectors(TConstArrayView<float> A, TConstArrayView<float> B, TConstArrayView<float> WeightsSqrt)
{
	check(A.Num() == B.Num() && A.Num() == WeightsSqrt.Num());

	Eigen::Map<const Eigen::ArrayXf> VA(A.GetData(), A.Num());
	Eigen::Map<const Eigen::ArrayXf> VB(B.GetData(), B.Num());
	Eigen::Map<const Eigen::ArrayXf> VW(WeightsSqrt.GetData(), WeightsSqrt.Num());

	return ((VA - VB) * VW).square().sum();
}

static FORCEINLINE float CompareAlignedFeatureVectors(TConstArrayView<float> A, TConstArrayView<float> B, TConstArrayView<float> WeightsSqrt)
{
	check(A.Num() == B.Num() && A.Num() == WeightsSqrt.Num());
	check(A.Num() % 4 == 0);
	check(IsAligned(A.GetData(), alignof(VectorRegister4Float)));
	check(IsAligned(B.GetData(), alignof(VectorRegister4Float)));
	check(IsAligned(WeightsSqrt.GetData(), alignof(VectorRegister4Float)));
	// sufficient condition to check for pointer overlapping
	check(A.GetData() != B.GetData() && A.GetData() != WeightsSqrt.GetData());

	const int32 NumVectors = A.Num() / 4;

	const VectorRegister4Float* RESTRICT VA = reinterpret_cast<const VectorRegister4Float*>(A.GetData());
	const VectorRegister4Float* RESTRICT VB = reinterpret_cast<const VectorRegister4Float*>(B.GetData());
	const VectorRegister4Float* RESTRICT VW = reinterpret_cast<const VectorRegister4Float*>(WeightsSqrt.GetData());

	VectorRegister4Float PartialCost = VectorZero();
	for (int32 VectorIdx = 0; VectorIdx < NumVectors; ++VectorIdx, ++VA, ++VB, ++VW)
	{
		const VectorRegister4Float Diff = VectorSubtract(*VA, *VB);
		const VectorRegister4Float WeightedDiff = VectorMultiply(Diff, *VW);
		PartialCost = VectorMultiplyAdd(WeightedDiff, WeightedDiff, PartialCost);
	}

	// calculating PartialCost.X + PartialCost.Y + PartialCost.Z + PartialCost.W
	VectorRegister4Float Swizzle = VectorSwizzle(PartialCost, 1, 0, 3, 2);	// (Y, X, W, Z) of PartialCost
	PartialCost = VectorAdd(PartialCost, Swizzle);							// (X + Y, Y + X, Z + W, W + Z)
	Swizzle = VectorSwizzle(PartialCost, 2, 3, 0, 1);						// (Z + W, W + Z, X + Y, Y + X)
	PartialCost = VectorAdd(PartialCost, Swizzle);							// (X + Y + Z + W, Y + X + W + Z, Z + W + X + Y, W + Z + Y + X)
	float Cost;
	VectorStoreFloat1(PartialCost, &Cost);

// keeping this debug code to validate CompareAlignedFeatureVectors against CompareFeatureVectors
//#if DO_CHECK
//	const float EigenCost = CompareFeatureVectors(A, B, WeightsSqrt);
//
//	if (!FMath::IsNearlyEqual(Cost, EigenCost))
//	{
//		const float RelativeDifference = Cost > EigenCost ? (Cost - EigenCost) / Cost : (EigenCost - Cost) / EigenCost;
//		check(FMath::IsNearlyZero(RelativeDifference, UE_KINDA_SMALL_NUMBER));
//	}
//#endif //DO_CHECK
	
	return Cost;
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

float CompareFeatureVectors(TConstArrayView<float> A, TConstArrayView<float> B)
{
	check(A.Num() == B.Num() && A.Num());

	Eigen::Map<const Eigen::ArrayXf> VA(A.GetData(), A.Num());
	Eigen::Map<const Eigen::ArrayXf> VB(B.GetData(), B.Num());

	return (VA - VB).square().sum();
}

// pruning utils
struct FPosePair
{
	int32 PoseIdxA = 0;
	int32 PoseIdxB = 0;
};
struct FPosePairSimilarity : public FPosePair
{
	float Similarity = 0.f;
};

static bool CalculateSimilarities(TArray<FPosePairSimilarity>& PosePairSimilarities, float SimilarityThreshold, 
	int32 DataCardinality, int32 NumPoses, const TAlignedArray<float>& Values,
	TFunctionRef<TConstArrayView<float>(int32, int32)> GetValuesVector)
{
	PosePairSimilarities.Reserve(1024 * 64);

	check(Values.Num() == NumPoses * DataCardinality);
	FKDTree KDTree(NumPoses, DataCardinality, Values.GetData());

	TArray<int32> ResultIndexes;
	TArray<float> ResultDistanceSqr;
	ResultIndexes.SetNum(NumPoses + 1);
	ResultDistanceSqr.SetNum(NumPoses + 1);

	for (int32 PoseIdx = 0; PoseIdx < NumPoses; ++PoseIdx)
	{
		TConstArrayView<float> ValuesA = GetValuesVector(PoseIdx, DataCardinality);

		// searching for duplicates within a radius of SimilarityThreshold
		FKDTree::FRadiusResultSet ResultSet(SimilarityThreshold, NumPoses, ResultIndexes, ResultDistanceSqr);
		KDTree.FindNeighbors(ResultSet, ValuesA);

		for (int32 ResultIndex = 0; ResultIndex < ResultSet.Num(); ++ResultIndex)
		{
			const int32 ResultPoseIdx = ResultIndexes[ResultIndex];
			if (PoseIdx != ResultPoseIdx)
			{
				FPosePairSimilarity PosePair;
				PosePair.PoseIdxA = PoseIdx;
				PosePair.PoseIdxB = ResultPoseIdx;
				PosePair.Similarity = ResultDistanceSqr[ResultIndex];
				PosePairSimilarities.Emplace(PosePair);
			}
		}
	}

	if (!PosePairSimilarities.IsEmpty())
	{
		PosePairSimilarities.Sort([](const FPosePairSimilarity& A, const FPosePairSimilarity& B)
		{
			return A.Similarity < B.Similarity;
		});
		return true;
	}
	return false;
}

static bool PruneValues(int32 DataCardinality, int32 NumPoses, const TArray<FPosePairSimilarity>& PosePairSimilarities, TAlignedArray<float>& Values,
	TFunctionRef<uint32(int32)> GetValueOffset, TFunctionRef<void(int32, uint32)> SetValueOffset)
{
	// mapping between the one value offset and all the poses sharing it
	TMap<uint32, TArray<int32>> ValueOffsetToPoses;
	for (int32 PoseIdx = 0; PoseIdx < NumPoses; ++PoseIdx)
	{
		const uint32 ValueOffset = GetValueOffset(PoseIdx);
		// FindOrAdd to support the eventuality of having multiple poses already sharing the same value offset
		ValueOffsetToPoses.FindOrAdd(ValueOffset).Add(PoseIdx);
	}

	// at this point ValueOffsetToPoses is fully populated with all the possible value offset, and since we're not adding, but eventually removing entries we can just use the [] operator
	uint32 ValueOffsetLast = Values.Num() - DataCardinality;
	for (int32 PosePairSimilarityIdx = 0; PosePairSimilarityIdx < PosePairSimilarities.Num(); ++PosePairSimilarityIdx)
	{
		const FPosePairSimilarity& PosePairSimilarity = PosePairSimilarities[PosePairSimilarityIdx];
		const uint32 ValueOffsetA = GetValueOffset(PosePairSimilarity.PoseIdxA);
		const uint32 ValueOffsetB = GetValueOffset(PosePairSimilarity.PoseIdxB);
				
		// if the two poses don't point already to the same value offset, we can remove one of them
		if (ValueOffsetA != ValueOffsetB)
		{
			// transferring all the poses associated to ValueOffsetB to ValueOffsetA
			TArray<int32>& PosesAtValueOffsetA = ValueOffsetToPoses[ValueOffsetA];
			TArray<int32>& PosesAtValueOffsetB = ValueOffsetToPoses[ValueOffsetB];

			for (int32 PoseAtValueOffsetB : PosesAtValueOffsetB)
			{
				SetValueOffset(PoseAtValueOffsetB, ValueOffsetA);
				PosesAtValueOffsetA.Add(PoseAtValueOffsetB);
			}

			// moving the ValueOffsetLast values into the location ValueOffsetB, that we just free up
			if (ValueOffsetB != ValueOffsetLast)
			{
				FMemory::Memcpy(&Values[ValueOffsetB], &Values[ValueOffsetLast], DataCardinality * sizeof(float));
				TArray<int32>& PosesAtValueOffsetLast = ValueOffsetToPoses[ValueOffsetLast];
						
				for (int32 PoseAtValueOffsetLast : PosesAtValueOffsetLast)
				{
					SetValueOffset(PoseAtValueOffsetLast, ValueOffsetB);
				}

				PosesAtValueOffsetB = PosesAtValueOffsetLast;
				PosesAtValueOffsetLast.Reset();
			}
			else
			{
				PosesAtValueOffsetB.Reset();
			}

			ValueOffsetLast -= DataCardinality;
		}
	}

	if (ValueOffsetLast + DataCardinality != Values.Num())
	{
		// resizing the Values array  
		Values.SetNum(ValueOffsetLast + DataCardinality);
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
// FPoseMetadata
FArchive& operator<<(FArchive& Ar, FPoseMetadata& Metadata)
{
	// storing more data than necessary for now to avoid to deal with endiannes
	uint32 ValueOffset = Metadata.GetValueOffset();
	uint32 AssetIndex = Metadata.GetAssetIndex();
	bool bInBlockTransition = Metadata.IsBlockTransition();
	FFloat16 CostAddend = Metadata.CostAddend;
	
	// @todo: optimize the archived size of FPoseMetadata, since most members are bitfields
	Ar << ValueOffset;
	Ar << AssetIndex;
	Ar << bInBlockTransition;
	Ar << CostAddend;

	Metadata = FPoseMetadata(ValueOffset, AssetIndex, bInBlockTransition, CostAddend);
	return Ar;
}

//////////////////////////////////////////////////////////////////////////
// FSearchIndexAsset
FFloatInterval FSearchIndexAsset::GetExtrapolationTimeInterval(int32 SchemaSampleRate, const FFloatInterval& AdditionalExtrapolationTime) const
{
	return FFloatInterval(FirstSampleIdx / float(SchemaSampleRate) + AdditionalExtrapolationTime.Min,
		LastSampleIdx / float(SchemaSampleRate) + AdditionalExtrapolationTime.Max);
}

bool FSearchIndexAsset::operator==(const FSearchIndexAsset& Other) const
{
	return
		SourceAssetIdx == Other.SourceAssetIdx &&
		bMirrored == Other.bMirrored &&
		bLooping == Other.bLooping &&
		bDisableReselection == Other.bDisableReselection &&
		PermutationIdx == Other.PermutationIdx &&
		BlendParameterX == Other.BlendParameterX &&
		BlendParameterY == Other.BlendParameterY &&
		FirstPoseIdx == Other.FirstPoseIdx &&
		FirstSampleIdx == Other.FirstSampleIdx &&
		LastSampleIdx == Other.LastSampleIdx;
}

FArchive& operator<<(FArchive& Ar, FSearchIndexAsset& IndexAsset)
{
	int32 SourceAssetIdx = IndexAsset.SourceAssetIdx;
	bool bMirrored = IndexAsset.bMirrored; 
	bool bLooping = IndexAsset.bLooping;
	bool bDisableReselection = IndexAsset.bDisableReselection;
	int32 PermutationIdx = IndexAsset.PermutationIdx;
	float BlendParameterX = IndexAsset.BlendParameterX;
	float BlendParameterY = IndexAsset.BlendParameterY;
	int32 FirstPoseIdx = IndexAsset.FirstPoseIdx;
	int32 FirstSampleIdx = IndexAsset.FirstSampleIdx;
	int32 LastSampleIdx = IndexAsset.LastSampleIdx;

	// @todo: optimize the archived size of FSearchIndexAsset, since most members are bitfields
	Ar << SourceAssetIdx;
	Ar << bMirrored;
	Ar << bLooping;
	Ar << bDisableReselection;
	Ar << PermutationIdx;
	Ar << BlendParameterX;
	Ar << BlendParameterY;
	Ar << FirstPoseIdx;
	Ar << FirstSampleIdx;
	Ar << LastSampleIdx;

	new(&IndexAsset) FSearchIndexAsset(SourceAssetIdx, bMirrored, bLooping, bDisableReselection,
		PermutationIdx, FVector(BlendParameterX, BlendParameterY, 0.f), FirstPoseIdx, FirstSampleIdx, LastSampleIdx);

	return Ar;
}

//////////////////////////////////////////////////////////////////////////
// FSearchStats
bool FSearchStats::operator==(const FSearchStats& Other) const
{
	return
		AverageSpeed == Other.AverageSpeed &&
		MaxSpeed == Other.MaxSpeed &&
		AverageAcceleration == Other.AverageAcceleration &&
		MaxAcceleration == Other.MaxAcceleration;
}

FArchive& operator<<(FArchive& Ar, FSearchStats& Stats)
{
	Ar << Stats.AverageSpeed;
	Ar << Stats.MaxSpeed;
	Ar << Stats.AverageAcceleration;
	Ar << Stats.MaxAcceleration;
	return Ar;
}

//////////////////////////////////////////////////////////////////////////
// FPoseSearchBaseIndex
const FSearchIndexAsset& FSearchIndexBase::GetAssetForPose(int32 PoseIdx) const
{
	const uint32 AssetIndex = PoseMetadata[PoseIdx].GetAssetIndex();
	check(Assets[AssetIndex].IsPoseInRange(PoseIdx));
	return Assets[AssetIndex];
}

const FSearchIndexAsset* FSearchIndexBase::GetAssetForPoseSafe(int32 PoseIdx) const
{
	if (PoseMetadata.IsValidIndex(PoseIdx))
	{
		const uint32 AssetIndex = PoseMetadata[PoseIdx].GetAssetIndex();
		if (Assets.IsValidIndex(AssetIndex))
		{
			return &Assets[AssetIndex];
		}
	}
	return nullptr;
}

bool FSearchIndexBase::IsEmpty() const
{
	return Assets.IsEmpty() || PoseMetadata.IsEmpty();
}

void FSearchIndexBase::Reset()
{
	*this = FSearchIndexBase();
}

void FSearchIndexBase::PruneDuplicateValues(float SimilarityThreshold, int32 DataCardinality, bool bDoNotGenerateValuesVectorToPoseIndexes)
{
	ValuesVectorToPoseIndexes = FSparsePoseMultiMap<int32>();

	const int32 NumPoses = GetNumPoses();
	if (SimilarityThreshold > 0.f && NumPoses >= 2)
	{
		TArray<FPosePairSimilarity> PosePairSimilarities;
		if (CalculateSimilarities(PosePairSimilarities, SimilarityThreshold, DataCardinality, NumPoses, Values,
			[this](int32 PoseIdx, int32 DataCardinality) { return GetPoseValuesBase(PoseIdx, DataCardinality); }))
		{
			PruneValues(DataCardinality, NumPoses, PosePairSimilarities, Values,
			[this](int32 PoseIdx) {	return PoseMetadata[PoseIdx].GetValueOffset(); },
			[this](int32 PoseIdx, uint32 ValueOffset) {	PoseMetadata[PoseIdx].SetValueOffset(ValueOffset); });
		}

		if (!bDoNotGenerateValuesVectorToPoseIndexes)
		{
			TMap<int32, TArray<int32>> ValuesVectorToPoseIndexesMap;
			ValuesVectorToPoseIndexesMap.Reserve(NumPoses);
			for (int32 PoseIdx = 0; PoseIdx < NumPoses; ++PoseIdx)
			{
				const FPoseMetadata& Metadata = PoseMetadata[PoseIdx];
				check(Metadata.GetValueOffset() % DataCardinality == 0);
				const int32 ValuesVectorIdx = Metadata.GetValueOffset() / DataCardinality;

				TArray<int32>& PoseIndexes = ValuesVectorToPoseIndexesMap.FindOrAdd(ValuesVectorIdx);
				PoseIndexes.Add(PoseIdx);
			}

			// sorting ValuesVectorToPoseIndexesMap keys to create a deterministic FSparsePoseMultiMap later on
			// we're not using TSortedMap for performance reasons, because ValuesVectorToPoseIndexesMap can be quite big
			TArray<int32> SortedKeys;
			SortedKeys.Reserve(ValuesVectorToPoseIndexesMap.Num());
			for (const TPair<int32, TArray<int32>>& Pair : ValuesVectorToPoseIndexesMap)
			{
				SortedKeys.Add(Pair.Key);
			}
			SortedKeys.Sort();

			FSparsePoseMultiMap<int32> SparsePoseMultiMap(ValuesVectorToPoseIndexesMap.Num(), NumPoses - 1);
			for (const int32& Key : SortedKeys)
			{
				const int32 PCAValuesVectorIdx = Key;
				const TArray<int32>& PoseIndexes = ValuesVectorToPoseIndexesMap[Key];
				SparsePoseMultiMap.Insert(PCAValuesVectorIdx, PoseIndexes);
			}

			for (int32 ValuesVectorIdx = 0; ValuesVectorIdx < SparsePoseMultiMap.Num(); ++ValuesVectorIdx)
			{
				const TConstArrayView<int32> PoseIndexes = SparsePoseMultiMap[ValuesVectorIdx];
				const TArray<int32>& TestPoseIndexes = ValuesVectorToPoseIndexesMap[ValuesVectorIdx];
				check(PoseIndexes == TestPoseIndexes);
			}

			ValuesVectorToPoseIndexes = SparsePoseMultiMap;
		}
	}
}

void FSearchIndexBase::AllocateData(int32 DataCardinality, int32 NumPoses)
{
	Values.Reset();
	PoseMetadata.Reset();

	Values.SetNumZeroed(DataCardinality * NumPoses);
	PoseMetadata.SetNumZeroed(NumPoses);
}

bool FSearchIndexBase::operator==(const FSearchIndexBase& Other) const
{
	return
		Values == Other.Values &&
		ValuesVectorToPoseIndexes == Other.ValuesVectorToPoseIndexes &&
		PoseMetadata == Other.PoseMetadata &&
		bAnyBlockTransition == Other.bAnyBlockTransition &&
		Assets == Other.Assets &&
		MinCostAddend == Other.MinCostAddend &&
		Stats == Other.Stats;
}

FArchive& operator<<(FArchive& Ar, FSearchIndexBase& Index)
{
	Ar << Index.Values;
	Ar << Index.ValuesVectorToPoseIndexes;
	Ar << Index.PoseMetadata;
	Ar << Index.bAnyBlockTransition;
	Ar << Index.Assets;
	Ar << Index.MinCostAddend;
	Ar << Index.Stats;
	return Ar;
}

//////////////////////////////////////////////////////////////////////////
// FSearchIndex
FSearchIndex::FSearchIndex(const FSearchIndex& Other)
	: FSearchIndexBase(Other)
	, WeightsSqrt(Other.WeightsSqrt)
	, PCAValues(Other.PCAValues)
	, PCAValuesVectorToPoseIndexes(Other.PCAValuesVectorToPoseIndexes)
	, PCAProjectionMatrix(Other.PCAProjectionMatrix)
	, Mean(Other.Mean)
	, KDTree(Other.KDTree)
	, VPTree(Other.VPTree)
	, PCAExplainedVariance(Other.PCAExplainedVariance)
{
	check(!PCAValues.IsEmpty() || KDTree.DataSource.PointCount == 0);
	KDTree.DataSource.Data = PCAValues.IsEmpty() ? nullptr : PCAValues.GetData();
}

FSearchIndex& FSearchIndex::operator=(const FSearchIndex& Other)
{
	if (this != &Other)
	{
		this->~FSearchIndex();
		new(this)FSearchIndex(Other);
	}
	return *this;
}

void FSearchIndex::Reset()
{
	FSearchIndex Default;
	*this = Default;
}

TConstArrayView<float> FSearchIndex::GetPoseValues(int32 PoseIdx) const
{
	return GetPoseValuesBase(PoseIdx, GetNumDimensions());
}

TConstArrayView<float> FSearchIndex::GetReconstructedPoseValues(int32 PoseIdx, TArrayView<float> BufferUsedForReconstruction) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_PoseSearch_PCAReconstruct);

	// @todo: reconstruction is not yet supported with pruned PCAValues
	check(PCAValuesVectorToPoseIndexes.Num() == 0);

	const int32 NumDimensions = GetNumDimensions();
	const int32 NumPoses = GetNumPoses();
	check(PoseIdx >= 0 && PoseIdx < NumPoses && NumDimensions > 0);
	check(BufferUsedForReconstruction.Num() == NumDimensions);

	const int32 NumberOfPrincipalComponents = PCAValues.Num() / NumPoses;
	
	// NoTe: if one of these checks trigger, most likely PCAValuesPruningSimilarityThreshold > 0.f and we pruned some PCAValues.
	// currently GetReconstructedPoseValues is not supported with PCAValues pruning
	check(NumPoses * NumberOfPrincipalComponents == PCAValues.Num());
	check(PCAProjectionMatrix.Num() == NumDimensions * NumberOfPrincipalComponents);

	const RowMajorVectorMapConst MapWeightsSqrt(WeightsSqrt.GetData(), 1, NumDimensions);
	const ColMajorMatrixMapConst MapPCAProjectionMatrix(PCAProjectionMatrix.GetData(), NumDimensions, NumberOfPrincipalComponents);
	const RowMajorVectorMapConst MapMean(Mean.GetData(), 1, NumDimensions);
	const RowMajorMatrixMapConst MapPCAValues(PCAValues.GetData(), NumPoses, NumberOfPrincipalComponents);

	const RowMajorVector ReciprocalWeightsSqrt = MapWeightsSqrt.cwiseInverse();
	const RowMajorVector WeightedReconstructedValues = MapPCAValues.row(PoseIdx) * MapPCAProjectionMatrix.transpose() + MapMean;

	RowMajorVectorMap ReconstructedPoseValues(BufferUsedForReconstruction.GetData(), 1, NumDimensions);
	ReconstructedPoseValues = WeightedReconstructedValues.array() * ReciprocalWeightsSqrt.array();

	return BufferUsedForReconstruction;
}

int32 FSearchIndex::GetNumDimensions() const
{
	return WeightsSqrt.Num();
}

int32 FSearchIndex::GetNumberOfPrincipalComponents() const
{
	const int32 NumDimensions = GetNumDimensions();
	check(NumDimensions > 0 && PCAProjectionMatrix.Num() > 0 && PCAProjectionMatrix.Num() % NumDimensions == 0);

	const int32 NumberOfPrincipalComponents = PCAProjectionMatrix.Num() / NumDimensions;
	return NumberOfPrincipalComponents;
}

TConstArrayView<float> FSearchIndex::PCAProject(TConstArrayView<float> PoseValues, TArrayView<float> BufferUsedForProjection) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_PoseSearch_PCAProject);

	const int32 NumDimensions = GetNumDimensions();
	const int32 NumberOfPrincipalComponents = GetNumberOfPrincipalComponents();
	check(PoseValues.Num() == NumDimensions);
	check(BufferUsedForProjection.Num() == NumberOfPrincipalComponents);

	const RowMajorVectorMapConst WeightsSqrtMap(WeightsSqrt.GetData(), 1, NumDimensions);
	const RowMajorVectorMapConst MeanMap(Mean.GetData(), 1, NumDimensions);
	const ColMajorMatrixMapConst PCAProjectionMatrixMap(PCAProjectionMatrix.GetData(), NumDimensions, NumberOfPrincipalComponents);
	const RowMajorVectorMapConst PoseValuesMap(PoseValues.GetData(), 1, NumDimensions);

	RowMajorVectorMap WeightedPoseValuesMap((float*)FMemory_Alloca(NumDimensions * sizeof(float)), 1, NumDimensions);
	WeightedPoseValuesMap = PoseValuesMap.array() * WeightsSqrtMap.array();

	RowMajorVectorMap CenteredPoseValuesMap((float*)FMemory_Alloca(NumDimensions * sizeof(float)), 1, NumDimensions);
	CenteredPoseValuesMap.noalias() = WeightedPoseValuesMap - MeanMap;

	RowMajorVectorMap ProjectedPoseValuesMap(BufferUsedForProjection.GetData(), 1, NumberOfPrincipalComponents);
	ProjectedPoseValuesMap.noalias() = CenteredPoseValuesMap * PCAProjectionMatrixMap;

	return BufferUsedForProjection;
}

void FSearchIndex::PrunePCAValuesFromBlockTransitionPoses(int32 NumberOfPrincipalComponents)
{
	if (!bAnyBlockTransition)
	{
		return;
	}

	check(PCAValues.Num() % NumberOfPrincipalComponents == 0);
	const int32 NumPCAValuesVectors = PCAValues.Num() / NumberOfPrincipalComponents;

	TArray<TPair<int32, TArray<int32>>> PrunedPCAValuesVectorToPoseIndexes;
	TAlignedArray<float> PrunedPCAValues;
	PrunedPCAValues.Reserve(PCAValues.Num());

	if (PCAValuesVectorToPoseIndexes.Num() > 0)
	{
		TArray<int32> PCAValuesVectorIdxPoseIndexes;
		for (int32 PCAValuesVectorIdx = 0; PCAValuesVectorIdx < NumPCAValuesVectors; ++PCAValuesVectorIdx)
		{
			PCAValuesVectorIdxPoseIndexes.Reset();
			for (int32 PoseIdx : PCAValuesVectorToPoseIndexes[PCAValuesVectorIdx])
			{
				if (!PoseMetadata[PoseIdx].IsBlockTransition())
				{
					PCAValuesVectorIdxPoseIndexes.Add(PoseIdx);
				}
			}

			if (!PCAValuesVectorIdxPoseIndexes.IsEmpty())
			{
				PrunedPCAValuesVectorToPoseIndexes.Emplace(PrunedPCAValuesVectorToPoseIndexes.Num(), PCAValuesVectorIdxPoseIndexes);
				PrunedPCAValues.Append(GetPCAPoseValues(PCAValuesVectorIdx));
			}
		}
	}
	else
	{
		for (int32 PCAValuesVectorIdx = 0; PCAValuesVectorIdx < NumPCAValuesVectors; ++PCAValuesVectorIdx)
		{
			// here there's a 1:1 mapping between PCAValuesVectorIdx and PoseIdx
			
			const int32 PoseIdx = PCAValuesVectorIdx;
			if (!PoseMetadata[PoseIdx].IsBlockTransition())
			{
				TConstArrayView<int32> PCAValuesVectorIdxPoseIndexes = MakeArrayView(&PoseIdx, 1);
				PrunedPCAValuesVectorToPoseIndexes.Emplace(PrunedPCAValuesVectorToPoseIndexes.Num(), PCAValuesVectorIdxPoseIndexes);
				PrunedPCAValues.Append(GetPCAPoseValues(PCAValuesVectorIdx));
			}
		}
	}

	PCAValues = PrunedPCAValues;
	PCAValuesVectorToPoseIndexes = FSparsePoseMultiMap<int32>(PrunedPCAValuesVectorToPoseIndexes.Num(), GetNumPoses() - 1);
	for (const TPair<int32, TArray<int32>>& Pair : PrunedPCAValuesVectorToPoseIndexes)
	{
		PCAValuesVectorToPoseIndexes.Insert(Pair.Key, Pair.Value);
	}
}

void FSearchIndex::PruneDuplicatePCAValues(float SimilarityThreshold, int32 NumberOfPrincipalComponents)
{
	PCAValuesVectorToPoseIndexes = FSparsePoseMultiMap<int32>();

	const uint32 NumPoses = GetNumPoses();
	if (SimilarityThreshold > 0.f && NumPoses >= 2 && NumberOfPrincipalComponents > 0)
	{
		check(PCAValues.Num() % NumberOfPrincipalComponents == 0);
		const int32 NumPCAValuesVectors = PCAValues.Num() / NumberOfPrincipalComponents;
		// so far we support only pruning an original PCAValues set, where there's a 1:1 mapping between PCAValuesVectors and Poses
		check(NumPCAValuesVectors == NumPoses);

		TArray<int32> PoseToPCAValueOffset;
		PoseToPCAValueOffset.AddUninitialized(NumPoses);
		for (uint32 PoseIdx = 0; PoseIdx < NumPoses; ++PoseIdx)
		{
			PoseToPCAValueOffset[PoseIdx] = PoseIdx * NumberOfPrincipalComponents;
		}

		TArray<FPosePairSimilarity> PosePairSimilarities;
		if (CalculateSimilarities(PosePairSimilarities, SimilarityThreshold, NumberOfPrincipalComponents, NumPoses, PCAValues,
			[this, &PoseToPCAValueOffset](int32 PoseIdx, int32 NumberOfPrincipalComponents) { return MakeArrayView(&PCAValues[PoseToPCAValueOffset[PoseIdx]], NumberOfPrincipalComponents); }))
		{
			if (PruneValues(NumberOfPrincipalComponents, NumPoses, PosePairSimilarities, PCAValues,
				[&PoseToPCAValueOffset](int32 PoseIdx) { return PoseToPCAValueOffset[PoseIdx]; },
				[&PoseToPCAValueOffset](int32 PoseIdx, int32 ValueOffset) { PoseToPCAValueOffset[PoseIdx] = ValueOffset; }))
			{
				// we pruned some PCAValues: we need to construct a mapping between PCAValuesVectorIdx to PoseIdx(s)
				TMap<int32, TArray<int32>> PCAValuesVectorToPoseIndexesMap;
				PCAValuesVectorToPoseIndexesMap.Reserve(NumPoses);
				for (uint32 PoseIdx = 0; PoseIdx < NumPoses; ++PoseIdx)
				{
					check(PoseToPCAValueOffset[PoseIdx] % NumberOfPrincipalComponents == 0);
					const int32 PCAValuesVectorIdx = PoseToPCAValueOffset[PoseIdx] / NumberOfPrincipalComponents;
					TArray<int32>& PoseIndexes = PCAValuesVectorToPoseIndexesMap.FindOrAdd(PCAValuesVectorIdx);
					check(!PoseIndexes.Contains(PoseIdx));
					PoseIndexes.Add(PoseIdx);
				}

				// sorting PCAValuesVectorToPoseIndexesMap keys to create a deterministic FSparsePoseMultiMap later on
				// we're not using TSortedMap for performance reasons, because PCAValuesVectorToPoseIndexesMap can be quite big
				TArray<int32> SortedKeys;
				SortedKeys.Reserve(PCAValuesVectorToPoseIndexesMap.Num());
				for (const TPair<int32, TArray<int32>>& Pair : PCAValuesVectorToPoseIndexesMap)
				{
					SortedKeys.Add(Pair.Key);
				}
				SortedKeys.Sort();

				FSparsePoseMultiMap<int32> SparsePoseMultiMap(PCAValuesVectorToPoseIndexesMap.Num(), NumPoses - 1);
				for (const int32& Key : SortedKeys)
				{
					const int32 PCAValuesVectorIdx = Key;
					const TArray<int32>& PoseIndexes = PCAValuesVectorToPoseIndexesMap[Key];
					SparsePoseMultiMap.Insert(PCAValuesVectorIdx, PoseIndexes);
				}

				for (int32 PCAValuesVectorIdx = 0; PCAValuesVectorIdx < SparsePoseMultiMap.Num(); ++PCAValuesVectorIdx)
				{
					const TConstArrayView<int32> PoseIndexes = SparsePoseMultiMap[PCAValuesVectorIdx];
					const TArray<int32>& TestPoseIndexes = PCAValuesVectorToPoseIndexesMap[PCAValuesVectorIdx];
					check(PoseIndexes == TestPoseIndexes);
				}

				PCAValuesVectorToPoseIndexes = SparsePoseMultiMap;
			}
		}
	}
}

TArray<float> FSearchIndex::GetPoseValuesSafe(int32 PoseIdx) const
{
	TArray<float> PoseValues;
	if (PoseIdx >= 0 && PoseIdx < GetNumPoses())
	{
		if (IsValuesEmpty())
		{
			const int32 NumDimensions = GetNumDimensions();
			PoseValues.SetNumUninitialized(NumDimensions);
			GetReconstructedPoseValues(PoseIdx, PoseValues);
		}
		else
		{
			PoseValues = GetPoseValues(PoseIdx);
		}
	}
	return PoseValues;
}

TConstArrayView<float> FSearchIndex::GetPCAPoseValues(int32 PCAValuesVectorIdx) const
{
	if (PCAValues.IsEmpty())
	{
		return TConstArrayView<float>();
	}

	const int32 NumDimensions = GetNumDimensions();
	const int32 NumberOfPrincipalComponents = GetNumberOfPrincipalComponents();

#if DO_CHECK
	check(PCAValues.Num() % NumberOfPrincipalComponents == 0);
	const int32 NumPCAValuesVectors = PCAValues.Num() / NumberOfPrincipalComponents;
	check(PCAValuesVectorIdx >= 0 && PCAValuesVectorIdx < NumPCAValuesVectors );
#endif // DO_CHECK

	const int32 ValueOffset = PCAValuesVectorIdx * NumberOfPrincipalComponents;
	return MakeArrayView(&PCAValues[ValueOffset], NumberOfPrincipalComponents);
}

FPoseSearchCost FSearchIndex::ComparePoses(int32 PoseIdx, float ContinuingPoseCostBias, TConstArrayView<float> PoseValues, TConstArrayView<float> QueryValues) const
{
	// base dissimilarity cost representing how the associated PoseIdx differ, in a weighted way, from the query pose (QueryValues)
	const float DissimilarityCost = CompareFeatureVectors(PoseValues, QueryValues, WeightsSqrt);

	// cost addend associated to Schema->BaseCostBias or overriden by UAnimNotifyState_PoseSearchModifyCost
	const float NotifyAddend = PoseMetadata[PoseIdx].GetCostAddend();
	return FPoseSearchCost(DissimilarityCost, NotifyAddend, ContinuingPoseCostBias);
}

FPoseSearchCost FSearchIndex::CompareAlignedPoses(int32 PoseIdx, float ContinuingPoseCostBias, TConstArrayView<float> PoseValues, TConstArrayView<float> QueryValues) const
{
	// base dissimilarity cost representing how the associated PoseIdx differ, in a weighted way, from the query pose (QueryValues)
	const float DissimilarityCost = CompareAlignedFeatureVectors(PoseValues, QueryValues, WeightsSqrt);

	// cost addend associated to Schema->BaseCostBias or overriden by UAnimNotifyState_PoseSearchModifyCost
	const float NotifyAddend = PoseMetadata[PoseIdx].GetCostAddend();
	return FPoseSearchCost(DissimilarityCost, NotifyAddend, ContinuingPoseCostBias);
}

void FSearchIndex::GetPoseToPCAValuesVectorIndexes(TArray<uint32>& PoseToPCAValuesVectorIndexes) const
{
	if (PCAValuesVectorToPoseIndexes.Num() > 0)
	{
		PoseToPCAValuesVectorIndexes.Init(INDEX_NONE, PCAValuesVectorToPoseIndexes.MaxValue + 1);
		for (int32 PCAValuesVectorIdx = 0; PCAValuesVectorIdx < PCAValuesVectorToPoseIndexes.Num(); ++PCAValuesVectorIdx)
		{
			for (int32 PoseIdx : PCAValuesVectorToPoseIndexes[PCAValuesVectorIdx])
			{
				PoseToPCAValuesVectorIndexes[PoseIdx] = PCAValuesVectorIdx;
			}
		}

		// if block transition pruning is enabled we could have poses without their PCAValuesVectorIdx counterpart
		// check(!PoseToPCAValuesVectorIndexes.Contains(INDEX_NONE));
	}
	else
	{
		PoseToPCAValuesVectorIndexes.Reset();
	}
}

bool FSearchIndex::operator==(const FSearchIndex& Other) const
{
	return
		FSearchIndexBase::operator==(Other) &&
		WeightsSqrt == Other.WeightsSqrt &&
		PCAValues == Other.PCAValues &&
		PCAValuesVectorToPoseIndexes == Other.PCAValuesVectorToPoseIndexes &&
		PCAProjectionMatrix == Other.PCAProjectionMatrix &&
		Mean == Other.Mean &&
		PCAExplainedVariance == Other.PCAExplainedVariance &&
		KDTree == Other.KDTree &&
		VPTree == Other.VPTree;
}

FArchive& operator<<(FArchive& Ar, FSearchIndex& Index)
{
	Ar << static_cast<FSearchIndexBase&>(Index);

	Ar << Index.WeightsSqrt;
	Ar << Index.PCAValues;
	Ar << Index.PCAValuesVectorToPoseIndexes;
	Ar << Index.PCAProjectionMatrix;
	Ar << Index.Mean;
	Ar << Index.PCAExplainedVariance;
	Ar << Index.VPTree;

	Serialize(Ar, Index.KDTree, Index.PCAValues.GetData());
	return Ar;
}

} // namespace UE::PoseSearch
