// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchDatabase.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "InstancedStruct.h"
#include "PoseSearch/PoseSearchContext.h"
#include "PoseSearch/PoseSearchDefines.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "PoseSearch/PoseSearchFeatureChannel.h"
#include "PoseSearch/PoseSearchHistory.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PoseSearchEigenHelper.h"
#include "UObject/ObjectSaveContext.h"


DECLARE_STATS_GROUP(TEXT("PoseSearch"), STATGROUP_PoseSearch, STATCAT_Advanced);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Search Brute Force"), STAT_PoseSearchBruteForce, STATGROUP_PoseSearch, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Search PCA/KNN"), STAT_PoseSearchPCAKNN, STATGROUP_PoseSearch, );
DEFINE_STAT(STAT_PoseSearchBruteForce);
DEFINE_STAT(STAT_PoseSearchPCAKNN);

namespace UE::PoseSearch
{

typedef TArray<size_t, TInlineAllocator<128>> FNonSelectableIdx;
static void PopulateNonSelectableIdx(FNonSelectableIdx& NonSelectableIdx, FSearchContext& SearchContext, const UPoseSearchDatabase* Database, TConstArrayView<float> QueryValues)
{
	check(Database);
#if UE_POSE_SEARCH_TRACE_ENABLED
	const FPoseSearchIndex& SearchIndex = Database->GetSearchIndex();
#endif

	const FPoseSearchIndexAsset* CurrentIndexAsset = SearchContext.CurrentResult.GetSearchIndexAsset();
	if (CurrentIndexAsset && SearchContext.IsCurrentResultFromDatabase(Database) && SearchContext.PoseJumpThresholdTime > 0.f)
	{
		const int32 PoseJumpIndexThreshold = FMath::FloorToInt(SearchContext.PoseJumpThresholdTime / Database->Schema->GetSamplingInterval());
		const bool IsLooping = Database->IsSourceAssetLooping(*CurrentIndexAsset);

		for (int32 i = -PoseJumpIndexThreshold; i <= -1; ++i)
		{
			int32 PoseIdx = SearchContext.CurrentResult.PoseIdx + i;
			bool bIsPoseInRange = false;
			if (IsLooping)
			{
				bIsPoseInRange = true;

				while (PoseIdx < CurrentIndexAsset->FirstPoseIdx)
				{
					PoseIdx += CurrentIndexAsset->NumPoses;
				}
			}
			else if (CurrentIndexAsset->IsPoseInRange(PoseIdx))
			{
				bIsPoseInRange = true;
			}

			if (bIsPoseInRange)
			{
				NonSelectableIdx.Add(PoseIdx);

#if UE_POSE_SEARCH_TRACE_ENABLED
				const FPoseSearchCost PoseCost = SearchIndex.ComparePoses(PoseIdx, SearchContext.QueryMirrorRequest, EPoseComparisonFlags::None, Database->Schema->MirrorMismatchCostBias, QueryValues);
				SearchContext.BestCandidates.Add(PoseCost, PoseIdx, Database, EPoseCandidateFlags::DiscardedBy_PoseJumpThresholdTime);
#endif
			}
			else
			{
				break;
			}
		}

		for (int32 i = 0; i <= PoseJumpIndexThreshold; ++i)
		{
			int32 PoseIdx = SearchContext.CurrentResult.PoseIdx + i;
			bool bIsPoseInRange = false;
			if (IsLooping)
			{
				bIsPoseInRange = true;

				while (PoseIdx >= CurrentIndexAsset->FirstPoseIdx + CurrentIndexAsset->NumPoses)
				{
					PoseIdx -= CurrentIndexAsset->NumPoses;
				}
			}
			else if (CurrentIndexAsset->IsPoseInRange(PoseIdx))
			{
				bIsPoseInRange = true;
			}

			if (bIsPoseInRange)
			{
				NonSelectableIdx.Add(PoseIdx);

#if UE_POSE_SEARCH_TRACE_ENABLED
				const FPoseSearchCost PoseCost = SearchIndex.ComparePoses(PoseIdx, SearchContext.QueryMirrorRequest, EPoseComparisonFlags::None, Database->Schema->MirrorMismatchCostBias, QueryValues);
				SearchContext.BestCandidates.Add(PoseCost, PoseIdx, Database, EPoseCandidateFlags::DiscardedBy_PoseJumpThresholdTime);
#endif
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
				NonSelectableIdx.Add(HistoricalPoseIndex.PoseIndex);

#if UE_POSE_SEARCH_TRACE_ENABLED
				check(HistoricalPoseIndex.PoseIndex >= 0);

				// if we're editing the database and removing assets it's possible that the PoseIndicesHistory contains invalid pose indexes
				if (HistoricalPoseIndex.PoseIndex < SearchIndex.NumPoses)
				{
					const FPoseSearchCost PoseCost = SearchIndex.ComparePoses(HistoricalPoseIndex.PoseIndex, SearchContext.QueryMirrorRequest, EPoseComparisonFlags::None, Database->Schema->MirrorMismatchCostBias, QueryValues);
					SearchContext.BestCandidates.Add(PoseCost, HistoricalPoseIndex.PoseIndex, Database, EPoseCandidateFlags::DiscardedBy_PoseReselectHistory);
				}
#endif
			}
		}
	}

	NonSelectableIdx.Sort();
}

struct FPoseFilters
{
	FPoseFilters(const UPoseSearchSchema* Schema, TConstArrayView<size_t> NonSelectableIdx, EPoseSearchPoseFlags OverallFlags)
	{
		NonSelectableIdxPoseFilter.NonSelectableIdx = NonSelectableIdx;

		if (EnumHasAnyFlags(OverallFlags, EPoseSearchPoseFlags::BlockTransition))
		{
			AllPoseFilters.Add(&BlockTransitionPoseFilter);
		}

		if (NonSelectableIdxPoseFilter.IsPoseFilterActive())
		{
			AllPoseFilters.Add(&NonSelectableIdxPoseFilter);
		}

		for (const IPoseFilter* ChannelPoseFilter : Schema->Channels)
		{
			if (ChannelPoseFilter && ChannelPoseFilter->IsPoseFilterActive())
			{
				AllPoseFilters.Add(ChannelPoseFilter);
			}
		}
	}

