// Copyright Epic Games, Inc. All Rights Reserved.

#include "Queries/TypedElementAlertQueries.h"

#include "Elements/Columns/TypedElementAlertColumns.h"
#include "Elements/Columns/TypedElementHiearchyColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementSelectionColumns.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "HAL/IConsoleManager.h"
#include "Math/UnrealMathUtility.h"

FAutoConsoleCommand AddRandomAlertToRowConsoleCommand(
	TEXT("TEDS.Debug.AddRandomAlertToSelectedRows"),
	TEXT("Add random alert to all selected rows that don't have one yet."),
	FConsoleCommandDelegate::CreateLambda(
		[]()
		{
			using namespace TypedElementDataStorage;
			using namespace TypedElementQueryBuilder;

			TRACE_CPUPROFILER_EVENT_SCOPE(TEDS.Debug.AddRandomAlertToSelectedRows);

			if (ITypedElementDataStorageInterface* DataStorage = UTypedElementRegistry::GetInstance()->GetMutableDataStorage())
			{
				static TypedElementQueryHandle Query = [DataStorage]
				{
					return DataStorage->RegisterQuery(
						Select()
						.Where()
							.All<FTypedElementSelectionColumn>()
							.None<FTypedElementAlertColumn>()
						.Compile());
				}();

				TArray<RowHandle> Rows;
				DataStorage->RunQuery(Query, CreateDirectQueryCallbackBinding(
					[&Rows](IDirectQueryContext& Context, RowHandle Row)
					{
						Rows.Add(Row);
					}));
				for (RowHandle Row : Rows)
				{
					bool bIsWarning = FMath::RandRange(0, 1) == 1;
					DataStorage->AddOrGetColumn(Row, FTypedElementAlertColumn
						{
							.Message = FText::FromString(bIsWarning ? TEXT("Test warning") : TEXT("Test error")),
							.AlertType = bIsWarning
								? FTypedElementAlertColumnType::Warning
								: FTypedElementAlertColumnType::Error
						});
					DataStorage->AddColumns<FTypedElementSyncBackToWorldTag>(Row);
				}
			}
		}
));

FAutoConsoleCommand ClearAllAlertsConsoleCommand(
	TEXT("TEDS.Debug.ClearAllAlertInfo"),
	TEXT("Removes all alerts and child alerts."),
	FConsoleCommandDelegate::CreateLambda(
		[]()
		{
			using namespace TypedElementDataStorage;
			using namespace TypedElementQueryBuilder;

			TRACE_CPUPROFILER_EVENT_SCOPE(TEDS.Debug.ClearAllAlertInfo);

			static TTypedElementColumnTypeList<FTypedElementSyncBackToWorldTag> BatchAddColumns;
			static TTypedElementColumnTypeList<FTypedElementAlertColumn, FTypedElementChildAlertColumn> BatchRemoveColumns;

			if (ITypedElementDataStorageInterface* DataStorage = UTypedElementRegistry::GetInstance()->GetMutableDataStorage())
			{
				static TypedElementQueryHandle AlertInfoQuery = [DataStorage]
				{
					return DataStorage->RegisterQuery(
						Select()
							.Where()
						.Any<FTypedElementAlertColumn, FTypedElementChildAlertColumn>()
							.Compile());
				}();
				TArray<RowHandle> Rows;
				DataStorage->RunQuery(AlertInfoQuery, CreateDirectQueryCallbackBinding(
					[&Rows](IDirectQueryContext& Context, RowHandle Row)
					{
						Rows.Add(Row);
					}));
				for (RowHandle Row : Rows)
				{
					DataStorage->RemoveColumn<FTypedElementAlertColumn>(Row);
					DataStorage->RemoveColumn<FTypedElementChildAlertColumn>(Row);
					DataStorage->AddColumn<FTypedElementSyncBackToWorldTag>(Row);
				}
			}
		}
));

