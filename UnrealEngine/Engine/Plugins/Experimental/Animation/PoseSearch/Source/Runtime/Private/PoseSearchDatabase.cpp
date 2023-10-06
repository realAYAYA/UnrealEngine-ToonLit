// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchDatabase.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "InstancedStruct.h"
#include "PoseSearch/PoseSearchAnimNotifies.h"
#include "PoseSearch/PoseSearchContext.h"
#include "PoseSearch/PoseSearchDefines.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "PoseSearch/PoseSearchFeatureChannel.h"
#include "PoseSearch/PoseSearchHistory.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "UObject/ObjectSaveContext.h"

DECLARE_STATS_GROUP(TEXT("PoseSearch"), STATGROUP_PoseSearch, STATCAT_Advanced);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Search Brute Force"), STAT_PoseSearch_BruteForce, STATGROUP_PoseSearch, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Search PCA/KNN"), STAT_PoseSearch_PCAKNN, STATGROUP_PoseSearch, );
DEFINE_STAT(STAT_PoseSearch_BruteForce);
DEFINE_STAT(STAT_PoseSearch_PCAKNN);

namespace UE::PoseSearch
{

typedef TArray<size_t, TInlineAllocator<256>> FNonSelectableIdx;
static void PopulateNonSelectableIdx(FNonSelectableIdx& NonSelectableIdx, FSearchContext& SearchContext, const UPoseSearchDatabase* Database
#if UE_POSE_SEARCH_TRACE_ENABLED
	, TConstArrayView<float> QueryValues
#endif //UE_POSE_SEARCH_TRACE_ENABLED
)
{
	check(Database);
#if UE_POSE_SEARCH_TRACE_ENABLED
	const FSearchIndex& SearchIndex = Database->GetSearchIndex();
#endif

	NonSelectableIdx.Reset();
	const FSearchIndexAsset* CurrentIndexAsset = SearchContext.GetCurrentResult().GetSearchIndexAsset();
	if (CurrentIndexAsset && SearchContext.IsCurrentResultFromDatabase(Database) && SearchContext.GetPoseJumpThresholdTime() > 0.f)
	{
		const int32 PoseJumpIndexThreshold = FMath::FloorToInt(SearchContext.GetPoseJumpThresholdTime() * Database->Schema->SampleRate);
		const bool IsLooping = Database->IsSourceAssetLooping(*CurrentIndexAsset);

		for (int32 i = -PoseJumpIndexThreshold; i <= -1; ++i)
		{
			int32 PoseIdx = SearchContext.GetCurrentResult().PoseIdx + i;
			bool bIsPoseInRange = false;
			if (IsLooping)
			{
				bIsPoseInRange = true;

				while (PoseIdx < CurrentIndexAsset->FirstPoseIdx)
				{
					PoseIdx += CurrentIndexAsset->GetNumPoses();
				}
			}
			else if (CurrentIndexAsset->IsPoseInRange(PoseIdx))
			{
				bIsPoseInRange = true;
			}

			if (bIsPoseInRange)
			{
				NonSelectableIdx.AddUnique(PoseIdx);

#if UE_POSE_SEARCH_TRACE_ENABLED
				const TArray<float> PoseValues = SearchIndex.GetPoseValuesSafe(PoseIdx);
				const FPoseSearchCost PoseCost = SearchIndex.ComparePoses(PoseIdx, 0.f, PoseValues, QueryValues);
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
			int32 PoseIdx = SearchContext.GetCurrentResult().PoseIdx + i;
			bool bIsPoseInRange = false;
			if (IsLooping)
			{
				bIsPoseInRange = true;

				while (PoseIdx >= CurrentIndexAsset->FirstPoseIdx + CurrentIndexAsset->GetNumPoses())
				{
					PoseIdx -= CurrentIndexAsset->GetNumPoses();
				}
			}
			else if (CurrentIndexAsset->IsPoseInRange(PoseIdx))
			{
				bIsPoseInRange = true;
			}

			if (bIsPoseInRange)
			{
				NonSelectableIdx.AddUnique(PoseIdx);

#if UE_POSE_SEARCH_TRACE_ENABLED
				const FPoseSearchCost PoseCost = SearchIndex.ComparePoses(PoseIdx, 0.f, SearchIndex.GetPoseValuesSafe(PoseIdx), QueryValues);
				SearchContext.BestCandidates.Add(PoseCost, PoseIdx, Database, EPoseCandidateFlags::DiscardedBy_PoseJumpThresholdTime);
#endif
			}
			else
			{
				break;
			}
		}
	}

	if (SearchContext.GetPoseIndicesHistory())
	{
		const FObjectKey DatabaseKey(Database);
		for (auto It = SearchContext.GetPoseIndicesHistory()->IndexToTime.CreateConstIterator(); It; ++It)
		{
			const FHistoricalPoseIndex& HistoricalPoseIndex = It.Key();
			if (HistoricalPoseIndex.DatabaseKey == DatabaseKey)
			{
				NonSelectableIdx.AddUnique(HistoricalPoseIndex.PoseIndex);

#if UE_POSE_SEARCH_TRACE_ENABLED
				check(HistoricalPoseIndex.PoseIndex >= 0);

				// if we're editing the database and removing assets it's possible that the PoseIndicesHistory contains invalid pose indexes
				if (HistoricalPoseIndex.PoseIndex < SearchIndex.GetNumPoses())
				{
					const FPoseSearchCost PoseCost = SearchIndex.ComparePoses(HistoricalPoseIndex.PoseIndex, 0.f, SearchIndex.GetPoseValuesSafe(HistoricalPoseIndex.PoseIndex), QueryValues);
					SearchContext.BestCandidates.Add(PoseCost, HistoricalPoseIndex.PoseIndex, Database, EPoseCandidateFlags::DiscardedBy_PoseReselectHistory);
				}
#endif
			}
		}
	}

	NonSelectableIdx.Sort();
}

struct FSearchFilters
{
	FSearchFilters(const UPoseSearchSchema* Schema, TConstArrayView<size_t> NonSelectableIdx, bool bAnyBlockTransition)
	{
		NonSelectableIdxFilter.NonSelectableIdx = NonSelectableIdx;

		if (bAnyBlockTransition)
		{
			Filters.Add(&BlockTransitionFilter);
		}

		if (NonSelectableIdxFilter.IsFilterActive())
		{
			Filters.Add(&NonSelectableIdxFilter);
		}

		for (const IPoseSearchFilter* Filter : Schema->GetChannels())
		{
			if (Filter->IsFilterActive())
			{
				Filters.Add(Filter);
			}
		}
	}

	bool AreFiltersValid(const FSearchIndex& SearchIndex, TConstArrayView<float> PoseValues, TConstArrayView<float> QueryValues, int32 PoseIdx, const FPoseMetadata& Metadata
#if UE_POSE_SEARCH_TRACE_ENABLED
		, UE::PoseSearch::FSearchContext& SearchContext, const UPoseSearchDatabase* Database
#endif
	) const
	{
		for (const IPoseSearchFilter* Filter : Filters)
		{
			if (!Filter->IsFilterValid(PoseValues, QueryValues, PoseIdx, Metadata))
			{
#if UE_POSE_SEARCH_TRACE_ENABLED
				if (Filter == &NonSelectableIdxFilter)
				{
					// candidate already added to SearchContext.BestCandidates by PopulateNonSelectableIdx
				}
				else if (Filter == &BlockTransitionFilter)
				{
					const FPoseSearchCost PoseCost = SearchIndex.ComparePoses(PoseIdx, 0.f, PoseValues, QueryValues);
					SearchContext.BestCandidates.Add(PoseCost, PoseIdx, Database, EPoseCandidateFlags::DiscardedBy_BlockTransition);
				}
				else
				{
					const FPoseSearchCost PoseCost = SearchIndex.ComparePoses(PoseIdx, 0.f, PoseValues, QueryValues);
					SearchContext.BestCandidates.Add(PoseCost, PoseIdx, Database, EPoseCandidateFlags::DiscardedBy_PoseFilter);
				}
#endif
				return false;
			}
		}
		return true;
	};

private:
	struct FNonSelectableIdxFilter : public IPoseSearchFilter
	{
		virtual bool IsFilterActive() const override
		{
			return !NonSelectableIdx.IsEmpty();
		}

		virtual bool IsFilterValid(TConstArrayView<float> PoseValues, TConstArrayView<float> QueryValues, int32 PoseIdx, const FPoseMetadata& Metadata) const override
		{
			return Algo::BinarySearch(NonSelectableIdx, PoseIdx) == INDEX_NONE;
		}

		TConstArrayView<size_t> NonSelectableIdx;
	};

	struct FBlockTransitionFilter : public IPoseSearchFilter
	{
		virtual bool IsFilterActive() const override
		{
			return true;
		}

		virtual bool IsFilterValid(TConstArrayView<float> PoseValues, TConstArrayView<float> QueryValues, int32 PoseIdx, const FPoseMetadata& Metadata) const override
		{
			return !Metadata.IsBlockTransition();
		}
	};

	FNonSelectableIdxFilter NonSelectableIdxFilter;
	FBlockTransitionFilter BlockTransitionFilter;

	TArray<const IPoseSearchFilter*, TInlineAllocator<64>> Filters;
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
	return Sequence &&
		Sequence->bLoop &&
		SamplingRange.Min == 0.f &&
		SamplingRange.Max == 0.f;
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
	return BlendSpace && BlendSpace->bLoop;
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

	if (bUseSingleSample)
	{
		HorizontalBlendNum = 1;
		VerticalBlendNum = 1;
	}
	else if (bUseGridForSampling)
	{
		HorizontalBlendNum = BlendSpace->GetBlendParameter(0).GridNum + 1;
		VerticalBlendNum = BlendSpace->IsA<UBlendSpace1D>() ? 1 : BlendSpace->GetBlendParameter(1).GridNum + 1;
	}
	else
	{
		HorizontalBlendNum = FMath::Max(NumberOfHorizontalSamples, 1);
		VerticalBlendNum = BlendSpace->IsA<UBlendSpace1D>() ? 1 : FMath::Max(NumberOfVerticalSamples, 1);
	}

	check(HorizontalBlendNum >= 1 && VerticalBlendNum >= 1);
}

FVector FPoseSearchDatabaseBlendSpace::BlendParameterForSampleRanges(int32 HorizontalBlendIndex, int32 VerticalBlendIndex) const
{
	check(BlendSpace);

	if (bUseSingleSample)
	{
		check(HorizontalBlendIndex == 0 && VerticalBlendIndex == 0);
		return FVector(BlendParamX, BlendParamY, 0.f);
	}
	
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
		0.f);
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
	return AnimComposite &&
		AnimComposite->bLoop &&
		SamplingRange.Min == 0.f &&
		SamplingRange.Max == 0.f;
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
// FPoseSearchDatabaseAnimMontage
UAnimationAsset* FPoseSearchDatabaseAnimMontage::GetAnimationAsset() const
{
	return AnimMontage;
}

UClass* FPoseSearchDatabaseAnimMontage::GetAnimationAssetStaticClass() const
{
	return UAnimMontage::StaticClass();
}

bool FPoseSearchDatabaseAnimMontage::IsLooping() const
{
	return AnimMontage &&
		AnimMontage->bLoop &&
		SamplingRange.Min == 0.f &&
		SamplingRange.Max == 0.f;
}

const FString FPoseSearchDatabaseAnimMontage::GetName() const
{
	return AnimMontage ? AnimMontage->GetName() : FString();
}

bool FPoseSearchDatabaseAnimMontage::IsRootMotionEnabled() const
{
	return AnimMontage ? AnimMontage->HasRootMotion() : false;
}

//////////////////////////////////////////////////////////////////////////
// UPoseSearchDatabase
UPoseSearchDatabase::~UPoseSearchDatabase()
{
}

void UPoseSearchDatabase::SetSearchIndex(const UE::PoseSearch::FSearchIndex& SearchIndex)
{
	check(IsInGameThread());
	SearchIndexPrivate = SearchIndex;
}

const UE::PoseSearch::FSearchIndex& UPoseSearchDatabase::GetSearchIndex() const
{
	// making sure the search index is consistent. if it fails the calling code hasn't been protected by FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex
	check(Schema && Schema->IsValid() && !SearchIndexPrivate.IsEmpty() && SearchIndexPrivate.WeightsSqrt.Num() == Schema->SchemaCardinality);
	return SearchIndexPrivate;
}

int32 UPoseSearchDatabase::GetPoseIndexFromTime(float Time, const UE::PoseSearch::FSearchIndexAsset& SearchIndexAsset) const
{
	const bool bIsLooping = IsSourceAssetLooping(SearchIndexAsset);
	return SearchIndexAsset.GetPoseIndexFromTime(Time, bIsLooping, Schema->SampleRate);
}

const FInstancedStruct& UPoseSearchDatabase::GetAnimationAssetStruct(int32 AnimationAssetIndex) const
{
	check(AnimationAssets.IsValidIndex(AnimationAssetIndex));
	return AnimationAssets[AnimationAssetIndex];
}

const FInstancedStruct& UPoseSearchDatabase::GetAnimationAssetStruct(const UE::PoseSearch::FSearchIndexAsset& SearchIndexAsset) const
{
	return GetAnimationAssetStruct(SearchIndexAsset.SourceAssetIdx);
}

FInstancedStruct& UPoseSearchDatabase::GetMutableAnimationAssetStruct(int32 AnimationAssetIndex)
{
	check(AnimationAssets.IsValidIndex(AnimationAssetIndex));
	return AnimationAssets[AnimationAssetIndex];
}

FInstancedStruct& UPoseSearchDatabase::GetMutableAnimationAssetStruct(const UE::PoseSearch::FSearchIndexAsset& SearchIndexAsset)
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

const FPoseSearchDatabaseAnimationAssetBase* UPoseSearchDatabase::GetAnimationAssetBase(const UE::PoseSearch::FSearchIndexAsset& SearchIndexAsset) const
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

FPoseSearchDatabaseAnimationAssetBase* UPoseSearchDatabase::GetMutableAnimationAssetBase(const UE::PoseSearch::FSearchIndexAsset& SearchIndexAsset)
{
	return GetMutableAnimationAssetBase(SearchIndexAsset.SourceAssetIdx);
}

const bool UPoseSearchDatabase::IsSourceAssetLooping(const UE::PoseSearch::FSearchIndexAsset& SearchIndexAsset) const
{
	return GetAnimationAssetBase(SearchIndexAsset.SourceAssetIdx)->IsLooping();
}

const FString UPoseSearchDatabase::GetSourceAssetName(const UE::PoseSearch::FSearchIndexAsset& SearchIndexAsset) const
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

float UPoseSearchDatabase::GetRealAssetTime(int32 PoseIdx) const
{
	check(Schema);
	const UE::PoseSearch::FSearchIndexAsset& Asset = GetSearchIndex().GetAssetForPose(PoseIdx);
	return Asset.GetTimeFromPoseIndex(PoseIdx, Schema->SampleRate);
}

float UPoseSearchDatabase::GetNormalizedAssetTime(int32 PoseIdx) const
{
	check(Schema);
	const UE::PoseSearch::FSearchIndexAsset& Asset = GetSearchIndex().GetAssetForPose(PoseIdx);
	const bool bIsBlendSpace = AnimationAssets[Asset.SourceAssetIdx].GetPtr<FPoseSearchDatabaseBlendSpace>() != nullptr;

	// sequences or anim composites
	float AssetTime = Asset.GetTimeFromPoseIndex(PoseIdx, Schema->SampleRate);

	if (bIsBlendSpace && Asset.GetNumPoses() > 1)
	{
		// For BlendSpaces the AssetTime is in the range [0, 1] while the Sampling Range
		// is in real time (seconds). We should be using but FAnimationAssetSampler::GetPlayLength(...) to normalize precisely,
		// but Asset.GetNumPoses() - 1 is a good enough estimator
		AssetTime = AssetTime * Schema->SampleRate / float(Asset.GetNumPoses() - 1);
		check(AssetTime >= 0.f && AssetTime <= 1.f);
	}

	return AssetTime;
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
#if UE_POSE_SEARCH_TRACE_ENABLED
		FPoseSearchCost BruteForcePoseCost = Result.BruteForcePoseCost;
#endif // UE_POSE_SEARCH_TRACE_ENABLED

		Result = SearchPCAKDTree(SearchContext);

#if UE_POSE_SEARCH_TRACE_ENABLED
		Result.BruteForcePoseCost = BruteForcePoseCost;
		if (PoseSearchMode == EPoseSearchMode::PCAKDTree_Compare)
		{
			check(Result.BruteForcePoseCost.GetTotalCost() <= Result.PoseCost.GetTotalCost());
		}
#endif // UE_POSE_SEARCH_TRACE_ENABLED
	}
	
	return Result;
}

template<bool bReconstructPoseValues, bool bAlignedAndPadded>
static inline void EvaluatePoseKernel(UE::PoseSearch::FSearchResult& Result, const UE::PoseSearch::FSearchIndex& SearchIndex, TConstArrayView<float> QueryValues, TArrayView<float> ReconstructedPoseValuesBuffer,
	int32 PoseIdx, const UE::PoseSearch::FSearchFilters& SearchFilters, UE::PoseSearch::FSearchContext& SearchContext, const UPoseSearchDatabase* Database, bool bUpdateBestCandidates, size_t ResultIndex)
{
	using namespace UE::PoseSearch;

	const TConstArrayView<float> PoseValues = bReconstructPoseValues ? SearchIndex.GetReconstructedPoseValues(PoseIdx, ReconstructedPoseValuesBuffer) : SearchIndex.GetPoseValues(PoseIdx);

	if (SearchFilters.AreFiltersValid(SearchIndex, PoseValues, QueryValues, PoseIdx, SearchIndex.PoseMetadata[PoseIdx]
#if UE_POSE_SEARCH_TRACE_ENABLED
		, SearchContext, Database
#endif
	))
	{
		const FPoseSearchCost PoseCost = bAlignedAndPadded ? SearchIndex.CompareAlignedPoses(PoseIdx, 0.f, PoseValues, QueryValues) : SearchIndex.ComparePoses(PoseIdx, 0.f, PoseValues, QueryValues);
		if (PoseCost < Result.PoseCost)
		{
			Result.PoseCost = PoseCost;
			Result.PoseIdx = PoseIdx;

#if UE_POSE_SEARCH_TRACE_ENABLED
			if (bUpdateBestCandidates)
			{
				Result.BestPosePos = ResultIndex;
			}
#endif // WITH_EDITORONLY_DATA
		}

#if UE_POSE_SEARCH_TRACE_ENABLED
		if (bUpdateBestCandidates)
		{
			SearchContext.BestCandidates.Add(PoseCost, PoseIdx, Database, EPoseCandidateFlags::Valid_Pose);
		}
#endif
	}
}

FPoseSearchCost UPoseSearchDatabase::SearchContinuingPose(UE::PoseSearch::FSearchContext& SearchContext) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_PoseSearch_ContinuingPose);

	using namespace UE::PoseSearch;

	check(SearchContext.GetCurrentResult().Database.Get() == this);

	FPoseSearchCost ContinuingPoseCost;