	bool AreFiltersValid(const FPoseSearchIndex& SearchIndex, TConstArrayView<float> QueryValues, int32 PoseIdx, const FPoseSearchPoseMetadata& Metadata
#if UE_POSE_SEARCH_TRACE_ENABLED
		, UE::PoseSearch::FSearchContext& SearchContext, const UPoseSearchDatabase* Database
#endif
	) const
	{
		TConstArrayView<float> PoseValues = SearchIndex.GetPoseValues(PoseIdx);
		for (const IPoseFilter* PoseFilter : AllPoseFilters)
		{
			if (!PoseFilter->IsPoseValid(PoseValues, QueryValues, PoseIdx, Metadata))
			{
#if UE_POSE_SEARCH_TRACE_ENABLED
				if (PoseFilter == &NonSelectableIdxPoseFilter)
				{
					// candidate already added to SearchContext.BestCandidates by PopulateNonSelectableIdx
				}
				else if (PoseFilter == &BlockTransitionPoseFilter)
				{
					const FPoseSearchCost PoseCost = SearchIndex.ComparePoses(PoseIdx, SearchContext.QueryMirrorRequest, EPoseComparisonFlags::None, Database->Schema->MirrorMismatchCostBias, QueryValues);
					SearchContext.BestCandidates.Add(PoseCost, PoseIdx, Database, EPoseCandidateFlags::DiscardedBy_BlockTransition);
				}
				else
				{
					const FPoseSearchCost PoseCost = SearchIndex.ComparePoses(PoseIdx, SearchContext.QueryMirrorRequest, EPoseComparisonFlags::None, Database->Schema->MirrorMismatchCostBias, QueryValues);
					SearchContext.BestCandidates.Add(PoseCost, PoseIdx, Database, EPoseCandidateFlags::DiscardedBy_PoseFilter);
				}
#endif
				return false;
			}
		}
		return true;
	};

private:
	struct FNonSelectableIdxPoseFilter : public IPoseFilter
	{
		virtual bool IsPoseFilterActive() const override
		{
			return !NonSelectableIdx.IsEmpty();
		}