FAutoConsoleCommand ClearSelectedAlertsConsoleCommand(
	TEXT("TEDS.Debug.ClearSelectedAlerts"),
	TEXT("Removes all selected alerts."),
	FConsoleCommandDelegate::CreateLambda(
		[]()
		{
			using namespace TypedElementDataStorage;
			using namespace TypedElementQueryBuilder;

			TRACE_CPUPROFILER_EVENT_SCOPE(TEDS.Debug.ClearSelectedAlerts);

			if (ITypedElementDataStorageInterface* DataStorage = UTypedElementRegistry::GetInstance()->GetMutableDataStorage())
			{
				static TypedElementQueryHandle AlertQuery = [DataStorage]
				{
					return DataStorage->RegisterQuery(
						Select()
						.Where()
							.All<FTypedElementAlertColumn, FTypedElementSelectionColumn>()
						.Compile());
				}();
				TArray<RowHandle> Rows;
				DataStorage->RunQuery(AlertQuery, CreateDirectQueryCallbackBinding(
					[&Rows](IDirectQueryContext& Context, RowHandle Row)
					{
						Rows.Add(Row);
					}));
				for (RowHandle Row : Rows)
				{
					DataStorage->RemoveColumn<FTypedElementAlertColumn>(Row);
					DataStorage->AddColumn<FTypedElementSyncBackToWorldTag>(Row);
				}
			}
		}
));

void UTypedElementAlertQueriesFactory::RegisterQueries(ITypedElementDataStorageInterface& DataStorage)
{
	RegisterSubQueries(DataStorage);
	
	RegisterUpdateAlertsQueries(DataStorage);
	
	RegisterOnAddQueries(DataStorage);
	RegisterOnAlertRemoveQueries(DataStorage);
	RegisterOnParentRemoveQueries(DataStorage);
}

void UTypedElementAlertQueriesFactory::RegisterSubQueries(ITypedElementDataStorageInterface& DataStorage)
{
	using namespace TypedElementQueryBuilder;
	
	ChildAlertColumnReadWriteQuery = DataStorage.RegisterQuery(
		Select()
			.ReadWrite<FTypedElementChildAlertColumn>()
		.Compile());

	ParentReadOnlyQuery = DataStorage.RegisterQuery(
		Select()
			.ReadOnly<FTypedElementParentColumn>()
		.Compile());
}

void UTypedElementAlertQueriesFactory::RegisterUpdateAlertsQueries(ITypedElementDataStorageInterface& DataStorage)
{
	using namespace TypedElementDataStorage;
	using namespace TypedElementQueryBuilder;

	DataStorage.RegisterQuery(
		Select(
			TEXT("Update alerts"),
			FProcessor(EQueryTickPhase::DuringPhysics, DataStorage.GetQueryTickGroupName(EQueryTickGroups::Default)),
			[](IQueryContext& Context, RowHandle Row, FTypedElementAlertColumn& Alert, const FTypedElementParentColumn& Parent)
			{
				Update<true, false>(Context, Row, &Alert, nullptr, Parent, 0, 1);
			})
		.Where()
			.Any<FTypedElementAlertUpdateTag, FTypedElementSyncFromWorldTag, FTypedElementSyncBackToWorldTag>()
			.None<FTypedElementChildAlertColumn>()
		.DependsOn()
			.SubQuery(ChildAlertColumnReadWriteQuery)
			.SubQuery(ParentReadOnlyQuery)
		.Compile());

	DataStorage.RegisterQuery(
		Select(
			TEXT("Update child alerts"),
			FProcessor(EQueryTickPhase::DuringPhysics, DataStorage.GetQueryTickGroupName(EQueryTickGroups::Default)),
			[](IQueryContext& Context, RowHandle Row, FTypedElementChildAlertColumn& ChildAlert, const FTypedElementParentColumn& Parent)
			{
				Update<false, true>(Context, Row, nullptr, &ChildAlert, Parent, 0, 1);
			})
		.Where()
			.Any<FTypedElementAlertUpdateTag, FTypedElementSyncFromWorldTag, FTypedElementSyncBackToWorldTag>()
			.None<FTypedElementAlertColumn>()
		.DependsOn()
			.SubQuery(ChildAlertColumnReadWriteQuery)
			.SubQuery(ParentReadOnlyQuery)
		.Compile());

	DataStorage.RegisterQuery(
		Select(
			TEXT("Update alerts and child alerts"),
			FProcessor(EQueryTickPhase::DuringPhysics, DataStorage.GetQueryTickGroupName(EQueryTickGroups::Default)),
			[](IQueryContext& Context, RowHandle Row, FTypedElementAlertColumn& Alert, FTypedElementChildAlertColumn& ChildAlert, 
				const FTypedElementParentColumn& Parent)
			{
				Update<true, true>(Context, Row, &Alert, &ChildAlert, Parent, 0, 1);
			})
		.Where()
			.Any<FTypedElementAlertUpdateTag, FTypedElementSyncFromWorldTag, FTypedElementSyncBackToWorldTag>()
		.DependsOn()
			.SubQuery(ChildAlertColumnReadWriteQuery)
			.SubQuery(ParentReadOnlyQuery)
		.Compile());
}