#if WITH_EDITOR
	if (!FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(this, ERequestAsyncBuildFlag::ContinueRequest))
	{
		return ContinuingPoseCost;
	}
#endif

	// extracting notifies from the database animation asset at time SampleTime to search for UAnimNotifyState_PoseSearchOverrideContinuingPoseCostBias eventually overriding the schema ContinuingPoseCostBias
	const FSearchIndex& SearchIndex = GetSearchIndex();
	const int32 PoseIdx = SearchContext.GetCurrentResult().PoseIdx;
	const FSearchIndexAsset& SearchIndexAsset = SearchIndex.GetAssetForPose(PoseIdx);
	const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAssetBase = GetAnimationAssetStruct(SearchIndexAsset).GetPtr<FPoseSearchDatabaseAnimationAssetBase>();
	check(DatabaseAnimationAssetBase);
	const FAnimationAssetSampler SequenceBaseSampler(DatabaseAnimationAssetBase->GetAnimationAsset(), SearchIndexAsset.BlendParameters);
	const float SampleTime = GetNormalizedAssetTime(PoseIdx);

	// @todo: change ExtractPoseSearchNotifyStates api to avoid NotifyStates allocation
	TArray<UAnimNotifyState_PoseSearchBase*> NotifyStates;
	SequenceBaseSampler.ExtractPoseSearchNotifyStates(SampleTime, NotifyStates);

	float ContinuingPoseCostBias = Schema->ContinuingPoseCostBias;
	for (const UAnimNotifyState_PoseSearchBase* PoseSearchNotify : NotifyStates)
	{
		if (const UAnimNotifyState_PoseSearchOverrideContinuingPoseCostBias* ContinuingPoseCostBiasNotify = Cast<const UAnimNotifyState_PoseSearchOverrideContinuingPoseCostBias>(PoseSearchNotify))
		{
			ContinuingPoseCostBias = ContinuingPoseCostBiasNotify->CostAddend;
			break;
		}
	}

	// since any PoseCost calculated here is at least SearchIndex.MinCostAddend + ContinuingPoseCostBias,
	// there's no point in performing the search if CurrentBestTotalCost is already better than that
	if (!GetSkipSearchIfPossible() || SearchContext.GetCurrentBestTotalCost() > SearchIndex.MinCostAddend + ContinuingPoseCostBias)
	{
		const int32 NumDimensions = Schema->SchemaCardinality;
		// FMemory_Alloca is forced 16 bytes aligned
		TArrayView<float> ReconstructedPoseValuesBuffer((float*)FMemory_Alloca(NumDimensions * sizeof(float)), NumDimensions);
		check(IsAligned(ReconstructedPoseValuesBuffer.GetData(), alignof(VectorRegister4Float)));
		const TConstArrayView<float> PoseValues = SearchIndex.Values.IsEmpty() ? SearchIndex.GetReconstructedPoseValues(PoseIdx, ReconstructedPoseValuesBuffer) : SearchIndex.GetPoseValues(PoseIdx);

		const int32 ContinuingPoseIdx = SearchContext.GetCurrentResult().PoseIdx;
		// is the data padded at 16 bytes (and 16 bytes aligned by construction)?
		if (NumDimensions % 4 == 0)
		{
			ContinuingPoseCost = SearchIndex.CompareAlignedPoses(ContinuingPoseIdx, ContinuingPoseCostBias, PoseValues, SearchContext.GetOrBuildQuery(Schema).GetValues());
		}
		// data is not 16 bytes padded
		else
		{
			ContinuingPoseCost = SearchIndex.ComparePoses(ContinuingPoseIdx, ContinuingPoseCostBias, PoseValues, SearchContext.GetOrBuildQuery(Schema).GetValues());
		}

#if UE_POSE_SEARCH_TRACE_ENABLED
		SearchContext.BestCandidates.Add(ContinuingPoseCost, ContinuingPoseIdx, this, EPoseCandidateFlags::Valid_ContinuingPose);
#endif // UE_POSE_SEARCH_TRACE_ENABLED
	}
	else
	{
#if UE_POSE_SEARCH_TRACE_ENABLED
		SearchContext.BestCandidates.Add(this);
#endif // UE_POSE_SEARCH_TRACE_ENABLED
	}

	return ContinuingPoseCost;
}

