// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementOutlinerHierarchy.h"

#include "MassActorSubsystem.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementQueryStorageInterfaces.h"
#include "TypedElementOutlinerItem.h"
#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Columns/TypedElementHiearchyColumns.h"
#include "Elements/Columns/TypedElementTypeInfoColumns.h"

FTypedElementOutlinerHierarchy::FTypedElementOutlinerHierarchy(FTypedElementOutlinerMode* InMode, TypedElementDataStorage::FQueryDescription InInitialQueryDescription)
: ISceneOutlinerHierarchy(InMode)
, TEDSOutlinerMode(InMode)
, InitialQueryDescription(MoveTemp(InInitialQueryDescription))
{
	RecompileQueries();
}

FTypedElementOutlinerHierarchy::~FTypedElementOutlinerHierarchy()
{
	UnregisterQueries();
}

void FTypedElementOutlinerHierarchy::CreateItems(TArray<FSceneOutlinerTreeItemPtr>& OutItems) const
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;

	ITypedElementDataStorageInterface* DataStorage = TEDSOutlinerMode->GetStorage();
	
	TypedElementDataStorage::DirectQueryCallback RowCollector = CreateDirectQueryCallbackBinding(
		[this, &OutItems](DSI::IDirectQueryContext& Context)
		{
			TConstArrayView<TypedElementRowHandle> Rows = Context.GetRowHandles();
			CreateItems_Internal(Rows, OutItems);
		});

	DataStorage->RunQuery(RowHandleQuery, RowCollector);
}

void FTypedElementOutlinerHierarchy::CreateItems_Internal(TConstArrayView<TypedElementRowHandle>& Rows, TArray<FSceneOutlinerTreeItemPtr>& OutItems) const
{
	for(const TypedElementRowHandle& Row : Rows)
	{
		if (FSceneOutlinerTreeItemPtr TreeItem = Mode->CreateItemFor<FTypedElementOutlinerTreeItem>(FTypedElementOutlinerTreeItem(Row, *TEDSOutlinerMode), false))
		{
			OutItems.Add(TreeItem);
		}
	}
}

void FTypedElementOutlinerHierarchy::CreateChildren(const FSceneOutlinerTreeItemPtr& Item,
	TArray<FSceneOutlinerTreeItemPtr>& OutChildren) const
{
	/* TEDS-Outliner TODO: This can probably be improved or optimized in the future
	 * 
	 * TEDS currently only supports lookup for parents using the FTypedElementParentColumn, so to get the children
	 * for a given row we currently have to go through every row (that matches our populate query) with a parent column to check if the parent
	 * is our row.
	 * This has to be done recursively to grab our children, grandchildren and so on...
	 */

	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;
	
	const FTypedElementOutlinerTreeItem* TEDSTreeItem = Item->CastTo<FTypedElementOutlinerTreeItem>();

	// If this item is not a TEDS item, we are not handling it
	if(!TEDSTreeItem)
	{
		return;
	}
		
	TypedElementRowHandle ItemRowHandle = TEDSTreeItem->GetRowHandle();
	ITypedElementDataStorageInterface* DataStorage = TEDSOutlinerMode->GetStorage();

	if(!DataStorage->HasRowBeenAssigned(ItemRowHandle))
	{
		return;
	}


	TSet<TypedElementDataStorage::RowHandle> MatchedRowsWithParentColumn;
	
	// Collect all entities that are owned by our entity
	TypedElementDataStorage::DirectQueryCallback ChildRowCollector = CreateDirectQueryCallbackBinding(
	[&MatchedRowsWithParentColumn] (const DSI::IDirectQueryContext& Context)
	{
		MatchedRowsWithParentColumn.Append(Context.GetRowHandles());
	});

	DataStorage->RunQuery(ChildRowHandleQuery, ChildRowCollector);

	TArray<TypedElementDataStorage::RowHandle> ChildItems;

	// Recursively get the children for each entity
	TFunction<void(TypedElementRowHandle)> GetChildrenRecursive = [&ChildItems, &MatchedRowsWithParentColumn, DataStorage, &GetChildrenRecursive](TypedElementRowHandle EntityRowHandle) -> void
	{
		for(TypedElementRowHandle ChildEntityRowHandle : MatchedRowsWithParentColumn)
		{
			// We should always have a parent column since we only grabbed rows with those
			const FTypedElementParentColumn* ParentColumn = DataStorage->GetColumn<FTypedElementParentColumn>(ChildEntityRowHandle);
				
			// Check if this entity is owned by the entity we are looking children for
			if(ParentColumn->Parent == EntityRowHandle)
			{
				ChildItems.Add(ChildEntityRowHandle);

				// Recursively look for children of this item
				GetChildrenRecursive(ChildEntityRowHandle);
			}
		}
	};
		
	GetChildrenRecursive(ItemRowHandle);

	// Actually create the items for the child entities 
	for (TypedElementRowHandle ChildItemRowHandle : ChildItems)
	{
		if (FSceneOutlinerTreeItemPtr ChildActorItem = Mode->CreateItemFor<FTypedElementOutlinerTreeItem>(FTypedElementOutlinerTreeItem(ChildItemRowHandle, *TEDSOutlinerMode)))
		{
			OutChildren.Add(ChildActorItem);
		}
	}

}