template<bool bHasAlert, bool bHasChildAlert>
void UTypedElementAlertQueriesFactory::Update(TypedElementDataStorage::IQueryContext& Context, TypedElementDataStorage::RowHandle Row,
	FTypedElementAlertColumn* Alert, FTypedElementChildAlertColumn* ChildAlert, const FTypedElementParentColumn& Parent,
	int32 ChildAlertColumnReadWriteSubquery, int32 ParentReadOnlySubquery)
{
	using namespace TypedElementDataStorage;
	using namespace TypedElementQueryBuilder;

	bool bAlertNeedsUpdating = false;
	bool bChildAlertsNeedUpdating = false;
	NeedsUpdating<bHasAlert, bHasChildAlert>(Alert, ChildAlert, Parent, bAlertNeedsUpdating, bChildAlertsNeedUpdating);

	bool bAlertNeedsDecrementing = false;
	bool bChildAlertsNeedDecrementing = false;
	NeedsDecrementing<bHasAlert, bHasChildAlert>(Alert, ChildAlert, Parent, bAlertNeedsDecrementing, bChildAlertsNeedDecrementing);

	if (bAlertNeedsUpdating || bChildAlertsNeedUpdating || bAlertNeedsDecrementing || bChildAlertsNeedDecrementing)
	{
		RowHandle CachedParentRow = GetParent<bHasAlert, bHasChildAlert>(Alert, ChildAlert);

		// If needed, first decrement the parents.
		if ((bAlertNeedsDecrementing || bChildAlertsNeedDecrementing) && Context.IsRowAvailable(CachedParentRow))
		{
			uint16 TotalErrorCounts = 0;
			uint16 TotalWarningCounts = 0;
			GetTotalCounts<bHasAlert, bHasChildAlert>(Alert, ChildAlert, TotalErrorCounts, TotalWarningCounts,
				bAlertNeedsDecrementing, bChildAlertsNeedDecrementing);

			if (TotalErrorCounts != 0 || TotalWarningCounts != 0)
			{
				DecrementParents(
					Context, CachedParentRow, TotalErrorCounts, TotalWarningCounts, ChildAlertColumnReadWriteSubquery);
				Context.AddColumns<FTypedElementSyncBackToWorldTag>(Row);

				// Prevent a second delete if there's a re-entry.
				MarkDecremented<bHasAlert, bHasChildAlert>(Alert, ChildAlert);
				
				// Columns that are needed may have been deleted, so wait a cycle to give them a change to process and be re-added.
				GuaranteeUpdateReentry(Context, Row);
				return;
			}
		}

		// Increment all the parents.
		RowHandle ParentRow = Parent.Parent;
		if (Context.IsRowAvailable(ParentRow))
		{
			if (Context.HasColumn<FTypedElementChildAlertColumn>(ParentRow))
			{
				// At this point all parents should have had a child alert column. It can be safely assumed that if a direct parent has
				// child alert column, that this is recursively true for its parents.
				uint16 TotalErrorCounts = 0;
				uint16 TotalWarningCounts = 0;
				GetTotalCounts<bHasAlert, bHasChildAlert>(Alert, ChildAlert, TotalErrorCounts, TotalWarningCounts, 
					bAlertNeedsUpdating, bChildAlertsNeedUpdating);
				IncrementParents(Context, ParentRow, TotalErrorCounts, TotalWarningCounts, ChildAlertColumnReadWriteSubquery);

				UpdateParent<bHasAlert, bHasChildAlert>(Alert, ChildAlert, ParentRow);
				Context.RemoveColumns<FTypedElementAlertUpdateTag>(Row);
			}
			else
			{
				// The parent doesn't have a child alert, so add it and continue in the next cycle so the column is added.
				AddChildAlertsToHierarchy(Context, ParentRow, ParentReadOnlySubquery);
				GuaranteeUpdateReentry(Context, Row);
			}
		}
	}
}