		virtual bool IsPoseValid(TConstArrayView<float> PoseValues, TConstArrayView<float> QueryValues, int32 PoseIdx, const FPoseSearchPoseMetadata& Metadata) const override
		{
			return Algo::BinarySearch(NonSelectableIdx, PoseIdx) == INDEX_NONE;
		}

		TConstArrayView<size_t> NonSelectableIdx;
	};

	struct FBlockTransitionPoseFilter : public IPoseFilter
	{
		virtual bool IsPoseFilterActive() const override
		{
			return true;
		}

		virtual bool IsPoseValid(TConstArrayView<float> PoseValues, TConstArrayView<float> QueryValues, int32 PoseIdx, const FPoseSearchPoseMetadata& Metadata) const override
		{
			return !EnumHasAnyFlags(Metadata.Flags, EPoseSearchPoseFlags::BlockTransition);
		}
	};

	FNonSelectableIdxPoseFilter NonSelectableIdxPoseFilter;
	FBlockTransitionPoseFilter BlockTransitionPoseFilter;

	TArray<const IPoseFilter*, TInlineAllocator<64>> AllPoseFilters;
};

} // namespace UE::PoseSearch

//////////////////////////////////////////////////////////////////////////
// FPoseSearchDatabaseSequence
UAnimationAsset* FPoseSearchDatabaseSequence::GetAnimationAsset() const
{
	return Sequence;
}

UClass* FPoseSearchDatabaseSequence::GetAnimationAssetStaticClass() const
{
	return UAnimSequence::StaticClass();
}

bool FPoseSearchDatabaseSequence::IsLooping() const
{
	return Sequence ? Sequence->bLoop : false;
}

const FString FPoseSearchDatabaseSequence::GetName() const
{
	return Sequence ? Sequence->GetName() : FString();
}

bool FPoseSearchDatabaseSequence::IsRootMotionEnabled() const
{
	return Sequence ? Sequence->HasRootMotion() : false;
}

//////////////////////////////////////////////////////////////////////////
// FPoseSearchDatabaseBlendSpace
UAnimationAsset* FPoseSearchDatabaseBlendSpace::GetAnimationAsset() const
{
	return BlendSpace.Get();
}

UClass* FPoseSearchDatabaseBlendSpace::GetAnimationAssetStaticClass() const
{
	return UBlendSpace::StaticClass();
}

bool FPoseSearchDatabaseBlendSpace::IsLooping() const
{
	return BlendSpace ? BlendSpace->bLoop : false;
}

const FString FPoseSearchDatabaseBlendSpace::GetName() const
{
	return BlendSpace ? BlendSpace->GetName() : FString();
}

bool FPoseSearchDatabaseBlendSpace::IsRootMotionEnabled() const
{
	bool bIsRootMotionUsedInBlendSpace = false;

	if (BlendSpace)
	{
		BlendSpace->ForEachImmutableSample([&bIsRootMotionUsedInBlendSpace](const FBlendSample& Sample)
			{
				const TObjectPtr<UAnimSequence> Sequence = Sample.Animation;

				if (IsValid(Sequence) && Sequence->HasRootMotion())
				{
					bIsRootMotionUsedInBlendSpace = true;
				}
			});
	}

	return bIsRootMotionUsedInBlendSpace;
}

void FPoseSearchDatabaseBlendSpace::GetBlendSpaceParameterSampleRanges(int32& HorizontalBlendNum, int32& VerticalBlendNum) const
{
	check(BlendSpace);

	HorizontalBlendNum = bUseGridForSampling ? BlendSpace->GetBlendParameter(0).GridNum + 1 : FMath::Max(NumberOfHorizontalSamples, 1);
	VerticalBlendNum = BlendSpace->IsA<UBlendSpace1D>() ? 1 : bUseGridForSampling ? BlendSpace->GetBlendParameter(1).GridNum + 1 : FMath::Max(NumberOfVerticalSamples, 1);

	check(HorizontalBlendNum >= 1 && VerticalBlendNum >= 1);
}