UE::PoseSearch::FSearchResult UPoseSearchDatabase::SearchPCAKDTree(UE::PoseSearch::FSearchContext& SearchContext) const
{
	SCOPE_CYCLE_COUNTER(STAT_PoseSearch_PCAKNN);

	using namespace UE::PoseSearch;

	FSearchResult Result;

	const int32 NumDimensions = Schema->SchemaCardinality;
	const FSearchIndex& SearchIndex = GetSearchIndex();

	const uint32 ClampedNumberOfPrincipalComponents = GetNumberOfPrincipalComponents();
	const uint32 ClampedKDTreeQueryNumNeighbors = FMath::Clamp<uint32>(KDTreeQueryNumNeighbors, 1, SearchIndex.GetNumPoses());

	//stack allocated temporaries
	TArrayView<size_t> ResultIndexes((size_t*)FMemory_Alloca((ClampedKDTreeQueryNumNeighbors + 1) * sizeof(size_t)), ClampedKDTreeQueryNumNeighbors + 1);
	TArrayView<float> ResultDistanceSqr((float*)FMemory_Alloca((ClampedKDTreeQueryNumNeighbors + 1) * sizeof(float)), ClampedKDTreeQueryNumNeighbors + 1);
	TArrayView<float> ProjectedQueryValues((float*)FMemory_Alloca(ClampedNumberOfPrincipalComponents * sizeof(float)), ClampedNumberOfPrincipalComponents);

#if DO_CHECK
	// KDTree in PCA space search
	if (PoseSearchMode == EPoseSearchMode::PCAKDTree_Validate)
	{
		// testing the KDTree is returning the proper searches for all the original points transformed in pca space
		for (int32 PoseIdx = 0; PoseIdx < SearchIndex.GetNumPoses(); ++PoseIdx)
		{
			FKDTree::KNNResultSet ResultSet(ClampedKDTreeQueryNumNeighbors, ResultIndexes, ResultDistanceSqr);
			SearchIndex.KDTree.FindNeighbors(ResultSet, SearchIndex.PCAProject(SearchIndex.GetPoseValues(PoseIdx), ProjectedQueryValues).GetData());

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
#endif //DO_CHECK

	// since any PoseCost calculated here is at least SearchIndex.MinCostAddend,
	// there's no point in performing the search if CurrentBestTotalCost is already better than that
	if (!GetSkipSearchIfPossible() || SearchContext.GetCurrentBestTotalCost() > SearchIndex.MinCostAddend)
	{
		TConstArrayView<float> QueryValues = SearchContext.GetOrBuildQuery(Schema).GetValues();

		FNonSelectableIdx NonSelectableIdx;
		PopulateNonSelectableIdx(NonSelectableIdx, SearchContext, this
#if UE_POSE_SEARCH_TRACE_ENABLED
			, QueryValues
#endif // UE_POSE_SEARCH_TRACE_ENABLED
		);
		FKDTree::KNNResultSet ResultSet(ClampedKDTreeQueryNumNeighbors, ResultIndexes, ResultDistanceSqr, NonSelectableIdx);

		check(QueryValues.Num() == NumDimensions);
		// projecting QueryValues into the PCA space ProjectedQueryValues and query the KDTree
		SearchIndex.KDTree.FindNeighbors(ResultSet, SearchIndex.PCAProject(QueryValues, ProjectedQueryValues).GetData());

		// NonSelectableIdx are already filtered out inside the kdtree search
		const FSearchFilters SearchFilters(Schema, TConstArrayView<size_t>(), SearchIndex.bAnyBlockTransition);
		
		// do we need to reconstruct pose values?
		if (SearchIndex.Values.IsEmpty())
		{
			// FMemory_Alloca is forced 16 bytes aligned
			TArrayView<float> ReconstructedPoseValuesBuffer((float*)FMemory_Alloca(NumDimensions * sizeof(float)), NumDimensions);
			check(IsAligned(ReconstructedPoseValuesBuffer.GetData(), alignof(VectorRegister4Float)));
			for (size_t ResultIndex = 0; ResultIndex < ResultSet.Num(); ++ResultIndex)
			{
				EvaluatePoseKernel<true, false>(Result, SearchIndex, QueryValues, ReconstructedPoseValuesBuffer, ResultIndexes[ResultIndex], SearchFilters, SearchContext, this, true, ResultIndex);
			}
		}
		// is the data padded at 16 bytes (and 16 bytes aligned by construction)?
		else if (NumDimensions % 4 == 0)
		{
			for (size_t ResultIndex = 0; ResultIndex < ResultSet.Num(); ++ResultIndex)
			{
				EvaluatePoseKernel<false, true>(Result, SearchIndex, QueryValues, TArrayView<float>(), ResultIndexes[ResultIndex], SearchFilters, SearchContext, this, true, ResultIndex);
			}
		}
		// no reconstruction, but data is not 16 bytes padded
		else
		{
			for (size_t ResultIndex = 0; ResultIndex < ResultSet.Num(); ++ResultIndex)
			{
				EvaluatePoseKernel<false, false>(Result, SearchIndex, QueryValues, TArrayView<float>(), ResultIndexes[ResultIndex], SearchFilters, SearchContext, this, true, ResultIndex);
			}
		}
	}
	else
	{
#if UE_POSE_SEARCH_TRACE_ENABLED
		// calling just for reporting non selectable poses
		TConstArrayView<float> QueryValues = SearchContext.GetOrBuildQuery(Schema).GetValues();
		FNonSelectableIdx NonSelectableIdx;
		PopulateNonSelectableIdx(NonSelectableIdx, SearchContext, this, QueryValues);
#endif // UE_POSE_SEARCH_TRACE_ENABLED
	}

#if UE_POSE_SEARCH_TRACE_ENABLED
	SearchContext.BestCandidates.Add(this);
#endif // UE_POSE_SEARCH_TRACE_ENABLED

	// finalizing Result properties
	if (Result.PoseIdx != INDEX_NONE)
	{
		Result.AssetTime = GetNormalizedAssetTime(Result.PoseIdx);
		Result.Database = this;
	}

	return Result;
}

UE::PoseSearch::FSearchResult UPoseSearchDatabase::SearchBruteForce(UE::PoseSearch::FSearchContext& SearchContext) const
{
	SCOPE_CYCLE_COUNTER(STAT_PoseSearch_BruteForce);
	
	using namespace UE::PoseSearch;
	
	FSearchResult Result;

	const FSearchIndex& SearchIndex = GetSearchIndex();

	// since any PoseCost calculated here is at least SearchIndex.MinCostAddend,
	// there's no point in performing the search if CurrentBestTotalCost is already better than that
	if (!GetSkipSearchIfPossible() || SearchContext.GetCurrentBestTotalCost() > SearchIndex.MinCostAddend)
	{
		TConstArrayView<float> QueryValues = SearchContext.GetOrBuildQuery(Schema).GetValues();

		FNonSelectableIdx NonSelectableIdx;
		PopulateNonSelectableIdx(NonSelectableIdx, SearchContext, this
#if UE_POSE_SEARCH_TRACE_ENABLED
			, QueryValues
#endif // UE_POSE_SEARCH_TRACE_ENABLED
		);
		check(Algo::IsSorted(NonSelectableIdx));

		const int32 NumDimensions = Schema->SchemaCardinality;
		const FSearchFilters SearchFilters(Schema, NonSelectableIdx, SearchIndex.bAnyBlockTransition);
		const bool bUpdateBestCandidates = PoseSearchMode == EPoseSearchMode::BruteForce;

		// do we need to reconstruct pose values?
		if (SearchIndex.Values.IsEmpty())
		{
			// FMemory_Alloca is forced 16 bytes aligned
			TArrayView<float> ReconstructedPoseValuesBuffer((float*)FMemory_Alloca(NumDimensions * sizeof(float)), NumDimensions);
			check(IsAligned(ReconstructedPoseValuesBuffer.GetData(), alignof(VectorRegister4Float)));
			for (int32 PoseIdx = 0; PoseIdx < SearchIndex.GetNumPoses(); ++PoseIdx)
			{
				EvaluatePoseKernel<true, false>(Result, SearchIndex, QueryValues, ReconstructedPoseValuesBuffer, PoseIdx, SearchFilters, SearchContext, this, bUpdateBestCandidates, PoseIdx);
			}
		}
		// is the data padded at 16 bytes (and 16 bytes aligned by construction)?
		else if (NumDimensions % 4 == 0)
		{
			for (int32 PoseIdx = 0; PoseIdx < SearchIndex.GetNumPoses(); ++PoseIdx)
			{
				EvaluatePoseKernel<false, true>(Result, SearchIndex, QueryValues, TArrayView<float>(), PoseIdx, SearchFilters, SearchContext, this, bUpdateBestCandidates, PoseIdx);
			}
		}
		// no reconstruction, but data is not 16 bytes padded
		else
		{
			for (int32 PoseIdx = 0; PoseIdx < SearchIndex.GetNumPoses(); ++PoseIdx)
			{
				EvaluatePoseKernel<false, false>(Result, SearchIndex, QueryValues, TArrayView<float>(), PoseIdx, SearchFilters, SearchContext, this, bUpdateBestCandidates, PoseIdx);
			}
		}
	}
	else
	{
#if UE_POSE_SEARCH_TRACE_ENABLED
		// calling just for reporting non selectable poses
		TConstArrayView<float> QueryValues = SearchContext.GetOrBuildQuery(Schema).GetValues();
		FNonSelectableIdx NonSelectableIdx;
		PopulateNonSelectableIdx(NonSelectableIdx, SearchContext, this, QueryValues);
#endif // UE_POSE_SEARCH_TRACE_ENABLED
	}

#if UE_POSE_SEARCH_TRACE_ENABLED
	SearchContext.BestCandidates.Add(this);
#endif // UE_POSE_SEARCH_TRACE_ENABLED

	// finalizing Result properties
	if (Result.PoseIdx != INDEX_NONE)
	{
		Result.AssetTime = GetNormalizedAssetTime(Result.PoseIdx);
		Result.Database = this;
	}

#if UE_POSE_SEARCH_TRACE_ENABLED
	Result.BruteForcePoseCost = Result.PoseCost; 
#endif // UE_POSE_SEARCH_TRACE_ENABLED

	return Result;
}