void UTypedElementAlertQueriesFactory::RegisterOnAddQueries(ITypedElementDataStorageInterface& DataStorage)
{
	using namespace TypedElementDataStorage;
	using namespace TypedElementQueryBuilder;
	
	DataStorage.RegisterQuery(
		Select(
			TEXT("Register alert with parent on alert add"),
			FObserver::OnAdd<FTypedElementAlertColumn>(),
			[](IQueryContext& Context, RowHandle Row)
			{
				Context.AddColumns<FTypedElementAlertUpdateTag>(Row);
			})
		.Where()
			.All<FTypedElementParentColumn>() // Only need to do an update pass if there are parents.
		.Compile());

	DataStorage.RegisterQuery(
		Select(
			TEXT("Register alert with parent on parent add"),
			FObserver::OnAdd<FTypedElementParentColumn>(),
			[](IQueryContext& Context, RowHandle Row)
			{
				Context.AddColumns<FTypedElementAlertUpdateTag>(Row);
			})
		.Where()
			.Any<FTypedElementAlertColumn, FTypedElementChildAlertColumn>()
		.Compile());
}

void UTypedElementAlertQueriesFactory::RegisterOnAlertRemoveQueries(ITypedElementDataStorageInterface& DataStorage)
{
	using namespace TypedElementDataStorage;
	using namespace TypedElementQueryBuilder;
	
	DataStorage.RegisterQuery(
		Select(
			TEXT("Remove alert"),
			FObserver::OnRemove<FTypedElementAlertColumn>(),
			[](IQueryContext& Context, TypedElementDataStorage::RowHandle Row, FTypedElementAlertColumn& Alert)
			{
				if (Alert.RemoveCycleId != Context.GetUpdateCycleId())
				{
					DecrementParents(Context, Alert.CachedParent,
						Alert.AlertType == FTypedElementAlertColumnType::Error ? 1 : 0,
						Alert.AlertType == FTypedElementAlertColumnType::Warning ? 1 : 0,
						0);
					Alert.CachedParent = InvalidRowHandle;
					Alert.RemoveCycleId = Context.GetUpdateCycleId();
					Context.AddColumns<FTypedElementSyncBackToWorldTag>(Row);
				}
			})
		.DependsOn()
			.SubQuery(ChildAlertColumnReadWriteQuery)
		.Compile());

	DataStorage.RegisterQuery(
		Select(
			TEXT("Remove child alert"),
			FObserver::OnRemove<FTypedElementChildAlertColumn>(),
			[](IQueryContext& Context, TypedElementDataStorage::RowHandle Row, FTypedElementChildAlertColumn& ChildAlert)
			{
				if (Context.GetUpdateCycleId() != ChildAlert.RemoveCycleId)
				{
					DecrementParents(Context, ChildAlert.CachedParent,
						ChildAlert.Counts[static_cast<size_t>(FTypedElementAlertColumnType::Error)],
						ChildAlert.Counts[static_cast<size_t>(FTypedElementAlertColumnType::Warning)],
						0);
					ChildAlert.RemoveCycleId = Context.GetUpdateCycleId();
					Context.AddColumns<FTypedElementSyncBackToWorldTag>(Row);
				}
			})
		.DependsOn()
			.SubQuery(ChildAlertColumnReadWriteQuery)
		.Compile());
}

