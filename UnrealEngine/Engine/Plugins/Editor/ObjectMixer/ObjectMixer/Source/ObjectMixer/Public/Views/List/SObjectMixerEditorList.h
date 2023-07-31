// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ObjectMixerEditorList.h"
#include "ObjectMixerEditorListRow.h"
#include "ObjectMixerEditorListFilters/IObjectMixerEditorListFilter.h"

#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STreeView.h"

#include "SObjectMixerEditorList.generated.h"

class SWrapBox;
class FObjectMixerEditorList;
class SBox;
class SComboButton;
class SSearchBox;
class SHeaderRow;

UENUM()
enum class EListViewColumnType
{
	BuiltIn,
	PropertyGenerated
};

USTRUCT()
struct FListViewColumnInfo
{
	GENERATED_BODY()
	
	TObjectPtr<FProperty> PropertyRef = nullptr;
	
	UPROPERTY()
	FName PropertyName = NAME_None;

	UPROPERTY()
	FText PropertyDisplayText = FText::GetEmpty();

	UPROPERTY()
	EListViewColumnType PropertyType = EListViewColumnType::PropertyGenerated;

	UPROPERTY()
	FName PropertyCategoryName = NAME_None;

	UPROPERTY()
	bool bIsDesiredForDisplay = false;

	UPROPERTY()
	bool bCanBeSorted = false;

	UPROPERTY()
	bool bUseFixedWidth = false;

	UPROPERTY()
	float FixedWidth = 25.0f;

	UPROPERTY()
	float FillWidth = 1.0f;
};

class OBJECTMIXEREDITOR_API SObjectMixerEditorList final : public SCompoundWidget
{

public:

	SLATE_BEGIN_ARGS(SObjectMixerEditorList)
	{}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FObjectMixerEditorList> ListModel);

	virtual ~SObjectMixerEditorList() override;

	TWeakPtr<FObjectMixerEditorList> GetListModelPtr()
	{
		return ListModelPtr;
	}

	void ClearList()
	{
		FlushMemory(false);
	}

	/**
	 * Refresh filters and sorting.
	 * Useful for when the list state has gone stale but the variable count has not changed.
	 */
	void RefreshList();

	/**
	 * Regenerate the list items and refresh the list. Call when adding or removing items.
	 */
	void RequestRebuildList(const FString& InItemToScrollTo = "");

	[[nodiscard]] TArray<FObjectMixerEditorListRowPtr> GetSelectedTreeViewItems() const;
	int32 GetSelectedTreeViewItemCount() const;
	
	void RequestSyncEditorSelectionToListSelection()
	{
		bIsEditorToListSelectionSyncRequested = true;
	}
	void SyncEditorSelectionToListSelection();

	void SetSelectedTreeViewItemActorsEditorVisible(const bool bNewIsVisible, const bool bIsRecursive = false);

	bool IsTreeViewItemSelected(TSharedRef<FObjectMixerEditorListRow> Item);

	[[nodiscard]] TArray<FObjectMixerEditorListRowPtr> GetTreeViewItems() const;
	void SetTreeViewItems(const TArray<FObjectMixerEditorListRowPtr>& InItems);

	[[nodiscard]] int32 GetTreeViewItemCount() const
	{
		return TreeViewRootObjects.Num();
	}
	
	TSet<TWeakPtr<FObjectMixerEditorListRow>> GetSoloRows();

	void AddSoloRow(TSharedRef<FObjectMixerEditorListRow> InRow);
	void RemoveSoloRow(TSharedRef<FObjectMixerEditorListRow> InRow);

	void ClearSoloRows();

	FText GetSearchTextFromSearchInputField() const;
	FString GetSearchStringFromSearchInputField() const;
	void ExecuteListViewSearchOnAllRows(const FString& SearchString, const bool bShouldRefreshAfterward = true);

	bool DoesTreeViewHaveVisibleChildren() const;

	bool IsTreeViewItemExpanded(const TSharedPtr<FObjectMixerEditorListRow>& Row) const;
	void SetTreeViewItemExpanded(const TSharedPtr<FObjectMixerEditorListRow>& RowToExpand, const bool bNewExpansion) const;

	/**
	 * Determines the style of the tree (flat list or hierarchy)
	 */
	EObjectMixerTreeViewMode GetTreeViewMode();
	
	const TArray<TSharedRef<IObjectMixerEditorListFilter>>& GetListFilters();
	void EvaluateIfRowsPassFilters(const bool bShouldRefreshAfterward = true);

	/** Saves tree item expanded states to be recalled after the tree view is regenerated. */
	void CacheTreeState(const TArray<TWeakPtr<IObjectMixerEditorListFilter>>& InFilterCombo);
	void RestoreTreeState(const TArray<TWeakPtr<IObjectMixerEditorListFilter>>& InFilterCombo, const bool bFlushCache = true);

	// Sorting

	const FName& GetActiveSortingColumnName() const
	{
		return ActiveSortingColumnName;
	}
	EColumnSortMode::Type GetSortModeForColumn(FName InColumnName) const;
	void OnSortColumnCalled(EColumnSortPriority::Type Priority, const FName& ColumnName, EColumnSortMode::Type SortMode);
	EColumnSortMode::Type CycleSortMode(const FName& InColumnName);
	void ExecuteSort(
		const FName& InColumnName, const EColumnSortMode::Type InColumnSortMode, const bool bShouldRefreshAfterward = true);
	void ClearSorting()
	{
		ActiveSortingColumnName = NAME_None;
		ActiveSortingType = EColumnSortMode::None;
	}

	// Columns

	FListViewColumnInfo* GetColumnInfoByPropertyName(const FName& InPropertyName);

	static const FName ItemNameColumnName;
	static const FName EditorVisibilityColumnName;
	static const FName EditorVisibilitySoloColumnName;

	inline static TFunction<bool(const FObjectMixerEditorListRowPtr&, const FObjectMixerEditorListRowPtr&)> SortByTypeThenName =
		[](const FObjectMixerEditorListRowPtr& A, const FObjectMixerEditorListRowPtr& B)
	{
		return (A->GetRowType() < B->GetRowType() ||
			(A->GetRowType() == B->GetRowType() && A->GetDisplayName().ToString() < B->GetDisplayName().ToString())
		);
	};