FSceneOutlinerTreeItemPtr FTypedElementOutlinerHierarchy::FindOrCreateParentItem(const ISceneOutlinerTreeItem& Item,
	const TMap<FSceneOutlinerTreeItemID, FSceneOutlinerTreeItemPtr>& Items, bool bCreate)
{
	using namespace TypedElementDataStorage;

	const FTypedElementOutlinerTreeItem* TEDSTreeItem = Item.CastTo<FTypedElementOutlinerTreeItem>();

	// If this item is not a TEDS item, we are not handling it
	if(!TEDSTreeItem)
	{
		return nullptr;
	}
		
	TypedElementRowHandle ItemRowHandle = TEDSTreeItem->GetRowHandle();
	ITypedElementDataStorageInterface* DataStorage = TEDSOutlinerMode->GetStorage();

	// If this entity does not have a parent entity, return nullptr
	FTypedElementParentColumn* ParentColumn = DataStorage->GetColumn<FTypedElementParentColumn>(ItemRowHandle);
	if(!ParentColumn)
	{
		return nullptr;
	}

	// If the parent is invalid for some reason, return nullptr
	TypedElementRowHandle ParentRowHandle = ParentColumn->Parent;
	if(ParentRowHandle == InvalidRowHandle)
	{
		return nullptr;
	}

	if (const FSceneOutlinerTreeItemPtr* ParentItem = Items.Find(ParentRowHandle))
	{
		return *ParentItem;
	}
	else if(bCreate)
	{
		return Mode->CreateItemFor<FTypedElementOutlinerTreeItem>(FTypedElementOutlinerTreeItem(ParentRowHandle, *TEDSOutlinerMode), true);
	}

	return nullptr;
}

void FTypedElementOutlinerHierarchy::OnItemAdded(TypedElementRowHandle ItemRowHandle)
{
	FSceneOutlinerHierarchyChangedData EventData;
	EventData.Type = FSceneOutlinerHierarchyChangedData::Added;
	EventData.Items.Add(Mode->CreateItemFor<FTypedElementOutlinerTreeItem>(FTypedElementOutlinerTreeItem(ItemRowHandle, *TEDSOutlinerMode)));
	HierarchyChangedEvent.Broadcast(EventData);

}

void FTypedElementOutlinerHierarchy::OnItemRemoved(TypedElementRowHandle ItemRowHandle)
{
	FSceneOutlinerHierarchyChangedData EventData;
	EventData.Type = FSceneOutlinerHierarchyChangedData::Removed;
	EventData.ItemIDs.Add(ItemRowHandle);
	HierarchyChangedEvent.Broadcast(EventData);
}

void FTypedElementOutlinerHierarchy::OnItemMoved(TypedElementRowHandle ItemRowHandle)
{
	FSceneOutlinerHierarchyChangedData EventData;
	EventData.Type = FSceneOutlinerHierarchyChangedData::Moved;
	EventData.ItemIDs.Add(ItemRowHandle);
	HierarchyChangedEvent.Broadcast(EventData);
}

