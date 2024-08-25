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
#include "PoseSearch/PoseSearchMultiSequence.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "Serialization/ArchiveCountMem.h"
#include "UObject/ObjectSaveContext.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#endif //WITH_EDITOR

#if WITH_EDITOR && WITH_ENGINE
#include "Editor/EditorEngine.h"
#endif //WITH_EDITOR && WITH_ENGINE

DECLARE_STATS_GROUP(TEXT("PoseSearch"), STATGROUP_PoseSearch, STATCAT_Advanced);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Search Brute Force"), STAT_PoseSearch_BruteForce, STATGROUP_PoseSearch, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Search PCA/KNN"), STAT_PoseSearch_PCAKNN, STATGROUP_PoseSearch, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Search VPTree"), STAT_PoseSearch_VPTree, STATGROUP_PoseSearch, );
DEFINE_STAT(STAT_PoseSearch_BruteForce);
DEFINE_STAT(STAT_PoseSearch_PCAKNN);
DEFINE_STAT(STAT_PoseSearch_VPTree);

namespace UE::PoseSearch
{
#if WITH_EDITOR && ENABLE_ANIM_DEBUG
static TAutoConsoleVariable<bool> CVarMotionMatchCompareAgainstBruteForce(TEXT("a.MotionMatch.CompareAgainstBruteForce"), false, TEXT("Compare optimized search against brute force search"));
static TAutoConsoleVariable<bool> CVarMotionMatchValidateKNNSearch(TEXT("a.MotionMatch.ValidateKNNSearch"), false, TEXT("Validate KNN search"));
#endif

typedef TArray<int32, TInlineAllocator<256, TMemStackAllocator<>>> FSelectableAssetIdx;
static void PopulateSelectableAssetIdx(FSelectableAssetIdx& SelectableAssetIdx, TConstArrayView<const UObject*> AssetsToConsider, const UPoseSearchDatabase* Database)
{
	check(Database);
	SelectableAssetIdx.Reset();
	if (!AssetsToConsider.IsEmpty())
	{
		const FSearchIndex& SearchIndex = Database->GetSearchIndex();

		for (int32 AssetIndex = 0; AssetIndex < SearchIndex.Assets.Num(); ++AssetIndex)
		{
			if (const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAssetBase = Database->GetAnimationAssetBase(SearchIndex.Assets[AssetIndex]))
			{
				if (AssetsToConsider.Contains(DatabaseAnimationAssetBase->GetAnimationAsset()))
				{
					SelectableAssetIdx.Add(AssetIndex);
				}
			}
		}
	}
}

typedef TArray<int32, TInlineAllocator<256, TMemStackAllocator<>>> FNonSelectableIdx;
static void PopulateNonSelectableIdx(FNonSelectableIdx& NonSelectableIdx, FSearchContext& SearchContext, const UPoseSearchDatabase* Database
#if UE_POSE_SEARCH_TRACE_ENABLED
	, TConstArrayView<float> QueryValues
#endif //UE_POSE_SEARCH_TRACE_ENABLED
)
{
	check(Database);
	const FSearchIndex& SearchIndex = Database->GetSearchIndex();

	NonSelectableIdx.Reset();
	if (SearchContext.IsCurrentResultFromDatabase(Database))
	{
		if (const FSearchIndexAsset* CurrentIndexAsset = SearchContext.GetCurrentResult().GetSearchIndexAsset(true))
		{
			if (CurrentIndexAsset->IsDisableReselection())
			{
				// excluding all the poses with CurrentIndexAsset->GetSourceAssetIdx()
				// @todo: optimize this code!
				for (const FSearchIndexAsset& SearchIndexAsset : SearchIndex.Assets)
				{
					if (SearchIndexAsset.GetSourceAssetIdx() == CurrentIndexAsset->GetSourceAssetIdx())
					{
						const int32 FirstPoseIdx = SearchIndexAsset.GetFirstPoseIdx();
						const int32 LastPoseIdx = FirstPoseIdx + SearchIndexAsset.GetNumPoses();
						for (int32 PoseIdx = FirstPoseIdx; PoseIdx < LastPoseIdx; ++PoseIdx)
						{
							// no need to AddUnique since there's no overlapping between pose indexes in the FSearchIndexAsset(s)
							NonSelectableIdx.Add(PoseIdx);

#if UE_POSE_SEARCH_TRACE_ENABLED
							const TArray<float> PoseValues = SearchIndex.GetPoseValuesSafe(PoseIdx);
							const FPoseSearchCost PoseCost = SearchIndex.ComparePoses(PoseIdx, 0.f, PoseValues, QueryValues);
							SearchContext.Track(Database, PoseIdx, EPoseCandidateFlags::DiscardedBy_PoseJumpThresholdTime, PoseCost);
#endif // UE_POSE_SEARCH_TRACE_ENABLED
						}
					}
				}
			}
			else if (!FMath::IsNearlyEqual(SearchContext.GetPoseJumpThresholdTime().Min, SearchContext.GetPoseJumpThresholdTime().Max))
			{
				const int32 CurrentResultPoseIdx = SearchContext.GetCurrentResult().PoseIdx;
				const int32 UnboundMinPoseIdx = CurrentResultPoseIdx + FMath::FloorToInt(SearchContext.GetPoseJumpThresholdTime().Min * Database->Schema->SampleRate);
				const int32 UnboundMaxPoseIdx = CurrentResultPoseIdx + FMath::CeilToInt(SearchContext.GetPoseJumpThresholdTime().Max * Database->Schema->SampleRate);
				const int32 CurrentIndexAssetFirstPoseIdx = CurrentIndexAsset->GetFirstPoseIdx();
				const int32 CurrentIndexAssetNumPoses = CurrentIndexAsset->GetNumPoses();
				const bool bIsLooping = CurrentIndexAsset->IsLooping();

				if (bIsLooping)
				{
					for (int32 UnboundPoseIdx = UnboundMinPoseIdx; UnboundPoseIdx < UnboundMaxPoseIdx; ++UnboundPoseIdx)
					{
						const int32 Modulo = (UnboundPoseIdx - CurrentIndexAssetFirstPoseIdx) % CurrentIndexAssetNumPoses;
						const int32 CurrentIndexAssetFirstPoseIdxPlusModulo = CurrentIndexAssetFirstPoseIdx + Modulo;
						const int32 PoseIdx = Modulo >= 0 ? CurrentIndexAssetFirstPoseIdxPlusModulo : CurrentIndexAssetFirstPoseIdxPlusModulo + CurrentIndexAssetNumPoses;

						NonSelectableIdx.AddUnique(PoseIdx);

#if UE_POSE_SEARCH_TRACE_ENABLED
						const TArray<float> PoseValues = SearchIndex.GetPoseValuesSafe(PoseIdx);
						const FPoseSearchCost PoseCost = SearchIndex.ComparePoses(PoseIdx, 0.f, PoseValues, QueryValues);
						SearchContext.Track(Database, PoseIdx, EPoseCandidateFlags::DiscardedBy_PoseJumpThresholdTime, PoseCost);
#endif // UE_POSE_SEARCH_TRACE_ENABLED
					}
				}
				else
				{
					const int32 MinPoseIdx = FMath::Max(CurrentIndexAssetFirstPoseIdx, UnboundMinPoseIdx);
					const int32 MaxPoseIdx = FMath::Min(CurrentIndexAssetFirstPoseIdx + CurrentIndexAssetNumPoses, UnboundMaxPoseIdx);

					for (int32 PoseIdx = MinPoseIdx; PoseIdx < MaxPoseIdx; ++PoseIdx)
					{
						NonSelectableIdx.AddUnique(PoseIdx);

#if UE_POSE_SEARCH_TRACE_ENABLED
						const TArray<float> PoseValues = SearchIndex.GetPoseValuesSafe(PoseIdx);
						const FPoseSearchCost PoseCost = SearchIndex.ComparePoses(PoseIdx, 0.f, PoseValues, QueryValues);
						SearchContext.Track(Database, PoseIdx, EPoseCandidateFlags::DiscardedBy_PoseJumpThresholdTime, PoseCost);
#endif // UE_POSE_SEARCH_TRACE_ENABLED
					}
				}
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
					SearchContext.Track(Database, HistoricalPoseIndex.PoseIndex, EPoseCandidateFlags::DiscardedBy_PoseReselectHistory, PoseCost);
				}
#endif // UE_POSE_SEARCH_TRACE_ENABLED
			}
		}
	}

