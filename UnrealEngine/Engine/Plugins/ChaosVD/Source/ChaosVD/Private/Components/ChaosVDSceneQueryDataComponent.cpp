// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/ChaosVDSceneQueryDataComponent.h"

#include "ChaosVDRecording.h"

void FChaosVDSceneQuerySelectionHandle::SetIsSelected(bool bNewSelected)
{
	if (const TSharedPtr<FChaosVDQueryDataWrapper> QueryDataPtr = QueryData.Pin())
	{
		QueryDataPtr->bIsSelectedInEditor = bNewSelected;
		if (QueryDataPtr && QueryDataPtr->SQVisitData.IsValidIndex(SQVisitIndex))
		{
			QueryDataPtr->SQVisitData[SQVisitIndex].bIsSelectedInEditor = bNewSelected;
		}
		else if (QueryDataPtr)
		{
			// If we end up with a invalid index, make sure that no visit is selected
			SQVisitIndex = INDEX_NONE;
			for (FChaosVDQueryVisitStep& VisitData : QueryDataPtr->SQVisitData)
			{
				VisitData.bIsSelectedInEditor = false;
			}
		}
	}
}

bool FChaosVDSceneQuerySelectionHandle::IsSelected() const
{
	if (const TSharedPtr<FChaosVDQueryDataWrapper> QueryDataPtr = QueryData.Pin())
	{
		return QueryDataPtr->bIsSelectedInEditor;
	}

	return false;
}

UChaosVDSceneQueryDataComponent::UChaosVDSceneQueryDataComponent() : CurrentSQSelectionHandle({nullptr, INDEX_NONE})
{
	PrimaryComponentTick.bCanEverTick = false;
	
	SetCanEverAffectNavigation(false);
	bNavigationRelevant = false;
}

void UChaosVDSceneQueryDataComponent::UpdateQueriesFromFrameData(const FChaosVDGameFrameData& InGameFrameData)
{
	const int32 RecordedQueriesNum = InGameFrameData.RecordedSceneQueries.Num();

	RecordedQueriesByType.Empty(RecordedQueriesNum);
	RecordedQueriesByID.Empty(RecordedQueriesNum);
	RecordedQueries.Empty(RecordedQueriesNum);

	// Until we have a way to track and auto-select new queries instances between frames, just clear the selection
	SelectQuery(FChaosVDSceneQuerySelectionHandle());

	for (const TPair<int32, TSharedPtr<FChaosVDQueryDataWrapper>>& QueryIDPair : InGameFrameData.RecordedSceneQueries)
	{
		if (TSharedPtr<FChaosVDQueryDataWrapper> QueryData = QueryIDPair.Value)
		{
			TArray<TSharedPtr<FChaosVDQueryDataWrapper>>& QueriesForType = RecordedQueriesByType.FindOrAdd(QueryData->Type);
			QueriesForType.Emplace(QueryData);

			RecordedQueriesByID.Add(QueryIDPair.Key, QueryData);
			RecordedQueries.Add(QueryData);
		}
	}
}

TConstArrayView<TSharedPtr<FChaosVDQueryDataWrapper>> UChaosVDSceneQueryDataComponent::GetQueriesByType(EChaosVDSceneQueryType Type) const
{
	if (const TArray<TSharedPtr<FChaosVDQueryDataWrapper>>* FoundQueries = RecordedQueriesByType.Find(Type))
	{
		return MakeArrayView(*FoundQueries);
	}

	return TArrayView<TSharedPtr<FChaosVDQueryDataWrapper>>();
}

TConstArrayView<TSharedPtr<FChaosVDQueryDataWrapper>> UChaosVDSceneQueryDataComponent::GetAllQueries() const
{
	return RecordedQueries;
}

TSharedPtr<FChaosVDQueryDataWrapper> UChaosVDSceneQueryDataComponent::GetQueryByID(int32 QueryID) const
{
	if (const TSharedPtr<FChaosVDQueryDataWrapper>* FoundQuery = RecordedQueriesByID.Find(QueryID))
	{
		return *FoundQuery;
	}

	return nullptr;
}

void UChaosVDSceneQueryDataComponent::SelectQuery(int32 QueryID)
{
	CurrentSQSelectionHandle.SetIsSelected(false);
	CurrentSQSelectionHandle = FChaosVDSceneQuerySelectionHandle(GetQueryByID(QueryID), 0);
	CurrentSQSelectionHandle.SetIsSelected(true);
	
	SelectionChangeDelegate.Broadcast(CurrentSQSelectionHandle);
}

void UChaosVDSceneQueryDataComponent::SelectQuery(const FChaosVDSceneQuerySelectionHandle& SelectionHandle)
{
	CurrentSQSelectionHandle.SetIsSelected(false);
	CurrentSQSelectionHandle = SelectionHandle;
	CurrentSQSelectionHandle.SetIsSelected(true);
	
	SelectionChangeDelegate.Broadcast(CurrentSQSelectionHandle);
}

bool UChaosVDSceneQueryDataComponent::IsQuerySelected(int32 QueryID) const
{
	if (const TSharedPtr<FChaosVDQueryDataWrapper> QueryDataPtr = CurrentSQSelectionHandle.GetQueryData().Pin())
	{
		return QueryDataPtr->ID == QueryID;
	}

	return false;
}
