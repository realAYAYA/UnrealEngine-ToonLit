// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "DataWrappers/ChaosVDQueryDataWrappers.h"
#include "ChaosVDSceneQueryDataComponent.generated.h"

struct FChaosVDGameFrameData;

/** Struct used to pass data about a specific query to other objects */
struct FChaosVDSceneQuerySelectionHandle
{
	FChaosVDSceneQuerySelectionHandle()
	{		
	}

	FChaosVDSceneQuerySelectionHandle(const TWeakPtr<FChaosVDQueryDataWrapper>& InQueryData, int32 SQVisitIndex)
		: QueryData(InQueryData)
		, SQVisitIndex(SQVisitIndex)
	{
		if (const TSharedPtr<FChaosVDQueryDataWrapper> QueryDataPtr = QueryData.Pin())
		{
			QueryID = QueryDataPtr->ID;
		}
	}

	void SetIsSelected(bool bNewSelected);
	bool IsSelected() const;

	TWeakPtr<FChaosVDQueryDataWrapper> GetQueryData() const { return QueryData; } 
	int32 GetQueryID() const { return QueryID; } 
	int32 GetSQVisitIndex() const { return SQVisitIndex; } 

private:
	TWeakPtr<FChaosVDQueryDataWrapper> QueryData = nullptr;
	int32 SQVisitIndex = INDEX_NONE;
	int32 QueryID = INDEX_NONE;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FChaosVDSQSelectionChangedDelegate, const FChaosVDSceneQuerySelectionHandle& SelectionHandle)

/** Actor Component that contains all the scene queries recorded at the current loaded frame */
UCLASS()
class CHAOSVD_API UChaosVDSceneQueryDataComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UChaosVDSceneQueryDataComponent();

	void UpdateQueriesFromFrameData(const FChaosVDGameFrameData& InGameFrameData);

	TConstArrayView<TSharedPtr<FChaosVDQueryDataWrapper>> GetQueriesByType(EChaosVDSceneQueryType Type) const;
	TConstArrayView<TSharedPtr<FChaosVDQueryDataWrapper>> GetAllQueries() const;
	TSharedPtr<FChaosVDQueryDataWrapper> GetQueryByID(int32 QueryID) const;

	void SelectQuery(int32 QueryID);
	void SelectQuery(const FChaosVDSceneQuerySelectionHandle& SelectionHandle);

	bool IsQuerySelected(int32 QueryID) const;

	FChaosVDSceneQuerySelectionHandle GetSelectedQueryHandle() const { return CurrentSQSelectionHandle; }

	FChaosVDSQSelectionChangedDelegate& GetOnSelectionChangeDelegate() { return SelectionChangeDelegate; }

protected:
	
	TMap<EChaosVDSceneQueryType, TArray<TSharedPtr<FChaosVDQueryDataWrapper>>> RecordedQueriesByType;
	TMap<int32, TSharedPtr<FChaosVDQueryDataWrapper>> RecordedQueriesByID;
	TArray<TSharedPtr<FChaosVDQueryDataWrapper>> RecordedQueries;
	
	FChaosVDSQSelectionChangedDelegate SelectionChangeDelegate;

	FChaosVDSceneQuerySelectionHandle CurrentSQSelectionHandle;
};