	NonSelectableIdx.Sort();
}

struct FSearchFilters
{
	FSearchFilters(const UPoseSearchSchema* Schema, TConstArrayView<int32> NonSelectableIdx, TConstArrayView<int32> SelectableAssetIdx, bool bAddBlockTransitionFilter)
	{
		if (bAddBlockTransitionFilter)
		{
			Filters.Add(&BlockTransitionFilter);
		}

		if (NonSelectableIdxFilter.Init(NonSelectableIdx).IsFilterActive())
		{
			Filters.Add(&NonSelectableIdxFilter);
		}

		if (SelectableAssetIdxFilter.Init(SelectableAssetIdx).IsFilterActive())
		{
			Filters.Add(&SelectableAssetIdxFilter);
		}

		for (const IPoseSearchFilter* Filter : Schema->GetChannels())
		{
			if (Filter->IsFilterActive())
			{
				Filters.Add(Filter);
			}
		}
	}

	bool AreFiltersValid(const FSearchIndex& SearchIndex, TConstArrayView<float> PoseValues, TConstArrayView<float> QueryValues, int32 PoseIdx
#if UE_POSE_SEARCH_TRACE_ENABLED
		, UE::PoseSearch::FSearchContext& SearchContext, const UPoseSearchDatabase* Database
#endif // UE_POSE_SEARCH_TRACE_ENABLED
	) const
	{
		for (const IPoseSearchFilter* Filter : Filters)
		{
			if (!Filter->IsFilterValid(PoseValues, QueryValues, PoseIdx, SearchIndex.PoseMetadata[PoseIdx]))
			{
#if UE_POSE_SEARCH_TRACE_ENABLED
				if (Filter == &NonSelectableIdxFilter)
				{
					// candidate already added to SearchContext.BestCandidates by PopulateNonSelectableIdx
				}
				else if (Filter == &SelectableAssetIdxFilter)
				{
					const FPoseSearchCost PoseCost = SearchIndex.ComparePoses(PoseIdx, 0.f, PoseValues, QueryValues);
					SearchContext.Track(Database, PoseIdx, EPoseCandidateFlags::DiscardedBy_AssetIdxFilter, PoseCost);
				}
				else if (Filter == &BlockTransitionFilter)
				{
					const FPoseSearchCost PoseCost = SearchIndex.ComparePoses(PoseIdx, 0.f, PoseValues, QueryValues);
					SearchContext.Track(Database, PoseIdx, EPoseCandidateFlags::DiscardedBy_BlockTransition, PoseCost);
				}
				else
				{
					const FPoseSearchCost PoseCost = SearchIndex.ComparePoses(PoseIdx, 0.f, PoseValues, QueryValues);
					SearchContext.Track(Database, PoseIdx, EPoseCandidateFlags::DiscardedBy_PoseFilter, PoseCost);
				}
#endif // UE_POSE_SEARCH_TRACE_ENABLED
				return false;
			}
		}
		return true;
	};

private:
	struct FNonSelectableIdxFilter : public IPoseSearchFilter
	{
		const FNonSelectableIdxFilter& Init(TConstArrayView<int32> InNonSelectableIdx)
		{
			check(Algo::IsSorted(InNonSelectableIdx));
			NonSelectableIdx = InNonSelectableIdx;
			return *this;
		}

		virtual bool IsFilterActive() const override
		{
			return !NonSelectableIdx.IsEmpty();
		}

		virtual bool IsFilterValid(TConstArrayView<float> PoseValues, TConstArrayView<float> QueryValues, int32 PoseIdx, const FPoseMetadata& Metadata) const override
		{
			return Algo::BinarySearch(NonSelectableIdx, PoseIdx) == INDEX_NONE;
		}

		TConstArrayView<int32> NonSelectableIdx;
	};

	struct FSelectableAssetIdxFilter : public IPoseSearchFilter
	{
		const FSelectableAssetIdxFilter& Init(TConstArrayView<int32> InSelectableAssetIdxFilter)
		{
			check(Algo::IsSorted(InSelectableAssetIdxFilter));
			SelectableAssetIdxFilter = InSelectableAssetIdxFilter;
			return *this;
		}

		virtual bool IsFilterActive() const override
		{
			return !SelectableAssetIdxFilter.IsEmpty();
		}

		virtual bool IsFilterValid(TConstArrayView<float> PoseValues, TConstArrayView<float> QueryValues, int32 PoseIdx, const FPoseMetadata& Metadata) const override
		{
			return Algo::BinarySearch(SelectableAssetIdxFilter, int32(Metadata.GetAssetIndex())) != INDEX_NONE;
		}

		TConstArrayView<int32> SelectableAssetIdxFilter;
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
	FSelectableAssetIdxFilter SelectableAssetIdxFilter;
	FBlockTransitionFilter BlockTransitionFilter;

	TArray<const IPoseSearchFilter*, TInlineAllocator<64, TMemStackAllocator<>>> Filters;
};

} // namespace UE::PoseSearch

//////////////////////////////////////////////////////////////////////////
// FPoseSearchDatabaseAnimationAssetBase

float FPoseSearchDatabaseAnimationAssetBase::GetPlayLength() const
{
	if (const UAnimationAsset* AnimationAsset = Cast<UAnimationAsset>(GetAnimationAsset()))
	{
		return AnimationAsset->GetPlayLength();
	}

	checkNoEntry();
	return 0;
}

#if WITH_EDITOR
int32 FPoseSearchDatabaseAnimationAssetBase::GetFrameAtTime(float Time) const
{
	if (const UAnimSequenceBase* SequenceBase = Cast<UAnimSequenceBase>(GetAnimationAsset()))
	{
		return SequenceBase->GetFrameAtTime(Time);
	}
	return 0.f;
}
#endif // WITH_EDITOR

UAnimationAsset* FPoseSearchDatabaseAnimationAssetBase::GetAnimationAssetForRole(const UE::PoseSearch::FRole& Role) const
{
	check(GetNumRoles() == 1);
	return CastChecked<UAnimationAsset>(GetAnimationAsset());
}

const FTransform& FPoseSearchDatabaseAnimationAssetBase::GetRootTransformOriginForRole(const UE::PoseSearch::FRole& Role) const
{
	check(GetNumRoles() == 1);
	return FTransform::Identity;
}

#if WITH_EDITORONLY_DATA
int64 FPoseSearchDatabaseAnimationAssetBase::GetEditorMemSize() const
{
	FArchiveCountMem EditorMemCount(GetAnimationAsset());
	return EditorMemCount.GetNum();
}

