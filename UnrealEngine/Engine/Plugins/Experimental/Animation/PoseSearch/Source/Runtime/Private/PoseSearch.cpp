// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearch.h"
#include "PoseSearch/PoseSearchAnimNotifies.h"

#include "PoseSearchDerivedData.h"
#include "PoseSearchEigenHelper.h"

#include "Algo/BinarySearch.h"
#include "Async/ParallelFor.h"
#include "Templates/IdentityFunctor.h"
#include "Templates/Invoke.h"
#include "Templates/Less.h"
#include "Features/IModularFeatures.h"
#include "DrawDebugHelpers.h"
#include "Animation/AnimPoseSearchProvider.h"
#include "Animation/AnimCurveTypes.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimMetaData.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/MirrorDataTable.h"
#include "AnimationRuntime.h"
#include "BonePose.h"
#include "Trace/PoseSearchTraceLogger.h"
#include "UObject/ObjectSaveContext.h"
#include "Misc/MemStack.h"

#include "Animation/BuiltInAttributeTypes.h"
#include "Animation/AnimRootMotionProvider.h"
#include "Animation/BlendSpace1D.h"


#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"
#include "PoseSearch/PoseSearchDerivedDataKey.h"
#endif // WITH_EDITOR

IMPLEMENT_ANIMGRAPH_MESSAGE(UE::PoseSearch::IPoseHistoryProvider);

#define LOCTEXT_NAMESPACE "PoseSearch"

DEFINE_LOG_CATEGORY(LogPoseSearch);

DECLARE_STATS_GROUP(TEXT("PoseSearch"), STATGROUP_PoseSearch, STATCAT_Advanced);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Search Brute Force"), STAT_PoseSearchBruteForce, STATGROUP_PoseSearch, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Search PCA/KNN"), STAT_PoseSearchPCAKNN, STATGROUP_PoseSearch, );
DEFINE_STAT(STAT_PoseSearchBruteForce);
DEFINE_STAT(STAT_PoseSearchPCAKNN);

namespace UE::PoseSearch
{

//////////////////////////////////////////////////////////////////////////
// Constants and utilities

// @todo: set UE_POSE_SEARCH_FORCE_SINGLE_THREAD to 0
// Temporarily disable parallel indexing due to a bug
// A single BoneContainer is being used while indexing assets, but some
// const BoneContainer methods change mutable lookup tables.
#define UE_POSE_SEARCH_FORCE_SINGLE_THREAD 1

#ifndef UE_POSE_SEARCH_FORCE_SINGLE_THREAD
#define UE_POSE_SEARCH_FORCE_SINGLE_THREAD 0
#endif

#if UE_POSE_SEARCH_FORCE_SINGLE_THREAD
	constexpr EParallelForFlags ParallelForFlags = EParallelForFlags::ForceSingleThread;
#else
	constexpr EParallelForFlags ParallelForFlags = EParallelForFlags::None;
#endif // UE_POSE_SEARCH_FORCE_SINGLE_THREAD

static inline float ArraySum(TConstArrayView<float> View, int32 StartIndex, int32 Offset)
{
	float Sum = 0.f;
	const int32 EndIndex = StartIndex + Offset;
	for (int i = StartIndex; i < EndIndex; ++i)
	{
		Sum += View[i];
	}
	return Sum;
}

static inline float CompareFeatureVectors(TConstArrayView<float> A, TConstArrayView<float> B, TConstArrayView<float> WeightsSqrt)
{
	check(A.Num() == B.Num() && A.Num() == WeightsSqrt.Num());

	Eigen::Map<const Eigen::ArrayXf> VA(A.GetData(), A.Num());
	Eigen::Map<const Eigen::ArrayXf> VB(B.GetData(), B.Num());
	Eigen::Map<const Eigen::ArrayXf> VW(WeightsSqrt.GetData(), WeightsSqrt.Num());

	return ((VA - VB) * VW).square().sum();
}

static inline float CompareFeatureVectors(TConstArrayView<float> A, TConstArrayView<float> B)
{
	check(A.Num() == B.Num());

	Eigen::Map<const Eigen::ArrayXf> VA(A.GetData(), A.Num());
	Eigen::Map<const Eigen::ArrayXf> VB(B.GetData(), B.Num());

	return (VA - VB).square().sum();
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

static inline bool IsSamplingRangeValid(FFloatInterval Range)
{
	return Range.IsValid() && (Range.Min >= 0.0f);
}

static inline FFloatInterval GetEffectiveSamplingRange(const UAnimSequenceBase* Sequence, FFloatInterval RequestedSamplingRange)
{
	const bool bSampleAll = (RequestedSamplingRange.Min == 0.0f) && (RequestedSamplingRange.Max == 0.0f);
	const float SequencePlayLength = Sequence->GetPlayLength();
	FFloatInterval Range;
	Range.Min = bSampleAll ? 0.0f : RequestedSamplingRange.Min;
	Range.Max = bSampleAll ? SequencePlayLength : FMath::Min(SequencePlayLength, RequestedSamplingRange.Max);
	return Range;
}

/**
* Algo::LowerBound adapted to TIndexedContainerIterator for use with indexable but not necessarily contiguous containers. Used here with TRingBuffer.
*
* Performs binary search, resulting in position of the first element >= Value using predicate
*
* @param First TIndexedContainerIterator beginning of range to search through, must be already sorted by SortPredicate
* @param Last TIndexedContainerIterator end of range
* @param Value Value to look for
* @param SortPredicate Predicate for sort comparison, defaults to <
*
* @returns Position of the first element >= Value, may be position after last element in range
*/
template <typename IteratorType, typename ValueType, typename ProjectionType, typename SortPredicateType>
FORCEINLINE auto LowerBound(IteratorType First, IteratorType Last, const ValueType& Value, ProjectionType Projection, SortPredicateType SortPredicate) -> decltype(First.GetIndex())
{
	using SizeType = decltype(First.GetIndex());

	check(First.GetIndex() <= Last.GetIndex());

	// Current start of sequence to check
	SizeType Start = First.GetIndex();

	// Size of sequence to check
	SizeType Size = Last.GetIndex() - Start;

	// With this method, if Size is even it will do one more comparison than necessary, but because Size can be predicted by the CPU it is faster in practice
	while (Size > 0)
	{
		const SizeType LeftoverSize = Size % 2;
		Size = Size / 2;

		const SizeType CheckIndex = Start + Size;
		const SizeType StartIfLess = CheckIndex + LeftoverSize;

		auto&& CheckValue = Invoke(Projection, *(First + CheckIndex));
		Start = SortPredicate(CheckValue, Value) ? StartIfLess : Start;
	}
	return Start;
}

template <typename IteratorType, typename ValueType, typename SortPredicateType>
FORCEINLINE auto LowerBound(IteratorType First, IteratorType Last, const ValueType& Value, SortPredicateType SortPredicate) -> decltype(First.GetIndex())
{
	return LowerBound(First, Last, Value, FIdentityFunctor(), SortPredicate);
}

template <typename IteratorType, typename ValueType>
FORCEINLINE auto LowerBound(IteratorType First, IteratorType Last, const ValueType& Value) -> decltype(First.GetIndex())
{
	return LowerBound(First, Last, Value, FIdentityFunctor(), TLess<>());
}

static TArrayView<size_t> PopulateNonSelectableIdx(TArrayView<size_t> NonSelectableIdxBuffer, FSearchContext& SearchContext, const UPoseSearchDatabase* Database, TArrayView<const float> QueryValues)
{
	check(Database);

	int32 NonSelectableIdxUsedSize = 0;
	if (SearchContext.IsCurrentResultFromDatabase(Database) && SearchContext.CurrentResult.IsValid() && SearchContext.PoseJumpThresholdTime > 0.f)
	{
		const FPoseSearchIndexAsset* CurrentIndexAsset = SearchContext.CurrentResult.SearchIndexAsset;
		const int32 PoseJumpIndexThreshold = FMath::FloorToInt(SearchContext.PoseJumpThresholdTime / Database->Schema->GetSamplingInterval());
		const bool IsLooping = Database->IsSourceAssetLooping(CurrentIndexAsset);

		for (int32 i = -PoseJumpIndexThreshold; i <= -1; ++i)
		{
			const int32 PoseIdx = SearchContext.CurrentResult.PoseIdx + i;
			const float DeltaTime = i * Database->Schema->GetSamplingInterval();

			// @todo: should we use the quantized time associated to InOutMotionMatchingState.DbPoseIdx instead of InOutMotionMatchingState.AssetPlayerTime?
			float PoseAssetPlayerTime = SearchContext.CurrentResult.AssetTime + DeltaTime;
			if (IsLooping)
			{
				while (PoseAssetPlayerTime < CurrentIndexAsset->SamplingInterval.Min)
				{
					PoseAssetPlayerTime += CurrentIndexAsset->SamplingInterval.Size();
				}
			}

			if (CurrentIndexAsset->SamplingInterval.Contains(PoseAssetPlayerTime))
			{
				if (NonSelectableIdxUsedSize < NonSelectableIdxBuffer.Num())
				{
					NonSelectableIdxBuffer[NonSelectableIdxUsedSize++] = PoseIdx;

#if UE_POSE_SEARCH_TRACE_ENABLED
					const FPoseSearchCost PoseCost = Database->ComparePoses(SearchContext, PoseIdx, EPoseComparisonFlags::None, QueryValues);
					SearchContext.BestCandidates.Add(PoseCost, PoseIdx, Database, EPoseCandidateFlags::DiscardedBy_PoseJumpThresholdTime);
#endif
				}
				else
				{
					UE_LOG(LogPoseSearch, Warning, TEXT("PopulateNonSelectableIdx couldn't add all the NonSelectableIdx"));
					TArrayView<size_t> NonSelectableIdx(NonSelectableIdxBuffer.GetData(), NonSelectableIdxUsedSize);
					NonSelectableIdx.Sort();
					return NonSelectableIdx;
				}
			}
			else
			{
				break;
			}
		}

		for (int32 i = 0; i <= PoseJumpIndexThreshold; ++i)
		{
			const int32 PoseIdx = SearchContext.CurrentResult.PoseIdx + i;
			const float DeltaTime = i * Database->Schema->GetSamplingInterval();

			// @todo: should we use the quantized time associated to InOutMotionMatchingState.DbPoseIdx instead of InOutMotionMatchingState.AssetPlayerTime?
			float PoseAssetPlayerTime = SearchContext.CurrentResult.AssetTime + DeltaTime;
			if (IsLooping)
			{
				while (PoseAssetPlayerTime > CurrentIndexAsset->SamplingInterval.Max)
				{
					PoseAssetPlayerTime -= CurrentIndexAsset->SamplingInterval.Size();
				}
			}

			if (CurrentIndexAsset->SamplingInterval.Contains(PoseAssetPlayerTime))
			{
				if (NonSelectableIdxUsedSize < NonSelectableIdxBuffer.Num())
				{
					NonSelectableIdxBuffer[NonSelectableIdxUsedSize++] = PoseIdx;

#if UE_POSE_SEARCH_TRACE_ENABLED
					const FPoseSearchCost PoseCost = Database->ComparePoses(SearchContext, PoseIdx, EPoseComparisonFlags::None, QueryValues);
					SearchContext.BestCandidates.Add(PoseCost, PoseIdx, Database, EPoseCandidateFlags::DiscardedBy_PoseJumpThresholdTime);
#endif
				}
				else
				{
					UE_LOG(LogPoseSearch, Warning, TEXT("PopulateNonSelectableIdx couldn't add all the NonSelectableIdx"));
					TArrayView<size_t> NonSelectableIdx(NonSelectableIdxBuffer.GetData(), NonSelectableIdxUsedSize);
					NonSelectableIdx.Sort();
					return NonSelectableIdx;
				}
			}
			else
			{
				break;
			}
		}
	}

	if (SearchContext.PoseIndicesHistory)
	{
		const FObjectKey DatabaseKey(Database);
		for (auto It = SearchContext.PoseIndicesHistory->IndexToTime.CreateConstIterator(); It; ++It)
		{
			const FHistoricalPoseIndex& HistoricalPoseIndex = It.Key();
			if (HistoricalPoseIndex.DatabaseKey == DatabaseKey)
			{
				if (NonSelectableIdxUsedSize < NonSelectableIdxBuffer.Num())
				{
					NonSelectableIdxBuffer[NonSelectableIdxUsedSize++] = HistoricalPoseIndex.PoseIndex;

#if UE_POSE_SEARCH_TRACE_ENABLED
					check(HistoricalPoseIndex.PoseIndex >= 0);
					const FPoseSearchIndex* SearchIndex = Database->GetSearchIndex();
					check(SearchIndex);
					
					// if we're editing the database and removing assets it's possible that the PoseIndicesHistory contains invalid pose indexes
					if (HistoricalPoseIndex.PoseIndex < SearchIndex->NumPoses)
					{
						const FPoseSearchCost PoseCost = Database->ComparePoses(SearchContext, HistoricalPoseIndex.PoseIndex, EPoseComparisonFlags::None, QueryValues);
						SearchContext.BestCandidates.Add(PoseCost, HistoricalPoseIndex.PoseIndex, Database, EPoseCandidateFlags::DiscardedBy_PoseReselectHistory);
					}
#endif
				}
				else
				{
					UE_LOG(LogPoseSearch, Warning, TEXT("PopulateNonSelectableIdx couldn't add all the NonSelectableIdx"));
					TArrayView<size_t> NonSelectableIdx(NonSelectableIdxBuffer.GetData(), NonSelectableIdxUsedSize);
					NonSelectableIdx.Sort();
					return NonSelectableIdx;
				}
			}
		}
	}

	TArrayView<size_t> NonSelectableIdx(NonSelectableIdxBuffer.GetData(), NonSelectableIdxUsedSize);
	NonSelectableIdx.Sort();
	return NonSelectableIdx;
}

} // namespace UE::PoseSearch


//////////////////////////////////////////////////////////////////////////
// UPoseSearchFeatureChannel
void UPoseSearchFeatureChannel::InitializeSchema(UE::PoseSearch::FSchemaInitializer& Initializer)
{
	ChannelIdx = Initializer.GetCurrentChannelIdx();
	ChannelDataOffset = Initializer.GetCurrentChannelDataOffset();
}

#if WITH_EDITOR
void UPoseSearchFeatureChannel::ComputeCostBreakdowns(UE::PoseSearch::ICostBreakDownData& CostBreakDownData, const UPoseSearchSchema* Schema) const
{
	CostBreakDownData.AddEntireBreakDownSection(FText::FromString(GetName()), Schema, ChannelDataOffset, ChannelCardinality);
}
#endif // WITH_EDITOR

// base implementation calculating a single mean deviation value (replicated ChannelCardinality times into MeanDeviations starting at DataOffset index) from all the features data associated to this channel
void UPoseSearchFeatureChannel::ComputeMeanDeviations(const Eigen::MatrixXd& CenteredPoseMatrix, Eigen::VectorXd& MeanDeviations) const
{
	using namespace UE::PoseSearch;

	int32 DataOffset = ChannelDataOffset;
	FFeatureVectorHelper::ComputeMeanDeviations(MinimumMeanDeviation, CenteredPoseMatrix, MeanDeviations, DataOffset, ChannelCardinality);

	check(DataOffset == ChannelDataOffset + ChannelCardinality);
}

//////////////////////////////////////////////////////////////////////////
// UPoseSearchSchema

namespace UE::PoseSearch
{

int32 FSchemaInitializer::AddBoneReference(const FBoneReference& BoneReference)
{
	return BoneReferences.AddUnique(BoneReference);
}

} // namespace UE::PoseSearch

void UPoseSearchSchema::Finalize(bool bRemoveEmptyChannels)
{
	using namespace UE::PoseSearch;

	if (bRemoveEmptyChannels)
	{
		Channels.RemoveAll([](TObjectPtr<UPoseSearchFeatureChannel>& Channel) { return !Channel; });
	}

	BoneReferences.Reset();

	FSchemaInitializer Initializer;
	for (int32 ChannelIdx = 0; ChannelIdx != Channels.Num(); ++ChannelIdx)
	{
		if (Channels[ChannelIdx].Get())
		{
			Initializer.CurrentChannelIdx = ChannelIdx;
			Channels[ChannelIdx]->InitializeSchema(Initializer);
		}
	}

	SchemaCardinality = Initializer.GetCurrentChannelDataOffset();

	BoneReferences = MoveTemp(Initializer.BoneReferences);

	ResolveBoneReferences();
}

void UPoseSearchSchema::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	Finalize();

	Super::PreSave(ObjectSaveContext);
}

void UPoseSearchSchema::PostLoad()
{
	Super::PostLoad();
	ResolveBoneReferences();
}

#if WITH_EDITOR
void UPoseSearchSchema::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Finalize(false);
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UPoseSearchSchema::ComputeCostBreakdowns(UE::PoseSearch::ICostBreakDownData& CostBreakDownData) const
{
	for (const TObjectPtr<UPoseSearchFeatureChannel>& Channel : Channels)
	{
		if (Channel)
		{
			Channel->ComputeCostBreakdowns(CostBreakDownData, this);
		}
	}
}
#endif

bool UPoseSearchSchema::IsValid() const
{
	bool bValid = Skeleton != nullptr;

	for (const FBoneReference& BoneRef : BoneReferences)
	{
		bValid &= BoneRef.HasValidSetup();
	}

	for (const TObjectPtr<UPoseSearchFeatureChannel>& Channel: Channels)
	{
		bValid &= Channel != nullptr;
	}

	bValid &= (BoneReferences.Num() == BoneIndices.Num());

	return bValid;
}

void UPoseSearchSchema::ResolveBoneReferences()
{
	// Initialize references to obtain bone indices
	for (FBoneReference& BoneRef : BoneReferences)
	{
		BoneRef.Initialize(Skeleton);
	}

	// Fill out bone index array
	BoneIndices.SetNum(BoneReferences.Num());
	for (int32 BoneRefIdx = 0; BoneRefIdx != BoneReferences.Num(); ++BoneRefIdx)
	{
		BoneIndices[BoneRefIdx] = BoneReferences[BoneRefIdx].BoneIndex;
	}

	// Build separate index array with parent indices guaranteed to be present. Sort for EnsureParentsPresent.
	BoneIndicesWithParents = BoneIndices;
	BoneIndicesWithParents.Sort();

	if (Skeleton)
	{
		FAnimationRuntime::EnsureParentsPresent(BoneIndicesWithParents, Skeleton->GetReferenceSkeleton());
	}

	// BoneIndicesWithParents should at least contain the root to support mirroring root motion
	if (BoneIndicesWithParents.Num() == 0)
	{
		BoneIndicesWithParents.Add(0);
	}
}


bool UPoseSearchSchema::BuildQuery(UE::PoseSearch::FSearchContext& SearchContext, FPoseSearchFeatureVectorBuilder& InOutQuery) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_PoseSearch_BuildQuery);

	InOutQuery.Init(this);

	bool bSuccess = true;
	for (const TObjectPtr<UPoseSearchFeatureChannel>& Channel : Channels)
	{
		bool bChannelSuccess = Channel->BuildQuery(SearchContext, InOutQuery);
		bSuccess &= bChannelSuccess;
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
// FPoseSearchIndexAsset
// 
const FPoseSearchIndexAsset* FPoseSearchIndex::FindAssetForPose(int32 PoseIdx) const
{
	auto Predicate = [PoseIdx](const FPoseSearchIndexAsset& Asset)
	{
		return Asset.IsPoseInRange(PoseIdx);
	};
	return Assets.FindByPredicate(Predicate);
}

float FPoseSearchIndex::GetAssetTime(int32 PoseIdx, const FPoseSearchIndexAsset* Asset) const
{
	if (!Asset)
	{
		Asset = FindAssetForPose(PoseIdx);
		if (!Asset)
		{
			UE_LOG(LogPoseSearch, Error, TEXT("Couldn't find asset for pose %i in database"), PoseIdx);
			return -1.0f;
		}
	}

	if (!Asset->IsPoseInRange(PoseIdx))
	{
		UE_LOG(LogPoseSearch, Error, TEXT("Pose %i out of range in database"), PoseIdx);
		return -1.0f;
	}

	if (Asset->Type == ESearchIndexAssetType::Sequence)
	{
		const FFloatInterval SamplingRange = Asset->SamplingInterval;

		float AssetTime = FMath::Min(SamplingRange.Min + Schema->GetSamplingInterval() * (PoseIdx - Asset->FirstPoseIdx), SamplingRange.Max);
		return AssetTime;
	}
	
	if (Asset->Type == ESearchIndexAssetType::BlendSpace)
	{
		const FFloatInterval SamplingRange = Asset->SamplingInterval;

		// For BlendSpaces the AssetTime is in the range [0, 1] while the Sampling Range
		// is in real time (seconds)
		float AssetTime = FMath::Min(SamplingRange.Min + Schema->GetSamplingInterval() * (PoseIdx - Asset->FirstPoseIdx), SamplingRange.Max) / (Asset->NumPoses * Schema->GetSamplingInterval());
		return AssetTime;
	}
	
	checkNoEntry();
	return -1.0f;
}


//////////////////////////////////////////////////////////////////////////
// FPoseSearchIndex

bool FPoseSearchIndex::IsValid() const
{
	const bool bSchemaValid = Schema && Schema->IsValid();
	const bool bSearchIndexValid = bSchemaValid && (NumPoses * Schema->SchemaCardinality == Values.Num());

	return bSearchIndexValid;
}

bool FPoseSearchIndex::IsEmpty() const
{
	const bool bEmpty = Assets.Num() == 0 || NumPoses == 0;
	return bEmpty;
}

TConstArrayView<float> FPoseSearchIndex::GetPoseValues(int32 PoseIdx) const
{
	check(PoseIdx >= 0 && PoseIdx < NumPoses && Schema && Schema->SchemaCardinality > 0);
	const int32 ValueOffset = PoseIdx * Schema->SchemaCardinality;
	return MakeArrayView(&Values[ValueOffset], Schema->SchemaCardinality);
}

void FPoseSearchIndex::Reset()
{
	NumPoses = 0;
	Assets.Reset();
	Values.Reset();
	Schema = nullptr;
}

float FPoseSearchIndex::ComputeMirrorMismatchAddend(int32 PoseIdx, UE::PoseSearch::FSearchContext& SearchContext) const
{
	if (SearchContext.QueryMirrorRequest != EPoseSearchBooleanRequest::Indifferent)
	{
		const FPoseSearchIndexAsset* IndexAsset = FindAssetForPose(PoseIdx);
		const bool bMirroringMismatch =
			(IndexAsset->bMirrored && SearchContext.QueryMirrorRequest == EPoseSearchBooleanRequest::FalseValue) ||
			(!IndexAsset->bMirrored && SearchContext.QueryMirrorRequest == EPoseSearchBooleanRequest::TrueValue);
		if (bMirroringMismatch)
		{
			check(Schema);
			return Schema->MirrorMismatchCostBias;
		}
	}
	return 0.f;
}

float FPoseSearchIndex::ComputeNotifyAddend(int32 PoseIdx) const
{
	return PoseMetadata[PoseIdx].CostAddend;
}

float FPoseSearchIndex::ComputeContinuingPoseCostAddend(int32 PoseIdx, UE::PoseSearch::EPoseComparisonFlags PoseComparisonFlags) const
{
	if (EnumHasAnyFlags(PoseComparisonFlags, UE::PoseSearch::EPoseComparisonFlags::ContinuingPose))
	{
		return PoseMetadata[PoseIdx].ContinuingPoseCostAddend;
	}
	return 0.f;
}

//////////////////////////////////////////////////////////////////////////
// UPoseSearchSequenceMetaData

void UPoseSearchSequenceMetaData::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	SearchIndex.Reset();

#if WITH_EDITOR
	if (!IsTemplate())
	{
		if (IsValidForIndexing())
		{
			UObject* Outer = GetOuter();
			if (UAnimSequence* Sequence = Cast<UAnimSequence>(Outer))
			{
				UE::PoseSearch::BuildIndex(Sequence, this);
			}
		}
	}
#endif

	Super::PreSave(ObjectSaveContext);
}

bool UPoseSearchSequenceMetaData::IsValidForIndexing() const
{
	return Schema && Schema->IsValid() && UE::PoseSearch::IsSamplingRangeValid(SamplingRange);
}

bool UPoseSearchSequenceMetaData::IsValidForSearch() const
{
	return IsValidForIndexing() && SearchIndex.IsValid() && !SearchIndex.IsEmpty();
}