protected:

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void OnActorSpawnedOrDestroyed(AActor* Object)
	{
		RequestRebuildList();
	}

	TWeakPtr<FObjectMixerEditorList> ListModelPtr;
	
	TSharedPtr<SHeaderRow> HeaderRow;
	TSharedRef<SWidget> GenerateHeaderRowContextMenu() const;

	bool bIsRebuildRequested = false;
	
	/**
	 * Regenerate the list items and refresh the list. Call when adding or removing items.
	 */
	void RebuildList();

	/**
	 * Only adds properties that pass a series of tests, including having only one unique entry in the column list array.
	 * @param bForceIncludeProperty If true, only Skiplist and Uniqueness tests will be checked, bypassing class, blueprint editability and other requirements.
	 * @param PropertySkipList These property names will be skipped when they are encountered in the iteration.
	 */
	bool AddUniquePropertyColumnsToHeaderRow(
		FProperty* Property,
		const bool bForceIncludeProperty = false,
		const TSet<FName>& PropertySkipList = {}
	);
	void AddBuiltinColumnsToHeaderRow();
	TSharedPtr<SHeaderRow> GenerateHeaderRow();
	ECheckBoxState HeaderCheckBoxState = ECheckBoxState::Checked;
	
	void FlushMemory(const bool bShouldKeepMemoryAllocated);
	
	void SetAllGroupsCollapsed();

	//  Tree View Implementation
	
	void BuildPerformanceCacheAndGenerateHeaderIfNeeded();
	void GenerateTreeView();
	void FindVisibleTreeViewObjects();
	void FindVisibleObjectsAndRequestTreeRefresh();

	void SelectedTreeItemsToSelectedInLevelEditor();
	
	/** For two-way selection sync, we need to pause selection sync under certain circumstances to prevent infinite loops. */
	bool bShouldPauseSyncSelection = false;
	bool bIsEditorToListSelectionSyncRequested = false;
	
	void OnGetRowChildren(FObjectMixerEditorListRowPtr Row, TArray<FObjectMixerEditorListRowPtr>& OutChildren) const;
	void OnRowChildExpansionChange(FObjectMixerEditorListRowPtr Row, const bool bIsExpanded, const bool bIsRecursive = false) const;

	void SetChildExpansionRecursively(const FObjectMixerEditorListRowPtr& InRow, const bool bNewIsExpanded) const;

	TSharedPtr<STreeView<FObjectMixerEditorListRowPtr>> TreeViewPtr;

	struct FTreeItemStateCache
	{
		uint32 UniqueId = -1;
		FString RowName = "";
		bool bIsExpanded = false;
		bool bIsSelected = false;
	};

	struct FFilterComboToStateCaches
	{
		TArray<TWeakPtr<IObjectMixerEditorListFilter>> FilterCombo = {};
		TArray<FTreeItemStateCache> Caches = {};
	};

	TArray<FFilterComboToStateCaches> FilterComboToStateCaches;
	
	TMap<UObject*, FObjectMixerEditorListRowPtr> ObjectsToRowsCreated;

	/** All Tree view objects */
	TArray<FObjectMixerEditorListRowPtr> TreeViewRootObjects;
	/** Visible Tree view objects, after filters */
	TArray<FObjectMixerEditorListRowPtr> VisibleTreeViewObjects;

	TArray<FListViewColumnInfo> ListViewColumns;

	// Sorting

	FName ActiveSortingColumnName = NAME_None;
	EColumnSortMode::Type ActiveSortingType = EColumnSortMode::None;

	TFunctionRef<bool(const FObjectMixerEditorListRowPtr&, const FObjectMixerEditorListRowPtr&)> SortByOrderAscending =
		[](const FObjectMixerEditorListRowPtr& A, const FObjectMixerEditorListRowPtr& B)
		{
			return A->GetSortOrder() < B->GetSortOrder();
		};

	// Performance cache
	TSet<UClass*> ObjectClassesToFilterCache;
	TSet<FName> ColumnsToShowByDefaultCache;
	TSet<FName> ColumnsToExcludeCache;
	TSet<FName> ForceAddedColumnsCache;
	EObjectMixerInheritanceInclusionOptions PropertyInheritanceInclusionOptionsCache = EObjectMixerInheritanceInclusionOptions::None;
	bool bShouldIncludeUnsupportedPropertiesCache = false;
};