FFloatInterval FPoseSearchDatabaseAnimationAssetBase::GetEffectiveSamplingRange(const UAnimSequenceBase* SequenceBase, const FFloatInterval& RequestedSamplingRange)
{
	const bool bSampleAll = (RequestedSamplingRange.Min == 0.0f) && (RequestedSamplingRange.Max == 0.0f);
	const float SequencePlayLength = SequenceBase->GetPlayLength();
	FFloatInterval Range;
	Range.Min = bSampleAll ? 0.0f : RequestedSamplingRange.Min;
	Range.Max = bSampleAll ? SequencePlayLength : FMath::Min(SequencePlayLength, RequestedSamplingRange.Max);

	if (Range.Min > Range.Max)
	{
		UE_LOG(LogPoseSearch, Warning, TEXT("Sampling range minimum (%f) is greated than max (%f). Setting min to be equal to max."), Range.Min, Range.Max)
		
		Range.Min = Range.Max;
	}
	
	return Range;
}
#endif // WITH_EDITORONLY_DATA

//////////////////////////////////////////////////////////////////////////
// FPoseSearchDatabaseSequence
UObject* FPoseSearchDatabaseSequence::GetAnimationAsset() const
{
	return Sequence.Get();
}

#if WITH_EDITORONLY_DATA
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
#endif // WITH_EDITORONLY_DATA

//////////////////////////////////////////////////////////////////////////
// FPoseSearchDatabaseBlendSpace
UObject* FPoseSearchDatabaseBlendSpace::GetAnimationAsset() const
{
	return BlendSpace.Get();
}

#if WITH_EDITOR
int32 FPoseSearchDatabaseBlendSpace::GetFrameAtTime(float Time) const
{
	// returning the percentage of time as value to diplay in the pose search debugger (NoTe: BlendSpace->GetPlayLength() is one)
	return FMath::RoundToInt(Time * 100.f);
}
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
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
				const UAnimSequence* Sequence = Sample.Animation.Get();
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

#endif // WITH_EDITORONLY_DATA

//////////////////////////////////////////////////////////////////////////
// FPoseSearchDatabaseAnimComposite
UObject* FPoseSearchDatabaseAnimComposite::GetAnimationAsset() const
{
	return AnimComposite.Get();
}

#if WITH_EDITORONLY_DATA
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
#endif // WITH_EDITORONLY_DATA

//////////////////////////////////////////////////////////////////////////
// FPoseSearchDatabaseAnimMontage
UObject* FPoseSearchDatabaseAnimMontage::GetAnimationAsset() const
{
	return AnimMontage.Get();
}

#if WITH_EDITORONLY_DATA
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
#endif // WITH_EDITORONLY_DATA

//////////////////////////////////////////////////////////////////////////
// FPoseSearchDatabaseMultiSequence
UObject* FPoseSearchDatabaseMultiSequence::GetAnimationAsset() const
{
	return MultiSequence.Get();
}

float FPoseSearchDatabaseMultiSequence::GetPlayLength() const
{
	return MultiSequence ? MultiSequence->GetPlayLength() : 0.f;
}

#if WITH_EDITOR
int32 FPoseSearchDatabaseMultiSequence::GetFrameAtTime(float Time) const
{
	return MultiSequence ? MultiSequence->GetFrameAtTime(Time) : 0;
}
#endif // WITH_EDITOR

int32 FPoseSearchDatabaseMultiSequence::GetNumRoles() const
{
	return MultiSequence ? MultiSequence->GetNumRoles() : 0;
}

UE::PoseSearch::FRole FPoseSearchDatabaseMultiSequence::GetRole(int32 RoleIndex) const
{
	return MultiSequence ? MultiSequence->GetRole(RoleIndex) : UE::PoseSearch::DefaultRole;
}

UAnimationAsset* FPoseSearchDatabaseMultiSequence::GetAnimationAssetForRole(const UE::PoseSearch::FRole& Role) const
{
	return MultiSequence ? MultiSequence->GetSequence(Role) : nullptr;
}

const FTransform& FPoseSearchDatabaseMultiSequence::GetRootTransformOriginForRole(const UE::PoseSearch::FRole& Role) const
{
	return MultiSequence ? MultiSequence->GetOrigin(Role) : FTransform::Identity;
}

#if WITH_EDITORONLY_DATA
UClass* FPoseSearchDatabaseMultiSequence::GetAnimationAssetStaticClass() const
{
	return UPoseSearchMultiSequence::StaticClass();
}

bool FPoseSearchDatabaseMultiSequence::IsLooping() const
{
	return MultiSequence &&
		MultiSequence->IsLooping() &&
		SamplingRange.Min == 0.f &&
		SamplingRange.Max == 0.f;
}

const FString FPoseSearchDatabaseMultiSequence::GetName() const
{
	return MultiSequence ? MultiSequence->GetName() : FString();
}

bool FPoseSearchDatabaseMultiSequence::IsRootMotionEnabled() const
{
	return MultiSequence ? MultiSequence->HasRootMotion() : false;
}
#endif // WITH_EDITORONLY_DATA

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
	check(Schema && !SearchIndexPrivate.IsEmpty() && SearchIndexPrivate.GetNumDimensions() == Schema->SchemaCardinality);
	return SearchIndexPrivate;
}

int32 UPoseSearchDatabase::GetPoseIndexFromTime(float Time, const UE::PoseSearch::FSearchIndexAsset& SearchIndexAsset) const
{
	return SearchIndexAsset.GetPoseIndexFromTime(Time, Schema->SampleRate);
}

void UPoseSearchDatabase::AddAnimationAsset(FInstancedStruct AnimationAsset)
{
	AnimationAssets.Add(AnimationAsset);
}

void UPoseSearchDatabase::RemoveAnimationAssetAt(int32 AnimationAssetIndex)
{
	AnimationAssets.RemoveAt(AnimationAssetIndex);
}

const FInstancedStruct& UPoseSearchDatabase::GetAnimationAssetStruct(int32 AnimationAssetIndex) const
{
	check(AnimationAssets.IsValidIndex(AnimationAssetIndex));
	return AnimationAssets[AnimationAssetIndex];
}

const FInstancedStruct& UPoseSearchDatabase::GetAnimationAssetStruct(const UE::PoseSearch::FSearchIndexAsset& SearchIndexAsset) const
{
	return GetAnimationAssetStruct(SearchIndexAsset.GetSourceAssetIdx());
}

FInstancedStruct& UPoseSearchDatabase::GetMutableAnimationAssetStruct(int32 AnimationAssetIndex)
{
	check(AnimationAssets.IsValidIndex(AnimationAssetIndex));
	return AnimationAssets[AnimationAssetIndex];
}

FInstancedStruct& UPoseSearchDatabase::GetMutableAnimationAssetStruct(const UE::PoseSearch::FSearchIndexAsset& SearchIndexAsset)
{
	return GetMutableAnimationAssetStruct(SearchIndexAsset.GetSourceAssetIdx());
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
	return GetAnimationAssetBase(SearchIndexAsset.GetSourceAssetIdx());
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
	return GetMutableAnimationAssetBase(SearchIndexAsset.GetSourceAssetIdx());
}

#if WITH_EDITOR
int32 UPoseSearchDatabase::GetNumberOfPrincipalComponents() const
{
	return FMath::Min<int32>(NumberOfPrincipalComponents, Schema->SchemaCardinality);
}
#endif //WITH_EDITOR

bool UPoseSearchDatabase::GetSkipSearchIfPossible() const
{
#if WITH_EDITOR && ENABLE_ANIM_DEBUG
	if (UE::PoseSearch::CVarMotionMatchCompareAgainstBruteForce.GetValueOnAnyThread())
	{
		return false;
	}
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG
	return true;
}

