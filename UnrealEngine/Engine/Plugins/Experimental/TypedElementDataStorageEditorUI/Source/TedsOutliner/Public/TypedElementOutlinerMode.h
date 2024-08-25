// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISceneOutlinerMode.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Compatibility/TedsCompatibilityUtils.h"

// TEDS-Outliner TODO: This can probably be moved to a more generic location for all TEDS related drag drops?
class FTEDSDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FTEDSDragDropOp, FDecoratedDragDropOp)

	/** Rows we are dragging */
	TArray<TypedElementDataStorage::RowHandle> DraggedRows;

	void Init(const TArray<TypedElementDataStorage::RowHandle>& InRowHandles)
	{
		DraggedRows = InRowHandles;
	}

	static TSharedRef<FTEDSDragDropOp> New(const TArray<TypedElementDataStorage::RowHandle>& InRowHandles)
	{
		TSharedRef<FTEDSDragDropOp> Operation = MakeShareable(new FTEDSDragDropOp);
		
		Operation->Init(InRowHandles);
		Operation->SetupDefaults();
		Operation->Construct();
		
		return Operation;
	}
};

struct FTypedElementOutlinerModeParams
{
	FTypedElementOutlinerModeParams(SSceneOutliner* InSceneOutliner)
		: SceneOutliner(InSceneOutliner)
		, bUseDefaultTEDSFilters(false)
	{}

	SSceneOutliner* SceneOutliner;

	// The query description that will be used to populate rows in the TEDS-Outliner
	TypedElementDataStorage::FQueryDescription QueryDescription;

	// The selection set to use for this Outliner, unset = create new unique selection set
	TOptional<FName> SelectionSetOverride;

	// TEDS queries that will be used to create filters in this Outliner
	// TEDS-Outliner TODO: Can we consolidate this with the SceneOutliner API to create filters? Currently has to be separate because FTEDSOutlinerFilter
	// needs a reference to the mode which is not possible since filters with the Outliner API are added before the mode is init
	TMap<FName, const TypedElementDataStorage::FQueryDescription> FilterQueries;

	// If true, this Outliner will automatically add all TEDS tags and columns as filters
	bool bUseDefaultTEDSFilters;
};

/*
 * TEDS driven Outliner mode where the Outliner is populated using the results of the RowHandleQueries passed in.
 * See CreateGenericTEDSOutliner() for example usage
 * Inherits from ISceneOutlinerMode - which contains all actions that depend on the type of item you are viewing in the Outliner
 */
class TEDSOUTLINER_API FTypedElementOutlinerMode : public ISceneOutlinerMode, public FBaseTEDSOutlinerMode
{
public:
	explicit FTypedElementOutlinerMode(const FTypedElementOutlinerModeParams& InParams);
	virtual ~FTypedElementOutlinerMode() override;

	/* ISceneOutlinerMode interface */
	virtual void Rebuild() override;
	virtual void SynchronizeSelection() override;
	virtual void OnItemSelectionChanged(FSceneOutlinerTreeItemPtr Item, ESelectInfo::Type SelectionType, const FSceneOutlinerItemSelection& Selection) override;
	virtual ESelectionMode::Type GetSelectionMode() const override { return ESelectionMode::Multi; }
	virtual bool CanSupportDragAndDrop() const override { return true; } // TODO: Can we check this from TEDS somehow (if the user requests a drag column?)
	virtual TSharedPtr<FDragDropOperation> CreateDragDropOperation(const FPointerEvent& MouseEvent, const TArray<FSceneOutlinerTreeItemPtr>& InTreeItems) const override;
	virtual bool ParseDragDrop(FSceneOutlinerDragDropPayload& OutPayload, const FDragDropOperation& Operation) const override;
	virtual FSceneOutlinerDragValidationInfo ValidateDrop(const ISceneOutlinerTreeItem& DropTarget, const FSceneOutlinerDragDropPayload& Payload) const override;
	virtual void OnDrop(ISceneOutlinerTreeItem& DropTarget, const FSceneOutlinerDragDropPayload& Payload, const FSceneOutlinerDragValidationInfo& ValidationInfo) const override;
	/* end ISceneOutlinerMode interface */

	// Set the final query used to populate row handles
	void SetRowHandleQuery(TypedElementDataStorage::QueryHandle InRowHandleQuery);

	// Add an external query to the Outliner
	void AddExternalQuery(FName QueryName, const TypedElementDataStorage::FQueryDescription& InQueryDescription);
	void RemoveExternalQuery(FName QueryName);

	// Append all external queries into the given query
	void AppendExternalQueries(TypedElementDataStorage::FQueryDescription& OutQuery);

	// TEDS-Outliner TODO: This should live in TEDS long term
	// Funtion to combine 2 queries (adds to second query to the first)
	static void AppendQuery(TypedElementDataStorage::FQueryDescription& Query1, const TypedElementDataStorage::FQueryDescription& Query2);
	
protected:
	
	virtual TUniquePtr<ISceneOutlinerHierarchy> CreateHierarchy() override;
	void RecompileQueries();
	void UnregisterQueries();
	void ClearSelection();
	void Tick();

protected:

	// TEDS-Outliner TODO: Should the queries be owned by mode or hierarchy? Currently half and half
	// Initial query provided by user
	TypedElementDataStorage::FQueryDescription InitialQueryDescription;

	// Final composite query (filters/searches etc)
	TypedElementDataStorage::QueryHandle FinalRowHandleQuery;

	// Query to get all selected rows, track selection added, track selection removed
	TypedElementDataStorage::QueryHandle SelectedRowsQuery;
	TypedElementDataStorage::QueryHandle SelectionAddedQuery;
	TypedElementDataStorage::QueryHandle SelectionRemovedQuery;

	// External queries that are currently active (Filters)
	TMap<FName, TypedElementDataStorage::FQueryDescription> ExternalQueries;

	FName SelectionSetName;
	bool bSelectionDirty;

	// We tick selection update because TEDS columns are sometimes not init in FObserver::OnAdd
	FTSTicker::FDelegateHandle TickerHandle;
};