UE::PoseSearch::FSearchResult UPoseSearchSequenceMetaData::Search(UE::PoseSearch::FSearchContext& SearchContext) const
{
	using namespace UE::PoseSearch;

	FSearchResult Result;

	if (!ensure(SearchIndex.IsValid() && !SearchIndex.IsEmpty()))
	{
		return Result;
	}

	Schema->BuildQuery(SearchContext, Result.ComposedQuery);
	TArrayView<const float> QueryValues = Result.ComposedQuery.GetValues();

	if (!ensure(QueryValues.Num() == SearchIndex.Schema->SchemaCardinality))
	{
		return Result;
	}

	FPoseSearchCost BestPoseCost;
	int32 BestPoseIdx = INDEX_NONE;
	for (const FPoseSearchIndexAsset& Asset : SearchIndex.Assets)
	{
		const int32 EndIndex = Asset.FirstPoseIdx + Asset.NumPoses;
		for (int32 PoseIdx = Asset.FirstPoseIdx; PoseIdx < EndIndex; ++PoseIdx)
		{
			const FPoseSearchPoseMetadata& Metadata = SearchIndex.PoseMetadata[PoseIdx];

			if (EnumHasAnyFlags(Metadata.Flags, EPoseSearchPoseFlags::BlockTransition))
			{
				continue;
			}

			const FPoseSearchCost PoseCost = ComparePoses(PoseIdx, EPoseComparisonFlags::ContinuingPose, QueryValues);
			if (PoseCost < BestPoseCost)
			{
				BestPoseCost = PoseCost;
				BestPoseIdx = PoseIdx;
			}
		}
	}

	Result.PoseCost = BestPoseCost;
	Result.PoseIdx = BestPoseIdx;
	Result.SearchIndexAsset = SearchIndex.FindAssetForPose(BestPoseIdx);
	Result.AssetTime = SearchIndex.GetAssetTime(BestPoseIdx, Result.SearchIndexAsset);
	Result.Database = nullptr;

	DrawFeatureVector(SearchContext.DebugDrawParams, Result.PoseIdx);
	
	EnumAddFlags(SearchContext.DebugDrawParams.Flags, EDebugDrawFlags::DrawQuery);
	DrawFeatureVector(SearchContext.DebugDrawParams, QueryValues);

	return Result;
}

FPoseSearchCost UPoseSearchSequenceMetaData::ComparePoses(
	int32 PoseIdx,
	UE::PoseSearch::EPoseComparisonFlags PoseComparisonFlags,
	TConstArrayView<float> QueryValues) const
{
	using namespace UE::PoseSearch;

	const float DissimilarityCost = CompareFeatureVectors(SearchIndex.GetPoseValues(PoseIdx), QueryValues);

	// @todo: shouldn't we include MirrorMismatchAddend as well?
	const float MirrorMismatchAddend = 0.f; // SearchIndex.ComputeMirrorMismatchAddend(PoseIdx, SearchContext);
	const float NotifyAddend = SearchIndex.ComputeNotifyAddend(PoseIdx);
	const float ContinuingPoseCostAddend = SearchIndex.ComputeContinuingPoseCostAddend(PoseIdx, PoseComparisonFlags);

	return FPoseSearchCost(DissimilarityCost, NotifyAddend, MirrorMismatchAddend, ContinuingPoseCostAddend);
}

//////////////////////////////////////////////////////////////////////////
// UPoseSearchDatabase

UPoseSearchDatabase::~UPoseSearchDatabase()
{
	if (PrivateDerivedData)
	{
#if WITH_EDITOR
		PrivateDerivedData->CancelCache();
#endif
		delete PrivateDerivedData;
	}
}

FPoseSearchIndex* UPoseSearchDatabase::GetSearchIndex()
{
	if (PrivateDerivedData == nullptr)
	{
		return nullptr;
	}

	return &PrivateDerivedData->SearchIndex;
}

const FPoseSearchIndex* UPoseSearchDatabase::GetSearchIndex() const
{
	return PrivateDerivedData ? &PrivateDerivedData->SearchIndex : nullptr;
}

const FPoseSearchIndex* UPoseSearchDatabase::GetSearchIndexSafe() const
{
	const FPoseSearchIndex* SearchIndex = GetSearchIndex();

	if (!SearchIndex || !SearchIndex->IsValid() || SearchIndex->IsEmpty())
	{
		if (!Schema)
		{
			UE_LOG(LogAnimation, Warning, TEXT("UPoseSearchDatabase %s failed to index. Reason: no Schema!"), *GetName());
		}
		else if (!Schema->IsValid())
		{
			UE_LOG(LogAnimation, Warning, TEXT("UPoseSearchDatabase %s failed to index. Reason: Schema %s is invalid"), *GetName(), *Schema->GetName());
		}
		else
		{
			UE_LOG(LogAnimation, Warning, TEXT("UPoseSearchDatabase %s failed to index. Reason: is there any unsaved modified asset?"), *GetName());
		}

		SearchIndex = nullptr;
	}

	return SearchIndex;
}

float UPoseSearchDatabase::GetAssetTime(int32 PoseIdx, const FPoseSearchIndexAsset* Asset) const
{
	const float AssetTime = GetSearchIndex()->GetAssetTime(PoseIdx, Asset);
	return AssetTime;
}

int32 UPoseSearchDatabase::GetPoseIndexFromTime(float Time, const FPoseSearchIndexAsset* SearchIndexAsset) const
{
	const bool bIsLooping = IsSourceAssetLooping(SearchIndexAsset);
	const FFloatInterval& Range = SearchIndexAsset->SamplingInterval;
	const bool bHasPoseIndex = SearchIndexAsset->FirstPoseIdx != INDEX_NONE && SearchIndexAsset->NumPoses > 0 && (bIsLooping || Range.Contains(Time));
	if (bHasPoseIndex)
	{
		int32 PoseOffset = FMath::RoundToInt(Schema->SampleRate * (Time - Range.Min));
		
		if (PoseOffset < 0)
		{
			if (bIsLooping)
			{
				PoseOffset = (PoseOffset % SearchIndexAsset->NumPoses) + SearchIndexAsset->NumPoses;
			}
			else
			{
				PoseOffset = 0;
			}
		}
		else if (PoseOffset >= SearchIndexAsset->NumPoses)
		{
			if (bIsLooping)
			{
				PoseOffset = PoseOffset % SearchIndexAsset->NumPoses;
			}
			else
			{
				PoseOffset = SearchIndexAsset->NumPoses - 1;
			}
		}

		int32 PoseIdx = SearchIndexAsset->FirstPoseIdx + PoseOffset;
		return PoseIdx;
	}

	return INDEX_NONE;
}

bool UPoseSearchDatabase::GetPoseIndicesAndLerpValueFromTime(float Time, const FPoseSearchIndexAsset* SearchIndexAsset, int32& PrevPoseIdx, int32& PoseIdx, int32& NextPoseIdx, float& LerpValue) const
{
	PoseIdx = GetPoseIndexFromTime(Time, SearchIndexAsset);
	if (PoseIdx == INDEX_NONE)
	{
		PrevPoseIdx = INDEX_NONE;
		NextPoseIdx = INDEX_NONE;
		LerpValue = 0.f;
		return false;
	}

	const FFloatInterval& Range = SearchIndexAsset->SamplingInterval;
	const float FloatPoseOffset = Schema->SampleRate * (Time - Range.Min);
	const int32 PoseOffset = FMath::RoundToInt(FloatPoseOffset);
	LerpValue = FloatPoseOffset - float(PoseOffset);

	const float PrevTime = Time - 1.f / Schema->SampleRate;
	const float NextTime = Time + 1.f / Schema->SampleRate;

	PrevPoseIdx = GetPoseIndexFromTime(PrevTime, SearchIndexAsset);
	if (PrevPoseIdx == INDEX_NONE)
	{
		PrevPoseIdx = PoseIdx;
	}

	NextPoseIdx = GetPoseIndexFromTime(NextTime, SearchIndexAsset);
	if (NextPoseIdx == INDEX_NONE)
	{
		NextPoseIdx = PoseIdx;
	}

	check(LerpValue >= -0.5f && LerpValue <= 0.5f);

	return true;
}

const FPoseSearchDatabaseAnimationAssetBase& UPoseSearchDatabase::GetAnimationSourceAsset(const FPoseSearchIndexAsset* SearchIndexAsset) const
{
	if (SearchIndexAsset->Type == ESearchIndexAssetType::Sequence)
	{
		return Sequences[SearchIndexAsset->SourceAssetIdx];
	}

	if (SearchIndexAsset->Type == ESearchIndexAssetType::BlendSpace)
	{
		return BlendSpaces[SearchIndexAsset->SourceAssetIdx];
	}

	checkNoEntry();
	return Sequences[SearchIndexAsset->SourceAssetIdx];
}

const FPoseSearchDatabaseSequence& UPoseSearchDatabase::GetSequenceSourceAsset(const FPoseSearchIndexAsset* SearchIndexAsset) const
{
	check(SearchIndexAsset->Type == ESearchIndexAssetType::Sequence);
	return Sequences[SearchIndexAsset->SourceAssetIdx];
}

const FPoseSearchDatabaseBlendSpace& UPoseSearchDatabase::GetBlendSpaceSourceAsset(const FPoseSearchIndexAsset* SearchIndexAsset) const
{
	check(SearchIndexAsset->Type == ESearchIndexAssetType::BlendSpace);
	return BlendSpaces[SearchIndexAsset->SourceAssetIdx];
}

const bool UPoseSearchDatabase::IsSourceAssetLooping(const FPoseSearchIndexAsset* SearchIndexAsset) const
{
	if (SearchIndexAsset->Type == ESearchIndexAssetType::Sequence)
	{
		return GetSequenceSourceAsset(SearchIndexAsset).Sequence->bLoop;
	}

	if (SearchIndexAsset->Type == ESearchIndexAssetType::BlendSpace)
	{
		return GetBlendSpaceSourceAsset(SearchIndexAsset).BlendSpace->bLoop;
	}
	
	checkNoEntry();
	return false;
}

const FString UPoseSearchDatabase::GetSourceAssetName(const FPoseSearchIndexAsset* SearchIndexAsset) const
{
	if (SearchIndexAsset->Type == ESearchIndexAssetType::Sequence)
	{
		return GetSequenceSourceAsset(SearchIndexAsset).Sequence->GetName();
	}
	
	if (SearchIndexAsset->Type == ESearchIndexAssetType::BlendSpace)
	{
		return GetBlendSpaceSourceAsset(SearchIndexAsset).BlendSpace->GetName();
	}
	
	checkNoEntry();
	return FString();
}

int32 UPoseSearchDatabase::GetNumberOfPrincipalComponents() const
{
	return FMath::Min<int32>(NumberOfPrincipalComponents, Schema->SchemaCardinality);
}

#if WITH_EDITOR

static void AddRawSequenceToWriter(UAnimSequence* Sequence, UE::PoseSearch::FDerivedDataKeyBuilder& KeyBuilder)
{
	if (Sequence)
	{
		FName SequenceName = Sequence->GetFName();
		FGuid SequenceGuid = Sequence->GetRawDataGuid();
		KeyBuilder << SequenceName;
		KeyBuilder << SequenceGuid;
		KeyBuilder << Sequence->bLoop;
	}
}

static void AddPoseSearchNotifiesToWriter(UAnimSequence* Sequence, UE::PoseSearch::FDerivedDataKeyBuilder& KeyBuilder)
{
	if (!Sequence)
	{
		return;
	}

	FAnimNotifyContext NotifyContext;
	Sequence->GetAnimNotifies(0.0f, Sequence->GetPlayLength(), NotifyContext);

	for (const FAnimNotifyEventReference& EventReference : NotifyContext.ActiveNotifies)
	{
		const FAnimNotifyEvent* NotifyEvent = EventReference.GetNotify();
		if (!NotifyEvent || !NotifyEvent->NotifyStateClass)
		{
			continue;
		}

		if (NotifyEvent->NotifyStateClass->IsA<UAnimNotifyState_PoseSearchBase>())
		{
			float StartTime = NotifyEvent->GetTriggerTime();
			float EndTime = NotifyEvent->GetEndTriggerTime();
			KeyBuilder << StartTime;
			KeyBuilder << EndTime;
			KeyBuilder.Update(NotifyEvent->NotifyStateClass);
		}
	}
}


void FPoseSearchDatabaseSequence::BuildDerivedDataKey(UE::PoseSearch::FDerivedDataKeyBuilder& KeyBuilder)
{
	KeyBuilder << bEnabled;
	KeyBuilder << SamplingRange;
	KeyBuilder << MirrorOption;

	AddRawSequenceToWriter(Sequence, KeyBuilder);
	AddRawSequenceToWriter(LeadInSequence, KeyBuilder);
	AddRawSequenceToWriter(FollowUpSequence, KeyBuilder);

	AddPoseSearchNotifiesToWriter(Sequence, KeyBuilder);
}

void FPoseSearchDatabaseBlendSpace::BuildDerivedDataKey(UE::PoseSearch::FDerivedDataKeyBuilder& KeyBuilder)
{
	KeyBuilder << bEnabled;
	KeyBuilder << MirrorOption;
	KeyBuilder << bUseGridForSampling;
	KeyBuilder << NumberOfHorizontalSamples;
	KeyBuilder << NumberOfVerticalSamples;

	const TArray<FBlendSample>& BlendSpaceSamples = BlendSpace->GetBlendSamples();
	for (const FBlendSample& Sample : BlendSpaceSamples)
	{
		AddRawSequenceToWriter(Sample.Animation, KeyBuilder);
		FVector SampleValue = Sample.SampleValue;
		float RateScale = Sample.RateScale;
 		KeyBuilder << SampleValue;
 		KeyBuilder << RateScale;
	}

	KeyBuilder << BlendSpace->bLoop;
}

void UPoseSearchDatabase::BuildDerivedDataKey(UE::PoseSearch::FDerivedDataKeyBuilder& KeyBuilder)
{
	KeyBuilder.Update(this);

	if (Schema)
	{
		KeyBuilder.Update(Schema.Get());
	}

	for (FPoseSearchDatabaseSequence& DbSequence : Sequences)
	{
		DbSequence.BuildDerivedDataKey(KeyBuilder);
	}

	for (FPoseSearchDatabaseBlendSpace& DbBlendSpace : BlendSpaces)
	{
		DbBlendSpace.BuildDerivedDataKey(KeyBuilder);
	}
}

#endif // WITH_EDITOR

bool UPoseSearchDatabase::GetSkipSearchIfPossible() const
{
	if (PoseSearchMode == EPoseSearchMode::PCAKDTree_Validate || PoseSearchMode == EPoseSearchMode::PCAKDTree_Compare)
	{
		return false;
	}

	return bSkipSearchIfPossible;
}

bool UPoseSearchDatabase::IsValidForIndexing() const
{
	bool bValid = Schema && Schema->IsValid() && (!Sequences.IsEmpty() || !BlendSpaces.IsEmpty());

	if (bValid)
	{
		for (const FPoseSearchDatabaseSequence& DbSequence : Sequences)
		{
			if (!DbSequence.Sequence)
			{
				bValid = false;
				break;
			}

			const USkeleton* SeqSkeleton = DbSequence.Sequence->GetSkeleton();
			if (!SeqSkeleton || !SeqSkeleton->IsCompatible(Schema->Skeleton))
			{
				bValid = false;
				break;
			}
		}

		for (const FPoseSearchDatabaseBlendSpace& DbBlendSpace : BlendSpaces)
		{
			if (!DbBlendSpace.BlendSpace)
			{
				bValid = false;
				break;
			}

			const USkeleton* SeqSkeleton = DbBlendSpace.BlendSpace->GetSkeleton();
			if (!SeqSkeleton || !SeqSkeleton->IsCompatible(Schema->Skeleton))
			{
				bValid = false;
				break;
			}
		}
	}

	return bValid;
}

bool UPoseSearchDatabase::IsValidForSearch() const
{
	const FPoseSearchIndex* SearchIndex = GetSearchIndex();
	bool bIsValid = IsValidForIndexing() && SearchIndex && SearchIndex->IsValid() && !SearchIndex->IsEmpty();

#if WITH_EDITOR
	const bool bIsCurrentDerivedData = 
		PrivateDerivedData &&
		PrivateDerivedData->PendingDerivedDataKey == PrivateDerivedData->DerivedDataKey.Hash;
	bIsValid = bIsValid && bIsCurrentDerivedData;
#endif // WITH_EDITOR

	return bIsValid;
}

bool UPoseSearchDatabase::IsValidPoseIndex(int32 PoseIdx) const
{
	const FPoseSearchIndex* SearchIndex = GetSearchIndex();
	return SearchIndex ? SearchIndex->IsValidPoseIndex(PoseIdx) : false;
}

void UPoseSearchDatabase::CollectSimpleSequences()
{
	for (auto& SimpleSequence: SimpleSequences)
	{
		auto Predicate = [&SimpleSequence](FPoseSearchDatabaseSequence& DbSequence) -> bool
		{
			return DbSequence.Sequence == SimpleSequence;
		};

		if (!Sequences.ContainsByPredicate(Predicate))
		{
			FPoseSearchDatabaseSequence& DbSequence = Sequences.AddDefaulted_GetRef();
			DbSequence.Sequence = SimpleSequence;
		}
	}

	SimpleSequences.Reset();
}

void UPoseSearchDatabase::CollectSimpleBlendSpaces()
{
	for (auto& SimpleBlendSpace : SimpleBlendSpaces)
	{
		auto Predicate = [&SimpleBlendSpace](FPoseSearchDatabaseBlendSpace& DbBlendSpace) -> bool
		{
			return DbBlendSpace.BlendSpace == SimpleBlendSpace;
		};

		if (!BlendSpaces.ContainsByPredicate(Predicate))
		{
			FPoseSearchDatabaseBlendSpace& DbBlendSpace = BlendSpaces.AddDefaulted_GetRef();
			DbBlendSpace.BlendSpace = SimpleBlendSpace;
		}
	}

	SimpleBlendSpaces.Reset();
}

void UPoseSearchDatabase::FindValidSequenceIntervals(const FPoseSearchDatabaseSequence& DbSequence, TArray<FFloatRange>& ValidRanges) const
{
	const UAnimSequence* Sequence = DbSequence.Sequence;
	check(DbSequence.Sequence);

	const float SequenceLength = DbSequence.Sequence->GetPlayLength();

	const FFloatInterval EffectiveSamplingInterval = UE::PoseSearch::GetEffectiveSamplingRange(DbSequence.Sequence, DbSequence.SamplingRange);
	FFloatRange EffectiveSamplingRange = FFloatRange::Inclusive(EffectiveSamplingInterval.Min, EffectiveSamplingInterval.Max);
	if (!DbSequence.IsLooping())
	{
		const FFloatRange ExcludeFromDatabaseRange(ExcludeFromDatabaseParameters.SequenceStartInterval, SequenceLength - ExcludeFromDatabaseParameters.SequenceEndInterval);
		EffectiveSamplingRange = FFloatRange::Intersection(EffectiveSamplingRange, ExcludeFromDatabaseRange);
	}

	// start from a single interval defined by the database sequence sampling range
	ValidRanges.Empty();
	ValidRanges.Add(EffectiveSamplingRange);

	FAnimNotifyContext NotifyContext;
	Sequence->GetAnimNotifies(0.0f, SequenceLength, NotifyContext);

	for (const FAnimNotifyEventReference& EventReference : NotifyContext.ActiveNotifies)
	{
		if (const FAnimNotifyEvent* NotifyEvent = EventReference.GetNotify())
		{
			if (const UAnimNotifyState_PoseSearchExcludeFromDatabase* ExclusionNotifyState = Cast<const UAnimNotifyState_PoseSearchExcludeFromDatabase>(NotifyEvent->NotifyStateClass))
			{
				FFloatRange ExclusionRange = FFloatRange::Inclusive(NotifyEvent->GetTriggerTime(), NotifyEvent->GetEndTriggerTime());

				// Split every valid range based on the exclusion range just found. Because this might increase the 
				// number of ranges in ValidRanges, the algorithm iterates from end to start.
				for (int RangeIdx = ValidRanges.Num() - 1; RangeIdx >= 0; --RangeIdx)
				{
					FFloatRange EvaluatedRange = ValidRanges[RangeIdx];
					ValidRanges.RemoveAt(RangeIdx);

					TArray<FFloatRange> Diff = FFloatRange::Difference(EvaluatedRange, ExclusionRange);
					ValidRanges.Append(Diff);
				}
			}
		}
	}
}

UAnimationAsset* FPoseSearchDatabaseBlendSpace::GetAnimationAsset() const
{
	return BlendSpace.Get();
}

bool FPoseSearchDatabaseBlendSpace::IsLooping() const
{
	return BlendSpace->bLoop;
}

void FPoseSearchDatabaseBlendSpace::GetBlendSpaceParameterSampleRanges(
	int32& HorizontalBlendNum,
	int32& VerticalBlendNum,
	float& HorizontalBlendMin,
	float& HorizontalBlendMax,
	float& VerticalBlendMin,
	float& VerticalBlendMax) const
{
	HorizontalBlendNum = bUseGridForSampling ? BlendSpace->GetBlendParameter(0).GridNum + 1 : FMath::Max(NumberOfHorizontalSamples, 1);
	VerticalBlendNum = bUseGridForSampling ? BlendSpace->GetBlendParameter(1).GridNum + 1 : FMath::Max(NumberOfVerticalSamples, 1);

	check(HorizontalBlendNum >= 1 && VerticalBlendNum >= 1);

	HorizontalBlendMin = BlendSpace->GetBlendParameter(0).Min;
	HorizontalBlendMax = BlendSpace->GetBlendParameter(0).Max;

	VerticalBlendMin = BlendSpace->GetBlendParameter(1).Min;
	VerticalBlendMax = BlendSpace->GetBlendParameter(1).Max;

	if (BlendSpace->IsA<UBlendSpace1D>())
	{
		VerticalBlendNum = 1;
		VerticalBlendMin = 0.0;
		VerticalBlendMax = 0.0;
	}
}

static FVector BlendParameterForSampleRanges(
	int32 HorizontalBlendIndex,
	int32 VerticalBlendIndex,
	int32 HorizontalBlendNum,
	int32 VerticalBlendNum,
	float HorizontalBlendMin,
	float HorizontalBlendMax,
	float VerticalBlendMin,
	float VerticalBlendMax)
{
	return FVector(
		HorizontalBlendNum > 1 ? 
			HorizontalBlendMin + (HorizontalBlendMax - HorizontalBlendMin) * 
			((float)HorizontalBlendIndex) / (HorizontalBlendNum - 1) : 
		HorizontalBlendMin,
		VerticalBlendNum > 1 ? 
			VerticalBlendMin + (VerticalBlendMax - VerticalBlendMin) * 
			((float)VerticalBlendIndex) / (VerticalBlendNum - 1) : 
		VerticalBlendMin,
		0.0f);
}