void UPoseSearchDatabase::PostLoad()
{
#if WITH_EDITOR
	using namespace UE::PoseSearch;

	ERequestAsyncBuildFlag Flag = ERequestAsyncBuildFlag::NewRequest;
#if WITH_ENGINE
	// If there isn't an EditorEngine (ex. Standalone Game via -game argument) we WaitForCompletion
	if (Cast<UEditorEngine>(GEngine) == nullptr)
	{
		Flag |= ERequestAsyncBuildFlag::WaitForCompletion;
	}
#endif // WITH_ENGINE

	FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(this, Flag);
#endif

	Super::PostLoad();
}

#if WITH_EDITOR
void UPoseSearchDatabase::SynchronizeWithExternalDependencies()
{
	TArray<FTopLevelAssetPath> AncestorClassNames;

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	TArray<FAssetIdentifier> Referencers;
	AssetRegistry.GetReferencers(GetPackage()->GetFName(), Referencers);

	TArray<UAnimSequenceBase*> SequencesBase;
	for (const FAssetIdentifier& Referencer : Referencers)
	{
		TArray<FAssetData> Assets;
		AssetRegistry.GetAssetsByPackageName(Referencer.PackageName, Assets);

		for (const FAssetData& Asset : Assets)
		{
			if (Asset.IsInstanceOf(UAnimSequenceBase::StaticClass()))
			{
				if (UAnimSequenceBase* SequenceBase = CastChecked<UAnimSequenceBase>(Asset.FastGetAsset(true)))
				{
					for (const FAnimNotifyEvent& NotifyEvent : SequenceBase->Notifies)
					{
						if (const UAnimNotifyState_PoseSearchBranchIn* BranchIn = Cast<UAnimNotifyState_PoseSearchBranchIn>(NotifyEvent.NotifyStateClass))
						{
							if (BranchIn->Database == this)
							{
								SequencesBase.AddUnique(SequenceBase);
								break;
							}
						}
					}
				}
			}
		}
	}

	if (!SequencesBase.IsEmpty())
	{
		SynchronizeWithExternalDependencies(SequencesBase);
	}
}

void UPoseSearchDatabase::SynchronizeWithExternalDependencies(TConstArrayView<UAnimSequenceBase*> SequencesBase)
{
	// cannot use TSet since FInstancedStruct doesn't implement GetTypeHash
	TArray<FInstancedStruct> NewAnimationAssets;

	// collecting all the database AnimationAsset(s) that don't require synchronization
	TArray<bool> DisableReselection;
	DisableReselection.Reserve(AnimationAssets.Num());
	for (FInstancedStruct& AnimationAsset : AnimationAssets)
	{
		FPoseSearchDatabaseAnimationAssetBase& AnimationAssetBase = AnimationAsset.GetMutable<FPoseSearchDatabaseAnimationAssetBase>();
		DisableReselection.Add(AnimationAssetBase.bDisableReselection);
		AnimationAssetBase.bDisableReselection = false;

		const bool bRequiresSynchronization = AnimationAssetBase.bSynchronizeWithExternalDependency && SequencesBase.Contains(AnimationAssetBase.GetAnimationAsset());
		if (!bRequiresSynchronization)
		{
			NewAnimationAssets.Add(AnimationAsset);
		}
	}

	// collecting all the SequencesBase(s) requiring synchronization
	for (UAnimSequenceBase* SequenceBase : SequencesBase)
	{
		if (SequenceBase)
		{
			for (const FAnimNotifyEvent& NotifyEvent : SequenceBase->Notifies)
			{
				if (const UAnimNotifyState_PoseSearchBranchIn* PoseSearchBranchIn = Cast<UAnimNotifyState_PoseSearchBranchIn>(NotifyEvent.NotifyStateClass))
				{
					if (PoseSearchBranchIn->Database == this)
					{
						auto GetSamplingRange = [](const FAnimNotifyEvent& NotifyEvent, const UAnimSequenceBase* SequenceBase) -> FFloatInterval
						{
							FFloatInterval SamplingRange(NotifyEvent.GetTime(), NotifyEvent.GetTime() + NotifyEvent.GetDuration());
							if (SamplingRange.Min <= NotifyEvent.TriggerTimeOffset && SamplingRange.Max >= SequenceBase->GetPlayLength() - NotifyEvent.TriggerTimeOffset)
							{
								SamplingRange = FFloatInterval(0.f, 0.f);
							}
							return SamplingRange;
						};

						if (UAnimSequence* Sequence = Cast<UAnimSequence>(SequenceBase))
						{
							FPoseSearchDatabaseSequence DatabaseSequence;
							DatabaseSequence.Sequence = Sequence;
							DatabaseSequence.SamplingRange = GetSamplingRange(NotifyEvent, SequenceBase);
							DatabaseSequence.bSynchronizeWithExternalDependency = true;
							NewAnimationAssets.Add(FInstancedStruct::Make(DatabaseSequence));
						}
						else if (UAnimComposite* AnimComposite = Cast<UAnimComposite>(SequenceBase))
						{
							FPoseSearchDatabaseAnimComposite DatabaseAnimComposite;
							DatabaseAnimComposite.AnimComposite = AnimComposite;
							DatabaseAnimComposite.SamplingRange = GetSamplingRange(NotifyEvent, SequenceBase);
							DatabaseAnimComposite.bSynchronizeWithExternalDependency = true;
							NewAnimationAssets.Add(FInstancedStruct::Make(DatabaseAnimComposite));
						}
						else if (UAnimMontage* AnimMontage = Cast<UAnimMontage>(SequenceBase))
						{
							FPoseSearchDatabaseAnimMontage DatabaseAnimMontage;
							DatabaseAnimMontage.AnimMontage = AnimMontage;
							DatabaseAnimMontage.SamplingRange = GetSamplingRange(NotifyEvent, SequenceBase);
							DatabaseAnimMontage.bSynchronizeWithExternalDependency = true;
							NewAnimationAssets.Add(FInstancedStruct::Make(DatabaseAnimMontage));
						}
					}
				}
			}
		}
	}

	// updating AnimationAssets from NewAnimationAssets preserving the original sorting
	bool bModified = false;
	for (int32 AnimationAssetIndex = AnimationAssets.Num() - 1; AnimationAssetIndex >= 0; --AnimationAssetIndex)
	{
		const int32 FoundIndex = NewAnimationAssets.Find(AnimationAssets[AnimationAssetIndex]);
		if (FoundIndex >= 0)
		{
			FPoseSearchDatabaseAnimationAssetBase& AnimationAssetBase = AnimationAssets[AnimationAssetIndex].GetMutable<FPoseSearchDatabaseAnimationAssetBase>();
			AnimationAssetBase.bDisableReselection = DisableReselection[AnimationAssetIndex];
			NewAnimationAssets.RemoveAt(FoundIndex); 
		}
		else
		{
			AnimationAssets.RemoveAt(AnimationAssetIndex);
			bModified = true;
		}
	}

	// adding the remaining AnimationAsset(s) from AnimationAssetsSet
	for (const FInstancedStruct& AnimationAsset : NewAnimationAssets)
	{
		AnimationAssets.Add(AnimationAsset);
		bModified = true;
	}

	if (bModified)
	{
		Modify();
		NotifySynchronizeWithExternalDependencies();
	}
}

bool UPoseSearchDatabase::Contains(const UObject* Object) const
{
	for (const FInstancedStruct& AnimationAsset : AnimationAssets)
	{
		if (const FPoseSearchDatabaseAnimationAssetBase* AnimationAssetBase = AnimationAsset.GetPtr<FPoseSearchDatabaseAnimationAssetBase>())
		{
			if (AnimationAssetBase->GetAnimationAsset() == Object)
			{
				return true;
			}
		}
	}
	return false;
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
	return EAsyncBuildIndexResult::InProgress != FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(this, ERequestAsyncBuildFlag::ContinueRequest);
}
#endif // WITH_EDITOR