void UTypedElementAlertQueriesFactory::RegisterOnParentRemoveQueries(ITypedElementDataStorageInterface& DataStorage)
{
	using namespace TypedElementDataStorage;
	using namespace TypedElementQueryBuilder;

	DataStorage.RegisterQuery(
		Select(
			TEXT("Update alert upon parent removal"),
			FObserver::OnRemove<FTypedElementParentColumn>(),
			[](IQueryContext& Context, TypedElementDataStorage::RowHandle Row, FTypedElementAlertColumn& Alert)
			{
				if (Alert.RemoveCycleId != Context.GetUpdateCycleId())
				{
					DecrementParents(Context, Alert.CachedParent,
						Alert.AlertType == FTypedElementAlertColumnType::Error ? 1 : 0,
						Alert.AlertType == FTypedElementAlertColumnType::Warning ? 1 : 0,
						0);
					Alert.CachedParent = InvalidRowHandle;
					Alert.RemoveCycleId = Context.GetUpdateCycleId();
					Context.AddColumns<FTypedElementSyncBackToWorldTag>(Row);
				}
			})
		.Where()
			.None<FTypedElementChildAlertColumn>()
		.DependsOn()
			.SubQuery(ChildAlertColumnReadWriteQuery)
		.Compile());

	DataStorage.RegisterQuery(
		Select(
			TEXT("Update child alert upon parent removal"),
			FObserver::OnRemove<FTypedElementParentColumn>(),
			[](IQueryContext& Context, TypedElementDataStorage::RowHandle Row, FTypedElementChildAlertColumn& ChildAlert)
			{
				if (Context.GetUpdateCycleId() != ChildAlert.RemoveCycleId)
				{
					DecrementParents(Context, ChildAlert.CachedParent,
						ChildAlert.Counts[static_cast<size_t>(FTypedElementAlertColumnType::Error)],
						ChildAlert.Counts[static_cast<size_t>(FTypedElementAlertColumnType::Warning)],
						0);
					ChildAlert.CachedParent = InvalidRowHandle;
					ChildAlert.RemoveCycleId = Context.GetUpdateCycleId();
					Context.AddColumns<FTypedElementSyncBackToWorldTag>(Row);
				}
			})
		.Where()
			.None<FTypedElementAlertColumn>()
		.DependsOn()
			.SubQuery(ChildAlertColumnReadWriteQuery)
		.Compile());

	DataStorage.RegisterQuery(
		Select(
			TEXT("Update alert and child alert upon parent removal"),
			FObserver::OnRemove<FTypedElementParentColumn>(),
			[](IQueryContext& Context, TypedElementDataStorage::RowHandle Row, FTypedElementAlertColumn& Alert, FTypedElementChildAlertColumn& ChildAlert)
			{
				uint64 UpdateCycleId = Context.GetUpdateCycleId();
				uint16 DecrementCounts[static_cast<size_t>(FTypedElementAlertColumnType::MAX)]{};
				bool bUpdate = false;

				if (Alert.RemoveCycleId != UpdateCycleId)
				{
					DecrementCounts[static_cast<size_t>(Alert.AlertType)]++;
					bUpdate = true;
				}
				if (Context.GetUpdateCycleId() != ChildAlert.RemoveCycleId)
				{
					for (size_t Counter = 0; Counter < static_cast<size_t>(FTypedElementAlertColumnType::MAX); ++Counter)
					{
						DecrementCounts[Counter] += ChildAlert.Counts[Counter];
					}
					bUpdate = true;
				}

				if (bUpdate)
				{
					DecrementParents(Context, ChildAlert.CachedParent,
						DecrementCounts[static_cast<size_t>(FTypedElementAlertColumnType::Error)],
						DecrementCounts[static_cast<size_t>(FTypedElementAlertColumnType::Warning)],
						0);
					Alert.CachedParent = InvalidRowHandle;
					Alert.RemoveCycleId = UpdateCycleId;
					ChildAlert.CachedParent = InvalidRowHandle;
					ChildAlert.RemoveCycleId = UpdateCycleId;
				}
			})
		.DependsOn()
			.SubQuery(ChildAlertColumnReadWriteQuery)
		.Compile());
}

template<bool bHasAlert, bool bHasChildAlert>
void UTypedElementAlertQueriesFactory::NeedsUpdating(
	FTypedElementAlertColumn* Alert, FTypedElementChildAlertColumn* ChildAlert, const FTypedElementParentColumn& Parent,
	bool& bAlertNeedsUpdate, bool& bChildAlertsNeedUpdate)
{
	using namespace TypedElementDataStorage;

	if constexpr (bHasAlert)
	{
		bAlertNeedsUpdate = Alert->CachedParent != Parent.Parent;
	}

	if constexpr (bHasChildAlert)
	{
		bChildAlertsNeedUpdate = ChildAlert->CachedParent != Parent.Parent;
	}
}

template<bool bHasAlert, bool bHasChildAlert>
void UTypedElementAlertQueriesFactory::NeedsDecrementing(FTypedElementAlertColumn* Alert, FTypedElementChildAlertColumn* ChildAlert,
	const FTypedElementParentColumn& Parent, bool& bAlertNeedsDecrementing, bool& bChildAlertsNeedDecrementing)
{
	using namespace TypedElementDataStorage;

	if constexpr (bHasAlert)
	{
		bAlertNeedsDecrementing = Alert->CachedParent != 0 && Alert->CachedParent != InvalidRowHandle;
	}

	if constexpr (bHasChildAlert)
	{
		bChildAlertsNeedDecrementing = !ChildAlert->bHasDecremented && ChildAlert->CachedParent != Parent.Parent;
	}
}

