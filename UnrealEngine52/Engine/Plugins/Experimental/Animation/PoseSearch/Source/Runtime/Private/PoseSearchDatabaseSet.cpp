// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchDatabaseSet.h"
#include "PoseSearch/PoseSearchContext.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "PoseSearch/PoseSearchSchema.h"

UE::PoseSearch::FSearchResult UPoseSearchDatabaseSet::Search(UE::PoseSearch::FSearchContext& SearchContext) const
{
	using namespace UE::PoseSearch;

	FSearchResult Result;
	FPoseSearchCost ContinuingCost;
#if WITH_EDITOR
	FPoseSearchCost BruteForcePoseCost;
#endif

	// evaluating the continuing pose before all the active entries
	const UPoseSearchDatabase* Database = SearchContext.CurrentResult.Database.Get();
	if (bEvaluateContinuingPoseFirst &&
		SearchContext.bCanAdvance &&
		!SearchContext.bForceInterrupt &&
		SearchContext.CurrentResult.IsValid()
#if WITH_EDITOR
		&& FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(Database, ERequestAsyncBuildFlag::ContinueRequest)
#endif
		)
	{
		check(Database);
		const FPoseSearchIndex& SearchIndex = Database->GetSearchIndex();
		SearchContext.GetOrBuildQuery(Database, Result.ComposedQuery);

		TConstArrayView<float> QueryValues = Result.ComposedQuery.GetValues();

		Result.PoseIdx = SearchContext.CurrentResult.PoseIdx;
		Result.PoseCost = SearchIndex.ComparePoses(Result.PoseIdx, SearchContext.QueryMirrorRequest, EPoseComparisonFlags::ContinuingPose, Database->Schema->MirrorMismatchCostBias, QueryValues);
		Result.ContinuingPoseCost = Result.PoseCost;
		ContinuingCost = Result.PoseCost;

		Result.AssetTime = SearchIndex.GetAssetTime(Result.PoseIdx, Database->Schema->GetSamplingInterval());
		Result.Database = Database;

		if (Database->GetSkipSearchIfPossible())
		{
			SearchContext.UpdateCurrentBestCost(Result.PoseCost);
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