#if WITH_EDITOR && ENABLE_ANIM_DEBUG
void UPoseSearchDatabase::TestSynchronizeWithExternalDependencies()
{
	TArray<FInstancedStruct> AnimationAssetsCopy = AnimationAssets;
	SynchronizeWithExternalDependencies();

	if (AnimationAssetsCopy != AnimationAssets)
	{
		UE_LOG(LogPoseSearch, Error, TEXT("TestSynchronizeWithExternalDependencies failed"));
		AnimationAssets = AnimationAssetsCopy;
	}
}
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG

void UPoseSearchDatabase::PreSaveRoot(FObjectPreSaveRootContext ObjectSaveContext)
{
#if WITH_EDITOR
	// in case the database desynchronized with the UAnimNotifyState_PoseSearchBranchIn referencing it, we need to resyncrhonize
	SynchronizeWithExternalDependencies();
#endif

	Super::PreSaveRoot(ObjectSaveContext);
}

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
	const bool bIsBlendSpace = AnimationAssets[Asset.GetSourceAssetIdx()].GetPtr<FPoseSearchDatabaseBlendSpace>() != nullptr;

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
	if (EAsyncBuildIndexResult::Success != FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(this, ERequestAsyncBuildFlag::ContinueRequest))
	{
		SearchContext.SetAsyncBuildIndexInProgress();
		return Result;
	}
#endif // WITH_EDITOR

	if (PoseSearchMode == EPoseSearchMode::BruteForce
#if WITH_EDITOR && ENABLE_ANIM_DEBUG
		|| CVarMotionMatchCompareAgainstBruteForce.GetValueOnAnyThread()
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG
		)
	{
		Result = SearchBruteForce(SearchContext);
	}

#if WITH_EDITOR && ENABLE_ANIM_DEBUG
	const FPoseSearchCost BruteForcePoseCost = Result.BruteForcePoseCost;
	const int32 BruteForcePoseIdx = Result.PoseIdx;
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG

	if (PoseSearchMode == EPoseSearchMode::VPTree)
	{
		Result = SearchVPTree(SearchContext);

#if WITH_EDITOR && ENABLE_ANIM_DEBUG
		Result.BruteForcePoseCost = BruteForcePoseCost;
		if (Result.PoseIdx != BruteForcePoseIdx && CVarMotionMatchCompareAgainstBruteForce.GetValueOnAnyThread())
		{
			UE_LOG(LogPoseSearch, Error, TEXT("UPoseSearchDatabase::Search - VPTree search PoseIdx %d differs from BruteForce search PoseIdx %d"), Result.PoseIdx, BruteForcePoseIdx);
		}
#endif // UE_POSE_SEARCH_TRACE_ENABLED
	}
	else if (PoseSearchMode == EPoseSearchMode::PCAKDTree)
	{
		Result = SearchPCAKDTree(SearchContext);

#if WITH_EDITOR && ENABLE_ANIM_DEBUG
		Result.BruteForcePoseCost = BruteForcePoseCost;
		if (CVarMotionMatchCompareAgainstBruteForce.GetValueOnAnyThread())
		{
			const float BruteForceTotalCost = Result.BruteForcePoseCost.GetTotalCost();
			const float PCAKDTreeTotalCost = Result.PoseCost.GetTotalCost();
			check(BruteForceTotalCost <= PCAKDTreeTotalCost);

			if (!FMath::IsNearlyEqual(BruteForceTotalCost, PCAKDTreeTotalCost))
			{
				UE_LOG(LogPoseSearch, Warning, TEXT("UPoseSearchDatabase::Search - PCAKDTree cost comparison %f (PCAKDTreeTotalCost %f, BruteForceTotalCost %f)"), PCAKDTreeTotalCost - BruteForceTotalCost, PCAKDTreeTotalCost, BruteForceTotalCost);
			}
		}
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG
	}

#if UE_POSE_SEARCH_TRACE_ENABLED
	// in case we skipped the search, or we didn't find any candidates we still have to track we requested to evaluate this database, so we keep track of this
	SearchContext.Track(this);
#endif // UE_POSE_SEARCH_TRACE_ENABLED

	return Result;
}

template<bool bReconstructPoseValues, bool bAlignedAndPadded>
static inline void EvaluatePoseKernel(UE::PoseSearch::FSearchResult& Result, const UE::PoseSearch::FSearchIndex& SearchIndex, TConstArrayView<float> QueryValues, TArrayView<float> ReconstructedPoseValuesBuffer,
	int32 PoseIdx, const UE::PoseSearch::FSearchFilters& SearchFilters, UE::PoseSearch::FSearchContext& SearchContext, const UPoseSearchDatabase* Database, bool bUpdateBestCandidates, int32 ResultIndex = -1)
{
	using namespace UE::PoseSearch;

	const TConstArrayView<float> PoseValues = bReconstructPoseValues ? SearchIndex.GetReconstructedPoseValues(PoseIdx, ReconstructedPoseValuesBuffer) : SearchIndex.GetPoseValues(PoseIdx);

	if (SearchFilters.AreFiltersValid(SearchIndex, PoseValues, QueryValues, PoseIdx
#if UE_POSE_SEARCH_TRACE_ENABLED
		, SearchContext, Database
#endif // UE_POSE_SEARCH_TRACE_ENABLED
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
#endif // UE_POSE_SEARCH_TRACE_ENABLED
		}

#if UE_POSE_SEARCH_TRACE_ENABLED
		if (bUpdateBestCandidates)
		{
			SearchContext.Track(Database, PoseIdx, EPoseCandidateFlags::Valid_Pose, PoseCost);
		}
#endif // UE_POSE_SEARCH_TRACE_ENABLED
	}
}

UE::PoseSearch::FSearchResult UPoseSearchDatabase::SearchContinuingPose(UE::PoseSearch::FSearchContext& SearchContext) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_PoseSearch_ContinuingPose);

	using namespace UE::PoseSearch;

	check(SearchContext.GetCurrentResult().Database.Get() == this);

	FSearchResult Result;
	Result.bIsContinuingPoseSearch = true;

#if WITH_EDITOR
	if (EAsyncBuildIndexResult::Success != FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(this, ERequestAsyncBuildFlag::ContinueRequest))
	{
		SearchContext.SetAsyncBuildIndexInProgress();
		return Result;
	}