template<bool bHasAlert, bool bHasChildAlert>
TypedElementDataStorage::RowHandle UTypedElementAlertQueriesFactory::GetParent(
	FTypedElementAlertColumn* Alert, FTypedElementChildAlertColumn* ChildAlert)
{
	using namespace TypedElementDataStorage;

	RowHandle Result = InvalidRowHandle;
	if constexpr (bHasAlert)
	{
		Result = Alert->CachedParent;
	}
	if constexpr (bHasChildAlert)
	{
		if constexpr (bHasAlert)
		{
			checkf(Result == 0 || Result == InvalidRowHandle || Result == ChildAlert->CachedParent,
				TEXT("Alert and Child Alert are not pointing at the same parent."));

		}
		Result = ChildAlert->CachedParent;
	}
	return Result;
}

template<bool bHasAlert, bool bHasChildAlert>
void UTypedElementAlertQueriesFactory::GetTotalCounts(FTypedElementAlertColumn* Alert, FTypedElementChildAlertColumn* ChildAlert,
	uint16& ErrorCount, uint16& WarningCount, bool IncludeAlert, bool IncludeChildAlerts)
{
	if constexpr (bHasAlert)
	{
		switch (Alert->AlertType)
		{
		case FTypedElementAlertColumnType::Error:
			ErrorCount += IncludeAlert ? 1 : 0;
			break;
		case FTypedElementAlertColumnType::Warning:
			WarningCount += IncludeAlert ? 1 : 0;
			break;
		default:
			checkf(false, TEXT("Unexpected alert type %i"), static_cast<int>(Alert->AlertType));
		}
	}
	if constexpr (bHasChildAlert)
	{
		ErrorCount += IncludeChildAlerts ? ChildAlert->Counts[static_cast<size_t>(FTypedElementAlertColumnType::Error)] : 0;
		WarningCount += IncludeChildAlerts ? ChildAlert->Counts[static_cast<size_t>(FTypedElementAlertColumnType::Warning)] : 0;
	}
}

template<bool bHasAlert, bool bHasChildAlert>
void UTypedElementAlertQueriesFactory::MarkDecremented(FTypedElementAlertColumn* Alert, FTypedElementChildAlertColumn* ChildAlert)
{
	using namespace TypedElementDataStorage;

	if constexpr (bHasAlert)
	{
		Alert->CachedParent = InvalidRowHandle;
	}
	if constexpr (bHasChildAlert)
	{
		ChildAlert->bHasDecremented = true;
	}
}

void UTypedElementAlertQueriesFactory::GuaranteeUpdateReentry(
	TypedElementDataStorage::IQueryContext& Context, TypedElementDataStorage::RowHandle Row)
{
	// If not completing a full pass, add this tag to make sure a follow up pass happens. This is needed
	// in case this update was triggered from a sync to/from external.
	if (!Context.HasColumn<FTypedElementAlertUpdateTag>())
	{
		Context.AddColumns<FTypedElementAlertUpdateTag>(Row);
	}
}

template<bool bHasAlert, bool bHasChildAlert>
void UTypedElementAlertQueriesFactory::UpdateParent(FTypedElementAlertColumn* Alert, FTypedElementChildAlertColumn* ChildAlert,
	TypedElementDataStorage::RowHandle NewParent)
{
	if constexpr (bHasAlert)
	{
		Alert->CachedParent = NewParent;
	}
	if constexpr (bHasChildAlert)
	{
		ChildAlert->CachedParent = NewParent;
		ChildAlert->bHasDecremented = false;
	}
}

void UTypedElementAlertQueriesFactory::AddChildAlertsToHierarchy(
	TypedElementDataStorage::IQueryContext& Context, TypedElementDataStorage::RowHandle Parent, int32 ParentQueryIndex)
{
	using namespace TypedElementDataStorage;

	bool bHasParent = true;
	while (bHasParent)
	{
		RowHandle NextParent = Parent;
		bHasParent = MoveToNextParent(NextParent, Context, ParentQueryIndex);

		// Increment parent's child counter or create a child alert if one doesn't exist.
		if (!Context.HasColumn<FTypedElementChildAlertColumn>(Parent))
		{
			FTypedElementChildAlertColumn ChildAlert;
			ChildAlert.CachedParent = NextParent;
			for (uint8 Index = 0; Index < static_cast<uint8>(FTypedElementAlertColumnType::MAX); ++Index)
			{
				ChildAlert.Counts[Index] = 0;
			}
			ChildAlert.RemoveCycleId = Context.GetUpdateCycleId();
			Context.AddColumn(Parent, MoveTemp(ChildAlert));
		}

		Parent = NextParent;
	}
}