void FTypedElementOutlinerHierarchy::RecompileQueries()
{
	using namespace TypedElementQueryBuilder;
	using namespace TypedElementDataStorage;

	UnregisterQueries();

	// Our final query to collect rows to populate the Outliner - currently the same as the initial query the user provided
	FQueryDescription FinalQueryDescription(InitialQueryDescription);

	// Add the filters the user has active to the query
	TEDSOutlinerMode->AppendExternalQueries(FinalQueryDescription);

	// Row to track addition of rows to the Outliner
	FQueryDescription RowAdditionQueryDescription =
		Select(
				TEXT("Add Row to Outliner"),
				FObserver::OnAdd<FTypedElementClassTypeInfoColumn>().ForceToGameThread(true),
				[this](IQueryContext& Context, TypedElementRowHandle Row)
				{
					OnItemAdded(Row);
				})
		.Where()
			.All<FTypedElementUObjectColumn>()
			.Compile();

	// Add the conditions from FinalQueryDescription to ensure we are tracking addition of the rows the user requested
	TEDSOutlinerMode->AppendQuery(RowAdditionQueryDescription, FinalQueryDescription);
	
	// Row to track removal of rows from the Outliner
	FQueryDescription RowRemovalQueryDescription =
		Select(
				TEXT("Remove Row from Outliner"),
				FObserver::OnRemove<FTypedElementClassTypeInfoColumn>().ForceToGameThread(true),
				[this](IQueryContext& Context, TypedElementRowHandle Row)
				{
					OnItemRemoved(Row);
				})
		.Where()
			.All<FTypedElementUObjectColumn>()
			.Compile();

	// Add the conditions from FinalQueryDescription to ensure we are tracking removal of the rows the user requested
	TEDSOutlinerMode->AppendQuery(RowRemovalQueryDescription, FinalQueryDescription);

	// Query to get all rows that match our conditions with a parent column (i.e all child rows)
	FQueryDescription ChildHandleQueryDescription =
						Select()
						.Where()
							.All<FTypedElementParentColumn>()
						.Compile();

	// Add the conditions from FinalQueryDescription to ensure we are tracking removal of the rows the user requested
	TEDSOutlinerMode->AppendQuery(ChildHandleQueryDescription, FinalQueryDescription);

	// Query to track when a row gets added a parent column
	FQueryDescription ParentAddedQueryDescription =
						Select(
						TEXT("Parent Attached"),
						FObserver::OnAdd<FTypedElementParentColumn>().ForceToGameThread(true),
						[this](IQueryContext& Context, TypedElementRowHandle Row)
						{
							OnItemMoved(Row);
						})
						.Where()
							.All<FTypedElementUObjectColumn>()
						.Compile();

	// Add the conditions from FinalQueryDescription to ensure we are the rows the user requested
	TEDSOutlinerMode->AppendQuery(ParentAddedQueryDescription, FinalQueryDescription);

	// Query to track when a row gets added a parent column
	FQueryDescription ParentRemovedQueryDescription =
						Select(
						TEXT("Parent Detached"),
						FObserver::OnRemove<FTypedElementParentColumn>().ForceToGameThread(true),
						[this](IQueryContext& Context, TypedElementRowHandle Row)
						{
							OnItemMoved(Row);
						})
						.Where()
							.All<FTypedElementUObjectColumn>()
						.Compile();

	// Add the conditions from FinalQueryDescription to ensure we are the rows the user requested
	TEDSOutlinerMode->AppendQuery(ParentRemovedQueryDescription, FinalQueryDescription);

	ITypedElementDataStorageInterface* DataStorage = TEDSOutlinerMode->GetStorage();

	RowHandleQuery = DataStorage->RegisterQuery(MoveTemp(FinalQueryDescription));
	RowAdditionQuery = DataStorage->RegisterQuery(MoveTemp(RowAdditionQueryDescription));
	RowRemovalQuery = DataStorage->RegisterQuery(MoveTemp(RowRemovalQueryDescription));
	ChildRowHandleQuery = DataStorage->RegisterQuery(MoveTemp(ChildHandleQueryDescription));
	ParentAddedQuery = DataStorage->RegisterQuery(MoveTemp(ParentAddedQueryDescription));
	ParentRemovedQuery = DataStorage->RegisterQuery(MoveTemp(ParentRemovedQueryDescription));

	// Sync the row handle query with the mode
	TEDSOutlinerMode->SetRowHandleQuery(RowHandleQuery);
}

void FTypedElementOutlinerHierarchy::UnregisterQueries()
{
	ITypedElementDataStorageInterface* DataStorage = TEDSOutlinerMode->GetStorage();

	DataStorage->UnregisterQuery(RowHandleQuery);
	DataStorage->UnregisterQuery(RowAdditionQuery);
	DataStorage->UnregisterQuery(RowRemovalQuery);
	DataStorage->UnregisterQuery(ChildRowHandleQuery);
	DataStorage->UnregisterQuery(ParentAddedQuery);
	DataStorage->UnregisterQuery(ParentRemovedQuery);
}