#endif // WITH_EDITOR

	// extracting notifies from the database animation asset at time SampleTime to search for UAnimNotifyState_PoseSearchOverrideContinuingPoseCostBias eventually overriding the database ContinuingPoseCostBias
	const FSearchIndex& SearchIndex = GetSearchIndex();
	const int32 PoseIdx = SearchContext.GetCurrentResult().PoseIdx;
	const FSearchIndexAsset& SearchIndexAsset = SearchIndex.GetAssetForPose(PoseIdx);
	const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAssetBase = GetAnimationAssetStruct(SearchIndexAsset).GetPtr<FPoseSearchDatabaseAnimationAssetBase>();
	check(DatabaseAnimationAssetBase);
	const UAnimationAsset* AnimationAsset = CastChecked<UAnimationAsset>(DatabaseAnimationAssetBase->GetAnimationAsset());
	
	// sampler used only to extract the notify states. RootTransformOrigin can be set as Identity, since will not be relevant
	const FAnimationAssetSampler SequenceBaseSampler(AnimationAsset, FTransform::Identity, SearchIndexAsset.GetBlendParameters());
	const float SampleTime = GetRealAssetTime(PoseIdx);

	float UpdatedContinuingPoseCostBias = ContinuingPoseCostBias;
	SequenceBaseSampler.ExtractPoseSearchNotifyStates(SampleTime, [&UpdatedContinuingPoseCostBias](const UAnimNotifyState_PoseSearchBase* PoseSearchNotify)
		{
			if (const UAnimNotifyState_PoseSearchOverrideContinuingPoseCostBias* ContinuingPoseCostBiasNotify = Cast<const UAnimNotifyState_PoseSearchOverrideContinuingPoseCostBias>(PoseSearchNotify))
			{
				UpdatedContinuingPoseCostBias = ContinuingPoseCostBiasNotify->CostAddend;
				return false;
			}
			return true;
		});

	// since any PoseCost calculated here is at least SearchIndex.MinCostAddend + UpdatedContinuingPoseCostBias,
	// there's no point in performing the search if CurrentBestTotalCost is already better than that
	if (!GetSkipSearchIfPossible() || SearchContext.GetCurrentBestTotalCost() > SearchIndex.MinCostAddend + UpdatedContinuingPoseCostBias)
	{
		const int32 NumDimensions = Schema->SchemaCardinality;
		// FMemory_Alloca is forced 16 bytes aligned
		TArrayView<float> ReconstructedPoseValuesBuffer((float*)FMemory_Alloca(NumDimensions * sizeof(float)), NumDimensions);
		check(IsAligned(ReconstructedPoseValuesBuffer.GetData(), alignof(VectorRegister4Float)));
		const TConstArrayView<float> PoseValues = SearchIndex.IsValuesEmpty() ? SearchIndex.GetReconstructedPoseValues(PoseIdx, ReconstructedPoseValuesBuffer) : SearchIndex.GetPoseValues(PoseIdx);

		const int32 ContinuingPoseIdx = SearchContext.GetCurrentResult().PoseIdx;
		// is the data padded at 16 bytes (and 16 bytes aligned by construction)?
		if (NumDimensions % 4 == 0)
		{
			Result.PoseCost = SearchIndex.CompareAlignedPoses(ContinuingPoseIdx, UpdatedContinuingPoseCostBias, PoseValues, SearchContext.GetOrBuildQuery(Schema));
		}
		// data is not 16 bytes padded
		else
		{
			Result.PoseCost = SearchIndex.ComparePoses(ContinuingPoseIdx, UpdatedContinuingPoseCostBias, PoseValues, SearchContext.GetOrBuildQuery(Schema));
		}

		Result.AssetTime = SearchContext.GetCurrentResult().AssetTime;
		Result.PoseIdx = PoseIdx;
		Result.Database = this;

#if UE_POSE_SEARCH_TRACE_ENABLED
		SearchContext.Track(this, ContinuingPoseIdx, EPoseCandidateFlags::Valid_ContinuingPose, Result.PoseCost);
#endif // UE_POSE_SEARCH_TRACE_ENABLED
	}

	return Result;
}

