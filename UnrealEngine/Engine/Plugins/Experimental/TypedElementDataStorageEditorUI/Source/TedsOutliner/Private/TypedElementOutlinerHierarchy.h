// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISceneOutlinerHierarchy.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "TypedElementOutlinerMode.h"

/*
 * Class that keeps track of hierarchy data and creates items using the given TEDS queries.
 * See CreateGenericTEDSOutliner()
 * Inherits from ISceneOutlinerHierarchy, which is responsible for creating the items you want to populate the Outliner
 * with and establishing hierarchical relationships between the items
 */
class FTypedElementOutlinerHierarchy : public ISceneOutlinerHierarchy
{
public:
	FTypedElementOutlinerHierarchy(FTypedElementOutlinerMode* InMode, TypedElementDataStorage::FQueryDescription InInitialQueryDescription);
	virtual ~FTypedElementOutlinerHierarchy() override;
			
	/** Create a linearization of all applicable items in the hierarchy */
	virtual void CreateItems(TArray<FSceneOutlinerTreeItemPtr>& OutItems) const;
	/** Create a linearization of all direct and indirect children of a given item in the hierarchy */
	virtual void CreateChildren(const FSceneOutlinerTreeItemPtr& Item, TArray<FSceneOutlinerTreeItemPtr>& OutChildren) const;
	/** Find or optionally create a parent item for a given tree item */
	virtual FSceneOutlinerTreeItemPtr FindOrCreateParentItem(const ISceneOutlinerTreeItem& Item, const TMap<FSceneOutlinerTreeItemID, FSceneOutlinerTreeItemPtr>& Items, bool bCreate = false);

protected:

	void CreateItems_Internal(TConstArrayView<TypedElementRowHandle>& Rows, TArray<FSceneOutlinerTreeItemPtr>& OutItems) const;

	void OnItemAdded(TypedElementRowHandle ItemRowHandle);
	void OnItemRemoved(TypedElementRowHandle ItemRowHandle);
	void OnItemMoved(TypedElementRowHandle ItemRowHandle);

	void RecompileQueries();
	void UnregisterQueries();
	
protected:

	FTypedElementOutlinerMode* TEDSOutlinerMode;

	// Initial query the user requested
	TypedElementDataStorage::FQueryDescription InitialQueryDescription;

	// Querys to track row handle collection, addition and removal
	TypedElementDataStorage::QueryHandle RowHandleQuery;
	TypedElementDataStorage::QueryHandle RowAdditionQuery;
	TypedElementDataStorage::QueryHandle RowRemovalQuery;

	// Query to get all child rows
	TypedElementDataStorage::QueryHandle ChildRowHandleQuery;

	// Query to track when a row gets added the parent column
	TypedElementDataStorage::QueryHandle ParentAddedQuery;

	// Query to track when a row gets removed the parent column
	TypedElementDataStorage::QueryHandle ParentRemovedQuery;

};