FVector FPoseSearchDatabaseBlendSpace::BlendParameterForSampleRanges(int32 HorizontalBlendIndex, int32 VerticalBlendIndex) const
{
	check(BlendSpace);

	const bool bWrapInputOnHorizontalAxis = BlendSpace->GetBlendParameter(0).bWrapInput;
	const bool bWrapInputOnVerticalAxis = BlendSpace->GetBlendParameter(1).bWrapInput;

	int32 HorizontalBlendNum, VerticalBlendNum;
	GetBlendSpaceParameterSampleRanges(HorizontalBlendNum, VerticalBlendNum);

	if (bWrapInputOnHorizontalAxis)
	{
		++HorizontalBlendNum;
	}

	if (bWrapInputOnVerticalAxis)
	{
		++VerticalBlendNum;
	}

	const float HorizontalBlendMin = BlendSpace->GetBlendParameter(0).Min;
	const float HorizontalBlendMax = BlendSpace->GetBlendParameter(0).Max;

	const float VerticalBlendMin = BlendSpace->GetBlendParameter(1).Min;
	const float VerticalBlendMax = BlendSpace->GetBlendParameter(1).Max;

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

//////////////////////////////////////////////////////////////////////////
// FPoseSearchDatabaseAnimComposite
UAnimationAsset* FPoseSearchDatabaseAnimComposite::GetAnimationAsset() const
{
	return AnimComposite;
}

UClass* FPoseSearchDatabaseAnimComposite::GetAnimationAssetStaticClass() const
{
	return UAnimComposite::StaticClass();
}

bool FPoseSearchDatabaseAnimComposite::IsLooping() const
{
	return AnimComposite ? AnimComposite->bLoop : false;
}

const FString FPoseSearchDatabaseAnimComposite::GetName() const
{
	return AnimComposite ? AnimComposite->GetName() : FString();
}

bool FPoseSearchDatabaseAnimComposite::IsRootMotionEnabled() const
{
	return AnimComposite ? AnimComposite->HasRootMotion() : false;
}

//////////////////////////////////////////////////////////////////////////
// UPoseSearchDatabase
UPoseSearchDatabase::~UPoseSearchDatabase()
{
}

void UPoseSearchDatabase::SetSearchIndex(const FPoseSearchIndex& SearchIndex)
{
	check(IsInGameThread());
	SearchIndexPrivate = SearchIndex;
}

const FPoseSearchIndex& UPoseSearchDatabase::GetSearchIndex() const
{
	// making sure the search index is consistent. if it fails the calling code hasn't been protected by FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex
	check(Schema && Schema->IsValid() && !SearchIndexPrivate.IsEmpty() && SearchIndexPrivate.WeightsSqrt.Num() == Schema->SchemaCardinality && SearchIndexPrivate.KDTree.Impl);
	return SearchIndexPrivate;
}

int32 UPoseSearchDatabase::GetPoseIndexFromTime(float Time, const FPoseSearchIndexAsset& SearchIndexAsset) const
{
	const bool bIsLooping = IsSourceAssetLooping(SearchIndexAsset);
	const FFloatInterval& Range = SearchIndexAsset.SamplingInterval;
	const bool bHasPoseIndex = SearchIndexAsset.FirstPoseIdx != INDEX_NONE && SearchIndexAsset.NumPoses > 0 && (bIsLooping || Range.Contains(Time));
	if (bHasPoseIndex)
	{
		int32 PoseOffset = FMath::RoundToInt(Schema->SampleRate * (Time - Range.Min));
		
		if (PoseOffset < 0)
		{
			if (bIsLooping)
			{
				PoseOffset = (PoseOffset % SearchIndexAsset.NumPoses) + SearchIndexAsset.NumPoses;
			}
			else
			{
				PoseOffset = 0;
			}
		}
		else if (PoseOffset >= SearchIndexAsset.NumPoses)
		{
			if (bIsLooping)
			{
				PoseOffset = PoseOffset % SearchIndexAsset.NumPoses;
			}
			else
			{
				PoseOffset = SearchIndexAsset.NumPoses - 1;
			}
		}

		int32 PoseIdx = SearchIndexAsset.FirstPoseIdx + PoseOffset;
		return PoseIdx;
	}

	return INDEX_NONE;
}

bool UPoseSearchDatabase::GetPoseIndicesAndLerpValueFromTime(float Time, const FPoseSearchIndexAsset& SearchIndexAsset, int32& PrevPoseIdx, int32& PoseIdx, int32& NextPoseIdx, float& LerpValue) const
{
	PoseIdx = GetPoseIndexFromTime(Time, SearchIndexAsset);
	if (PoseIdx == INDEX_NONE)
	{
		PrevPoseIdx = INDEX_NONE;
		NextPoseIdx = INDEX_NONE;
		LerpValue = 0.f;
		return false;
	}

	const FFloatInterval& Range = SearchIndexAsset.SamplingInterval;
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

const FInstancedStruct& UPoseSearchDatabase::GetAnimationAssetStruct(int32 AnimationAssetIndex) const
{
	check(AnimationAssets.IsValidIndex(AnimationAssetIndex));
	return AnimationAssets[AnimationAssetIndex];
}

const FInstancedStruct& UPoseSearchDatabase::GetAnimationAssetStruct(const FPoseSearchIndexAsset& SearchIndexAsset) const
{
	return GetAnimationAssetStruct(SearchIndexAsset.SourceAssetIdx);
}

FInstancedStruct& UPoseSearchDatabase::GetMutableAnimationAssetStruct(int32 AnimationAssetIndex)
{
	check(AnimationAssets.IsValidIndex(AnimationAssetIndex));
	return AnimationAssets[AnimationAssetIndex];
}

FInstancedStruct& UPoseSearchDatabase::GetMutableAnimationAssetStruct(const FPoseSearchIndexAsset& SearchIndexAsset)
{
	return GetMutableAnimationAssetStruct(SearchIndexAsset.SourceAssetIdx);
}

const FPoseSearchDatabaseAnimationAssetBase* UPoseSearchDatabase::GetAnimationAssetBase(int32 AnimationAssetIndex) const
{
	if (AnimationAssets.IsValidIndex(AnimationAssetIndex))
	{
		return AnimationAssets[AnimationAssetIndex].GetPtr<FPoseSearchDatabaseAnimationAssetBase>();
	}

	return nullptr;
}

const FPoseSearchDatabaseAnimationAssetBase* UPoseSearchDatabase::GetAnimationAssetBase(const FPoseSearchIndexAsset& SearchIndexAsset) const
{
	return GetAnimationAssetBase(SearchIndexAsset.SourceAssetIdx);
}

FPoseSearchDatabaseAnimationAssetBase* UPoseSearchDatabase::GetMutableAnimationAssetBase(int32 AnimationAssetIndex)
{
	if (AnimationAssets.IsValidIndex(AnimationAssetIndex))
	{
		return AnimationAssets[AnimationAssetIndex].GetMutablePtr<FPoseSearchDatabaseAnimationAssetBase>();
	}

	return nullptr;
}

FPoseSearchDatabaseAnimationAssetBase* UPoseSearchDatabase::GetMutableAnimationAssetBase(const FPoseSearchIndexAsset& SearchIndexAsset)
{
	return GetMutableAnimationAssetBase(SearchIndexAsset.SourceAssetIdx);
}

const bool UPoseSearchDatabase::IsSourceAssetLooping(const FPoseSearchIndexAsset& SearchIndexAsset) const
{
	return GetAnimationAssetBase(SearchIndexAsset.SourceAssetIdx)->IsLooping();
}

const FString UPoseSearchDatabase::GetSourceAssetName(const FPoseSearchIndexAsset& SearchIndexAsset) const
{
	return GetAnimationAssetBase(SearchIndexAsset.SourceAssetIdx)->GetName();
}

int32 UPoseSearchDatabase::GetNumberOfPrincipalComponents() const
{
	return FMath::Min<int32>(NumberOfPrincipalComponents, Schema->SchemaCardinality);
}

bool UPoseSearchDatabase::GetSkipSearchIfPossible() const
{
	if (PoseSearchMode == EPoseSearchMode::PCAKDTree_Validate || PoseSearchMode == EPoseSearchMode::PCAKDTree_Compare)
	{
		return false;
	}

	return bSkipSearchIfPossible;
}



void UPoseSearchDatabase::PostLoad()
{
#if WITH_EDITORONLY_DATA
	for (const FPoseSearchDatabaseSequence& DatabaseSequence : Sequences_DEPRECATED)
	{
		AnimationAssets.Add(FInstancedStruct::Make(DatabaseSequence));
	}
	Sequences_DEPRECATED.Empty();

	for (const FPoseSearchDatabaseBlendSpace& DatabaseBlendSpace : BlendSpaces_DEPRECATED)
	{
		AnimationAssets.Add(FInstancedStruct::Make(DatabaseBlendSpace));
	}
	BlendSpaces_DEPRECATED.Empty();
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	using namespace UE::PoseSearch;
	FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(this, ERequestAsyncBuildFlag::NewRequest);
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

void UPoseSearchDatabase::NotifyDerivedDataRebuild() const
{
	OnDerivedDataRebuild.Broadcast();
}

void UPoseSearchDatabase::BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform)
{
	using namespace UE::PoseSearch;
	Super::BeginCacheForCookedPlatformData(TargetPlatform);
	FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(this, ERequestAsyncBuildFlag::NewRequest);
}

bool UPoseSearchDatabase::IsCachedCookedPlatformDataLoaded(const ITargetPlatform* TargetPlatform)
{
	using namespace UE::PoseSearch;
	check(IsInGameThread());
	return FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(this, ERequestAsyncBuildFlag::ContinueRequest);
}
#endif // WITH_EDITOR

void UPoseSearchDatabase::PostSaveRoot(FObjectPostSaveRootContext ObjectSaveContext)
{
#if WITH_EDITOR
	using namespace UE::PoseSearch;
	if (!IsTemplate() && !ObjectSaveContext.IsProceduralSave())
	{
		FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(this, ERequestAsyncBuildFlag::NewRequest | ERequestAsyncBuildFlag::WaitForCompletion);
	}
#endif

	Super::PostSaveRoot(ObjectSaveContext);
}

void UPoseSearchDatabase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsFilterEditorOnly())
	{
		if (Ar.IsLoading() || Ar.IsCooking())
		{
			Ar << SearchIndexPrivate;
		}
	}
}