UE::PoseSearch::FSearchResult UPoseSearchDatabase::SearchPCAKDTree(UE::PoseSearch::FSearchContext& SearchContext) const
{
	SCOPE_CYCLE_COUNTER(STAT_PoseSearch_PCAKNN);

	using namespace UE::PoseSearch;

	FSearchResult Result;

	const int32 NumDimensions = Schema->SchemaCardinality;
	const FSearchIndex& SearchIndex = GetSearchIndex();

	// since any PoseCost calculated here is at least SearchIndex.MinCostAddend,
	// there's no point in performing the search if CurrentBestTotalCost is already better than that
	if (!GetSkipSearchIfPossible() || SearchContext.GetCurrentBestTotalCost() > SearchIndex.MinCostAddend)
	{
		const uint32 ClampedNumberOfPrincipalComponents = SearchIndex.GetNumberOfPrincipalComponents();
		const uint32 ClampedKDTreeQueryNumNeighbors = FMath::Clamp<uint32>(KDTreeQueryNumNeighbors, 1, SearchIndex.GetNumPoses());
		const bool bArePCAValuesPruned = SearchIndex.PCAValuesVectorToPoseIndexes.Num() > 0;

		//stack allocated temporaries
		TArrayView<int32> ResultIndexes((int32*)FMemory_Alloca((ClampedKDTreeQueryNumNeighbors + 1) * sizeof(int32)), ClampedKDTreeQueryNumNeighbors + 1);
		TArrayView<float> ResultDistanceSqr((float*)FMemory_Alloca((ClampedKDTreeQueryNumNeighbors + 1) * sizeof(float)), ClampedKDTreeQueryNumNeighbors + 1);
		TArrayView<float> ProjectedQueryValues((float*)FMemory_Alloca(ClampedNumberOfPrincipalComponents * sizeof(float)), ClampedNumberOfPrincipalComponents);
	
		TConstArrayView<float> QueryValues = SearchContext.GetOrBuildQuery(Schema);

		FSelectableAssetIdx SelectableAssetIdx;
		PopulateSelectableAssetIdx(SelectableAssetIdx, SearchContext.GetAssetsToConsider(), this);

		FNonSelectableIdx NonSelectableIdx;
		PopulateNonSelectableIdx(NonSelectableIdx, SearchContext, this
#if UE_POSE_SEARCH_TRACE_ENABLED
			, QueryValues
#endif // UE_POSE_SEARCH_TRACE_ENABLED
		);

		bool bRunNonSelectableIdxPostKDTree = bArePCAValuesPruned;
#if WITH_EDITOR && ENABLE_ANIM_DEBUG
		const bool bValidateKNNSearch = CVarMotionMatchValidateKNNSearch.GetValueOnAnyThread();
		bRunNonSelectableIdxPostKDTree |= bValidateKNNSearch;
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG

		check(QueryValues.Num() == NumDimensions);
		// projecting QueryValues into the PCA space 
		TConstArrayView<float> PCAQueryValues = SearchIndex.PCAProject(QueryValues, ProjectedQueryValues);
		check(PCAQueryValues.Num() == ClampedNumberOfPrincipalComponents);

		int32 NumResults = 0;
		if (bRunNonSelectableIdxPostKDTree || NonSelectableIdx.IsEmpty())
		{
			FKDTree::FKNNResultSet ResultSet(ClampedKDTreeQueryNumNeighbors, ResultIndexes, ResultDistanceSqr);
			NumResults = SearchIndex.KDTree.FindNeighbors(ResultSet, PCAQueryValues);
		}
		else
		{
			FKDTree::FFilteredKNNResultSet ResultSet(ClampedKDTreeQueryNumNeighbors, ResultIndexes, ResultDistanceSqr, NonSelectableIdx);
			NumResults = SearchIndex.KDTree.FindNeighbors(ResultSet, PCAQueryValues);
		}

#if WITH_EDITOR && ENABLE_ANIM_DEBUG
		if (bValidateKNNSearch)
		{
			const int32 NumPCAValuesVectors = SearchIndex.GetNumPCAValuesVectors(ClampedNumberOfPrincipalComponents);

			TArray<TPair<int32, float>> PCAValueIndexCost;
			PCAValueIndexCost.SetNumUninitialized(NumPCAValuesVectors);

			// validating that the best n "ClampedKDTreeQueryNumNeighbors" are actually the best candidates
			for (int32 PCAValueIndex = 0; PCAValueIndex < NumPCAValuesVectors; ++PCAValueIndex)
			{
				PCAValueIndexCost[PCAValueIndex].Key = PCAValueIndex;
				PCAValueIndexCost[PCAValueIndex].Value = CompareFeatureVectors(SearchIndex.GetPCAPoseValues(PCAValueIndex), PCAQueryValues);
			}

			PCAValueIndexCost.Sort([](const TPair<int32, float>& A, const TPair<int32, float>& B)
				{
					return A.Value < B.Value;
				});

			for (int32 ResultIndex = 0; ResultIndex < NumResults; ++ResultIndex)
			{
				if (PCAValueIndexCost[ResultIndex].Key != ResultIndexes[ResultIndex])
				{
					if (!FMath::IsNearlyEqual(PCAValueIndexCost[ResultIndex].Value, ResultDistanceSqr[ResultIndex], UE_KINDA_SMALL_NUMBER))
					{
						UE_LOG(LogPoseSearch, Error, TEXT("UPoseSearchDatabase::SearchPCAKDTree - KDTree search order is inconsistent with exaustive search in PCA space"));
					}
					else
					{
						UE_LOG(LogPoseSearch, Log, TEXT("UPoseSearchDatabase::SearchPCAKDTree - found two points at the same distance from the query in different order between KDTree and exaustive search"));
					}
				}
				else if (!FMath::IsNearlyEqual(PCAValueIndexCost[ResultIndex].Value, ResultDistanceSqr[ResultIndex], UE_KINDA_SMALL_NUMBER))
				{
					UE_LOG(LogPoseSearch, Error, TEXT("UPoseSearchDatabase::SearchPCAKDTree - KDTree search cost is inconsistent with exaustive search in PCA space"));
				}
			}
		}
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG

		// NonSelectableIdx are already filtered out inside the kdtree search.
		// Also kdtrees don't contain block transition poses by construction, so FSearchFilters input bAddBlockTransitionFilter can be set to false
		const FSearchFilters SearchFilters(Schema, bRunNonSelectableIdxPostKDTree ? NonSelectableIdx : TConstArrayView<int32>(), SelectableAssetIdx, false);
		
		// are the PCAValues pruned out of duplicates (multiple poses are associated with the same PCAValuesVectorIdx)
		if (bArePCAValuesPruned)
		{
			// @todo: reconstruction is not yet supported with pruned PCAValues
			check(!SearchIndex.IsValuesEmpty());
			
			const int32 MaxNumEvaluatePoseKernelCalls = KDTreeQueryNumNeighborsWithDuplicates > 0 ? KDTreeQueryNumNeighborsWithDuplicates : INT32_MAX;

			if (NumDimensions % 4 == 0)
			{
				int32 NumEvaluatePoseKernelCalls = 0;
				for (int32 ResultIndex = 0; ResultIndex < NumResults; ++ResultIndex)
				{
					const TConstArrayView<int32> PoseIndexes = SearchIndex.PCAValuesVectorToPoseIndexes[ResultIndexes[ResultIndex]];
					for (int32 Index = 0; Index < PoseIndexes.Num() && NumEvaluatePoseKernelCalls < MaxNumEvaluatePoseKernelCalls; ++Index, ++NumEvaluatePoseKernelCalls)
					{
						EvaluatePoseKernel<false, true>(Result, SearchIndex, QueryValues, TArrayView<float>(), PoseIndexes[Index], SearchFilters, SearchContext, this, true, ResultIndex);
					}
				}
			}
			else
			{
				int32 NumEvaluatePoseKernelCalls = 0;
				for (int32 ResultIndex = 0; ResultIndex < NumResults; ++ResultIndex)
				{
					const TConstArrayView<int32> PoseIndexes = SearchIndex.PCAValuesVectorToPoseIndexes[ResultIndexes[ResultIndex]];
					for (int32 Index = 0; Index < PoseIndexes.Num() && NumEvaluatePoseKernelCalls < MaxNumEvaluatePoseKernelCalls; ++Index, ++NumEvaluatePoseKernelCalls)
					{
						EvaluatePoseKernel<false, false>(Result, SearchIndex, QueryValues, TArrayView<float>(), PoseIndexes[Index], SearchFilters, SearchContext, this, true, ResultIndex);
					}
				}
			}
		}
		// do we need to reconstruct pose values?
		else if (SearchIndex.IsValuesEmpty())
		{
			// FMemory_Alloca is forced 16 bytes aligned
			TArrayView<float> ReconstructedPoseValuesBuffer((float*)FMemory_Alloca(NumDimensions * sizeof(float)), NumDimensions);
			check(IsAligned(ReconstructedPoseValuesBuffer.GetData(), alignof(VectorRegister4Float)));
			for (int32 ResultIndex = 0; ResultIndex < NumResults; ++ResultIndex)
			{
				EvaluatePoseKernel<true, false>(Result, SearchIndex, QueryValues, ReconstructedPoseValuesBuffer, ResultIndexes[ResultIndex], SearchFilters, SearchContext, this, true, ResultIndex);
			}
		}
		// is the data padded at 16 bytes (and 16 bytes aligned by construction)?
		else if (NumDimensions % 4 == 0)
		{
			for (int32 ResultIndex = 0; ResultIndex < NumResults; ++ResultIndex)
			{
				EvaluatePoseKernel<false, true>(Result, SearchIndex, QueryValues, TArrayView<float>(), ResultIndexes[ResultIndex], SearchFilters, SearchContext, this, true, ResultIndex);
			}
		}
		// no reconstruction, but data is not 16 bytes padded
		else
		{
			for (int32 ResultIndex = 0; ResultIndex < NumResults; ++ResultIndex)
			{
				EvaluatePoseKernel<false, false>(Result, SearchIndex, QueryValues, TArrayView<float>(), ResultIndexes[ResultIndex], SearchFilters, SearchContext, this, true, ResultIndex);
			}
		}
	}
	else
	{
#if UE_POSE_SEARCH_TRACE_ENABLED
		// calling just for reporting non selectable poses
		TConstArrayView<float> QueryValues = SearchContext.GetOrBuildQuery(Schema);
		FNonSelectableIdx NonSelectableIdx;
		PopulateNonSelectableIdx(NonSelectableIdx, SearchContext, this, QueryValues);
#endif // UE_POSE_SEARCH_TRACE_ENABLED
	}

	// finalizing Result properties
	if (Result.PoseIdx != INDEX_NONE)
	{
		Result.AssetTime = GetNormalizedAssetTime(Result.PoseIdx);
		Result.Database = this;
	}

	return Result;
}