bool UPoseSearchDatabase::TryInitSearchIndexAssets(FPoseSearchIndex& OutSearchIndex) const
{
	OutSearchIndex.Assets.Empty();
	
	bool bAnyMirrored = false;
	
	TArray<FFloatRange> ValidRanges;

	for (int32 SequenceIdx = 0; SequenceIdx < Sequences.Num(); ++SequenceIdx)
	{
		const FPoseSearchDatabaseSequence& Sequence = Sequences[SequenceIdx];

		if (!Sequence.bEnabled)
		{
			continue;
		}

		bool bAddUnmirrored = 
			Sequence.MirrorOption == EPoseSearchMirrorOption::UnmirroredOnly ||
			Sequence.MirrorOption == EPoseSearchMirrorOption::UnmirroredAndMirrored;
		bool bAddMirrored =
			Sequence.MirrorOption == EPoseSearchMirrorOption::MirroredOnly ||
			Sequence.MirrorOption == EPoseSearchMirrorOption::UnmirroredAndMirrored;

		ValidRanges.Reset();
		FindValidSequenceIntervals(Sequence, ValidRanges);
		for (const FFloatRange& Range : ValidRanges)
		{
			if (bAddUnmirrored)
			{
				OutSearchIndex.Assets.Add(
					FPoseSearchIndexAsset(
						ESearchIndexAssetType::Sequence,
						SequenceIdx,
						false,
						FFloatInterval(Range.GetLowerBoundValue(), Range.GetUpperBoundValue())));
			}

			if (bAddMirrored)
			{
				OutSearchIndex.Assets.Add(
					FPoseSearchIndexAsset(
						ESearchIndexAssetType::Sequence,
						SequenceIdx,
						true,
						FFloatInterval(Range.GetLowerBoundValue(), Range.GetUpperBoundValue())));
				bAnyMirrored = true;
			}
		}
	}

	TArray<FBlendSampleData> BlendSamples;

	for (int32 BlendSpaceIdx = 0; BlendSpaceIdx < BlendSpaces.Num(); ++BlendSpaceIdx)
	{
		const FPoseSearchDatabaseBlendSpace& BlendSpace = BlendSpaces[BlendSpaceIdx];

		if (!BlendSpace.bEnabled)
		{
			continue;
		}

		bool bAddUnmirrored =
			BlendSpace.MirrorOption == EPoseSearchMirrorOption::UnmirroredOnly ||
			BlendSpace.MirrorOption == EPoseSearchMirrorOption::UnmirroredAndMirrored;
		bool bAddMirrored =
			BlendSpace.MirrorOption == EPoseSearchMirrorOption::MirroredOnly ||
			BlendSpace.MirrorOption == EPoseSearchMirrorOption::UnmirroredAndMirrored;

		int32 HorizontalBlendNum, VerticalBlendNum;
		float HorizontalBlendMin, HorizontalBlendMax, VerticalBlendMin, VerticalBlendMax;

		BlendSpace.GetBlendSpaceParameterSampleRanges(
			HorizontalBlendNum,
			VerticalBlendNum,
			HorizontalBlendMin,
			HorizontalBlendMax,
			VerticalBlendMin,
			VerticalBlendMax);

		for (int32 HorizontalIndex = 0; HorizontalIndex < HorizontalBlendNum; HorizontalIndex++)
		{
			for (int32 VerticalIndex = 0; VerticalIndex < VerticalBlendNum; VerticalIndex++)
			{
				FVector BlendParameters = BlendParameterForSampleRanges(
					HorizontalIndex,
					VerticalIndex,
					HorizontalBlendNum,
					VerticalBlendNum,
					HorizontalBlendMin,
					HorizontalBlendMax,
					VerticalBlendMin,
					VerticalBlendMax);
						
				int32 TriangulationIndex = 0;
				BlendSpace.BlendSpace->GetSamplesFromBlendInput(BlendParameters, BlendSamples, TriangulationIndex, true);

				float PlayLength = BlendSpace.BlendSpace->GetAnimationLengthFromSampleData(BlendSamples);

				if (bAddUnmirrored)
				{
					OutSearchIndex.Assets.Add(
						FPoseSearchIndexAsset(
							ESearchIndexAssetType::BlendSpace,
							BlendSpaceIdx,
							false,
							FFloatInterval(0.0f, PlayLength),
							BlendParameters));
				}

				if (bAddMirrored)
				{
					OutSearchIndex.Assets.Add(
						FPoseSearchIndexAsset(
							ESearchIndexAssetType::BlendSpace,
							BlendSpaceIdx,
							true,
							FFloatInterval(0.0f, PlayLength),
							BlendParameters));
					bAnyMirrored = true;
				}
			}
		}
	}

	if (bAnyMirrored && !Schema->MirrorDataTable)
	{
		UE_LOG(
			LogPoseSearch, 
			Error, 
			TEXT("Database %s is asking for mirrored sequences but MirrorDataBase is null in %s"),
			*GetNameSafe(this), 
			*GetNameSafe(Schema));
		OutSearchIndex.Assets.Empty();
		return false;
	}

	return true;
}

void UPoseSearchDatabase::PostLoad()
{
#if WITH_EDITOR
	if (!PrivateDerivedData)
	{
		BeginCacheDerivedData();
	}
#endif

	Super::PostLoad();
}

#if WITH_EDITOR
void UPoseSearchDatabase::RegisterOnDerivedDataRebuild(const FOnDerivedDataRebuild& Delegate)
{
	OnDerivedDataRebuild.Add(Delegate);
}
void UPoseSearchDatabase::UnregisterOnDerivedDataRebuild(void* Unregister)
{
	OnDerivedDataRebuild.RemoveAll(Unregister);
}

void UPoseSearchDatabase::NotifyDerivedDataBuildStarted()
{
	OnDerivedDataRebuild.Broadcast();
}

void UPoseSearchDatabase::RegisterOnAssetChange(const FOnAssetChange& Delegate)
{
	OnAssetChange.Add(Delegate);
}

void UPoseSearchDatabase::UnregisterOnAssetChange(void* Unregister)
{
	OnAssetChange.RemoveAll(Unregister);
}

void UPoseSearchDatabase::NotifyAssetChange()
{
	OnAssetChange.Broadcast();
}

void UPoseSearchDatabase::RegisterOnGroupChange(const FOnGroupChange& Delegate)
{
	OnGroupChange.Add(Delegate);
}

void UPoseSearchDatabase::UnregisterOnGroupChange(void* Unregister)
{
	OnGroupChange.RemoveAll(Unregister);
}

void UPoseSearchDatabase::NotifyGroupChange()
{
	OnGroupChange.Broadcast();
}

void UPoseSearchDatabase::BeginCacheDerivedData()
{
	bool bPerformCache = true;
			
	using namespace UE::DerivedData;
	if (PrivateDerivedData)
	{
		const FIoHash ExistingDerivedDataHash = PrivateDerivedData->PendingDerivedDataKey;
		if (!ExistingDerivedDataHash.IsZero())
		{
			const FIoHash CurrentHash = UE::PoseSearch::FPoseSearchDatabaseAsyncCacheTask::CreateKey(*this);
			if (ExistingDerivedDataHash == CurrentHash)
			{
				bPerformCache = false;
			}
		}
	}

	if (bPerformCache)
	{
		if (!PrivateDerivedData)
		{
			PrivateDerivedData = new FPoseSearchDatabaseDerivedData();
		}

		PrivateDerivedData->Cache(*this, false);
	}
}

FIoHash UPoseSearchDatabase::GetSearchIndexHash() const
{
	if (!PrivateDerivedData)
	{
		return FIoHash::Zero;
	}

	return PrivateDerivedData->DerivedDataKey.Hash;
}

bool UPoseSearchDatabase::IsDerivedDataBuildPending() const
{
	if (!PrivateDerivedData)
	{
		return true;
	}

	return PrivateDerivedData->DerivedDataKey.Hash != PrivateDerivedData->PendingDerivedDataKey;
}
#endif // WITH_EDITOR


void UPoseSearchDatabase::PostSaveRoot(FObjectPostSaveRootContext ObjectSaveContext)
{
	Super::PostSaveRoot(ObjectSaveContext);

#if WITH_EDITOR
	if (!IsTemplate() && !ObjectSaveContext.IsProceduralSave())
	{
		if (IsValidForIndexing())
		{
			if (!PrivateDerivedData)
			{
				PrivateDerivedData = new FPoseSearchDatabaseDerivedData();
			}

			PrivateDerivedData->Cache(*this, true);
		}
	}
#endif
}

void UPoseSearchDatabase::Serialize(FArchive& Ar)
{
	using namespace UE::PoseSearch;

 	Super::Serialize(Ar);

	if (Ar.IsFilterEditorOnly())
	{
		if (Ar.IsLoading())
		{
			if (!PrivateDerivedData)
			{
				PrivateDerivedData = new FPoseSearchDatabaseDerivedData();
				PrivateDerivedData->SearchIndex.Schema = Schema;
			}
		}
		
		if (Ar.IsLoading() || (Ar.IsCooking() && IsDerivedDataValid()))
		{
			FPoseSearchIndex* SearchIndex = GetSearchIndex();
			Ar << *SearchIndex;
		}
		else
		{
			UE_LOG(
				LogPoseSearch,
				Warning,
				TEXT("Invalid condition in UPoseSearchDatabase::Serialize for asset %s. It might be that the asset has a missing or invalid schema. IsLoading: %d, IsCooking: %d, IsDerivedDataValid: %d"),
				*GetNameSafe(this),
				Ar.IsLoading(), Ar.IsCooking(), IsDerivedDataValid());
		}
	}
}

bool UPoseSearchDatabase::IsDerivedDataValid()
{
	const FPoseSearchIndex* SearchIndex = GetSearchIndex();
	bool bIsValid = SearchIndex && SearchIndex->IsValid();
	return bIsValid;
}

#if WITH_EDITOR
void UPoseSearchDatabase::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	bool bAssetChange = false;

	if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UPoseSearchDatabase, SimpleSequences))
	{
		if (!SimpleSequences.IsEmpty())
		{
			CollectSimpleSequences();
			bAssetChange = true;
		}
	}

	if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UPoseSearchDatabase, SimpleBlendSpaces))
	{
		if (!SimpleBlendSpaces.IsEmpty())
		{
			CollectSimpleBlendSpaces();
			bAssetChange = true;
		}
	}

	if (PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UPoseSearchDatabase, Sequences) ||
		PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UPoseSearchDatabase, BlendSpaces))
	{
		bAssetChange = true;
	}

	if (bAssetChange)
	{
		NotifyAssetChange();
	}

	BeginCacheDerivedData();
}

void UPoseSearchDatabase::BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform)
{
	Super::BeginCacheForCookedPlatformData(TargetPlatform);

	BeginCacheDerivedData();
}

bool UPoseSearchDatabase::IsCachedCookedPlatformDataLoaded(const ITargetPlatform* TargetPlatform)
{
	check(IsInGameThread());

	if (!PrivateDerivedData)
	{
		PrivateDerivedData = new FPoseSearchDatabaseDerivedData();
		PrivateDerivedData->Cache(*this, true);
		return false;
	}

	if (PrivateDerivedData->AsyncTask && PrivateDerivedData->AsyncTask->Poll())
	{
		PrivateDerivedData->FinishCache();
	}

	if (PrivateDerivedData->AsyncTask)
	{
		return false;
	}

	return true;
}

#endif // WITH_EDITOR

FPoseSearchCost UPoseSearchDatabase::ComparePoses(UE::PoseSearch::FSearchContext& SearchContext, int32 PoseIdx, UE::PoseSearch::EPoseComparisonFlags PoseComparisonFlags, TConstArrayView<float> QueryValues) const
{
	const FPoseSearchIndex* SearchIndex = GetSearchIndex();
	check(SearchIndex);

	const float DissimilarityCost = UE::PoseSearch::CompareFeatureVectors(SearchIndex->GetPoseValues(PoseIdx), QueryValues, SearchIndex->WeightsSqrt);
	const float MirrorMismatchAddend = SearchIndex->ComputeMirrorMismatchAddend(PoseIdx, SearchContext);
	const float NotifyAddend = SearchIndex->ComputeNotifyAddend(PoseIdx);
	const float ContinuingPoseCostAddend = SearchIndex->ComputeContinuingPoseCostAddend(PoseIdx, PoseComparisonFlags);

	return FPoseSearchCost(DissimilarityCost, NotifyAddend, MirrorMismatchAddend, ContinuingPoseCostAddend);
}

UE::PoseSearch::FSearchResult UPoseSearchDatabase::Search(UE::PoseSearch::FSearchContext& SearchContext) const
{
	using namespace UE::PoseSearch;

	FSearchResult Result;

#if WITH_EDITOR
	if (IsDerivedDataBuildPending())
	{
		return Result;
	}
#endif

	const FPoseSearchIndex* SearchIndex = GetSearchIndexSafe();
	if (!SearchIndex)
	{
		return Result;
	}

	if (PoseSearchMode == EPoseSearchMode::BruteForce || PoseSearchMode == EPoseSearchMode::PCAKDTree_Compare)
	{
		Result = SearchBruteForce(SearchContext);
	}

	if (PoseSearchMode != EPoseSearchMode::BruteForce)
	{
#if WITH_EDITORONLY_DATA
		FPoseSearchCost BruteForcePoseCost = Result.BruteForcePoseCost;
#endif

		Result = SearchPCAKDTree(SearchContext);

#if WITH_EDITORONLY_DATA
		Result.BruteForcePoseCost = BruteForcePoseCost;
		if (PoseSearchMode == EPoseSearchMode::PCAKDTree_Compare)
		{
			check(Result.BruteForcePoseCost.GetTotalCost() <= Result.PoseCost.GetTotalCost());
		}
#endif
	}
	
	return Result;
}

UE::PoseSearch::FSearchResult UPoseSearchDatabase::SearchPCAKDTree(UE::PoseSearch::FSearchContext& SearchContext) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_PoseSearch_PCA_KNN);
	SCOPE_CYCLE_COUNTER(STAT_PoseSearchPCAKNN);

	using namespace UE::PoseSearch;

	FSearchResult Result;

	const int32 NumDimensions = Schema->SchemaCardinality;
	const FPoseSearchIndex* SearchIndex = GetSearchIndex();
	check(SearchIndex);

	const uint32 ClampedNumberOfPrincipalComponents = GetNumberOfPrincipalComponents();
	const uint32 ClampedKDTreeQueryNumNeighbors = FMath::Clamp<uint32>(KDTreeQueryNumNeighbors, 1, SearchIndex->NumPoses);


	//stack allocated temporaries
	TArrayView<size_t> ResultIndexes((size_t*)FMemory_Alloca((ClampedKDTreeQueryNumNeighbors + 1) * sizeof(size_t)), ClampedKDTreeQueryNumNeighbors + 1);
	TArrayView<float> ResultDistanceSqr((float*)FMemory_Alloca((ClampedKDTreeQueryNumNeighbors + 1) * sizeof(float)), ClampedKDTreeQueryNumNeighbors + 1);
	RowMajorVectorMap WeightedQueryValues((float*)FMemory_Alloca(NumDimensions * sizeof(float)), 1, NumDimensions);
	RowMajorVectorMap CenteredQueryValues((float*)FMemory_Alloca(NumDimensions * sizeof(float)), 1, NumDimensions);
	RowMajorVectorMap ProjectedQueryValues((float*)FMemory_Alloca(ClampedNumberOfPrincipalComponents * sizeof(float)), 1, ClampedNumberOfPrincipalComponents);
	
	constexpr int NonSelectableIdxDataSize = 128;
	size_t* NonSelectableIdxData((size_t*)FMemory_Alloca(NonSelectableIdxDataSize * sizeof(size_t)));
	TArrayView<size_t> NonSelectableIdxBuffer(NonSelectableIdxData, NonSelectableIdxDataSize);

	// KDTree in PCA space search
	if (PoseSearchMode == EPoseSearchMode::PCAKDTree_Validate)
	{
		const RowMajorVectorMapConst MapWeightsSqrt(SearchIndex->WeightsSqrt.GetData(), 1, NumDimensions);

		// testing the KDTree is returning the proper searches for all the original points transformed in pca space
		for (int32 PoseIdx = 0; PoseIdx < SearchIndex->NumPoses; ++PoseIdx)
		{
			FKDTree::KNNResultSet ResultSet(ClampedKDTreeQueryNumNeighbors, ResultIndexes, ResultDistanceSqr);
			TArrayView<const float> PoseValues = SearchIndex->GetPoseValues(PoseIdx);

			const RowMajorVectorMapConst Mean(SearchIndex->Mean.GetData(), 1, NumDimensions);
			const ColMajorMatrixMapConst PCAProjectionMatrix(SearchIndex->PCAProjectionMatrix.GetData(), NumDimensions, ClampedNumberOfPrincipalComponents);

			const RowMajorVectorMapConst QueryValues(PoseValues.GetData(), 1, NumDimensions);
			WeightedQueryValues = QueryValues.array() * MapWeightsSqrt.array();
			CenteredQueryValues.noalias() = WeightedQueryValues - Mean;
			ProjectedQueryValues.noalias() = CenteredQueryValues * PCAProjectionMatrix;

			SearchIndex->KDTree.FindNeighbors(ResultSet, ProjectedQueryValues.data());

			size_t ResultIndex = 0;
			for (; ResultIndex < ResultSet.Num(); ++ResultIndex)
			{
				if (PoseIdx == ResultIndexes[ResultIndex])
				{
					check(ResultDistanceSqr[ResultIndex] < UE_KINDA_SMALL_NUMBER);
					break;
				}
			}
			check(ResultIndex < ResultSet.Num());
		}
	}

	SearchContext.GetOrBuildQuery(this, Result.ComposedQuery);

	TArrayView<const float> QueryValues = Result.ComposedQuery.GetValues();

	const bool IsCurrentResultFromThisDatabase = SearchContext.IsCurrentResultFromDatabase(this);

	// evaluating the continuing pose only if it hasn't already being evaluated and the related animation can advance
	if (IsCurrentResultFromThisDatabase && SearchContext.bCanAdvance && !Result.ContinuingPoseCost.IsValid())
	{
		Result.PoseIdx = SearchContext.CurrentResult.PoseIdx;
		Result.PoseCost = ComparePoses(SearchContext, Result.PoseIdx, EPoseComparisonFlags::ContinuingPose, QueryValues);
		Result.ContinuingPoseCost = Result.PoseCost;

		if (GetSkipSearchIfPossible())
		{
			SearchContext.UpdateCurrentBestCost(Result.PoseCost);
		}
	}

	// since any PoseCost calculated here is at least SearchIndex->MinCostAddend,
	// there's no point in performing the search if CurrentBestTotalCost is already better than that
	if (SearchContext.GetCurrentBestTotalCost() > SearchIndex->MinCostAddend)
	{
		TConstArrayView<size_t> NonSelectableIdx = PopulateNonSelectableIdx(NonSelectableIdxBuffer, SearchContext, this, QueryValues);
		const RowMajorVectorMapConst MapWeightsSqrt(SearchIndex->WeightsSqrt.GetData(), 1, NumDimensions);
		FKDTree::KNNResultSet ResultSet(ClampedKDTreeQueryNumNeighbors, ResultIndexes, ResultDistanceSqr, NonSelectableIdx);

		check(QueryValues.Num() == NumDimensions);

		const RowMajorVectorMapConst Mean(SearchIndex->Mean.GetData(), 1, NumDimensions);
		const ColMajorMatrixMapConst PCAProjectionMatrix(SearchIndex->PCAProjectionMatrix.GetData(), NumDimensions, ClampedNumberOfPrincipalComponents);

		// transforming query values into PCA space to query the KDTree
		const RowMajorVectorMapConst QueryValuesMap(QueryValues.GetData(), 1, NumDimensions);
		WeightedQueryValues = QueryValuesMap.array() * MapWeightsSqrt.array();
		CenteredQueryValues.noalias() = WeightedQueryValues - Mean;
		ProjectedQueryValues.noalias() = CenteredQueryValues * PCAProjectionMatrix;

		SearchIndex->KDTree.FindNeighbors(ResultSet, ProjectedQueryValues.data());

		for (size_t ResultIndex = 0; ResultIndex < ResultSet.Num(); ++ResultIndex)
		{
			const int32 PoseIdx = ResultIndexes[ResultIndex];

			const FPoseSearchPoseMetadata& Metadata = SearchIndex->PoseMetadata[PoseIdx];

			if (EnumHasAnyFlags(Metadata.Flags, EPoseSearchPoseFlags::BlockTransition))
			{
#if UE_POSE_SEARCH_TRACE_ENABLED
				const FPoseSearchCost PoseCost = ComparePoses(SearchContext, PoseIdx, EPoseComparisonFlags::None, QueryValues);
				SearchContext.BestCandidates.Add(PoseCost, PoseIdx, this, EPoseCandidateFlags::DiscardedBy_BlockTransition);
#endif
				continue;
			}

			const FPoseSearchCost PoseCost = ComparePoses(SearchContext, PoseIdx, EPoseComparisonFlags::None, QueryValues);
			if (PoseCost < Result.PoseCost)
			{
				Result.PoseCost = PoseCost;
				Result.PoseIdx = PoseIdx;
			}

#if UE_POSE_SEARCH_TRACE_ENABLED
			SearchContext.BestCandidates.Add(PoseCost, PoseIdx, this, EPoseCandidateFlags::Valid_Pose);
#endif
		}

		if (GetSkipSearchIfPossible() && Result.PoseCost.IsValid())
		{
			SearchContext.UpdateCurrentBestCost(Result.PoseCost);
		}
	}

	// finalizing Result properties
	if (Result.PoseIdx != INDEX_NONE)
	{
		Result.SearchIndexAsset = SearchIndex->FindAssetForPose(Result.PoseIdx);
		Result.AssetTime = SearchIndex->GetAssetTime(Result.PoseIdx, Result.SearchIndexAsset);
		Result.Database = this;
#if WITH_EDITOR
		Result.SearchIndexHash = GetSearchIndexHash();
#endif // WITH_EDITOR
	}

	return Result;
}

UE::PoseSearch::FSearchResult UPoseSearchDatabase::SearchBruteForce(UE::PoseSearch::FSearchContext& SearchContext) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_PoseSearch_Brute_Force);
	SCOPE_CYCLE_COUNTER(STAT_PoseSearchBruteForce);
	
	using namespace UE::PoseSearch;
	
	FSearchResult Result;

	const FPoseSearchIndex* SearchIndex = GetSearchIndex();
	check(SearchIndex);

	SearchContext.GetOrBuildQuery(this, Result.ComposedQuery);
	TArrayView<const float> QueryValues = Result.ComposedQuery.GetValues();

	const bool IsCurrentResultFromThisDatabase = SearchContext.IsCurrentResultFromDatabase(this);
	if (IsCurrentResultFromThisDatabase)
	{
		// evaluating the continuing pose only if it hasn't already being evaluated and the related animation can advance
		if (SearchContext.bCanAdvance && !Result.ContinuingPoseCost.IsValid())
		{
			Result.PoseIdx = SearchContext.CurrentResult.PoseIdx;
			Result.PoseCost = ComparePoses(SearchContext, Result.PoseIdx, EPoseComparisonFlags::ContinuingPose, QueryValues);
			Result.ContinuingPoseCost = Result.PoseCost;

			if (GetSkipSearchIfPossible())
			{
				SearchContext.UpdateCurrentBestCost(Result.PoseCost);
			}
		}
	}

	constexpr int NonSelectableIdxDataSize = 128;
	size_t* NonSelectableIdxData((size_t*)FMemory_Alloca(NonSelectableIdxDataSize * sizeof(size_t)));
	TArrayView<size_t> NonSelectableIdxBuffer(NonSelectableIdxData, NonSelectableIdxDataSize);
	TConstArrayView<size_t> NonSelectableIdx = PopulateNonSelectableIdx(NonSelectableIdxBuffer, SearchContext, this, QueryValues);
	check(Algo::IsSorted(NonSelectableIdx));

	// since any PoseCost calculated here is at least SearchIndex->MinCostAddend,
	// there's no point in performing the search if CurrentBestTotalCost is already better than that
	if (SearchContext.GetCurrentBestTotalCost() > SearchIndex->MinCostAddend)
	{
		for (const FPoseSearchIndexAsset& Asset : SearchIndex->Assets)
		{
			const bool CheckForNonSelectableIdx = IsCurrentResultFromThisDatabase && (&Asset == SearchContext.CurrentResult.SearchIndexAsset);
			const int32 EndIndex = Asset.FirstPoseIdx + Asset.NumPoses;
			for (int32 PoseIdx = Asset.FirstPoseIdx; PoseIdx < EndIndex; ++PoseIdx)
			{
				const FPoseSearchPoseMetadata& Metadata = SearchIndex->PoseMetadata[PoseIdx];

				if (EnumHasAnyFlags(Metadata.Flags, EPoseSearchPoseFlags::BlockTransition))
				{
#if UE_POSE_SEARCH_TRACE_ENABLED
					const FPoseSearchCost PoseCost = ComparePoses(SearchContext, PoseIdx, EPoseComparisonFlags::None, QueryValues);
					SearchContext.BestCandidates.Add(PoseCost, PoseIdx, this, EPoseCandidateFlags::DiscardedBy_BlockTransition);
#endif
					continue;
				}

				if (CheckForNonSelectableIdx && Algo::BinarySearch(NonSelectableIdx, PoseIdx) != INDEX_NONE)
				{
					continue;
				}

				const FPoseSearchCost PoseCost = ComparePoses(SearchContext, PoseIdx, EPoseComparisonFlags::None, QueryValues);
				if (PoseCost < Result.PoseCost)
				{
					Result.PoseCost = PoseCost;
					Result.PoseIdx = PoseIdx;
				}

#if UE_POSE_SEARCH_TRACE_ENABLED
				if (PoseSearchMode == EPoseSearchMode::BruteForce)
				{
					SearchContext.BestCandidates.Add(PoseCost, PoseIdx, this, EPoseCandidateFlags::Valid_Pose);
				}
#endif
			}
		}

		if (GetSkipSearchIfPossible() && Result.PoseCost.IsValid())
		{
			SearchContext.UpdateCurrentBestCost(Result.PoseCost);
		}
	}

	// finalizing Result properties
	if (Result.PoseIdx != INDEX_NONE)
	{
		Result.SearchIndexAsset = SearchIndex->FindAssetForPose(Result.PoseIdx);
		Result.AssetTime = SearchIndex->GetAssetTime(Result.PoseIdx, Result.SearchIndexAsset);
		Result.Database = this;
#if WITH_EDITOR
		Result.SearchIndexHash = GetSearchIndexHash();
#endif // WITH_EDITOR
	}

#if WITH_EDITORONLY_DATA
	Result.BruteForcePoseCost = Result.PoseCost; 
#endif

	return Result;
}

void UPoseSearchDatabase::BuildQuery(UE::PoseSearch::FSearchContext& SearchContext, FPoseSearchFeatureVectorBuilder& OutQuery) const
{
	check(Schema && Schema->IsValid());
	Schema->BuildQuery(SearchContext, OutQuery);
}