void UTypedElementAlertQueriesFactory::IncrementParents(
	TypedElementDataStorage::IQueryContext& Context, TypedElementDataStorage::RowHandle ParentRow,
	uint16 ErrorIncrement, uint16 WarningIncrement,
	int32 ChildAlertQueryIndex)
{
	using namespace TypedElementDataStorage;
	using namespace TypedElementQueryBuilder;
	
	// Assume that if the immediate parent has a child alert, then all of those parent's parents have a child alert as well.
	while (true)
	{
		// Increment counter on the parent child alert.
		FQueryResult Result = Context.RunSubquery(ChildAlertQueryIndex, ParentRow, CreateSubqueryCallbackBinding(
			[&ParentRow, ErrorIncrement, WarningIncrement](
				ISubqueryContext& Context, RowHandle Row, FTypedElementChildAlertColumn& ChildAlert)
			{
				ChildAlert.Counts[static_cast<size_t>(FTypedElementAlertColumnType::Error)] += ErrorIncrement;
				ChildAlert.Counts[static_cast<size_t>(FTypedElementAlertColumnType::Warning)] += WarningIncrement;
				ParentRow = ChildAlert.CachedParent;
				Context.AddColumns<FTypedElementSyncBackToWorldTag>(Row);
			}));
		checkf(Result.Count > 0, TEXT("Expected to be able to setup the child alert, but it was missing on the parent column."));
		if (Result.Count == 0 || !Context.IsRowAvailable(ParentRow))
		{
			break;
		}
	}
}

void UTypedElementAlertQueriesFactory::DecrementParents(
	TypedElementDataStorage::IQueryContext& Context, TypedElementDataStorage::RowHandle ParentRow,
	uint16 ErrorDecrement, uint16 WarningDecrement,
	int32 ChildAlertQueryIndex)
{
	using namespace TypedElementDataStorage;
	using namespace TypedElementQueryBuilder;

	while (true)
	{
		FQueryResult Result = Context.RunSubquery(ChildAlertQueryIndex, ParentRow, CreateSubqueryCallbackBinding(
			[&ParentRow, ErrorDecrement, WarningDecrement](
				ISubqueryContext& Context, RowHandle Row, FTypedElementChildAlertColumn& ChildAlert)
			{
				ChildAlert.Counts[static_cast<size_t>(FTypedElementAlertColumnType::Error)] -= ErrorDecrement;
				ChildAlert.Counts[static_cast<size_t>(FTypedElementAlertColumnType::Warning)] -= WarningDecrement;
				if (ChildAlert.Counts[static_cast<size_t>(FTypedElementAlertColumnType::Error)] +
					ChildAlert.Counts[static_cast<size_t>(FTypedElementAlertColumnType::Warning)] == 0)
				{
					Context.RemoveColumns<FTypedElementChildAlertColumn>(Row);
				}
				ParentRow = ChildAlert.CachedParent;
				Context.AddColumns<FTypedElementSyncBackToWorldTag>(Row);
			}));
		if (Result.Count == 0 || !Context.IsRowAvailable(ParentRow))
		{
			break;
		}
	}
}

bool UTypedElementAlertQueriesFactory::MoveToNextParent(
	TypedElementDataStorage::RowHandle& Parent, TypedElementDataStorage::IQueryContext& Context, int32 SubQueryIndex)
{
	using namespace TypedElementDataStorage;
	using namespace TypedElementQueryBuilder;
	
	FQueryResult Result = Context.RunSubquery(SubQueryIndex, Parent, CreateSubqueryCallbackBinding(
		[&Parent](const FTypedElementParentColumn& NextParent)
		{
			Parent = NextParent.Parent;
		}));
	Parent = Result.Count != 0 ? Parent : InvalidRowHandle;
	return Result.Count != 0;
}



//
// UTypedElementAlertColumnMementoTranslator
//

const UScriptStruct* UTypedElementAlertColumnMementoTranslator::GetColumnType() const
{ 
	return FTypedElementAlertColumn::StaticStruct();
}