UE::PoseSearch::FSearchResult UPoseSearchDatabase::SearchVPTree(UE::PoseSearch::FSearchContext& SearchContext) const
{
	SCOPE_CYCLE_COUNTER(STAT_PoseSearch_VPTree);

	using namespace UE::PoseSearch;

	FSearchResult Result;

	const FSearchIndex& SearchIndex = GetSearchIndex();

	// since any PoseCost calculated here is at least SearchIndex.MinCostAddend,
	// there's no point in performing the search if CurrentBestTotalCost is already better than that
	if (!GetSkipSearchIfPossible() || SearchContext.GetCurrentBestTotalCost() > SearchIndex.MinCostAddend)
	{
		TConstArrayView<float> QueryValues = SearchContext.GetOrBuildQuery(Schema);

		FSelectableAssetIdx SelectableAssetIdx;
		PopulateSelectableAssetIdx(SelectableAssetIdx, SearchContext.GetAssetsToConsider(), this);

		// @todo: implement filtering within the VPTree as KDTree does
		FNonSelectableIdx NonSelectableIdx;
		PopulateNonSelectableIdx(NonSelectableIdx, SearchContext, this
#if UE_POSE_SEARCH_TRACE_ENABLED
			, QueryValues
#endif // UE_POSE_SEARCH_TRACE_ENABLED
		);

		const int32 NumDimensions = Schema->SchemaCardinality;
		check(QueryValues.Num() == NumDimensions);
		
		const FSearchFilters SearchFilters(Schema, NonSelectableIdx, SelectableAssetIdx, SearchIndex.bAnyBlockTransition);

		// @todo: implement a FVPTreeDataSource for aligned and padded features vector like CompareAlignedPoses does 
		FVPTreeDataSource DataSource(SearchIndex);
		FVPTreeResultSet ResultSet(KDTreeQueryNumNeighbors);
		SearchIndex.VPTree.FindNeighbors(QueryValues, ResultSet, DataSource);
		
		int32 NumEvaluatePoseKernelCalls = 0;
		const TConstArrayView<FIndexDistance> UnsortedResults = ResultSet.GetUnsortedResults();

		const bool bAreValuesPruned = SearchIndex.ValuesVectorToPoseIndexes.Num() > 0;
		if (bAreValuesPruned)
		{
			const int32 MaxNumEvaluatePoseKernelCalls = KDTreeQueryNumNeighborsWithDuplicates > 0 ? KDTreeQueryNumNeighborsWithDuplicates : INT32_MAX;
			for (int32 ResultIndex = 0; ResultIndex < UnsortedResults.Num(); ++ResultIndex)
			{
				const FIndexDistance& IndexDistance = UnsortedResults[ResultIndex];

				// @todo: IndexDistance.Distance is the Sqrt(DissimilarityCost), so there's no need to calculate it again in SearchIndex.ComparePoses
				const TConstArrayView<int32> PoseIndexes = SearchIndex.ValuesVectorToPoseIndexes[IndexDistance.Index];
				for (int32 Index = 0; Index < PoseIndexes.Num() && NumEvaluatePoseKernelCalls < MaxNumEvaluatePoseKernelCalls; ++Index, ++NumEvaluatePoseKernelCalls)
				{
					EvaluatePoseKernel<false, false>(Result, SearchIndex, QueryValues, TArrayView<float>(), PoseIndexes[Index], SearchFilters, SearchContext, this, true, ResultIndex);
				}
			}
		}
		else
		{
			for (int32 ResultIndex = 0; ResultIndex < UnsortedResults.Num(); ++ResultIndex)
			{
				const FIndexDistance& IndexDistance = UnsortedResults[ResultIndex];

				// @todo: IndexDistance.Distance is the Sqrt(DissimilarityCost), so there's no need to calculate it again in SearchIndex.ComparePoses
				EvaluatePoseKernel<false, false>(Result, SearchIndex, QueryValues, TArrayView<float>(), IndexDistance.Index, SearchFilters, SearchContext, this, true, ResultIndex);
			}
		}
	}
	else
	{
#if UE_POSE_SEARCH_TRACE_ENABLED
		// calling just for reporting non selectable poses
		TConstArrayView<float> QueryValues = SearchContext.GetOrBuildQuery(Schema);
		FNonSelectableIdx NonSelectableIdx;
		PopulateNonSelectableIdx(NonSelectableIdx, SearchContext, this, QueryValues);
#endif // UE_POSE_SEARCH_TRACE_ENABLED
	}

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
		TConstArrayView<float> QueryValues = SearchContext.GetOrBuildQuery(Schema);

		FSelectableAssetIdx SelectableAssetIdx;
		PopulateSelectableAssetIdx(SelectableAssetIdx, SearchContext.GetAssetsToConsider(), this);

		FNonSelectableIdx NonSelectableIdx;
		PopulateNonSelectableIdx(NonSelectableIdx, SearchContext, this
#if UE_POSE_SEARCH_TRACE_ENABLED
			, QueryValues
#endif // UE_POSE_SEARCH_TRACE_ENABLED
		);

		const int32 NumDimensions = Schema->SchemaCardinality;
		const bool bUpdateBestCandidates = PoseSearchMode == EPoseSearchMode::BruteForce;

		const FSearchFilters SearchFilters(Schema, NonSelectableIdx, FSelectableAssetIdx(), SearchIndex.bAnyBlockTransition);

		if (SelectableAssetIdx.IsEmpty())
		{
			// do we need to reconstruct pose values?
			if (SearchIndex.IsValuesEmpty())
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
			int32 ResultIndex = -1;

			// do we need to reconstruct pose values?
			if (SearchIndex.IsValuesEmpty())
			{
				// FMemory_Alloca is forced 16 bytes aligned
				TArrayView<float> ReconstructedPoseValuesBuffer((float*)FMemory_Alloca(NumDimensions * sizeof(float)), NumDimensions);
				check(IsAligned(ReconstructedPoseValuesBuffer.GetData(), alignof(VectorRegister4Float)));

				for (int32 AssetIdx : SelectableAssetIdx)
				{
					const FSearchIndexAsset& SearchIndexAsset = SearchIndex.Assets[AssetIdx];
					const int32 FirstPoseIdx = SearchIndexAsset.GetFirstPoseIdx();
					const int32 LastPoseIdx = FirstPoseIdx + SearchIndexAsset.GetNumPoses();
					for (int32 PoseIdx = FirstPoseIdx; PoseIdx < LastPoseIdx; ++PoseIdx)
					{
						EvaluatePoseKernel<true, false>(Result, SearchIndex, QueryValues, ReconstructedPoseValuesBuffer, PoseIdx, SearchFilters, SearchContext, this, bUpdateBestCandidates, ++ResultIndex);
					}
				}
			}
			// is the data padded at 16 bytes (and 16 bytes aligned by construction)?
			else if (NumDimensions % 4 == 0)
			{
				for (int32 AssetIdx : SelectableAssetIdx)
				{
					const FSearchIndexAsset& SearchIndexAsset = SearchIndex.Assets[AssetIdx];
					const int32 FirstPoseIdx = SearchIndexAsset.GetFirstPoseIdx();
					const int32 LastPoseIdx = FirstPoseIdx + SearchIndexAsset.GetNumPoses();
					for (int32 PoseIdx = FirstPoseIdx; PoseIdx < LastPoseIdx; ++PoseIdx)
					{
						EvaluatePoseKernel<false, true>(Result, SearchIndex, QueryValues, TArrayView<float>(), PoseIdx, SearchFilters, SearchContext, this, bUpdateBestCandidates, ++ResultIndex);
					}
				}
			}
			// no reconstruction, but data is not 16 bytes padded
			else
			{
				for (int32 AssetIdx : SelectableAssetIdx)
				{
					const FSearchIndexAsset& SearchIndexAsset = SearchIndex.Assets[AssetIdx];
					const int32 FirstPoseIdx = SearchIndexAsset.GetFirstPoseIdx();
					const int32 LastPoseIdx = FirstPoseIdx + SearchIndexAsset.GetNumPoses();
					for (int32 PoseIdx = FirstPoseIdx; PoseIdx < LastPoseIdx; ++PoseIdx)
					{
						EvaluatePoseKernel<false, false>(Result, SearchIndex, QueryValues, TArrayView<float>(), PoseIdx, SearchFilters, SearchContext, this, bUpdateBestCandidates, ++ResultIndex);
					}
				}
			}
		}
	}
	else
	{
#if UE_POSE_SEARCH_TRACE_ENABLED
		// calling just for reporting non selectable poses
		TConstArrayView<float> QueryValues = SearchContext.GetOrBuildQuery(Schema);
		FNonSelectableIdx NonSelectableIdx;
		PopulateNonSelectableIdx(NonSelectableIdx, SearchContext, this, QueryValues);
#endif // UE_POSE_SEARCH_TRACE_ENABLED
	}

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