UE::PoseSearch::FSearchResult UPoseSearchDatabaseSet::Search(UE::PoseSearch::FSearchContext& SearchContext) const
{
	using namespace UE::PoseSearch;

	FSearchResult Result;
	FPoseSearchCost ContinuingCost;
#if WITH_EDITOR
	FPoseSearchCost BruteForcePoseCost;
#endif

	// evaluating the continuing pose before all the active entries
	if (bEvaluateContinuingPoseFirst && 
		SearchContext.bCanAdvance &&
		!SearchContext.bForceInterrupt &&
		SearchContext.CurrentResult.IsValid())
	{
		const UPoseSearchDatabase* Database = SearchContext.CurrentResult.Database.Get();
		check(Database);
		const FPoseSearchIndex* SearchIndex = Database->GetSearchIndexSafe();
		if (SearchIndex)
		{
			SearchContext.GetOrBuildQuery(Database, Result.ComposedQuery);

			TArrayView<const float> QueryValues = Result.ComposedQuery.GetValues();

			const FPoseSearchIndexAsset* PoseSearchIndexAsset = SearchIndex->FindAssetForPose(SearchContext.CurrentResult.PoseIdx);
			check(PoseSearchIndexAsset);

			Result.PoseIdx = SearchContext.CurrentResult.PoseIdx;
			Result.PoseCost = Database->ComparePoses(SearchContext, Result.PoseIdx, EPoseComparisonFlags::ContinuingPose, QueryValues);
			Result.ContinuingPoseCost = Result.PoseCost;
			ContinuingCost = Result.PoseCost;

			Result.SearchIndexAsset = SearchIndex->FindAssetForPose(Result.PoseIdx);
			Result.AssetTime = SearchIndex->GetAssetTime(Result.PoseIdx, Result.SearchIndexAsset);
			Result.Database = Database;
#if WITH_EDITOR
			Result.SearchIndexHash = Database->GetSearchIndexHash();
#endif // WITH_EDITOR

			if (Database->GetSkipSearchIfPossible())
			{
				SearchContext.UpdateCurrentBestCost(Result.PoseCost);
			}
		}
	}

	for (const FPoseSearchDatabaseSetEntry& Entry : AssetsToSearch)
	{
		if (!IsValid(Entry.Searchable))
		{
			UE_LOG(LogPoseSearch, Warning, TEXT("Invalid entry in Database Set %s"), *GetName());
			continue;
		}

		const bool bSearchEntry =
			!Entry.Tag.IsValid() ||
			SearchContext.ActiveTagsContainer == nullptr ||
			SearchContext.ActiveTagsContainer->IsEmpty() ||
			SearchContext.ActiveTagsContainer->HasTag(Entry.Tag);

		if (bSearchEntry)
		{
			FSearchResult EntryResult = Entry.Searchable->Search(SearchContext);

			if (EntryResult.PoseCost.GetTotalCost() < Result.PoseCost.GetTotalCost())
			{
				Result = EntryResult;
			}

			if (EntryResult.ContinuingPoseCost.GetTotalCost() < ContinuingCost.GetTotalCost())
			{
				ContinuingCost = EntryResult.ContinuingPoseCost;
			}
#if WITH_EDITOR
			if (EntryResult.BruteForcePoseCost.GetTotalCost() < BruteForcePoseCost.GetTotalCost())
			{
				BruteForcePoseCost = EntryResult.BruteForcePoseCost;
			}
#endif
			if (Entry.PostSearchStatus == EPoseSearchPostSearchStatus::Stop)
			{
				break;
			}
		}
	}

	Result.ContinuingPoseCost = ContinuingCost;

#if WITH_EDITOR
	Result.BruteForcePoseCost = BruteForcePoseCost;
#endif

	if (!Result.IsValid())
	{
		UE_LOG(LogPoseSearch, Warning, TEXT("Invalid result searching %s"), *GetName());
	}

	return Result;
}

//////////////////////////////////////////////////////////////////////////
// FPoseSearchFeatureVectorBuilder

void FPoseSearchFeatureVectorBuilder::Init(const UPoseSearchSchema* InSchema)
{
	check(InSchema && InSchema->IsValid());
	Schema = InSchema;
	ResetFeatures();
}

void FPoseSearchFeatureVectorBuilder::Reset()
{
	Schema = nullptr;
	Values.Reset(0);
}

void FPoseSearchFeatureVectorBuilder::ResetFeatures()
{
	Values.Reset(0);
	Values.SetNumZeroed(Schema->SchemaCardinality);
}

void FPoseSearchFeatureVectorBuilder::CopyFromSearchIndex(const FPoseSearchIndex& SearchIndex, int32 PoseIdx)
{
	check(Schema == SearchIndex.Schema);
	Values = SearchIndex.GetPoseValues(PoseIdx);
}

bool FPoseSearchFeatureVectorBuilder::IsInitialized() const
{
	return (Schema != nullptr) && (Values.Num() == Schema->SchemaCardinality);
}

bool FPoseSearchFeatureVectorBuilder::IsInitializedForSchema(const UPoseSearchSchema* InSchema) const
{
	return (Schema == InSchema) && IsInitialized();
}

bool FPoseSearchFeatureVectorBuilder::IsCompatible(const FPoseSearchFeatureVectorBuilder& OtherBuilder) const
{
	return IsInitialized() && (Schema == OtherBuilder.Schema);
}