UE::PoseSearch::FSearchResult UPoseSearchDatabase::Search(UE::PoseSearch::FSearchContext& SearchContext) const
{
	using namespace UE::PoseSearch;

	FSearchResult Result;

#if WITH_EDITOR
	if (!FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(this, ERequestAsyncBuildFlag::ContinueRequest))
	{
		return Result;
	}
#endif

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
	const FPoseSearchIndex& SearchIndex = GetSearchIndex();

	const uint32 ClampedNumberOfPrincipalComponents = GetNumberOfPrincipalComponents();
	const uint32 ClampedKDTreeQueryNumNeighbors = FMath::Clamp<uint32>(KDTreeQueryNumNeighbors, 1, SearchIndex.NumPoses);

	//stack allocated temporaries
	TArrayView<size_t> ResultIndexes((size_t*)FMemory_Alloca((ClampedKDTreeQueryNumNeighbors + 1) * sizeof(size_t)), ClampedKDTreeQueryNumNeighbors + 1);
	TArrayView<float> ResultDistanceSqr((float*)FMemory_Alloca((ClampedKDTreeQueryNumNeighbors + 1) * sizeof(float)), ClampedKDTreeQueryNumNeighbors + 1);
	RowMajorVectorMap WeightedQueryValues((float*)FMemory_Alloca(NumDimensions * sizeof(float)), 1, NumDimensions);
	RowMajorVectorMap CenteredQueryValues((float*)FMemory_Alloca(NumDimensions * sizeof(float)), 1, NumDimensions);
	RowMajorVectorMap ProjectedQueryValues((float*)FMemory_Alloca(ClampedNumberOfPrincipalComponents * sizeof(float)), 1, ClampedNumberOfPrincipalComponents);
	
	// KDTree in PCA space search
	if (PoseSearchMode == EPoseSearchMode::PCAKDTree_Validate)
	{
		const RowMajorVectorMapConst MapWeightsSqrt(SearchIndex.WeightsSqrt.GetData(), 1, NumDimensions);

		// testing the KDTree is returning the proper searches for all the original points transformed in pca space
		for (int32 PoseIdx = 0; PoseIdx < SearchIndex.NumPoses; ++PoseIdx)
		{
			FKDTree::KNNResultSet ResultSet(ClampedKDTreeQueryNumNeighbors, ResultIndexes, ResultDistanceSqr);
			TConstArrayView<float> PoseValues = SearchIndex.GetPoseValues(PoseIdx);

			const RowMajorVectorMapConst Mean(SearchIndex.Mean.GetData(), 1, NumDimensions);
			const ColMajorMatrixMapConst PCAProjectionMatrix(SearchIndex.PCAProjectionMatrix.GetData(), NumDimensions, ClampedNumberOfPrincipalComponents);

			const RowMajorVectorMapConst QueryValues(PoseValues.GetData(), 1, NumDimensions);
			WeightedQueryValues = QueryValues.array() * MapWeightsSqrt.array();
			CenteredQueryValues.noalias() = WeightedQueryValues - Mean;
			ProjectedQueryValues.noalias() = CenteredQueryValues * PCAProjectionMatrix;

			SearchIndex.KDTree.FindNeighbors(ResultSet, ProjectedQueryValues.data());

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

	TConstArrayView<float> QueryValues = Result.ComposedQuery.GetValues();

	const bool IsCurrentResultFromThisDatabase = SearchContext.IsCurrentResultFromDatabase(this);

	// evaluating the continuing pose only if it hasn't already being evaluated and the related animation can advance
	if (!SearchContext.bForceInterrupt && IsCurrentResultFromThisDatabase && SearchContext.bCanAdvance && !Result.ContinuingPoseCost.IsValid())
	{
		Result.PoseIdx = SearchContext.CurrentResult.PoseIdx;
		Result.PoseCost = SearchIndex.ComparePoses(Result.PoseIdx, SearchContext.QueryMirrorRequest, EPoseComparisonFlags::ContinuingPose, Schema->MirrorMismatchCostBias, QueryValues);
		Result.ContinuingPoseCost = Result.PoseCost;

		if (GetSkipSearchIfPossible())
		{
			SearchContext.UpdateCurrentBestCost(Result.PoseCost);
		}
	}

	// since any PoseCost calculated here is at least SearchIndex.MinCostAddend,
	// there's no point in performing the search if CurrentBestTotalCost is already better than that
	if (SearchContext.GetCurrentBestTotalCost() > SearchIndex.MinCostAddend)
	{
		FNonSelectableIdx NonSelectableIdx;
		PopulateNonSelectableIdx(NonSelectableIdx, SearchContext, this, QueryValues);
		FKDTree::KNNResultSet ResultSet(ClampedKDTreeQueryNumNeighbors, ResultIndexes, ResultDistanceSqr, NonSelectableIdx);

		check(QueryValues.Num() == NumDimensions);

		const RowMajorVectorMapConst Mean(SearchIndex.Mean.GetData(), 1, NumDimensions);
		const ColMajorMatrixMapConst PCAProjectionMatrix(SearchIndex.PCAProjectionMatrix.GetData(), NumDimensions, ClampedNumberOfPrincipalComponents);

		// transforming query values into PCA space to query the KDTree
		const RowMajorVectorMapConst QueryValuesMap(QueryValues.GetData(), 1, NumDimensions);
		const RowMajorVectorMapConst MapWeightsSqrt(SearchIndex.WeightsSqrt.GetData(), 1, NumDimensions);
		WeightedQueryValues = QueryValuesMap.array() * MapWeightsSqrt.array();
		CenteredQueryValues.noalias() = WeightedQueryValues - Mean;
		ProjectedQueryValues.noalias() = CenteredQueryValues * PCAProjectionMatrix;

		SearchIndex.KDTree.FindNeighbors(ResultSet, ProjectedQueryValues.data());

		// NonSelectableIdx are already filtered out inside the kdtree search
		const FPoseFilters PoseFilters(Schema, TConstArrayView<size_t>(), SearchIndex.OverallFlags);
		for (size_t ResultIndex = 0; ResultIndex < ResultSet.Num(); ++ResultIndex)
		{
			const int32 PoseIdx = ResultIndexes[ResultIndex];
			if (PoseFilters.AreFiltersValid(SearchIndex, QueryValues, PoseIdx, SearchIndex.PoseMetadata[PoseIdx]
#if UE_POSE_SEARCH_TRACE_ENABLED
				, SearchContext, this
#endif
			))
			{
				const FPoseSearchCost PoseCost = SearchIndex.ComparePoses(PoseIdx, SearchContext.QueryMirrorRequest, EPoseComparisonFlags::None, Schema->MirrorMismatchCostBias, QueryValues);
				if (PoseCost < Result.PoseCost)
				{
					Result.PoseCost = PoseCost;
					Result.PoseIdx = PoseIdx;
				}

#if UE_POSE_SEARCH_TRACE_ENABLED
				SearchContext.BestCandidates.Add(PoseCost, PoseIdx, this, EPoseCandidateFlags::Valid_Pose);
#endif
			}
		}

		if (GetSkipSearchIfPossible() && Result.PoseCost.IsValid())
		{
			SearchContext.UpdateCurrentBestCost(Result.PoseCost);
		}
	}
	else
	{
#if UE_POSE_SEARCH_TRACE_ENABLED
		// calling just for reporting non selectable poses
		FNonSelectableIdx NonSelectableIdx;
		PopulateNonSelectableIdx(NonSelectableIdx, SearchContext, this, QueryValues);
#endif
	}

	// finalizing Result properties
	if (Result.PoseIdx != INDEX_NONE)
	{
		Result.AssetTime = SearchIndex.GetAssetTime(Result.PoseIdx, Schema->GetSamplingInterval());
		Result.Database = this;
	}

	return Result;
}

UE::PoseSearch::FSearchResult UPoseSearchDatabase::SearchBruteForce(UE::PoseSearch::FSearchContext& SearchContext) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_PoseSearch_Brute_Force);
	SCOPE_CYCLE_COUNTER(STAT_PoseSearchBruteForce);
	
	using namespace UE::PoseSearch;
	
	FSearchResult Result;

	const FPoseSearchIndex& SearchIndex = GetSearchIndex();

	SearchContext.GetOrBuildQuery(this, Result.ComposedQuery);
	TConstArrayView<float> QueryValues = Result.ComposedQuery.GetValues();

	const bool IsCurrentResultFromThisDatabase = SearchContext.IsCurrentResultFromDatabase(this);
	if (!SearchContext.bForceInterrupt && IsCurrentResultFromThisDatabase)
	{
		// evaluating the continuing pose only if it hasn't already being evaluated and the related animation can advance
		if (SearchContext.bCanAdvance && !Result.ContinuingPoseCost.IsValid())
		{
			Result.PoseIdx = SearchContext.CurrentResult.PoseIdx;
			Result.PoseCost = SearchIndex.ComparePoses(Result.PoseIdx, SearchContext.QueryMirrorRequest, EPoseComparisonFlags::ContinuingPose, Schema->MirrorMismatchCostBias, QueryValues);
			Result.ContinuingPoseCost = Result.PoseCost;

			if (GetSkipSearchIfPossible())
			{
				SearchContext.UpdateCurrentBestCost(Result.PoseCost);
			}
		}
	}

	// since any PoseCost calculated here is at least SearchIndex.MinCostAddend,
	// there's no point in performing the search if CurrentBestTotalCost is already better than that
	if (SearchContext.GetCurrentBestTotalCost() > SearchIndex.MinCostAddend)
	{
		FNonSelectableIdx NonSelectableIdx;
		PopulateNonSelectableIdx(NonSelectableIdx, SearchContext, this, QueryValues);
		check(Algo::IsSorted(NonSelectableIdx));

		const FPoseFilters PoseFilters(Schema, NonSelectableIdx, SearchIndex.OverallFlags);
		for (int32 PoseIdx = 0; PoseIdx < SearchIndex.NumPoses; ++PoseIdx)
		{
			if (PoseFilters.AreFiltersValid(SearchIndex, QueryValues, PoseIdx, SearchIndex.PoseMetadata[PoseIdx]
#if UE_POSE_SEARCH_TRACE_ENABLED
				, SearchContext, this
#endif
			))
			{
				const FPoseSearchCost PoseCost = SearchIndex.ComparePoses(PoseIdx, SearchContext.QueryMirrorRequest, EPoseComparisonFlags::None, Schema->MirrorMismatchCostBias, QueryValues);
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
	else
	{
#if UE_POSE_SEARCH_TRACE_ENABLED
		// calling just for reporting non selectable poses
		FNonSelectableIdx NonSelectableIdx;
		PopulateNonSelectableIdx(NonSelectableIdx, SearchContext, this, QueryValues);
#endif
	}

	// finalizing Result properties
	if (Result.PoseIdx != INDEX_NONE)
	{
		Result.AssetTime = SearchIndex.GetAssetTime(Result.PoseIdx, Schema->GetSamplingInterval());
		Result.Database = this;
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