namespace UE::PoseSearch
{

void FPoseIndicesHistory::Update(const FSearchResult& SearchResult, float DeltaTime, float MaxTime)
{
	if (MaxTime > 0.f)
	{
		for (auto It = IndexToTime.CreateIterator(); It; ++It)
		{
			It.Value() += DeltaTime;
			if (It.Value() > MaxTime)
			{
				It.RemoveCurrent();
			}
		}

		if (SearchResult.IsValid())
		{
			FHistoricalPoseIndex HistoricalPoseIndex;
			HistoricalPoseIndex.PoseIndex = SearchResult.PoseIdx;
			HistoricalPoseIndex.DatabaseKey = FObjectKey(SearchResult.Database.Get());
			IndexToTime.Add(HistoricalPoseIndex, 0.f);
		}
	}
	else
	{
		IndexToTime.Reset();
	}
}

FTransform FSearchContext::TryGetTransformAndCacheResults(float SampleTime, const UPoseSearchSchema* Schema, int8 SchemaBoneIdx, bool& Error)
{
	check(History && Schema);

	static constexpr FBoneIndexType RootBoneIdx = 0xFFFF;
	const FBoneIndexType BoneIndexType = SchemaBoneIdx >= 0 ? Schema->BoneIndices[SchemaBoneIdx] : RootBoneIdx;

	// @todo: use an hashmap if we end up having too many entries
	const FCachedEntry* Entry = CachedEntries.FindByPredicate([SampleTime, BoneIndexType](const FSearchContext::FCachedEntry& Entry)
	{
		return Entry.SampleTime == SampleTime && Entry.BoneIndexType == BoneIndexType;
	});

	if (Entry)
	{
		Error = false;
		return Entry->Transform;
	}

	if (BoneIndexType != RootBoneIdx)
	{
		TArray<FTransform> SampledLocalPose;
		if (History->TrySampleLocalPose(-SampleTime, &Schema->BoneIndicesWithParents, &SampledLocalPose, nullptr))
		{
			TArray<FTransform> SampledComponentPose;
			FAnimationRuntime::FillUpComponentSpaceTransforms(Schema->Skeleton->GetReferenceSkeleton(), SampledLocalPose, SampledComponentPose);

			// adding bunch of entries, without caring about adding eventual duplicates
			for (const FBoneIndexType NewEntryBoneIndexType : Schema->BoneIndicesWithParents)
			{
				// @todo: maybe add them with a single allocation with CachedEntries.AddDefaulted(Schema->BoneIndicesWithParents.Num())
				FCachedEntry& NewEntry = CachedEntries[CachedEntries.AddDefaulted()];
				NewEntry.SampleTime = SampleTime;
				NewEntry.BoneIndexType = NewEntryBoneIndexType;
				NewEntry.Transform = SampledComponentPose[NewEntryBoneIndexType];
			}

			Error = false;
			return SampledComponentPose[BoneIndexType];
		}

		Error = true;
		return FTransform::Identity;
	}
	
	FTransform SampledRootTransform;
	if (History->TrySampleLocalPose(-SampleTime, nullptr, nullptr, &SampledRootTransform))
	{
		FCachedEntry& NewEntry = CachedEntries[CachedEntries.AddDefaulted()];
		NewEntry.SampleTime = SampleTime;
		NewEntry.BoneIndexType = BoneIndexType;
		NewEntry.Transform = SampledRootTransform;

		Error = false;
		return SampledRootTransform;
	}
	
	Error = true;
	return FTransform::Identity;
}

void FSearchContext::ClearCachedEntries()
{
	CachedEntries.Reset();
}

void FSearchContext::ResetCurrentBestCost()
{
	CurrentBestTotalCost = MAX_flt;
}

void FSearchContext::UpdateCurrentBestCost(const FPoseSearchCost& PoseSearchCost)
{
	check(PoseSearchCost.IsValid());

	if (PoseSearchCost.GetTotalCost() < CurrentBestTotalCost)
	{
		CurrentBestTotalCost = PoseSearchCost.GetTotalCost();
	};
}

const FPoseSearchFeatureVectorBuilder* FSearchContext::GetCachedQuery(const UPoseSearchDatabase* Database) const
{
	const FSearchContext::FCachedQuery* CachedQuery = CachedQueries.FindByPredicate([Database](const FSearchContext::FCachedQuery& CachedQuery)
	{
		return CachedQuery.Database == Database;
	});

	if (CachedQuery)
	{
		return &CachedQuery->FeatureVectorBuilder;
	}
	return nullptr;
}

bool FSearchContext::GetOrBuildQuery(const UPoseSearchDatabase* Database, FPoseSearchFeatureVectorBuilder& FeatureVectorBuilder)
{
	const FPoseSearchFeatureVectorBuilder* CachedFeatureVectorBuilder = GetCachedQuery(Database);
	if (CachedFeatureVectorBuilder)
	{
		FeatureVectorBuilder = *CachedFeatureVectorBuilder;
		return true;
	}

	FSearchContext::FCachedQuery& NewCachedQuery = CachedQueries[CachedQueries.AddDefaulted()];
	NewCachedQuery.Database = Database;
	Database->BuildQuery(*this, NewCachedQuery.FeatureVectorBuilder);
	FeatureVectorBuilder = NewCachedQuery.FeatureVectorBuilder;
	return false;
}

bool FSearchContext::IsCurrentResultFromDatabase(const UPoseSearchDatabase* Database) const
{
	return !bForceInterrupt && CurrentResult.IsValid() && CurrentResult.Database == Database;
}

TConstArrayView<float> FSearchContext::GetCurrentResultPrevPoseVector() const
{
	check(CurrentResult.IsValid());
	const FPoseSearchIndex* SearchIndex = CurrentResult.Database->GetSearchIndex();
	check(SearchIndex);
	return SearchIndex->GetPoseValues(CurrentResult.PrevPoseIdx);
}

TConstArrayView<float> FSearchContext::GetCurrentResultPoseVector() const
{
	check(CurrentResult.IsValid());
	const FPoseSearchIndex* SearchIndex = CurrentResult.Database->GetSearchIndex();
	check(SearchIndex);
	return SearchIndex->GetPoseValues(CurrentResult.PoseIdx);
}

TConstArrayView<float> FSearchContext::GetCurrentResultNextPoseVector() const
{
	check(CurrentResult.IsValid());
	const FPoseSearchIndex* SearchIndex = CurrentResult.Database->GetSearchIndex();
	check(SearchIndex);
	return SearchIndex->GetPoseValues(CurrentResult.NextPoseIdx);
}

//////////////////////////////////////////////////////////////////////////
// FPoseHistory

/**
* Fills skeleton transforms with evaluated compact pose transforms.
* Bones that weren't evaluated are filled with the bone's reference pose.
*/
static void CopyCompactToSkeletonPose(const FCompactPose& Pose, TArray<FTransform>& OutLocalTransforms)
{
	const FBoneContainer& BoneContainer = Pose.GetBoneContainer();
	const USkeleton* SkeletonAsset = BoneContainer.GetSkeletonAsset();
	check(SkeletonAsset);

	const FReferenceSkeleton& RefSkeleton = SkeletonAsset->GetReferenceSkeleton();
	TArrayView<const FTransform> RefSkeletonTransforms = MakeArrayView(RefSkeleton.GetRefBonePose());
	const int32 NumSkeletonBones = RefSkeleton.GetNum();

	OutLocalTransforms.SetNum(NumSkeletonBones);

	for (auto SkeletonBoneIdx = FSkeletonPoseBoneIndex(0); SkeletonBoneIdx != NumSkeletonBones; ++SkeletonBoneIdx)
	{
		FCompactPoseBoneIndex CompactBoneIdx = BoneContainer.GetCompactPoseIndexFromSkeletonPoseIndex(SkeletonBoneIdx);
		OutLocalTransforms[SkeletonBoneIdx.GetInt()] = CompactBoneIdx.IsValid() ? Pose[CompactBoneIdx] : RefSkeletonTransforms[SkeletonBoneIdx.GetInt()];
	}
}

void FPoseHistory::Init(int32 InNumPoses, float InTimeHorizon)
{
	Poses.Reserve(InNumPoses);
	TimeHorizon = InTimeHorizon;
}

void FPoseHistory::Init(const FPoseHistory& History)
{
	Poses = History.Poses;
	TimeHorizon = History.TimeHorizon;
}

bool FPoseHistory::TrySampleLocalPose(float SecondsAgo, const TArray<FBoneIndexType>* RequiredBones, TArray<FTransform>* LocalPose, FTransform* RootTransform) const
{
	const int32 NextIdx = LowerBound(Poses.begin(), Poses.end(), SecondsAgo, [](const FPose& Pose, float Value)
	{
		return Value < Pose.Time;
	});
	if (NextIdx <= 0 || NextIdx >= Poses.Num())
	{
		// We may not have accumulated enough poses yet
		return false;
	}

	const int32 PrevIdx = NextIdx - 1;

	const FPose& PrevPose = Poses[PrevIdx];
	const FPose& NextPose = Poses[NextIdx];

#if DO_CHECK
	check(PrevPose.LocalTransforms.Num() == NextPose.LocalTransforms.Num());
	FBoneIndexType MaxBoneIndexType = 0;
	if (RequiredBones)
	{
		for (FBoneIndexType BoneIndexType : *RequiredBones)
		{
			if (BoneIndexType > MaxBoneIndexType)
			{
				MaxBoneIndexType = BoneIndexType;
			}
		}
		check(MaxBoneIndexType < PrevPose.LocalTransforms.Num());
	}
#endif
	// Compute alpha between previous and next Poses
	const float Alpha = FMath::GetMappedRangeValueUnclamped(FVector2f(PrevPose.Time, NextPose.Time), FVector2f(0.0f, 1.0f), SecondsAgo);

	// Lerp between poses by alpha to produce output local pose at requested sample time
	if (LocalPose)
	{
		check(RequiredBones);
		*LocalPose = PrevPose.LocalTransforms;
		FAnimationRuntime::LerpBoneTransforms(*LocalPose, NextPose.LocalTransforms, Alpha, *RequiredBones);
	}

	if (RootTransform)
	{
		RootTransform->Blend(PrevPose.RootTransform, NextPose.RootTransform, Alpha);
	}
	return true;
}

bool FPoseHistory::Update(float SecondsElapsed, const FPoseContext& PoseContext, FTransform ComponentTransform, FText* OutError, ERootUpdateMode UpdateMode)
{
	// Age our elapsed times
	for (FPose& Pose : Poses)
	{
		Pose.Time += SecondsElapsed;
	}

	if (Poses.Num() != Poses.Max())
	{
		// Consume every pose until the queue is full
		Poses.Emplace();
	}
	else
	{
		// Exercise pose retention policy. We must guarantee there is always one additional pose
		// beyond the time horizon so we can compute derivatives at the time horizon. We also
		// want to evenly distribute poses across the entire history buffer so we only push additional
		// poses when enough time has elapsed.

		const float SampleInterval = GetSampleTimeInterval();

		bool bCanEvictOldest = Poses[1].Time >= TimeHorizon + SampleInterval;
		bool bShouldPushNewest = Poses[Poses.Num() - 2].Time >= SampleInterval;

		if (bCanEvictOldest && bShouldPushNewest)
		{
			FPose PoseTemp = MoveTemp(Poses.First());
			Poses.PopFront();
			Poses.Emplace(MoveTemp(PoseTemp));
		}
	}

	// Regardless of the retention policy, we always update the most recent pose
	FPose& CurrentPose = Poses.Last();
	CurrentPose.Time = 0.f;
	CopyCompactToSkeletonPose(PoseContext.Pose, CurrentPose.LocalTransforms);

	// Initialize with Previous Root Transform or Identity
	CurrentPose.RootTransform = Poses.Num() > 1 ? Poses[Poses.Num() - 2].RootTransform : FTransform::Identity;
	
	// Update using either AniumRootMotionProvider or Component Transform
	if (UpdateMode == ERootUpdateMode::RootMotionDelta)
	{
		const UE::Anim::IAnimRootMotionProvider* RootMotionProvider = UE::Anim::IAnimRootMotionProvider::Get();

		if (RootMotionProvider)
		{
			if (RootMotionProvider->HasRootMotion(PoseContext.CustomAttributes))
			{
				FTransform RootMotionDelta = FTransform::Identity;
				RootMotionProvider->ExtractRootMotion(PoseContext.CustomAttributes, RootMotionDelta);

				CurrentPose.RootTransform = RootMotionDelta * CurrentPose.RootTransform;
			}
#if WITH_EDITORONLY_DATA	
			else
			{
				if (OutError)
				{
					*OutError = LOCTEXT("PoseHistoryRootMotionProviderError",
						"Input to Pose History has no Root Motion Attribute. Try disabling 'Use Root Motion'.");
				}
				return false;
			}
#endif
		}
#if WITH_EDITORONLY_DATA	
		else
		{
			if (OutError)
			{
				*OutError = LOCTEXT("PoseHistoryRootMotionAttributeError",
					"Could not get Root Motion Provider. Try disabling 'Use Root Motion'.");
			}
			return false;
		}
#endif
	}
	else if (UpdateMode == ERootUpdateMode::ComponentTransformDelta)
	{
		CurrentPose.RootTransform = ComponentTransform;
	}
	else
	{
		checkNoEntry();
	}

	return true;
}

float FPoseHistory::GetSampleTimeInterval() const
{
	// Reserve one pose for computing derivatives at the time horizon
	return TimeHorizon / (Poses.Max() - 1);
}

//////////////////////////////////////////////////////////////////////////
// FFeatureVectorHelper
void FFeatureVectorHelper::EncodeQuat(TArrayView<float> Values, int32& DataOffset, const FQuat& Quat)
{
	const FVector X = Quat.GetAxisX();
	const FVector Y = Quat.GetAxisY();

	Values[DataOffset + 0] = X.X;
	Values[DataOffset + 1] = X.Y;
	Values[DataOffset + 2] = X.Z;
	Values[DataOffset + 3] = Y.X;
	Values[DataOffset + 4] = Y.Y;
	Values[DataOffset + 5] = Y.Z;

	DataOffset += EncodeQuatCardinality;
}

void FFeatureVectorHelper::EncodeQuat(TArrayView<float> Values, int32& DataOffset, TArrayView<const float> PrevValues, TArrayView<const float> CurValues, TArrayView<const float> NextValues, float LerpValue)
{
	FQuat Quat = DecodeQuatInternal(CurValues, DataOffset);

	// linear interpolation
	if (!FMath::IsNearlyZero(LerpValue))
	{
		if (LerpValue < 0.f)
		{
			Quat = FQuat::Slerp(Quat, DecodeQuatInternal(PrevValues, DataOffset), -LerpValue);
		}
		else
		{
			Quat = FQuat::Slerp(Quat, DecodeQuatInternal(NextValues, DataOffset), LerpValue);
		}
	}
	// @todo: do we need to add options for cubic interpolation?

	EncodeQuat(Values, DataOffset, Quat);
}

FQuat FFeatureVectorHelper::DecodeQuat(TArrayView<const float> Values, int32& DataOffset)
{
	const FQuat Quat = DecodeQuatInternal(Values, DataOffset);
	DataOffset += EncodeQuatCardinality;
	return Quat;
}

FQuat FFeatureVectorHelper::DecodeQuatInternal(TArrayView<const float> Values, int32 DataOffset)
{
	const FVector X(Values[DataOffset + 0], Values[DataOffset + 1], Values[DataOffset + 2]);
	const FVector Y(Values[DataOffset + 3], Values[DataOffset + 4], Values[DataOffset + 5]);
	const FVector Z = FVector::CrossProduct(X, Y);

	FMatrix M(FMatrix::Identity);
	M.SetColumn(0, X);
	M.SetColumn(1, Y);
	M.SetColumn(2, Z);

	return FQuat(M);
}

void FFeatureVectorHelper::EncodeVector(TArrayView<float> Values, int32& DataOffset, const FVector& Vector)
{
	Values[DataOffset + 0] = Vector.X;
	Values[DataOffset + 1] = Vector.Y;
	Values[DataOffset + 2] = Vector.Z;
	DataOffset += EncodeVectorCardinality;
}

void FFeatureVectorHelper::EncodeVector(TArrayView<float> Values, int32& DataOffset, TArrayView<const float> PrevValues, TArrayView<const float> CurValues, TArrayView<const float> NextValues, float LerpValue, bool bNormalize)
{
	FVector Vector = DecodeVectorInternal(CurValues, DataOffset);

	// linear interpolation
	if (!FMath::IsNearlyZero(LerpValue))
	{
		if (LerpValue < 0.f)
		{
			Vector = FMath::Lerp(Vector, DecodeVectorInternal(PrevValues, DataOffset), -LerpValue);
		}
		else
		{
			Vector = FMath::Lerp(Vector, DecodeVectorInternal(NextValues, DataOffset), LerpValue);
		}
	}
	// @todo: do we need to add options for cubic interpolation?

	if (bNormalize)
	{
		Vector = Vector.GetSafeNormal(UE_SMALL_NUMBER, FVector::XAxisVector);
	}

	EncodeVector(Values, DataOffset, Vector);
}

FVector FFeatureVectorHelper::DecodeVector(TArrayView<const float> Values, int32& DataOffset)
{
	const FVector Vector = DecodeVectorInternal(Values, DataOffset);
	DataOffset += EncodeVectorCardinality;
	return Vector;
}

FVector FFeatureVectorHelper::DecodeVectorInternal(TArrayView<const float> Values, int32 DataOffset)
{
	return FVector(Values[DataOffset + 0], Values[DataOffset + 1], Values[DataOffset + 2]);
}

void FFeatureVectorHelper::EncodeVector2D(TArrayView<float> Values, int32& DataOffset, const FVector2D& Vector2D)
{
	Values[DataOffset + 0] = Vector2D.X;
	Values[DataOffset + 1] = Vector2D.Y;
	DataOffset += EncodeVector2DCardinality;
}

void FFeatureVectorHelper::EncodeVector2D(TArrayView<float> Values, int32& DataOffset, TArrayView<const float> PrevValues, TArrayView<const float> CurValues, TArrayView<const float> NextValues, float LerpValue)
{
	FVector2D Vector2D = DecodeVector2DInternal(CurValues, DataOffset);

	// linear interpolation
	if (!FMath::IsNearlyZero(LerpValue))
	{
		if (LerpValue < 0.f)
		{
			Vector2D = FMath::Lerp(Vector2D, DecodeVector2DInternal(PrevValues, DataOffset), -LerpValue);
		}
		else
		{
			Vector2D = FMath::Lerp(Vector2D, DecodeVector2DInternal(NextValues, DataOffset), LerpValue);
		}
	}
	// @todo: do we need to add options for cubic interpolation?

	EncodeVector2D(Values, DataOffset, Vector2D);
}

FVector2D FFeatureVectorHelper::DecodeVector2D(TArrayView<const float> Values, int32& DataOffset)
{
	const FVector2D Vector2D = DecodeVector2DInternal(Values, DataOffset);
	DataOffset += EncodeVector2DCardinality;
	return Vector2D;
}

FVector2D FFeatureVectorHelper::DecodeVector2DInternal(TArrayView<const float> Values, int32 DataOffset)
{
	return FVector2D(Values[DataOffset + 0], Values[DataOffset + 1]);
}

void FFeatureVectorHelper::ComputeMeanDeviations(float MinMeanDeviation, const Eigen::MatrixXd& CenteredPoseMatrix, Eigen::VectorXd& MeanDeviations, int32& DataOffset, int32 Cardinality)
{
	const int32 NumPoses = CenteredPoseMatrix.cols();

	// Construct a submatrix for the feature and find the average distance to the feature's centroid.
	// Since we've already mean centered the data, the average distance to the centroid is simply the average norm.
	const double FeatureMeanDeviation = CenteredPoseMatrix.block(DataOffset, 0, Cardinality, NumPoses).colwise().norm().mean();

	// Fill the feature's corresponding scaling axes with the average distance
	// Avoid scaling by zero by leaving near-zero deviations as 1.0
	MeanDeviations.segment(DataOffset, Cardinality).setConstant(FeatureMeanDeviation > MinMeanDeviation ? FeatureMeanDeviation : 1.f);

	DataOffset += Cardinality;
}

void FFeatureVectorHelper::SetMeanDeviations(float Deviation, Eigen::VectorXd& MeanDeviations, int32& DataOffset, int32 Cardinality)
{
	// Fill the feature's corresponding scaling axes with the supplied value
	MeanDeviations.segment(DataOffset, Cardinality).setConstant(Deviation);

	DataOffset += Cardinality;
}

//////////////////////////////////////////////////////////////////////////
// FDebugDrawParams
bool FDebugDrawParams::CanDraw() const
{
	if (!World)
	{
		return false;
	}

	const FPoseSearchIndex* SearchIndex = GetSearchIndex();
	if (!SearchIndex)
	{
		return false;
	}

	return SearchIndex->IsValid() && !SearchIndex->IsEmpty() && SearchIndex->Schema->SchemaCardinality > 0;
}

FColor FDebugDrawParams::GetColor(int32 ColorPreset) const
{
	FLinearColor Color = FLinearColor::Red;

	const UPoseSearchSchema* Schema = GetSchema();
	if (!Schema || !Schema->IsValid())
	{
		Color = FLinearColor::Red;
	}
	else if (ColorPreset < 0 || ColorPreset >= Schema->ColorPresets.Num())
	{
		if (EnumHasAnyFlags(Flags, EDebugDrawFlags::DrawQuery))
		{
			Color = FLinearColor::Blue;
		}
		else
		{
			Color = FLinearColor::Green;
		}
	}
	else
	{
		if (EnumHasAnyFlags(Flags, EDebugDrawFlags::DrawQuery))
		{
			Color = Schema->ColorPresets[ColorPreset].Query;
		}
		else
		{
			Color = Schema->ColorPresets[ColorPreset].Result;
		}
	}

	return Color.ToFColor(true);
}

const FPoseSearchIndex* FDebugDrawParams::GetSearchIndex() const
{
	if (Database)
	{
		return Database->GetSearchIndex();
	}

	if (SequenceMetaData)
	{
		return &SequenceMetaData->SearchIndex;
	}

	return nullptr;
}

const UPoseSearchSchema* FDebugDrawParams::GetSchema() const
{
	if (Database)
	{
		return Database->Schema;
	}

	if (SequenceMetaData)
	{
		return SequenceMetaData->Schema;
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
// FSearchResult
// 

void FSearchResult::Update(float NewAssetTime)
{
	if (!IsValid())
	{
		Reset();
		return;
	}

	if (SearchIndexAsset->Type == ESearchIndexAssetType::Sequence)
	{
		const FPoseSearchDatabaseSequence& DbSequence = Database->GetSequenceSourceAsset(SearchIndexAsset);

		if (Database->GetPoseIndicesAndLerpValueFromTime(NewAssetTime, SearchIndexAsset, PrevPoseIdx, PoseIdx, NextPoseIdx, LerpValue))
		{
			AssetTime = NewAssetTime;
		}
		else
		{
			Reset();
		}
	}
	else if (SearchIndexAsset->Type == ESearchIndexAssetType::BlendSpace)
	{
		const FPoseSearchDatabaseBlendSpace& DbBlendSpace = Database->GetBlendSpaceSourceAsset(SearchIndexAsset);

		TArray<FBlendSampleData> BlendSamples;
		int32 TriangulationIndex = 0;
		DbBlendSpace.BlendSpace->GetSamplesFromBlendInput(SearchIndexAsset->BlendParameters, BlendSamples, TriangulationIndex, true);

		const float PlayLength = DbBlendSpace.BlendSpace->GetAnimationLengthFromSampleData(BlendSamples);

		// Asset player time for blendspaces is normalized [0, 1] so we need to convert 
		// to a real time before we advance it
		const float RealTime = NewAssetTime * PlayLength;
		if (Database->GetPoseIndicesAndLerpValueFromTime(RealTime, SearchIndexAsset, PrevPoseIdx, PoseIdx, NextPoseIdx, LerpValue))
		{
			AssetTime = NewAssetTime;
		}
		else
		{
			Reset();
		}
	}
	else
	{
		checkNoEntry();
	}
}

bool FSearchResult::IsValid() const
{
	bool bIsValid =
		PoseIdx != INDEX_NONE &&
		Database.IsValid();

#if WITH_EDITOR
	bIsValid = bIsValid &&
		!Database->IsDerivedDataBuildPending() &&
		Database->GetSearchIndexHash() == SearchIndexHash;
#endif // WITH_EDITOR

	return bIsValid;
}

void FSearchResult::Reset()
{
	PoseIdx = INDEX_NONE;
	SearchIndexAsset = nullptr;
	Database = nullptr;
	ComposedQuery.Reset();
	AssetTime = 0.0f;

#if WITH_EDITOR
	SearchIndexHash = FIoHash::Zero;
#endif // WITH_EDITOR
}


//////////////////////////////////////////////////////////////////////////
// FAssetSamplerContext

static void FillCompactPoseAndComponentRefRotations(FAssetSamplingContext& Context)
{
	if (Context.MirrorDataTable)
	{
		const UMirrorDataTable* MirrorDataTablePtr = Context.MirrorDataTable.Get();
		MirrorDataTablePtr->FillCompactPoseAndComponentRefRotations(
			Context.BoneContainer,
			Context.CompactPoseMirrorBones,
			Context.ComponentSpaceRefRotations);
	}
	else
	{
		Context.CompactPoseMirrorBones.Reset();
		Context.ComponentSpaceRefRotations.Reset();
	}
}

void FAssetSamplingContext::Init(const UPoseSearchSchema* Schema)
{
	MirrorDataTable = Schema->MirrorDataTable;
	BoneContainer.InitializeTo(Schema->BoneIndicesWithParents, FCurveEvaluationOption(false), *Schema->Skeleton);
	FillCompactPoseAndComponentRefRotations(*this);
}

FTransform FAssetSamplingContext::MirrorTransform(const FTransform& InTransform) const
{
	const EAxis::Type MirrorAxis = MirrorDataTable->MirrorAxis;
	FVector T = InTransform.GetTranslation();
	T = FAnimationRuntime::MirrorVector(T, MirrorAxis);
	const FQuat ReferenceRotation = ComponentSpaceRefRotations[FCompactPoseBoneIndex(0)];
	FQuat Q = InTransform.GetRotation();
	Q = FAnimationRuntime::MirrorQuat(Q, MirrorAxis);
	Q *= FAnimationRuntime::MirrorQuat(ReferenceRotation, MirrorAxis).Inverse() * ReferenceRotation;
	FTransform Result = FTransform(Q, T, InTransform.GetScale3D());
	return Result;
}

//////////////////////////////////////////////////////////////////////////
// class ICostBreakDownData

void ICostBreakDownData::AddEntireBreakDownSection(const FText& Label, const UPoseSearchSchema* Schema, int32 DataOffset, int32 Cardinality)
{
	BeginBreakDownSection(Label);

	const int32 Count = Num();
	for (int32 i = 0; i < Count; ++i)
	{
		if (IsCostVectorFromSchema(i, Schema))
		{
			const float CostBreakdown = ArraySum(GetCostVector(i, Schema), DataOffset, Cardinality);
			SetCostBreakDown(CostBreakdown, i, Schema);
		}
	}

	EndBreakDownSection(Label);
}

//////////////////////////////////////////////////////////////////////////
// Root motion extrapolation

// Uses distance delta between NextRootDistanceIndex and NextRootDistanceIndex - 1 and extrapolates it to ExtrapolationTime
static float ExtrapolateAccumulatedRootDistance(
	int32 SamplingRate,
	TArrayView<const float> AccumulatedRootDistance,
	int32 NextRootDistanceIndex, 
	float ExtrapolationTime,
	const FPoseSearchExtrapolationParameters& ExtrapolationParameters)
{
	check(NextRootDistanceIndex > 0 && NextRootDistanceIndex < AccumulatedRootDistance.Num());

	const float DistanceDelta =
		AccumulatedRootDistance[NextRootDistanceIndex] -
		AccumulatedRootDistance[NextRootDistanceIndex - 1];
	const float Speed = DistanceDelta * SamplingRate;
	const float ExtrapolationSpeed = Speed >= ExtrapolationParameters.LinearSpeedThreshold ?
		Speed : 0.0f;
	const float ExtrapolatedDistance = ExtrapolationSpeed * ExtrapolationTime;

	return ExtrapolatedDistance;
}

static float ExtractAccumulatedRootDistance(
	int32 SamplingRate,
	TArrayView<const float> AccumulatedRootDistance,
	float PlayLength,
	float Time,
	const FPoseSearchExtrapolationParameters& ExtrapolationParameters)
{
	const float ClampedTime = FMath::Clamp(Time, 0.0f, PlayLength);

	// Find the distance sample that corresponds with the time and split into whole and partial parts
	float IntegralDistanceSample;
	float DistanceAlpha = FMath::Modf(ClampedTime * SamplingRate, &IntegralDistanceSample);
	float DistanceIdx = (int32)IntegralDistanceSample;

	// Verify the distance offset and any residual portion would be in bounds
	check(DistanceIdx + (DistanceAlpha > 0.0f ? 1 : 0) < AccumulatedRootDistance.Num());

	// Look up the distance and interpolate between distance samples if necessary
	float Distance = AccumulatedRootDistance[DistanceIdx];
	if (DistanceAlpha > 0.0f)
	{
		float NextDistance = AccumulatedRootDistance[DistanceIdx + 1];
		Distance = FMath::Lerp(Distance, NextDistance, DistanceAlpha);
	}

	const float ExtrapolationTime = Time - ClampedTime;

	if (ExtrapolationTime != 0.0f)
	{
		// If extrapolationTime is not zero, we extrapolate the beginning or the end of the animation to estimate
		// the root distance.
		const int32 DistIdx = (ExtrapolationTime > 0.0f) ? AccumulatedRootDistance.Num() - 1 : 1;
		const float ExtrapolatedDistance = ExtrapolateAccumulatedRootDistance(
			SamplingRate,
			AccumulatedRootDistance,
			DistIdx,
			ExtrapolationTime,
			ExtrapolationParameters);
		Distance += ExtrapolatedDistance;
	}

	return Distance;
}

static FTransform ExtrapolateRootMotion(
	FTransform SampleToExtrapolate,
	float SampleStart, 
	float SampleEnd, 
	float ExtrapolationTime,
	const FPoseSearchExtrapolationParameters& ExtrapolationParameters)
{
	const float SampleDelta = SampleEnd - SampleStart;
	check(!FMath::IsNearlyZero(SampleDelta));

	const FVector LinearVelocityToExtrapolate = SampleToExtrapolate.GetTranslation() / SampleDelta;
	const float LinearSpeedToExtrapolate = LinearVelocityToExtrapolate.Size();
	const bool bCanExtrapolateTranslation =
		LinearSpeedToExtrapolate >= ExtrapolationParameters.LinearSpeedThreshold;

	const float AngularSpeedToExtrapolateRad = SampleToExtrapolate.GetRotation().GetAngle() / SampleDelta;
	const bool bCanExtrapolateRotation =
		FMath::RadiansToDegrees(AngularSpeedToExtrapolateRad) >= ExtrapolationParameters.AngularSpeedThreshold;

	if (!bCanExtrapolateTranslation && !bCanExtrapolateRotation)
	{
		return FTransform::Identity;
	}

	if (!bCanExtrapolateTranslation)
	{
		SampleToExtrapolate.SetTranslation(FVector::ZeroVector);
	}

	if (!bCanExtrapolateRotation)
	{
		SampleToExtrapolate.SetRotation(FQuat::Identity);
	}

	// converting ExtrapolationTime to a positive number to avoid dealing with the negative extrapolation and inverting
	// transforms later on.
	const float AbsExtrapolationTime = FMath::Abs(ExtrapolationTime);
	const float AbsSampleDelta = FMath::Abs(SampleDelta);
	const FTransform AbsTimeSampleToExtrapolate =
		ExtrapolationTime >= 0.0f ? SampleToExtrapolate : SampleToExtrapolate.Inverse();

	// because we're extrapolating rotation, the extrapolation must be integrated over time
	const float SampleMultiplier = AbsExtrapolationTime / AbsSampleDelta;
	float IntegralNumSamples;
	float RemainingSampleFraction = FMath::Modf(SampleMultiplier, &IntegralNumSamples);
	int32 NumSamples = (int32)IntegralNumSamples;

	// adding full samples to the extrapolated root motion
	FTransform ExtrapolatedRootMotion = FTransform::Identity;
	for (int i = 0; i < NumSamples; ++i)
	{
		ExtrapolatedRootMotion = AbsTimeSampleToExtrapolate * ExtrapolatedRootMotion;
	}

	// and a blend with identity for whatever is left
	FTransform RemainingExtrapolatedRootMotion;
	RemainingExtrapolatedRootMotion.Blend(
		FTransform::Identity,
		AbsTimeSampleToExtrapolate,
		RemainingSampleFraction);

	ExtrapolatedRootMotion = RemainingExtrapolatedRootMotion * ExtrapolatedRootMotion;
	return ExtrapolatedRootMotion;
}


//////////////////////////////////////////////////////////////////////////
// FSequenceSampler

struct FSequenceSampler : public IAssetSampler
{
public:
	struct FInput
	{
		const UAnimSequence* Sequence = nullptr;
		bool bLoopable = false;
		int32 RootDistanceSamplingRate = 60;
		FPoseSearchExtrapolationParameters ExtrapolationParameters;
	} Input;

	void Init(const FInput& Input);
	void Process();

	float GetPlayLength() const override { return Input.Sequence->GetPlayLength(); };
	bool IsLoopable() const override { return Input.bLoopable; };

	float GetTimeFromRootDistance(float Distance) const override;

	float GetTotalRootDistance() const override { return TotalRootDistance; };
	FTransform GetTotalRootTransform() const override { return TotalRootTransform; }

	virtual void ExtractPose(const FAnimExtractContext& ExtractionCtx, FAnimationPoseData& OutAnimPoseData) const override;
	virtual float ExtractRootDistance(float Time) const override;
	virtual FTransform ExtractRootTransform(float Time) const override;
	virtual void ExtractPoseSearchNotifyStates(float Time, TArray<class UAnimNotifyState_PoseSearchBase*>& NotifyStates) const override;
	virtual const UAnimationAsset* GetAsset() const override { return Input.Sequence; }

private:
	float TotalRootDistance = 0.0f;
	FTransform TotalRootTransform = FTransform::Identity;
	TArray<float> AccumulatedRootDistance;

	void ProcessRootDistance();
};

void FSequenceSampler::Init(const FInput& InInput)
{
	check(InInput.Sequence);

	Input = InInput;
}

void FSequenceSampler::Process()
{
	ProcessRootDistance();
}

float FSequenceSampler::GetTimeFromRootDistance(float Distance) const
{
	int32 NextSampleIdx = 1;
	int32 PrevSampleIdx = 0;
	if (Distance > 0.0f)
	{
		// Search for the distance value. Because the values will be extrapolated if necessary
		// LowerBound might go past the end of the array, in which case the last valid index is used
		int32 ClipDistanceLowerBoundIndex = Algo::LowerBound(AccumulatedRootDistance, Distance);
		NextSampleIdx = FMath::Min(
			ClipDistanceLowerBoundIndex,
			AccumulatedRootDistance.Num() - 1);

		// Compute distance interpolation amount
		PrevSampleIdx = FMath::Max(0, NextSampleIdx - 1);
	}

	float NextDistance = AccumulatedRootDistance[NextSampleIdx];
	float PrevDistance = AccumulatedRootDistance[PrevSampleIdx];
	float DistanceSampleAlpha = FMath::GetRangePct(PrevDistance, NextDistance, Distance);

	// Convert to time
	float ClipTime = (float(NextSampleIdx) - (1.0f - DistanceSampleAlpha)) / Input.RootDistanceSamplingRate;
	return ClipTime;
}

void FSequenceSampler::ExtractPose(const FAnimExtractContext& ExtractionCtx, FAnimationPoseData& OutAnimPoseData) const
{
	Input.Sequence->GetAnimationPose(OutAnimPoseData, ExtractionCtx);
}

FTransform FSequenceSampler::ExtractRootTransform(float Time) const
{
	if (Input.bLoopable)
	{
		FTransform LoopableRootTransform = Input.Sequence->ExtractRootMotion(0.0f, Time, true);
		return LoopableRootTransform;
	}

	const float ExtrapolationSampleTime = Input.ExtrapolationParameters.SampleTime;

	const float PlayLength = Input.Sequence->GetPlayLength();
	const float ClampedTime = FMath::Clamp(Time, 0.0f, PlayLength);
	const float ExtrapolationTime = Time - ClampedTime;

	FTransform RootTransform = FTransform::Identity;

	// If Time is less than zero, ExtrapolationTime will be negative. In this case, we extrapolate the beginning of the 
	// animation to estimate where the root would be at Time
	if (ExtrapolationTime < -SMALL_NUMBER)
	{
		FTransform SampleToExtrapolate = Input.Sequence->ExtractRootMotionFromRange(0.0f, ExtrapolationSampleTime);

		const FTransform ExtrapolatedRootMotion = ExtrapolateRootMotion(
			SampleToExtrapolate,
			0.0f, ExtrapolationSampleTime, 
			ExtrapolationTime,
			Input.ExtrapolationParameters);
		RootTransform = ExtrapolatedRootMotion;
	}
	else
	{
		RootTransform = Input.Sequence->ExtractRootMotionFromRange(0.0f, ClampedTime);

		// If Time is greater than PlayLength, ExtrapolationTIme will be a positive number. In this case, we extrapolate
		// the end of the animation to estimate where the root would be at Time
		if (ExtrapolationTime > SMALL_NUMBER)
		{
			FTransform SampleToExtrapolate = Input.Sequence->ExtractRootMotionFromRange(PlayLength - ExtrapolationSampleTime, PlayLength);

			const FTransform ExtrapolatedRootMotion = ExtrapolateRootMotion(
				SampleToExtrapolate,
				PlayLength - ExtrapolationSampleTime, PlayLength,
				ExtrapolationTime,
				Input.ExtrapolationParameters);
			RootTransform = ExtrapolatedRootMotion * RootTransform;
		}
	}

	return RootTransform;
}

float FSequenceSampler::ExtractRootDistance(float Time) const
{
	return ExtractAccumulatedRootDistance(
		Input.RootDistanceSamplingRate,
		AccumulatedRootDistance,
		Input.Sequence->GetPlayLength(),
		Time,
		Input.ExtrapolationParameters);
}

void FSequenceSampler::ExtractPoseSearchNotifyStates(
	float Time, 
	TArray<UAnimNotifyState_PoseSearchBase*>& NotifyStates) const
{
	// getting pose search notifies in an interval of size ExtractionInterval, centered on Time
	constexpr float ExtractionInterval = 1.0f / 120.0f;
	FAnimNotifyContext NotifyContext;
	Input.Sequence->GetAnimNotifies(Time - (ExtractionInterval * 0.5f), ExtractionInterval, NotifyContext);

	// check which notifies actually overlap Time and are of the right base type
	for (const FAnimNotifyEventReference& EventReference : NotifyContext.ActiveNotifies)
	{
		const FAnimNotifyEvent* NotifyEvent = EventReference.GetNotify();
		if (!NotifyEvent)
		{
			continue;
		}

		if (NotifyEvent->GetTriggerTime() > Time ||
			NotifyEvent->GetEndTriggerTime() < Time)
		{
			continue;
		}

		UAnimNotifyState_PoseSearchBase* PoseSearchAnimNotify = 
			Cast<UAnimNotifyState_PoseSearchBase>(NotifyEvent->NotifyStateClass);
		if (PoseSearchAnimNotify)
		{
			NotifyStates.Add(PoseSearchAnimNotify);
		}
	}
}

void FSequenceSampler::ProcessRootDistance()
{
	// Note the distance sampling interval is independent of the schema's sampling interval
	const float DistanceSamplingInterval = 1.0f / Input.RootDistanceSamplingRate;

	const FTransform InitialRootTransform = Input.Sequence->ExtractRootTrackTransform(0.0f, nullptr);

	uint32 NumDistanceSamples = FMath::CeilToInt(Input.Sequence->GetPlayLength() * Input.RootDistanceSamplingRate) + 1;
	AccumulatedRootDistance.Reserve(NumDistanceSamples);

	// Build a distance lookup table by sampling root motion at a fixed rate and accumulating
	// absolute translation deltas. During indexing we'll bsearch this table and interpolate
	// between samples in order to convert distance offsets to time offsets.
	// See also FAssetIndexer::AddTrajectoryDistanceFeatures().

	double TotalAccumulatedRootDistance = 0.0;
	FTransform LastRootTransform = InitialRootTransform;
	float SampleTime = 0.0f;
	for (int32 SampleIdx = 0; SampleIdx != NumDistanceSamples; ++SampleIdx)
	{
		SampleTime = FMath::Min(SampleIdx * DistanceSamplingInterval, Input.Sequence->GetPlayLength());

		FTransform RootTransform = Input.Sequence->ExtractRootTrackTransform(SampleTime, nullptr);
		FTransform LocalRootMotion = RootTransform.GetRelativeTransform(LastRootTransform);
		LastRootTransform = RootTransform;

		TotalAccumulatedRootDistance += LocalRootMotion.GetTranslation().Size();
		AccumulatedRootDistance.Add((float)TotalAccumulatedRootDistance);
	}

	// Verify we sampled the final frame of the clip
	check(SampleTime == Input.Sequence->GetPlayLength());

	// Also emit root motion summary info to help with sample wrapping in 
	// FAssetIndexer::GetSampleTimeFromDistance() and FAssetIndexer::GetSampleInfo()
	TotalRootTransform = LastRootTransform.GetRelativeTransform(InitialRootTransform);
	TotalRootDistance = AccumulatedRootDistance.Last();
}

//////////////////////////////////////////////////////////////////////////
// FBlendSpaceSampler

struct FBlendSpaceSampler : public IAssetSampler
{
public:
	struct FInput
	{
		const FAssetSamplingContext* SamplingContext = nullptr;
		const UBlendSpace* BlendSpace = nullptr;
		bool bLoopable = false;
		int32 RootDistanceSamplingRate = 60;
		int32 RootTransformSamplingRate = 60;
		FPoseSearchExtrapolationParameters ExtrapolationParameters;
		FVector BlendParameters;
	} Input;

	void Init(const FInput& Input);

	void Process();

	float GetPlayLength() const override { return PlayLength; };
	bool IsLoopable() const override { return Input.bLoopable; };

	float GetTimeFromRootDistance(float Distance) const override;

	float GetTotalRootDistance() const override { return TotalRootDistance; };
	FTransform GetTotalRootTransform() const override { return TotalRootTransform; }

	virtual void ExtractPose(const FAnimExtractContext& ExtractionCtx, FAnimationPoseData& OutAnimPoseData) const override;
	virtual float ExtractRootDistance(float Time) const override;
	virtual FTransform ExtractRootTransform(float Time) const override;
	virtual void ExtractPoseSearchNotifyStates(float Time, TArray<class UAnimNotifyState_PoseSearchBase*>& NotifyStates) const override;

	virtual const UAnimationAsset* GetAsset() const override { return Input.BlendSpace; }

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

void FBlendSpaceSampler::Init(const FInput& InInput)
{
	check(InInput.BlendSpace);

	Input = InInput;
}

void FBlendSpaceSampler::Process()
{
	FMemMark Mark(FMemStack::Get());

	ProcessPlayLength();
	ProcessRootTransform();
	ProcessRootDistance();
}

float FBlendSpaceSampler::GetTimeFromRootDistance(float Distance) const
{
	int32 NextSampleIdx = 1;
	int32 PrevSampleIdx = 0;
	if (Distance > 0.0f)
	{
		// Search for the distance value. Because the values will be extrapolated if necessary
		// LowerBound might go past the end of the array, in which case the last valid index is used
		int32 ClipDistanceLowerBoundIndex = Algo::LowerBound(AccumulatedRootDistance, Distance);
		NextSampleIdx = FMath::Min(
			ClipDistanceLowerBoundIndex,
			AccumulatedRootDistance.Num() - 1);

		// Compute distance interpolation amount
		PrevSampleIdx = FMath::Max(0, NextSampleIdx - 1);
	}

	float NextDistance = AccumulatedRootDistance[NextSampleIdx];
	float PrevDistance = AccumulatedRootDistance[PrevSampleIdx];
	float DistanceSampleAlpha = FMath::GetRangePct(PrevDistance, NextDistance, Distance);

	// Convert to time
	float ClipTime = (float(NextSampleIdx) - (1.0f - DistanceSampleAlpha)) / Input.RootDistanceSamplingRate;
	return ClipTime;
}

void FBlendSpaceSampler::ExtractPose(const FAnimExtractContext& ExtractionCtx, FAnimationPoseData& OutAnimPoseData) const
{
	TArray<FBlendSampleData> BlendSamples;
	int32 TriangulationIndex = 0;
	Input.BlendSpace->GetSamplesFromBlendInput(Input.BlendParameters, BlendSamples, TriangulationIndex, true);

	for (int32 BlendSampleIdex = 0; BlendSampleIdex < BlendSamples.Num(); BlendSampleIdex++)
	{
		float Scale = BlendSamples[BlendSampleIdex].Animation->GetPlayLength() / PlayLength;

		FDeltaTimeRecord BlendSampleDeltaTimeRecord;
		BlendSampleDeltaTimeRecord.Set(ExtractionCtx.DeltaTimeRecord.GetPrevious() * Scale, ExtractionCtx.DeltaTimeRecord.Delta * Scale);

		BlendSamples[BlendSampleIdex].DeltaTimeRecord = BlendSampleDeltaTimeRecord;
		BlendSamples[BlendSampleIdex].PreviousTime = ExtractionCtx.DeltaTimeRecord.GetPrevious() * Scale;
		BlendSamples[BlendSampleIdex].Time = ExtractionCtx.CurrentTime * Scale;
	}

	Input.BlendSpace->GetAnimationPose(BlendSamples, ExtractionCtx, OutAnimPoseData);
}

FTransform FBlendSpaceSampler::ExtractRootTransform(float Time) const
{
	if (Input.bLoopable)
	{
		FTransform LoopableRootTransform = ExtractBlendSpaceRootMotion(0.0f, Time, true);
		return LoopableRootTransform;
	}

	const float ExtrapolationSampleTime = Input.ExtrapolationParameters.SampleTime;

	const float ClampedTime = FMath::Clamp(Time, 0.0f, PlayLength);
	const float ExtrapolationTime = Time - ClampedTime;

	FTransform RootTransform = FTransform::Identity;

	// If Time is less than zero, ExtrapolationTime will be negative. In this case, we extrapolate the beginning of the 
	// animation to estimate where the root would be at Time
	if (ExtrapolationTime < -SMALL_NUMBER)
	{
		FTransform SampleToExtrapolate = ExtractBlendSpaceRootMotionFromRange(0.0f, ExtrapolationSampleTime);

		const FTransform ExtrapolatedRootMotion = ExtrapolateRootMotion(
			SampleToExtrapolate,
			0.0f, ExtrapolationSampleTime,
			ExtrapolationTime,
			Input.ExtrapolationParameters);
		RootTransform = ExtrapolatedRootMotion;
	}
	else
	{
		RootTransform = ExtractBlendSpaceRootMotionFromRange(0.0f, ClampedTime);

		// If Time is greater than PlayLength, ExtrapolationTIme will be a positive number. In this case, we extrapolate
		// the end of the animation to estimate where the root would be at Time
		if (ExtrapolationTime > SMALL_NUMBER)
		{
			FTransform SampleToExtrapolate = ExtractBlendSpaceRootMotionFromRange(PlayLength - ExtrapolationSampleTime, PlayLength);

			const FTransform ExtrapolatedRootMotion = ExtrapolateRootMotion(
				SampleToExtrapolate,
				PlayLength - ExtrapolationSampleTime, PlayLength,
				ExtrapolationTime,
				Input.ExtrapolationParameters);
			RootTransform = ExtrapolatedRootMotion * RootTransform;
		}
	}

	return RootTransform;
}

float FBlendSpaceSampler::ExtractRootDistance(float Time) const
{
	return ExtractAccumulatedRootDistance(
		Input.RootDistanceSamplingRate,
		AccumulatedRootDistance,
		PlayLength,
		Time,
		Input.ExtrapolationParameters);
}

static int32 GetHighestWeightSample(const TArray<struct FBlendSampleData>& SampleDataList)
{
	int32 HighestWeightIndex = 0;
	float HighestWeight = SampleDataList[HighestWeightIndex].GetClampedWeight();
	for (int32 I = 1; I < SampleDataList.Num(); I++)
	{
		if (SampleDataList[I].GetClampedWeight() > HighestWeight)
		{
			HighestWeightIndex = I;
			HighestWeight = SampleDataList[I].GetClampedWeight();
		}
	}
	return HighestWeightIndex;
}

void FBlendSpaceSampler::ExtractPoseSearchNotifyStates(
	float Time,
	TArray<UAnimNotifyState_PoseSearchBase*>& NotifyStates) const
{
	if (Input.BlendSpace->NotifyTriggerMode == ENotifyTriggerMode::HighestWeightedAnimation)
	{
		// Set up blend samples
		TArray<FBlendSampleData> BlendSamples;
		int32 TriangulationIndex = 0;
		Input.BlendSpace->GetSamplesFromBlendInput(Input.BlendParameters, BlendSamples, TriangulationIndex, true);

		// Find highest weighted
		const int32 HighestWeightIndex = GetHighestWeightSample(BlendSamples);

		check(HighestWeightIndex != -1);

		// getting pose search notifies in an interval of size ExtractionInterval, centered on Time
		constexpr float ExtractionInterval = 1.0f / 120.0f;

		float SampleTime = Time * (BlendSamples[HighestWeightIndex].Animation->GetPlayLength() / PlayLength);

		// Get notifies for highest weighted
		FAnimNotifyContext NotifyContext;
		BlendSamples[HighestWeightIndex].Animation->GetAnimNotifies(
			(SampleTime - (ExtractionInterval * 0.5f)),
			ExtractionInterval, 
			NotifyContext);

		// check which notifies actually overlap Time and are of the right base type
		for (const FAnimNotifyEventReference& EventReference : NotifyContext.ActiveNotifies)
		{
			const FAnimNotifyEvent* NotifyEvent = EventReference.GetNotify();
			if (!NotifyEvent)
			{
				continue;
			}

			if (NotifyEvent->GetTriggerTime() > SampleTime ||
				NotifyEvent->GetEndTriggerTime() < SampleTime)
			{
				continue;
			}

			UAnimNotifyState_PoseSearchBase* PoseSearchAnimNotify =
				Cast<UAnimNotifyState_PoseSearchBase>(NotifyEvent->NotifyStateClass);
			if (PoseSearchAnimNotify)
			{
				NotifyStates.Add(PoseSearchAnimNotify);
			}
		}
	}
}

FTransform FBlendSpaceSampler::ExtractBlendSpaceRootTrackTransform(float Time) const
{
	checkf(AccumulatedRootTransform.Num() > 0, TEXT("ProcessRootTransform must be run first"));

	int32 Index = Time * Input.RootTransformSamplingRate;
	int32 FirstIndexClamped = FMath::Clamp(Index + 0, 0, AccumulatedRootTransform.Num() - 1);
	int32 SecondIndexClamped = FMath::Clamp(Index + 1, 0, AccumulatedRootTransform.Num() - 1);
	float Alpha = FMath::Fmod(Time * Input.RootTransformSamplingRate, 1.0f);
	FTransform OutputTransform;
	OutputTransform.Blend(
		AccumulatedRootTransform[FirstIndexClamped],
		AccumulatedRootTransform[SecondIndexClamped],
		Alpha);

	return OutputTransform;
}

FTransform FBlendSpaceSampler::ExtractBlendSpaceRootMotionFromRange(float StartTrackPosition, float EndTrackPosition) const
{
	checkf(AccumulatedRootTransform.Num() > 0, TEXT("ProcessRootTransform must be run first"));

	FTransform RootTransformRefPose = ExtractBlendSpaceRootTrackTransform(0.0f);

	FTransform StartTransform = ExtractBlendSpaceRootTrackTransform(StartTrackPosition);
	FTransform EndTransform = ExtractBlendSpaceRootTrackTransform(EndTrackPosition);

	// Transform to Component Space
	const FTransform RootToComponent = RootTransformRefPose.Inverse();
	StartTransform = RootToComponent * StartTransform;
	EndTransform = RootToComponent * EndTransform;

	return EndTransform.GetRelativeTransform(StartTransform);
}

FTransform FBlendSpaceSampler::ExtractBlendSpaceRootMotion(float StartTime, float DeltaTime, bool bAllowLooping) const
{
	FRootMotionMovementParams RootMotionParams;

	if (DeltaTime != 0.f)
	{
		bool const bPlayingBackwards = (DeltaTime < 0.f);

		float PreviousPosition = StartTime;
		float CurrentPosition = StartTime;
		float DesiredDeltaMove = DeltaTime;

		do
		{
			// Disable looping here. Advance to desired position, or beginning / end of animation 
			const ETypeAdvanceAnim AdvanceType = FAnimationRuntime::AdvanceTime(false, DesiredDeltaMove, CurrentPosition, PlayLength);

			// Verify position assumptions
			//ensureMsgf(bPlayingBackwards ? (CurrentPosition <= PreviousPosition) : (CurrentPosition >= PreviousPosition), TEXT("in Animation %s(Skeleton %s) : bPlayingBackwards(%d), PreviousPosition(%0.2f), Current Position(%0.2f)"),
			//	*GetName(), *GetNameSafe(GetSkeleton()), bPlayingBackwards, PreviousPosition, CurrentPosition);

			RootMotionParams.Accumulate(ExtractBlendSpaceRootMotionFromRange(PreviousPosition, CurrentPosition));

			// If we've hit the end of the animation, and we're allowed to loop, keep going.
			if ((AdvanceType == ETAA_Finished) && bAllowLooping)
			{
				const float ActualDeltaMove = (CurrentPosition - PreviousPosition);
				DesiredDeltaMove -= ActualDeltaMove;

				PreviousPosition = bPlayingBackwards ? PlayLength : 0.f;
				CurrentPosition = PreviousPosition;
			}
			else
			{
				break;
			}
		} while (true);
	}

	return RootMotionParams.GetRootMotionTransform();
}

void FBlendSpaceSampler::ProcessPlayLength()
{
	TArray<FBlendSampleData> BlendSamples;
	int32 TriangulationIndex = 0;
	Input.BlendSpace->GetSamplesFromBlendInput(Input.BlendParameters, BlendSamples, TriangulationIndex, true);

	PlayLength = Input.BlendSpace->GetAnimationLengthFromSampleData(BlendSamples);

	checkf(PlayLength > 0.0f, TEXT("Blendspace has zero play length"));
}

void FBlendSpaceSampler::ProcessRootTransform()
{
	// Pre-compute root motion

	int32 NumRootSamples = FMath::Max(PlayLength * Input.RootTransformSamplingRate + 1, 1);
	AccumulatedRootTransform.SetNumUninitialized(NumRootSamples);

	TArray<FBlendSampleData> BlendSamples;
	int32 TriangulationIndex = 0;
	Input.BlendSpace->GetSamplesFromBlendInput(Input.BlendParameters, BlendSamples, TriangulationIndex, true);

	FTransform RootMotionAccumulation = FTransform::Identity;

	AccumulatedRootTransform[0] = RootMotionAccumulation;

	for (int32 SampleIdx = 1; SampleIdx < NumRootSamples; ++SampleIdx)
	{
		float PreviousTime = float(SampleIdx - 1) / Input.RootTransformSamplingRate;
		float CurrentTime = float(SampleIdx - 0) / Input.RootTransformSamplingRate;

		FDeltaTimeRecord DeltaTimeRecord;
		DeltaTimeRecord.Set(PreviousTime, CurrentTime - PreviousTime);
		FAnimExtractContext ExtractionCtx(CurrentTime, true, DeltaTimeRecord, Input.bLoopable);

		for (int32 BlendSampleIdex = 0; BlendSampleIdex < BlendSamples.Num(); BlendSampleIdex++)
		{
			float Scale = BlendSamples[BlendSampleIdex].Animation->GetPlayLength() / PlayLength;

			FDeltaTimeRecord BlendSampleDeltaTimeRecord;
			BlendSampleDeltaTimeRecord.Set(DeltaTimeRecord.GetPrevious() * Scale, DeltaTimeRecord.Delta * Scale);

			BlendSamples[BlendSampleIdex].DeltaTimeRecord = BlendSampleDeltaTimeRecord;
			BlendSamples[BlendSampleIdex].PreviousTime = PreviousTime * Scale;
			BlendSamples[BlendSampleIdex].Time = CurrentTime * Scale;
		}

		FCompactPose Pose;
		FBlendedCurve BlendedCurve;
		Anim::FStackAttributeContainer StackAttributeContainer;
		FAnimationPoseData AnimPoseData(Pose, BlendedCurve, StackAttributeContainer);

		Pose.SetBoneContainer(&Input.SamplingContext->BoneContainer);
		BlendedCurve.InitFrom(Input.SamplingContext->BoneContainer);

		Input.BlendSpace->GetAnimationPose(BlendSamples, ExtractionCtx, AnimPoseData);

		const Anim::IAnimRootMotionProvider* RootMotionProvider = Anim::IAnimRootMotionProvider::Get();

		if (ensureMsgf(RootMotionProvider, TEXT("Could not get Root Motion Provider.")))
		{
			if (ensureMsgf(RootMotionProvider->HasRootMotion(StackAttributeContainer), TEXT("Blend Space had no Root Motion Attribute.")))
			{
				FTransform RootMotionDelta;
				RootMotionProvider->ExtractRootMotion(StackAttributeContainer, RootMotionDelta);

				RootMotionAccumulation = RootMotionDelta * RootMotionAccumulation;
			}
		}

		AccumulatedRootTransform[SampleIdx] = RootMotionAccumulation;
	}
}

void FBlendSpaceSampler::ProcessRootDistance()
{
	checkf(AccumulatedRootTransform.Num() > 0, TEXT("ProcessRootTransform must be run first"));

	// Note the distance sampling interval is independent of the schema's sampling interval
	const float DistanceSamplingInterval = 1.0f / Input.RootDistanceSamplingRate;

	const FTransform InitialRootTransform = FTransform::Identity;

	uint32 NumDistanceSamples = FMath::CeilToInt(PlayLength * Input.RootDistanceSamplingRate) + 1;
	AccumulatedRootDistance.Reserve(NumDistanceSamples);

	// Build a distance lookup table by sampling root motion at a fixed rate and accumulating
	// absolute translation deltas. During indexing we'll bsearch this table and interpolate
	// between samples in order to convert distance offsets to time offsets.
	// See also FAssetIndexer::AddTrajectoryDistanceFeatures().
	double TotalAccumulatedRootDistance = 0.0;
	FTransform LastRootTransform = InitialRootTransform;
	float SampleTime = 0.0f;
	for (int32 SampleIdx = 0; SampleIdx != NumDistanceSamples; ++SampleIdx)
	{
		SampleTime = FMath::Min(SampleIdx * DistanceSamplingInterval, PlayLength);

		FTransform RootTransform = ExtractBlendSpaceRootTrackTransform(SampleTime);
		FTransform LocalRootMotion = RootTransform.GetRelativeTransform(LastRootTransform);
		LastRootTransform = RootTransform;

		TotalAccumulatedRootDistance += LocalRootMotion.GetTranslation().Size();
		AccumulatedRootDistance.Add((float)TotalAccumulatedRootDistance);
	}

	// Verify we sampled the final frame of the clip
	check(SampleTime == PlayLength);

	// Also emit root motion summary info to help with sample wrapping in 
	// FAssetIndexer::GetSampleTimeFromDistance() and FAssetIndexer::GetSampleInfo()
	TotalRootTransform = LastRootTransform.GetRelativeTransform(InitialRootTransform);
	TotalRootDistance = AccumulatedRootDistance.Last();
}

//////////////////////////////////////////////////////////////////////////
// FAssetIndexer helpers

struct FSamplingParam
{
	float WrappedParam = 0.0f;
	int32 NumCycles = 0;
	
	// If the animation can't loop, WrappedParam contains the clamped value and whatever is left is stored here
	float Extrapolation = 0.0f;
};

static FSamplingParam WrapOrClampSamplingParam(bool bCanWrap, float SamplingParamExtent, float SamplingParam)
{
	// This is a helper function used by both time and distance sampling. A schema may specify time or distance
	// offsets that are multiple cycles of a clip away from the current pose being sampled.
	// And that time or distance offset may before the beginning of the clip (SamplingParam < 0.0f)
	// or after the end of the clip (SamplingParam > SamplingParamExtent). So this function
	// helps determine how many cycles need to be applied and what the wrapped value should be, clamping
	// if necessary.

	FSamplingParam Result;

	Result.WrappedParam = SamplingParam;

	if (bCanWrap)
	{
		if (SamplingParam < 0.0f)
		{
			while (Result.WrappedParam < 0.0f)
			{
				Result.WrappedParam += SamplingParamExtent;
				++Result.NumCycles;
			}
		}

		else
		{
			while (Result.WrappedParam > SamplingParamExtent)
			{
				Result.WrappedParam -= SamplingParamExtent;
				++Result.NumCycles;
			}
		}
	}

	float ParamClamped = FMath::Clamp(Result.WrappedParam, 0.0f, SamplingParamExtent);
	if (ParamClamped != Result.WrappedParam)
	{
		check(!bCanWrap);
		Result.Extrapolation = Result.WrappedParam - ParamClamped;
		Result.WrappedParam = ParamClamped;
	}
	
	return Result;
}


//////////////////////////////////////////////////////////////////////////
// FAssetIndexer

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
	void Init(const FAssetIndexingContext& IndexingContext);
	bool Process();

public: // IAssetIndexer

	const FAssetIndexingContext& GetIndexingContext() const override { return IndexingContext; }
	FSampleInfo GetSampleInfo(float SampleTime) const override;
	FSampleInfo GetSampleInfoRelative(float SampleTime, const FSampleInfo& Origin) const override;
	const float GetSampleTimeFromDistance(float Distance) const override;
	FTransform MirrorTransform(const FTransform& Transform) const override;
	FTransform GetTransformAndCacheResults(float SampleTime, float OriginTime, int8 SchemaBoneIdx, bool& Clamped) override;

private:
	FAssetIndexingContext IndexingContext;
	TArray<FPoseSearchFeatureVectorBuilder> FeatureVectorBuilders;
	FPoseSearchPoseMetadata Metadata;
	
	void AddMetadata(int32 SampleIdx);

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

	TArray<CachedEntry> CachedEntries;
};

void FAssetIndexer::Reset()
{
	Output.FirstIndexedSample = 0;
	Output.LastIndexedSample = 0;
	Output.NumIndexedPoses = 0;

	Output.FeatureVectorTable.Reset(0);
	Output.PoseMetadata.Reset(0);
	Output.AllFeaturesNotAdded.Reset();
}

void FAssetIndexer::Init(const FAssetIndexingContext& InIndexingContext)
{
	check(InIndexingContext.Schema);
	check(InIndexingContext.Schema->IsValid());
	check(InIndexingContext.MainSampler);

	IndexingContext = InIndexingContext;

	Reset();

	Output.FirstIndexedSample = FMath::FloorToInt(IndexingContext.RequestedSamplingRange.Min * IndexingContext.Schema->SampleRate);
	Output.LastIndexedSample = 
		FMath::Max(0, FMath::CeilToInt(IndexingContext.RequestedSamplingRange.Max * IndexingContext.Schema->SampleRate));
	Output.NumIndexedPoses = Output.LastIndexedSample - Output.FirstIndexedSample + 1;
	
	Output.FeatureVectorTable.SetNumZeroed(IndexingContext.Schema->SchemaCardinality * Output.NumIndexedPoses);

	Output.PoseMetadata.SetNum(Output.NumIndexedPoses);

	FeatureVectorBuilders.SetNum(Output.NumIndexedPoses);
	for (FPoseSearchFeatureVectorBuilder& Builder : FeatureVectorBuilders)
	{
		Builder.Init(IndexingContext.Schema);
	}
}

bool FAssetIndexer::Process()
{
	check(IndexingContext.Schema);
	check(IndexingContext.Schema->IsValid());
	check(IndexingContext.MainSampler);

	FMemMark Mark(FMemStack::Get());

	IndexingContext.BeginSampleIdx = Output.FirstIndexedSample;
	IndexingContext.EndSampleIdx = Output.LastIndexedSample + 1;
	const int32 NumSamplesInRange = IndexingContext.EndSampleIdx - IndexingContext.BeginSampleIdx;

	FAssetIndexingOutput AssetIndexingOutput{FeatureVectorBuilders};

	// Index each channel
	for (int32 ChannelIdx = 0; ChannelIdx != IndexingContext.Schema->Channels.Num(); ++ChannelIdx)
	{
		const TObjectPtr<UPoseSearchFeatureChannel>& Channel = IndexingContext.Schema->Channels[ChannelIdx];
		Channel->IndexAsset(*this, AssetIndexingOutput);
	}

	// @todo: this step can be avoided now since Schema.SchemaCardinality is known and the data can be preallocated before Channel->IndexAsset
	
	// Merge spans of feature vectors into contiguous buffer
	if (IndexingContext.Schema->SchemaCardinality > 0)
	{
		for (int32 VectorIdx = 0; VectorIdx != NumSamplesInRange; ++VectorIdx)
		{
			const int32 SampleIdx = VectorIdx + IndexingContext.BeginSampleIdx;
			const int32 PoseIdx = SampleIdx - Output.FirstIndexedSample;
			const int32 FirstValueIdx = PoseIdx * IndexingContext.Schema->SchemaCardinality;
			TArrayView<float> WriteValues = MakeArrayView(&Output.FeatureVectorTable[FirstValueIdx], IndexingContext.Schema->SchemaCardinality);
			TArrayView<const float> ReadValues = FeatureVectorBuilders[VectorIdx].GetValues();
			check(WriteValues.Num() == ReadValues.Num());
			FMemory::Memcpy(WriteValues.GetData(), ReadValues.GetData(), WriteValues.Num() * WriteValues.GetTypeSize());
		}
	}

	// Generate pose metadata
	for (int32 SampleIdx = IndexingContext.BeginSampleIdx; SampleIdx != IndexingContext.EndSampleIdx; ++SampleIdx)
	{
		AddMetadata(SampleIdx);
		const int32 PoseIdx = SampleIdx - Output.FirstIndexedSample;
		Output.PoseMetadata[PoseIdx] = Metadata;
	}

	return true;
}

const float FAssetIndexer::GetSampleTimeFromDistance(float SampleDistance) const
{
	auto CanWrapDistanceSamples = [](const IAssetSampler* Sampler) -> bool
	{
		constexpr float SMALL_ROOT_DISTANCE = 1.0f;
		return Sampler->IsLoopable() && Sampler->GetTotalRootDistance() > SMALL_ROOT_DISTANCE;
	};

	float MainTotalDistance = IndexingContext.MainSampler->GetTotalRootDistance();
	bool bMainCanWrap = CanWrapDistanceSamples(IndexingContext.MainSampler);

	float SampleTime = MAX_flt;

	if (!bMainCanWrap)
	{
		// Use the lead in anim if we would have to clamp to the beginning of the main anim
		if (IndexingContext.LeadInSampler && (SampleDistance < 0.0f))
		{
			const IAssetSampler* ClipSampler = IndexingContext.LeadInSampler;

			bool bLeadInCanWrap = CanWrapDistanceSamples(IndexingContext.LeadInSampler);
			float LeadRelativeDistance = SampleDistance + ClipSampler->GetTotalRootDistance();
			FSamplingParam SamplingParam = WrapOrClampSamplingParam(bLeadInCanWrap, ClipSampler->GetTotalRootDistance(), LeadRelativeDistance);

			float ClipTime = ClipSampler->GetTimeFromRootDistance(
				SamplingParam.WrappedParam + SamplingParam.Extrapolation);

			// Make the lead in clip time relative to the main sequence again and unwrap
			SampleTime = -((SamplingParam.NumCycles * ClipSampler->GetPlayLength()) + (ClipSampler->GetPlayLength() - ClipTime));
		}

		// Use the follow up anim if we would have clamp to the end of the main anim
		else if (IndexingContext.FollowUpSampler && (SampleDistance > MainTotalDistance))
		{
			const IAssetSampler* ClipSampler = IndexingContext.FollowUpSampler;

			bool bFollowUpCanWrap = CanWrapDistanceSamples(IndexingContext.FollowUpSampler);
			float FollowRelativeDistance = SampleDistance - MainTotalDistance;
			FSamplingParam SamplingParam = WrapOrClampSamplingParam(bFollowUpCanWrap, ClipSampler->GetTotalRootDistance(), FollowRelativeDistance);

			float ClipTime = ClipSampler->GetTimeFromRootDistance(
				SamplingParam.WrappedParam + SamplingParam.Extrapolation);

			// Make the follow up clip time relative to the main sequence again and unwrap
			SampleTime = IndexingContext.MainSampler->GetPlayLength() + SamplingParam.NumCycles * ClipSampler->GetPlayLength() + ClipTime;
		}
	}

	// Use the main anim if we didn't use the lead-in or follow-up anims.
	// The main anim sample may have been wrapped or clamped
	if (SampleTime == MAX_flt)
	{
		float MainRelativeDistance = SampleDistance;
		if (SampleDistance < 0.0f && bMainCanWrap)
		{
			// In this case we're sampling a loop backwards, so MainRelativeDistance must adjust so the number of cycles 
			// is counted correctly.
			MainRelativeDistance += IndexingContext.MainSampler->GetTotalRootDistance();
		}

		FSamplingParam SamplingParam = WrapOrClampSamplingParam(bMainCanWrap, MainTotalDistance, MainRelativeDistance);
		float ClipTime = IndexingContext.MainSampler->GetTimeFromRootDistance(
			SamplingParam.WrappedParam + SamplingParam.Extrapolation);

		// Unwrap the main clip time
		if (bMainCanWrap)
		{
			if (SampleDistance < 0.0f)
			{
				SampleTime = -((SamplingParam.NumCycles * IndexingContext.MainSampler->GetPlayLength()) + (IndexingContext.MainSampler->GetPlayLength() - ClipTime));
			}
			else
			{
				SampleTime = SamplingParam.NumCycles * IndexingContext.MainSampler->GetPlayLength() + ClipTime;
			}
		}
		else
		{
			SampleTime = ClipTime;
		}
	}

	return SampleTime;
}

FAssetIndexer::FSampleInfo FAssetIndexer::GetSampleInfo(float SampleTime) const
{
	FSampleInfo Sample;

	FTransform RootMotionLast = FTransform::Identity;
	FTransform RootMotionInitial = FTransform::Identity;

	float RootDistanceLast = 0.0f;
	float RootDistanceInitial = 0.0f;

	auto CanWrapTimeSamples = [](const IAssetSampler* Sampler) -> bool
	{
		return Sampler->IsLoopable();
	};

	float MainPlayLength = IndexingContext.MainSampler->GetPlayLength();
	bool bMainCanWrap = CanWrapTimeSamples(IndexingContext.MainSampler);

	FSamplingParam SamplingParam;
	if (!bMainCanWrap)
	{
		// Use the lead in anim if we would have to clamp to the beginning of the main anim
		if (IndexingContext.LeadInSampler && (SampleTime < 0.0f))
		{
			const IAssetSampler* ClipSampler = IndexingContext.LeadInSampler;

			bool bLeadInCanWrap = CanWrapTimeSamples(IndexingContext.LeadInSampler);
			float LeadRelativeTime = SampleTime + ClipSampler->GetPlayLength();
			SamplingParam = WrapOrClampSamplingParam(bLeadInCanWrap, ClipSampler->GetPlayLength(), LeadRelativeTime);

			Sample.Clip = IndexingContext.LeadInSampler;

			check(SamplingParam.Extrapolation <= 0.0f);
			if (SamplingParam.Extrapolation < 0.0f)
			{
				RootMotionInitial = IndexingContext.LeadInSampler->GetTotalRootTransform().Inverse();
				RootDistanceInitial = -IndexingContext.LeadInSampler->GetTotalRootDistance();
			}
			else
			{
				RootMotionInitial = FTransform::Identity;
				RootDistanceInitial = 0.0f;
			}

			RootMotionLast = IndexingContext.LeadInSampler->GetTotalRootTransform();
			RootDistanceLast = IndexingContext.LeadInSampler->GetTotalRootDistance();
		}

		// Use the follow up anim if we would have clamp to the end of the main anim
		else if (IndexingContext.FollowUpSampler && (SampleTime > MainPlayLength))
		{
			const IAssetSampler* ClipSampler = IndexingContext.FollowUpSampler;

			bool bFollowUpCanWrap = CanWrapTimeSamples(IndexingContext.FollowUpSampler);
			float FollowRelativeTime = SampleTime - MainPlayLength;
			SamplingParam = WrapOrClampSamplingParam(bFollowUpCanWrap, ClipSampler->GetPlayLength(), FollowRelativeTime);

			Sample.Clip = IndexingContext.FollowUpSampler;

			RootMotionInitial = IndexingContext.MainSampler->GetTotalRootTransform();
			RootDistanceInitial = IndexingContext.MainSampler->GetTotalRootDistance();

			RootMotionLast = IndexingContext.FollowUpSampler->GetTotalRootTransform();
			RootDistanceLast = IndexingContext.FollowUpSampler->GetTotalRootDistance();
		}
	}

	// Use the main anim if we didn't use the lead-in or follow-up anims.
	// The main anim sample may have been wrapped or clamped
	if (!Sample.IsValid())
	{
		float MainRelativeTime = SampleTime;
		if (SampleTime < 0.0f && bMainCanWrap)
		{
			// In this case we're sampling a loop backwards, so MainRelativeTime must adjust so the number of cycles is
			// counted correctly.
			MainRelativeTime += MainPlayLength;
		}

		SamplingParam = WrapOrClampSamplingParam(bMainCanWrap, MainPlayLength, MainRelativeTime);

		Sample.Clip = IndexingContext.MainSampler;

		RootMotionInitial = FTransform::Identity;
		RootDistanceInitial = 0.0f;

		RootMotionLast = IndexingContext.MainSampler->GetTotalRootTransform();
		RootDistanceLast = IndexingContext.MainSampler->GetTotalRootDistance();
	}


	if (FMath::Abs(SamplingParam.Extrapolation) > SMALL_NUMBER)
	{
		Sample.bClamped = true;
		Sample.ClipTime = SamplingParam.WrappedParam + SamplingParam.Extrapolation;
		const FTransform ClipRootMotion = Sample.Clip->ExtractRootTransform(Sample.ClipTime);
		const float ClipDistance = Sample.Clip->ExtractRootDistance(Sample.ClipTime);

		Sample.RootTransform = ClipRootMotion * RootMotionInitial;
		Sample.RootDistance = RootDistanceInitial + ClipDistance;
	}
	else
	{
		Sample.ClipTime = SamplingParam.WrappedParam;

		// Determine how to accumulate motion for every cycle of the anim. If the sample
		// had to be clamped, this motion will end up not getting applied below.
		// Also invert the accumulation direction if the requested sample was wrapped backwards.
		FTransform RootMotionPerCycle = RootMotionLast;
		float RootDistancePerCycle = RootDistanceLast;
		if (SampleTime < 0.0f)
		{
			RootMotionPerCycle = RootMotionPerCycle.Inverse();
			RootDistancePerCycle *= -1;
		}

		// Find the remaining motion deltas after wrapping
		FTransform RootMotionRemainder = Sample.Clip->ExtractRootTransform(Sample.ClipTime);
		float RootDistanceRemainder = Sample.Clip->ExtractRootDistance(Sample.ClipTime);

		// Invert motion deltas if we wrapped backwards
		if (SampleTime < 0.0f)
		{
			RootMotionRemainder.SetToRelativeTransform(RootMotionLast);
			RootDistanceRemainder = -(RootDistanceLast - RootDistanceRemainder);
		}

		Sample.RootTransform = RootMotionInitial;
		Sample.RootDistance = RootDistanceInitial;

		// Note if the sample was clamped, no motion will be applied here because NumCycles will be zero
		int32 CyclesRemaining = SamplingParam.NumCycles;
		while (CyclesRemaining--)
		{
			Sample.RootTransform = RootMotionPerCycle * Sample.RootTransform;
         	Sample.RootDistance += RootDistancePerCycle;
		}

		Sample.RootTransform = RootMotionRemainder * Sample.RootTransform;
		Sample.RootDistance += RootDistanceRemainder;
	}

	return Sample;
}

FAssetIndexer::FSampleInfo FAssetIndexer::GetSampleInfoRelative(float SampleTime, const FSampleInfo& Origin) const
{
	FSampleInfo Sample = GetSampleInfo(SampleTime);
	Sample.RootTransform.SetToRelativeTransform(Origin.RootTransform);
	Sample.RootDistance = Origin.RootDistance - Sample.RootDistance;
	return Sample;
}

FTransform FAssetIndexer::MirrorTransform(const FTransform& Transform) const
{
	return IndexingContext.bMirrored ? IndexingContext.SamplingContext->MirrorTransform(Transform) : Transform;
}

void FAssetIndexer::AddMetadata(int32 SampleIdx)
{
	const float SequenceLength = IndexingContext.MainSampler->GetPlayLength();
	const float SampleTime = FMath::Min(SampleIdx * IndexingContext.Schema->GetSamplingInterval(), SequenceLength);

	Metadata = FPoseSearchPoseMetadata();
	Metadata.CostAddend = IndexingContext.Schema->BaseCostBias;
	Metadata.ContinuingPoseCostAddend = IndexingContext.Schema->ContinuingPoseCostBias;
	
	TArray<UAnimNotifyState_PoseSearchBase*> NotifyStates;
	IndexingContext.MainSampler->ExtractPoseSearchNotifyStates(SampleTime, NotifyStates);
	for (const UAnimNotifyState_PoseSearchBase* PoseSearchNotify : NotifyStates)
	{
		if (PoseSearchNotify->GetClass()->IsChildOf<UAnimNotifyState_PoseSearchBlockTransition>())
		{
			EnumAddFlags(Metadata.Flags, EPoseSearchPoseFlags::BlockTransition);
		}
		else if (PoseSearchNotify->GetClass()->IsChildOf<UAnimNotifyState_PoseSearchModifyCost>())
		{
			const UAnimNotifyState_PoseSearchModifyCost* ModifyCostNotify =
				Cast<const UAnimNotifyState_PoseSearchModifyCost>(PoseSearchNotify);
			Metadata.CostAddend = ModifyCostNotify->CostAddend;
		}
		else if (PoseSearchNotify->GetClass()->IsChildOf<UAnimNotifyState_PoseSearchOverrideContinuingPoseCostBias>())
		{
			const UAnimNotifyState_PoseSearchOverrideContinuingPoseCostBias* ContinuingPoseCostBias =
				Cast<const UAnimNotifyState_PoseSearchOverrideContinuingPoseCostBias>(PoseSearchNotify);
			Metadata.ContinuingPoseCostAddend = ContinuingPoseCostBias->CostAddend;
		}
	}
}

FTransform FAssetIndexer::GetTransformAndCacheResults(float SampleTime, float OriginTime, int8 SchemaBoneIdx, bool& Clamped)
{
	// @todo: use an hashmap if we end up having too many entries
	CachedEntry* Entry = CachedEntries.FindByPredicate([SampleTime, OriginTime](const FAssetIndexer::CachedEntry& Entry)
	{
		return Entry.SampleTime == SampleTime && Entry.OriginTime == OriginTime;
	});

	const FAssetSamplingContext* SamplingContext = IndexingContext.SamplingContext;

	if (!Entry)
	{
		Entry = &CachedEntries[CachedEntries.AddDefaulted()];

		Entry->SampleTime = SampleTime;
		Entry->OriginTime = OriginTime;

		if (!SamplingContext->BoneContainer.IsValid())
		{
			UE_LOG(LogPoseSearch,
				Warning, 
				TEXT("Invalid BoneContainer encountered in FAssetIndexer::GetTransformAndCacheResults. Asset: %s. Schema: %s. BoneContainerAsset: %s. NumBoneIndices: %d"),
				*GetNameSafe(IndexingContext.MainSampler->GetAsset()),
				*GetNameSafe(IndexingContext.Schema),
				*GetNameSafe(SamplingContext->BoneContainer.GetAsset()),
				SamplingContext->BoneContainer.GetCompactPoseNumBones());
		}

		Entry->Pose.SetBoneContainer(&SamplingContext->BoneContainer);
		Entry->UnusedCurve.InitFrom(SamplingContext->BoneContainer);

		IAssetIndexer::FSampleInfo Origin = GetSampleInfo(OriginTime);
		IAssetIndexer::FSampleInfo Sample = GetSampleInfoRelative(SampleTime, Origin);

		float CurrentTime = Sample.ClipTime;
		float PreviousTime = CurrentTime - SamplingContext->FiniteDelta;

		FDeltaTimeRecord DeltaTimeRecord;
		DeltaTimeRecord.Set(PreviousTime, CurrentTime - PreviousTime);
		FAnimExtractContext ExtractionCtx(CurrentTime, true, DeltaTimeRecord, Sample.Clip->IsLoopable());

		Sample.Clip->ExtractPose(ExtractionCtx, Entry->AnimPoseData);

		if (IndexingContext.bMirrored)
		{
			FAnimationRuntime::MirrorPose(
				Entry->AnimPoseData.GetPose(),
				IndexingContext.Schema->MirrorDataTable->MirrorAxis,
				SamplingContext->CompactPoseMirrorBones,
				SamplingContext->ComponentSpaceRefRotations
			);
			// Note curves and attributes are not used during the indexing process and therefore don't need to be mirrored
		}

		Entry->ComponentSpacePose.InitPose(Entry->Pose);
		Entry->RootTransform = Sample.RootTransform;
		Entry->Clamped = Sample.bClamped;
	}

	const FBoneReference& BoneReference = IndexingContext.Schema->BoneReferences[SchemaBoneIdx];
	FCompactPoseBoneIndex CompactBoneIndex = SamplingContext->BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(BoneReference.BoneIndex));

	const FTransform BoneTransform = Entry->ComponentSpacePose.GetComponentSpaceTransform(CompactBoneIndex) * MirrorTransform(Entry->RootTransform);
	Clamped = Entry->Clamped;

	return BoneTransform;
}

//////////////////////////////////////////////////////////////////////////
// PoseSearch API

void DrawFeatureVector(const FDebugDrawParams& DrawParams, TArrayView<const float> PoseVector)
{
	if (DrawParams.CanDraw())
	{
		const UPoseSearchSchema* Schema = DrawParams.GetSchema();
		check(Schema);

		if (PoseVector.Num() == Schema->SchemaCardinality)
		{
			for (int32 ChannelIdx = 0; ChannelIdx != Schema->Channels.Num(); ++ChannelIdx)
			{
				if (DrawParams.ChannelMask & (1 << ChannelIdx))
				{
					Schema->Channels[ChannelIdx]->DebugDraw(DrawParams, PoseVector);
				}
			}
		}
	}
}

void DrawFeatureVector(const FDebugDrawParams& DrawParams, int32 PoseIdx)
{
	// if we're editing the schema while in PIE with Rewind Debugger active, PoseIdx could be out of bound / stale
	if (DrawParams.CanDraw() && PoseIdx >= 0 && PoseIdx < DrawParams.GetSearchIndex()->NumPoses)
	{
		DrawFeatureVector(DrawParams, DrawParams.GetSearchIndex()->GetPoseValues(PoseIdx));
	}
}

void DrawSearchIndex(const FDebugDrawParams& DrawParams)
{
	if (DrawParams.CanDraw())
	{
		const FPoseSearchIndex* SearchIndex = DrawParams.GetSearchIndex();
		const int32 LastPoseIdx = SearchIndex->NumPoses;
		for (int32 PoseIdx = 0; PoseIdx != LastPoseIdx; ++PoseIdx)
		{
			DrawFeatureVector(DrawParams, PoseIdx);
		}
	}
}

static Eigen::VectorXd ComputeChannelsDeviations(const FPoseSearchIndex* SearchIndex)
{
	// This function performs a modified z-score normalization where features are normalized
	// by mean absolute deviation rather than standard deviation. Both methods are preferable
	// here to min-max scaling because they preserve outliers.
	// 
	// Mean absolute deviation is preferred here over standard deviation because the latter
	// emphasizes outliers since squaring the distance from the mean increases variance 
	// exponentially rather than additively and square rooting the sum of squares does not 
	// remove that bias. [1]
	//
	// References:
	// [1] Gorard, S. (2005), "Revisiting a 90-Year-Old Debate: The Advantages of the Mean Deviation."
	//     British Journal of Educational Studies, 53: 417-430.

	using namespace Eigen;

	check(SearchIndex->IsValid());

	const int32 NumPoses = SearchIndex->NumPoses;
	const int32 NumDimensions = SearchIndex->Schema->SchemaCardinality;

	// Compute per Channel average distances
	VectorXd MeanDeviations(NumDimensions);
	MeanDeviations.setConstant(1.0);

	if (NumPoses > 0)
	{
		// Map input buffer
		auto PoseMatrixSourceMap = RowMajorMatrixMapConst(
			SearchIndex->Values.GetData(),
			NumPoses,		// rows
			NumDimensions	// cols
		);

		// @todo: evaluate removing the cast to double

		// Copy row major float matrix to column major double matrix
		MatrixXd PoseMatrix = PoseMatrixSourceMap.transpose().cast<double>();
		checkSlow(PoseMatrix.rows() == NumDimensions);
		checkSlow(PoseMatrix.cols() == NumPoses);

		// Mean center
		VectorXd SampleMean = PoseMatrix.rowwise().mean();
		PoseMatrix = PoseMatrix.colwise() - SampleMean;

		for (int ChannelIdx = 0; ChannelIdx != SearchIndex->Schema->Channels.Num(); ++ChannelIdx)
		{
			const UPoseSearchFeatureChannel* Channel = SearchIndex->Schema->Channels[ChannelIdx].Get();
			Channel->ComputeMeanDeviations(PoseMatrix, MeanDeviations);
		}
	}

	return MeanDeviations;
}

static void PreprocessSearchIndexWeights(FPoseSearchIndex& SearchIndex, const UPoseSearchDatabase* Database)
{
	const int32 NumDimensions = Database->Schema->SchemaCardinality;
	SearchIndex.WeightsSqrt.Init(1.f, NumDimensions);

	for (int ChannelIdx = 0; ChannelIdx != Database->Schema->Channels.Num(); ++ChannelIdx)
	{
		const UPoseSearchFeatureChannel* Channel = Database->Schema->Channels[ChannelIdx].Get();
		Channel->FillWeights(SearchIndex.WeightsSqrt);
	}

	Eigen::VectorXd ChannelsMeanDeviations = ComputeChannelsDeviations(Database->GetSearchIndex());
	TArray<float> Deviation;
	Deviation.Init(1.f, NumDimensions);
	for (int32 Dimension = 0; Dimension != NumDimensions; ++Dimension)
	{
		const float ChannelsMeanDeviation = ChannelsMeanDeviations[Dimension];
		Deviation[Dimension] = ChannelsMeanDeviation;
	}

	EPoseSearchDataPreprocessor DataPreprocessor = Database->GetSearchIndex()->Schema->DataPreprocessor;

	if (DataPreprocessor == EPoseSearchDataPreprocessor::Normalize)
	{
		// normalizing user weights: the idea behind this step is to be able to compare poses from databases using different schemas
		RowMajorVectorMap MapWeights(SearchIndex.WeightsSqrt.GetData(), 1, NumDimensions);
		const float WeightsSum = MapWeights.sum();
		if (!FMath::IsNearlyZero(WeightsSum))
		{
			MapWeights *= (1.0f / WeightsSum);
		}
	}

	// extracting the square root
	for (int32 Dimension = 0; Dimension != NumDimensions; ++Dimension)
	{
		SearchIndex.WeightsSqrt[Dimension] = FMath::Sqrt(SearchIndex.WeightsSqrt[Dimension]);
	}

	if (DataPreprocessor == EPoseSearchDataPreprocessor::Normalize || DataPreprocessor == EPoseSearchDataPreprocessor::NormalizeOnlyByDeviation)
	{
		for (int32 Dimension = 0; Dimension != NumDimensions; ++Dimension)
		{
			// the idea here is to premultiply the weights by the inverse of the variance (proportional to the square of the deviation) to have a "weighted Mahalanobis" distance
			SearchIndex.WeightsSqrt[Dimension] /= Deviation[Dimension];
		}
	}

#if WITH_EDITORONLY_DATA
	SearchIndex.Deviation = Deviation;
#endif // WITH_EDITORONLY_DATA
}

// it calculates Mean, PCAValues, and PCAProjectionMatrix
static void PreprocessSearchIndexPCAData(FPoseSearchIndex& SearchIndex, const UPoseSearchDatabase* Database)
{
	// binding SearchIndex.Values and SearchIndex.PCAValues Eigen row major matrix maps
	check(Database && Database->Schema);
	const int32 NumPoses = SearchIndex.NumPoses;
	const uint32 NumberOfPrincipalComponents = Database->GetNumberOfPrincipalComponents();
	const int32 NumDimensions = Database->Schema->SchemaCardinality;

	SearchIndex.PCAValues.Reset();
	SearchIndex.Mean.Reset();
	SearchIndex.PCAProjectionMatrix.Reset();

	SearchIndex.PCAValues.AddZeroed(NumPoses * NumberOfPrincipalComponents);
	SearchIndex.Mean.AddZeroed(NumDimensions);
	SearchIndex.PCAProjectionMatrix.AddZeroed(NumDimensions * NumberOfPrincipalComponents);

#if WITH_EDITORONLY_DATA
	SearchIndex.PCAExplainedVariance = 0.f;
#endif

	if (NumDimensions > 0)
	{
		const RowMajorVectorMapConst MapWeightsSqrt(SearchIndex.WeightsSqrt.GetData(), 1, NumDimensions);
		const RowMajorMatrixMapConst MapValues(SearchIndex.Values.GetData(), NumPoses, NumDimensions);
		const RowMajorMatrix WeightedValues = MapValues.array().rowwise() * MapWeightsSqrt.array();
		RowMajorMatrixMap MapPCAValues(SearchIndex.PCAValues.GetData(), NumPoses, NumberOfPrincipalComponents);

		// calculating the mean
		RowMajorVectorMap Mean(SearchIndex.Mean.GetData(), 1, NumDimensions);
		Mean = WeightedValues.colwise().mean();

		// use the mean to center the data points
		const RowMajorMatrix CenteredValues = WeightedValues.rowwise() - Mean;

		// estimating the covariance matrix (with dimensionality of NumDimensions, NumDimensions)
		// formula: https://en.wikipedia.org/wiki/Covariance_matrix#Estimation
		// details: https://en.wikipedia.org/wiki/Estimation_of_covariance_matrices
		const ColMajorMatrix CovariantMatrix = (CenteredValues.transpose() * CenteredValues) / float(NumPoses - 1);
		const Eigen::SelfAdjointEigenSolver<ColMajorMatrix> EigenSolver(CovariantMatrix);

		check(EigenSolver.info() == Eigen::Success);

		// validating EigenSolver results
		const ColMajorMatrix EigenVectors = EigenSolver.eigenvectors().real();

		if (Database->PoseSearchMode == EPoseSearchMode::PCAKDTree_Validate && NumberOfPrincipalComponents == NumDimensions)
		{
			const RowMajorVector ReciprocalWeightsSqrt = MapWeightsSqrt.cwiseInverse();
			const RowMajorMatrix ProjectedValues = CenteredValues * EigenVectors;
			for (Eigen::Index RowIndex = 0; RowIndex < MapValues.rows(); ++RowIndex)
			{
				const RowMajorVector WeightedReconstructedPoint = ProjectedValues.row(RowIndex) * EigenVectors.transpose() + Mean;
				const RowMajorVector ReconstructedPoint = WeightedReconstructedPoint.array() * ReciprocalWeightsSqrt.array();
				const float Error = (ReconstructedPoint - MapValues.row(RowIndex)).squaredNorm();
				check(Error < UE_KINDA_SMALL_NUMBER);
			}
		}

		// sorting EigenVectors by EigenValues, so we pick the most significant ones to compose our PCA projection matrix.
		const RowMajorVector EigenValues = EigenSolver.eigenvalues().real();
		TArray<size_t> Indexer;
		Indexer.Reserve(NumDimensions);
		for (size_t DimensionIndex = 0; DimensionIndex < NumDimensions; ++DimensionIndex)
		{
			Indexer.Push(DimensionIndex);
		}
		Indexer.Sort([&EigenValues](size_t a, size_t b)
		{
			return EigenValues[a] > EigenValues[b];
		});

		// composing the PCA projection matrix with the PCANumComponents most significant EigenVectors
		ColMajorMatrixMap PCAProjectionMatrix(SearchIndex.PCAProjectionMatrix.GetData(), NumDimensions, NumberOfPrincipalComponents);
		float AccumulatedVariance = 0.f;
		for (size_t PCAComponentIndex = 0; PCAComponentIndex < NumberOfPrincipalComponents; ++PCAComponentIndex)
		{
			PCAProjectionMatrix.col(PCAComponentIndex) = EigenVectors.col(Indexer[PCAComponentIndex]);
			AccumulatedVariance += EigenValues[Indexer[PCAComponentIndex]];
		}

#if WITH_EDITORONLY_DATA
		// calculating the total variance knowing that eigen values measure variance along the principal components:
		const float TotalVariance = EigenValues.sum();
		// and explained variance as ratio between AccumulatedVariance and TotalVariance: https://ro-che.info/articles/2017-12-11-pca-explained-variance
		SearchIndex.PCAExplainedVariance = TotalVariance > UE_KINDA_SMALL_NUMBER ? AccumulatedVariance / TotalVariance : 0.f;
#endif // WITH_EDITORONLY_DATA

		MapPCAValues = CenteredValues * PCAProjectionMatrix;

		if (Database->PoseSearchMode == EPoseSearchMode::PCAKDTree_Validate && NumberOfPrincipalComponents == NumDimensions)
		{
			const RowMajorVector ReciprocalWeightsSqrt = MapWeightsSqrt.cwiseInverse();
			for (Eigen::Index RowIndex = 0; RowIndex < MapValues.rows(); ++RowIndex)
			{
				const RowMajorVector WeightedReconstructedValues = MapPCAValues.row(RowIndex) * PCAProjectionMatrix.transpose() + Mean;
				const RowMajorVector ReconstructedValues = WeightedReconstructedValues.array() * ReciprocalWeightsSqrt.array();
				const float Error = (ReconstructedValues - MapValues.row(RowIndex)).squaredNorm();
				check(Error < UE_KINDA_SMALL_NUMBER);
			}
		}
	}
}

static void PreprocessSearchIndexKDTree(FPoseSearchIndex& SearchIndex, const UPoseSearchDatabase* Database)
{
	const int32 NumPoses = SearchIndex.NumPoses;
	const uint32 NumberOfPrincipalComponents = Database->GetNumberOfPrincipalComponents();
	SearchIndex.KDTree.Construct(NumPoses, NumberOfPrincipalComponents, SearchIndex.PCAValues.GetData(), Database->KDTreeMaxLeafSize);

	if (Database->PoseSearchMode == EPoseSearchMode::PCAKDTree_Validate)
	{
		// testing the KDTree is returning the proper searches for all the points in pca space
		int32 NumberOfFailingPoints = 0;
		for (size_t PointIndex = 0; PointIndex < NumPoses; ++PointIndex)
		{
			TArray<size_t> ResultIndexes;
			TArray<float> ResultDistanceSqr;
			ResultIndexes.SetNum(Database->KDTreeQueryNumNeighbors + 1);
			ResultDistanceSqr.SetNum(Database->KDTreeQueryNumNeighbors + 1);
			FKDTree::KNNResultSet ResultSet(Database->KDTreeQueryNumNeighbors, ResultIndexes, ResultDistanceSqr);
			SearchIndex.KDTree.FindNeighbors(ResultSet, &SearchIndex.PCAValues[PointIndex * NumberOfPrincipalComponents]);

			size_t ResultIndex = 0;
			for (; ResultIndex < ResultSet.Num(); ++ResultIndex)
			{
				if (PointIndex == ResultIndexes[ResultIndex])
				{
					check(ResultDistanceSqr[ResultIndex] < UE_KINDA_SMALL_NUMBER);
					break;
				}
			}
			if (ResultIndex == ResultSet.Num())
			{
				++NumberOfFailingPoints;
			}
		}

		check(NumberOfFailingPoints == 0);

		// testing the KDTree is returning the proper searches for all the original points transformed in pca space
		NumberOfFailingPoints = 0;
		const int32 NumDimensions = Database->Schema->SchemaCardinality;
		for (size_t PointIndex = 0; PointIndex < NumPoses; ++PointIndex)
		{
			TArray<size_t> ResultIndexes;
			TArray<float> ResultDistanceSqr;
			ResultIndexes.SetNum(Database->KDTreeQueryNumNeighbors + 1);
			ResultDistanceSqr.SetNum(Database->KDTreeQueryNumNeighbors + 1);
			FKDTree::KNNResultSet ResultSet(Database->KDTreeQueryNumNeighbors, ResultIndexes, ResultDistanceSqr);

			const RowMajorVectorMapConst MapValues(&SearchIndex.Values[PointIndex * NumDimensions], 1, NumDimensions);
			const RowMajorVectorMapConst MapWeightsSqrt(SearchIndex.WeightsSqrt.GetData(), 1, NumDimensions);
			const RowMajorVectorMapConst Mean(SearchIndex.Mean.GetData(), 1, NumDimensions);
			const ColMajorMatrixMapConst PCAProjectionMatrix(SearchIndex.PCAProjectionMatrix.GetData(), NumDimensions, NumberOfPrincipalComponents);

			const RowMajorMatrix WeightedValues = MapValues.array() * MapWeightsSqrt.array();
			const RowMajorMatrix CenteredValues = WeightedValues - Mean;
			const RowMajorVector ProjectedValues  = CenteredValues * PCAProjectionMatrix;

			SearchIndex.KDTree.FindNeighbors(ResultSet, ProjectedValues.data());

			size_t ResultIndex = 0;
			for (; ResultIndex < ResultSet.Num(); ++ResultIndex)
			{
				if (PointIndex == ResultIndexes[ResultIndex])
				{
					check(ResultDistanceSqr[ResultIndex] < UE_KINDA_SMALL_NUMBER);
					break;
				}
			}
			if (ResultIndex == ResultSet.Num())
			{
				++NumberOfFailingPoints;
			}
		}

		check(NumberOfFailingPoints == 0);
	}
}

static void PreprocessSearchIndex(FPoseSearchIndex& SearchIndex, const UPoseSearchDatabase* Database)
{
	PreprocessSearchIndexWeights(SearchIndex, Database);
	PreprocessSearchIndexPCAData(SearchIndex, Database);
	PreprocessSearchIndexKDTree(SearchIndex, Database);
}

bool BuildIndex(const UAnimSequence* Sequence, UPoseSearchSequenceMetaData* SequenceMetaData)
{
	check(Sequence);
	check(SequenceMetaData);

	if (!SequenceMetaData->IsValidForIndexing())
	{
		return false;
	}

	USkeleton* SeqSkeleton = Sequence->GetSkeleton();
	if (!SeqSkeleton || !SeqSkeleton->IsCompatible(SequenceMetaData->Schema->Skeleton))
	{
		return false;
	}

	FAssetSamplingContext SamplingContext;
	SamplingContext.Init(SequenceMetaData->Schema);

	FSequenceSampler Sampler;
	FSequenceSampler::FInput SamplerInput;
	SamplerInput.ExtrapolationParameters = SequenceMetaData->ExtrapolationParameters;
	SamplerInput.Sequence = Sequence;
	SamplerInput.bLoopable = false;
	Sampler.Init(SamplerInput);
	Sampler.Process();

	FAssetIndexer Indexer;
	FAssetIndexingContext IndexerContext;
	IndexerContext.SamplingContext = &SamplingContext;
	IndexerContext.MainSampler = &Sampler;
	IndexerContext.Schema = SequenceMetaData->Schema;
	IndexerContext.RequestedSamplingRange = GetEffectiveSamplingRange(Sequence, SequenceMetaData->SamplingRange);
	Indexer.Init(IndexerContext);
	if (!Indexer.Process())
	{
		return false;
	}

	SequenceMetaData->SearchIndex.Assets.Empty();
	FPoseSearchIndexAsset SearchIndexAsset;
	SearchIndexAsset.SourceAssetIdx = 0;
	SearchIndexAsset.FirstPoseIdx = 0;
	SearchIndexAsset.NumPoses = Indexer.Output.NumIndexedPoses;
	SearchIndexAsset.SamplingInterval = IndexerContext.RequestedSamplingRange;

	SequenceMetaData->SearchIndex.Values = Indexer.Output.FeatureVectorTable;
	SequenceMetaData->SearchIndex.NumPoses = Indexer.Output.NumIndexedPoses;
	SequenceMetaData->SearchIndex.Schema = SequenceMetaData->Schema;
	SequenceMetaData->SearchIndex.Assets.Add(SearchIndexAsset);
	SequenceMetaData->SearchIndex.PoseMetadata = Indexer.Output.PoseMetadata;

	// todo: do we need to PreprocessSearchIndex?
	// PreprocessSearchIndex(&SequenceMetaData->SearchIndex, Database);
	return true;
}

struct FDatabaseIndexingContext
{
	const UPoseSearchDatabase* Database = nullptr;
	FPoseSearchIndex* SearchIndex = nullptr;

	FAssetSamplingContext SamplingContext;
	TArray<FSequenceSampler> SequenceSamplers;
	TMap<const UAnimSequence*, int32> SequenceSamplerMap;
	TArray<FBlendSpaceSampler> BlendSpaceSamplers;
	TMap<TPair<const UBlendSpace*, FVector>, int32> BlendSpaceSamplerMap;

	TArray<FAssetIndexer> Indexers;

	void PrepareSamplers();
	void PrepareIndexers();
	bool IndexAssets();
	void JoinIndex();
	float CalculateMinCostAddend() const;
};

void FDatabaseIndexingContext::PrepareSamplers()
{
	SamplingContext.Init(Database->Schema);

	// Prepare samplers for all sequences

	auto AddSequenceSampler = [&](const UAnimSequence* Sequence, bool bLoopable)
	{
		if (!SequenceSamplerMap.Contains(Sequence))
		{
			int32 SequenceSamplerIdx = SequenceSamplers.AddDefaulted();
			SequenceSamplerMap.Add(Sequence, SequenceSamplerIdx);

			FSequenceSampler::FInput Input;
			Input.ExtrapolationParameters = Database->ExtrapolationParameters;
			Input.Sequence = Sequence;
			Input.bLoopable = bLoopable;
			SequenceSamplers[SequenceSamplerIdx].Init(Input);
		}
	};

	for (const FPoseSearchDatabaseSequence& DbSequence : Database->Sequences)
	{
		if (DbSequence.Sequence)
		{
			AddSequenceSampler(DbSequence.Sequence, DbSequence.Sequence->bLoop);
		}

		if (DbSequence.LeadInSequence)
		{
			AddSequenceSampler(DbSequence.LeadInSequence, DbSequence.LeadInSequence->bLoop);
		}

		if (DbSequence.FollowUpSequence)
		{
			AddSequenceSampler(DbSequence.FollowUpSequence, DbSequence.FollowUpSequence->bLoop);
		}
	}

	ParallelFor(
		SequenceSamplers.Num(),
		[this](int32 SamplerIdx) { SequenceSamplers[SamplerIdx].Process(); },
		ParallelForFlags
	);

	// Prepare samplers for all blend spaces

	for (const FPoseSearchDatabaseBlendSpace& DbBlendSpace : Database->BlendSpaces)
	{
		if (DbBlendSpace.BlendSpace)
		{
			int32 HorizontalBlendNum, VerticalBlendNum;
			float HorizontalBlendMin, HorizontalBlendMax, VerticalBlendMin, VerticalBlendMax;

			DbBlendSpace.GetBlendSpaceParameterSampleRanges(
				HorizontalBlendNum,
				VerticalBlendNum,
				HorizontalBlendMin,
				HorizontalBlendMax,
				VerticalBlendMin,
				VerticalBlendMax);

			for (int32 HorizontalIndex = 0; HorizontalIndex < HorizontalBlendNum; HorizontalIndex++)
			{
				for (int32 VerticalIndex = 0; VerticalIndex < VerticalBlendNum; VerticalIndex++)
				{
					FVector BlendParameters = BlendParameterForSampleRanges(
						HorizontalIndex,
						VerticalIndex,
						HorizontalBlendNum,
						VerticalBlendNum,
						HorizontalBlendMin,
						HorizontalBlendMax,
						VerticalBlendMin,
						VerticalBlendMax);

					if (!BlendSpaceSamplerMap.Contains({ DbBlendSpace.BlendSpace, BlendParameters }))
					{
						int32 BlendSpaceSamplerIdx = BlendSpaceSamplers.AddDefaulted();
						BlendSpaceSamplerMap.Add({ DbBlendSpace.BlendSpace, BlendParameters }, BlendSpaceSamplerIdx);

						FBlendSpaceSampler::FInput Input;
						Input.SamplingContext = &SamplingContext;
						Input.ExtrapolationParameters = Database->ExtrapolationParameters;
						Input.BlendSpace = DbBlendSpace.BlendSpace;
						Input.bLoopable = DbBlendSpace.BlendSpace->bLoop;
						Input.BlendParameters = BlendParameters;

						BlendSpaceSamplers[BlendSpaceSamplerIdx].Init(Input);
					}
				}
			}
		}
	}

	ParallelFor(
		BlendSpaceSamplers.Num(),
		[this](int32 SamplerIdx) { BlendSpaceSamplers[SamplerIdx].Process(); },
		ParallelForFlags
	);
}

void FDatabaseIndexingContext::PrepareIndexers()
{
	Indexers.Reserve(Database->GetSearchIndex()->Assets.Num());

	auto GetSequenceSampler = [&](const UAnimSequence* Sequence) -> const FSequenceSampler*
	{
		return Sequence ? &SequenceSamplers[SequenceSamplerMap[Sequence]] : nullptr;
	};

	auto GetBlendSpaceSampler = [&](const UBlendSpace* BlendSpace, const FVector BlendParameters) -> const FBlendSpaceSampler*
	{
		return BlendSpace ? &BlendSpaceSamplers[BlendSpaceSamplerMap[{BlendSpace, BlendParameters}]] : nullptr;
	};

	Indexers.Reserve(Database->GetSearchIndex()->Assets.Num());
	for (int32 AssetIdx = 0; AssetIdx != SearchIndex->Assets.Num(); ++AssetIdx)
	{
		const FPoseSearchIndexAsset& SearchIndexAsset = SearchIndex->Assets[AssetIdx];

		FAssetIndexingContext IndexerContext;
		IndexerContext.SamplingContext = &SamplingContext;
		IndexerContext.Schema = Database->Schema;
		IndexerContext.RequestedSamplingRange = SearchIndexAsset.SamplingInterval;
		IndexerContext.bMirrored = SearchIndexAsset.bMirrored;

		if (SearchIndexAsset.Type == ESearchIndexAssetType::Sequence)
		{
			const FPoseSearchDatabaseSequence& DbSequence = Database->GetSequenceSourceAsset(&SearchIndexAsset);
			const float SequenceLength = DbSequence.Sequence->GetPlayLength();
			IndexerContext.MainSampler = GetSequenceSampler(DbSequence.Sequence);
			IndexerContext.LeadInSampler = SearchIndexAsset.SamplingInterval.Min == 0.0f ? GetSequenceSampler(DbSequence.LeadInSequence) : nullptr;
			IndexerContext.FollowUpSampler = SearchIndexAsset.SamplingInterval.Max == SequenceLength ? GetSequenceSampler(DbSequence.FollowUpSequence) : nullptr;
		}
		else if (SearchIndexAsset.Type == ESearchIndexAssetType::BlendSpace)
		{
			const FPoseSearchDatabaseBlendSpace& DbBlendSpace = Database->GetBlendSpaceSourceAsset(&SearchIndexAsset);
			IndexerContext.MainSampler = GetBlendSpaceSampler(DbBlendSpace.BlendSpace, SearchIndexAsset.BlendParameters);
		}
		else
		{
			checkNoEntry();
		}

		FAssetIndexer& Indexer = Indexers.AddDefaulted_GetRef();
		Indexer.Init(IndexerContext);
	}
}

bool FDatabaseIndexingContext::IndexAssets()
{
	// Index asset data
	ParallelFor(
		Indexers.Num(),
		[this](int32 AssetIdx) { Indexers[AssetIdx].Process(); },
		ParallelForFlags
	);
	return true;
}

float FDatabaseIndexingContext::CalculateMinCostAddend() const
{
	float MinCostAddend = 0.f;

	check(SearchIndex);
	if (!SearchIndex->PoseMetadata.IsEmpty())
	{
		MinCostAddend = MAX_FLT;
		for (const FPoseSearchPoseMetadata& PoseMetadata : SearchIndex->PoseMetadata)
		{
			if (PoseMetadata.CostAddend < MinCostAddend)
			{
				MinCostAddend = PoseMetadata.CostAddend;
			}
		}
	}
	return MinCostAddend;
}

void FDatabaseIndexingContext::JoinIndex()
{
	// Write index info to asset and count up total poses and storage required
	int32 TotalPoses = 0;
	int32 TotalFloats = 0;

	check(SearchIndex);

	// Join animation data into a single search index
	SearchIndex->Values.Reset();
	SearchIndex->PoseMetadata.Reset();
	SearchIndex->PCAValues.Reset();

	for (int32 AssetIdx = 0; AssetIdx != SearchIndex->Assets.Num(); ++AssetIdx)
	{
		const FAssetIndexer::FOutput& Output = Indexers[AssetIdx].Output;

		FPoseSearchIndexAsset& SearchIndexAsset = SearchIndex->Assets[AssetIdx];
		SearchIndexAsset.NumPoses = Output.NumIndexedPoses;
		SearchIndexAsset.FirstPoseIdx = TotalPoses;

		SearchIndex->Values.Append(Output.FeatureVectorTable.GetData(), Output.FeatureVectorTable.Num());
		SearchIndex->PoseMetadata.Append(Output.PoseMetadata);

		TotalPoses += Output.NumIndexedPoses;
		TotalFloats += Output.FeatureVectorTable.Num();
	}
	
	SearchIndex->NumPoses = TotalPoses;
	SearchIndex->Schema = Database->Schema;
	SearchIndex->MinCostAddend = CalculateMinCostAddend();

	check(TotalFloats == TotalPoses * Database->Schema->SchemaCardinality);
}

bool BuildIndex(const UPoseSearchDatabase* Database, FPoseSearchIndex& OutSearchIndex)
{
	check(Database);

	bool bSuccess = true;

	if (!Database->IsValidForIndexing())
	{
		UE_LOG(LogPoseSearch, Warning, TEXT("Database '%f' is invalid for indexing"), *Database->GetName());
		return false;
	}

	UE_LOG(LogPoseSearch, Log, TEXT("Building PoseSearch index for database %s"), *GetNameSafe(Database));

	OutSearchIndex.Schema = Database->Schema;

	if (!Database->TryInitSearchIndexAssets(OutSearchIndex))
	{
		return false;
	}

	FDatabaseIndexingContext DbIndexingContext;
	DbIndexingContext.Database = Database;
	DbIndexingContext.SearchIndex = &OutSearchIndex;

	DbIndexingContext.PrepareSamplers();
	DbIndexingContext.PrepareIndexers();
	bSuccess &= DbIndexingContext.IndexAssets();
	DbIndexingContext.JoinIndex();

	PreprocessSearchIndex(OutSearchIndex, Database);
	
	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
// FModule

class FModule : public IModuleInterface, public UE::Anim::IPoseSearchProvider
{
public: // IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

public: // IPoseSearchProvider
	virtual UE::Anim::IPoseSearchProvider::FSearchResult Search(const FAnimationBaseContext& GraphContext, const UAnimSequenceBase* Sequence) override;

private:
#if WITH_EDITOR
	void OnObjectSaved(UObject* SavedObject, FObjectPreSaveContext SaveContext);
#endif // WITH_EDITOR
};

void FModule::StartupModule()
{
	IModularFeatures::Get().RegisterModularFeature(UE::Anim::IPoseSearchProvider::ModularFeatureName, this);

#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectPreSave.AddRaw(this, &FModule::OnObjectSaved);
#endif // WITH_EDITOR
}

void FModule::ShutdownModule()
{
	IModularFeatures::Get().UnregisterModularFeature(UE::Anim::IPoseSearchProvider::ModularFeatureName, this);
}

UE::Anim::IPoseSearchProvider::FSearchResult FModule::Search(const FAnimationBaseContext& GraphContext, const UAnimSequenceBase* Sequence)
{
	UE::Anim::IPoseSearchProvider::FSearchResult ProviderResult;

	using namespace UE::PoseSearch;

	const UPoseSearchSequenceMetaData* MetaData = Sequence ? Sequence->FindMetaDataByClass<UPoseSearchSequenceMetaData>() : nullptr;
	if (!MetaData || !MetaData->IsValidForSearch())
	{
		return ProviderResult;
	}

	IPoseHistoryProvider* PoseHistoryProvider = GraphContext.GetMessage<IPoseHistoryProvider>();
	if (!PoseHistoryProvider)
	{
		return ProviderResult;
	}

	FPoseHistory& PoseHistory = PoseHistoryProvider->GetPoseHistory();

	FSearchContext SearchContext;
	SearchContext.OwningComponent = GraphContext.AnimInstanceProxy->GetSkelMeshComponent();
	SearchContext.BoneContainer = &GraphContext.AnimInstanceProxy->GetRequiredBones();
	SearchContext.History = &PoseHistoryProvider->GetPoseHistory();

	UE::PoseSearch::FSearchResult Result = MetaData->Search(SearchContext);

	ProviderResult.Dissimilarity = Result.PoseCost.GetTotalCost();
	ProviderResult.PoseIdx = Result.PoseIdx;
	ProviderResult.TimeOffsetSeconds = Result.AssetTime;
	return ProviderResult;
}

#if WITH_EDITOR

void GetPoseSearchDatabaseAssetDataList(TArray<FAssetData>& OutPoseSearchDatabaseAssetDataList)
{
	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FARFilter Filter;
	Filter.bRecursiveClasses = true;
	Filter.ClassPaths.Add(UPoseSearchDatabase::StaticClass()->GetClassPathName());

	OutPoseSearchDatabaseAssetDataList.Reset();
	AssetRegistryModule.Get().GetAssets(Filter, OutPoseSearchDatabaseAssetDataList);
}

void ForEachPoseSearchDatabase(bool bLoadAssets, TFunctionRef<void(UPoseSearchDatabase&)> InFunction)
{
	TArray<FAssetData> PoseSearchDatabaseAssetDataList;
	GetPoseSearchDatabaseAssetDataList(PoseSearchDatabaseAssetDataList);
	for (const auto& PoseSearchDbAssetData : PoseSearchDatabaseAssetDataList)
	{
		if (UPoseSearchDatabase* PoseSearchDb = 
			Cast<UPoseSearchDatabase>(PoseSearchDbAssetData.FastGetAsset(bLoadAssets)))
		{
			InFunction(*PoseSearchDb);
		}
	}
}

void FModule::OnObjectSaved(UObject* SavedObject, FObjectPreSaveContext SaveContext)
{
	if (UAnimSequence* SavedSequence = Cast<UAnimSequence>(SavedObject))
	{
		ForEachPoseSearchDatabase(false, [SavedSequence](UPoseSearchDatabase& PoseSearchDb)
		{
			bool bSequenceFound =
				PoseSearchDb.Sequences.ContainsByPredicate([SavedSequence](FPoseSearchDatabaseSequence& DbSequence)
			{
				bool bIsMatch =
					SavedSequence == DbSequence.Sequence ||
					SavedSequence == DbSequence.LeadInSequence ||
					SavedSequence == DbSequence.FollowUpSequence;
				return bIsMatch;
			});

			if (bSequenceFound)
			{
				PoseSearchDb.BeginCacheDerivedData();
			}
		});
	}
	else if (UBlendSpace* SavedBlendSpace = Cast<UBlendSpace>(SavedObject))
	{
		ForEachPoseSearchDatabase(false, [SavedBlendSpace](UPoseSearchDatabase& PoseSearchDb)
		{
			bool bBlendSpaceFound = PoseSearchDb.BlendSpaces.ContainsByPredicate(
					[SavedBlendSpace](FPoseSearchDatabaseBlendSpace& DbBlendSpace)
			{
				return SavedBlendSpace == DbBlendSpace.BlendSpace;
			});

			if (bBlendSpaceFound)
			{
				PoseSearchDb.BeginCacheDerivedData();
			}
		});
	}
	else if (UPoseSearchSchema* SavedSchema = Cast<UPoseSearchSchema>(SavedObject))
	{
		ForEachPoseSearchDatabase(false, [SavedSchema](UPoseSearchDatabase& PoseSearchDb)
		{
			if (PoseSearchDb.Schema == SavedSchema)
			{
				PoseSearchDb.BeginCacheDerivedData();
			}
		});
	}
	else if (USkeleton* SavedSkeleton = Cast<USkeleton>(SavedObject))
	{
		ForEachPoseSearchDatabase(false, [SavedSkeleton](UPoseSearchDatabase& PoseSearchDb)
		{
			if (PoseSearchDb.Schema && PoseSearchDb.Schema->Skeleton == SavedSkeleton)
			{
				PoseSearchDb.BeginCacheDerivedData();
			}
		});
	}
}
#endif // WITH_EDITOR

} // namespace UE::PoseSearch

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(UE::PoseSearch::FModule, PoseSearch)