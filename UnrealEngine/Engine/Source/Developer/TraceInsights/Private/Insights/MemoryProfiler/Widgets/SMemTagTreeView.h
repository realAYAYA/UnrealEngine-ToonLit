// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/FilterCollection.h"
#include "Misc/TextFilter.h"
#include "SlateFwd.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWidget.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STreeView.h"

// Insights
#include "Insights/MemoryProfiler/ViewModels/MemTagNodeGroupingAndSorting.h"
#include "Insights/MemoryProfiler/ViewModels/MemTagNode.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

class FMenuBuilder;
class FMemoryGraphTrack;
class SMemoryProfilerWindow;

namespace TraceServices
{
	class IAnalysisSession;
}

namespace Insights
{
	class FTable;
	class FTableColumn;
	class ITableCellValueSorter;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

/** The filter collection - used for updating the list of tree nodes. */
typedef TFilterCollection<const FMemTagNodePtr&> FMemTagNodeFilterCollection;

/** The text based filter - used for updating the list of tree nodes. */
typedef TTextFilter<const FMemTagNodePtr&> FMemTagNodeTextFilter;

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * A custom widget used to display the list of LLM tags and thier aggregated stats.
 */
class SMemTagTreeView : public SCompoundWidget
{
public:
	/** Default constructor. */
	SMemTagTreeView();

	/** Virtual destructor. */
	virtual ~SMemTagTreeView();

	SLATE_BEGIN_ARGS(SMemTagTreeView) {}
	SLATE_END_ARGS()

	/**
	 * Converts profiler window weak pointer to a shared pointer and returns it.
	 * Make sure the returned pointer is valid before trying to dereference it.
	 */
	TSharedPtr<SMemoryProfilerWindow> GetProfilerWindow() const
	{
		return ProfilerWindowWeakPtr.Pin();
	}

	/**
	 * Construct this widget
	 * @param InArgs - The declaration data for this widget
	 */
	void Construct(const FArguments& InArgs, TSharedPtr<SMemoryProfilerWindow> InProfilerWindow);

	TSharedPtr<Insights::FTable> GetTable() const { return Table; }

	void Reset();

	/**
	 * Rebuilds the tree (if necessary).
	 * @param bResync - If true, it forces a resync with list of LLM tags from Analysis, even if the list did not changed since last sync.
	 */
	void RebuildTree(bool bResync);

	void ResetStats();
	void UpdateStats(double StartTime, double EndTime);

	FMemTagNodePtr GetMemTagNode(Insights::FMemoryTagId MemTagId) const { return MemTagNodesIdMap.FindRef(MemTagId); }
	void SelectMemTagNode(Insights::FMemoryTagId MemTagId);

private:
	TSharedRef<SWidget> MakeTrackersMenu();
	void CreateTrackersMenuSection(FMenuBuilder& MenuBuilder);
	TSharedRef<SWidget> ConstructTagsFilteringWidgetArea();
	TSharedRef<SWidget> ConstructTagsGroupingWidgetArea();
	TSharedRef<SWidget> ConstructTracksMiniToolbar();

	void UpdateTree();

	void UpdateStatsInternal();

	/** Called when the analysis session has changed. */
	void InsightsManager_OnSessionChanged();

	/**
	 * Populates OutSearchStrings with the strings that should be used in searching.
	 *
	 * @param GroupOrStatNodePtr - the group and stat node to get a text description from.
	 * @param OutSearchStrings   - an array of strings to use in searching.
	 *
	 */
	void HandleItemToStringArray(const FMemTagNodePtr& GroupOrStatNodePtr, TArray<FString>& OutSearchStrings) const;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Tree View - Context Menu

	TSharedPtr<SWidget> TreeView_GetMenuContent();
	void TreeView_BuildCreateGraphTracksMenu(FMenuBuilder& MenuBuilder);
	void TreeView_BuildRemoveGraphTracksMenu(FMenuBuilder& MenuBuilder);
	void TreeView_BuildSortByMenu(FMenuBuilder& MenuBuilder);
	void TreeView_BuildViewColumnMenu(FMenuBuilder& MenuBuilder);

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Tree View - Columns' Header

	void InitializeAndShowHeaderColumns();

	FText GetColumnHeaderText(const FName ColumnId) const;

	TSharedRef<SWidget> TreeViewHeaderRow_GenerateColumnMenu(const Insights::FTableColumn& Column);

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Tree View - Misc

	void TreeView_Refresh();

	/**
	 * Called by STreeView to retrieves the children for the specified parent item.
	 * @param InParent    - The parent node to retrieve the children from.
	 * @param OutChildren - List of children for the parent node.
	 */
	void TreeView_OnGetChildren(FMemTagNodePtr InParent, TArray<FMemTagNodePtr>& OutChildren);

	/** Called by STreeView when selection has changed. */
	void TreeView_OnSelectionChanged(FMemTagNodePtr SelectedItem, ESelectInfo::Type SelectInfo);

	/** Called by STreeView when a tree item is double clicked. */
	void TreeView_OnMouseButtonDoubleClick(FMemTagNodePtr TreeNode);

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Tree View - Table Row

	/** Called by STreeView to generate a table row for the specified item. */
	TSharedRef<ITableRow> TreeView_OnGenerateRow(FMemTagNodePtr TreeNode, const TSharedRef<STableViewBase>& OwnerTable);

	bool TableRow_ShouldBeEnabled(FMemTagNodePtr NodePtr) const;

	void TableRow_SetHoveredCell(TSharedPtr<Insights::FTable> TablePtr, TSharedPtr<Insights::FTableColumn> ColumnPtr, FMemTagNodePtr NodePtr);
	EHorizontalAlignment TableRow_GetColumnOutlineHAlignment(const FName ColumnId) const;

	FText TableRow_GetHighlightText() const;
	FName TableRow_GetHighlightedNodeName() const;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Filtering

	/** Populates the group and stat tree with items based on the current data. */
	void ApplyFiltering();

	void FilterOutZeroCountMemTags_OnCheckStateChanged(ECheckBoxState NewRadioState);
	ECheckBoxState FilterOutZeroCountMemTags_IsChecked() const;

	bool SearchBox_IsEnabled() const;
	void SearchBox_OnTextChanged(const FText& InFilterText);

	void ToggleTracker(Insights::FMemoryTrackerId InTrackerId);
	bool IsTrackerChecked(Insights::FMemoryTrackerId InTrackerId) const;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Grouping

	void CreateGroups();
	void CreateGroupByOptionsSources();

	void GroupBy_OnSelectionChanged(TSharedPtr<EMemTagNodeGroupingMode> NewGroupingMode, ESelectInfo::Type SelectInfo);

	TSharedRef<SWidget> GroupBy_OnGenerateWidget(TSharedPtr<EMemTagNodeGroupingMode> InGroupingMode) const;

	FText GroupBy_GetSelectedText() const;

	FText GroupBy_GetSelectedTooltipText() const;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Sorting

	static const FName GetDefaultColumnBeingSorted();
	static const EColumnSortMode::Type GetDefaultColumnSortMode();

	void CreateSortings();

	void UpdateCurrentSortingByColumn();
	void SortTreeNodes();
	void SortTreeNodesRec(FMemTagNode& Node, const Insights::ITableCellValueSorter& Sorter);

	EColumnSortMode::Type GetSortModeForColumn(const FName ColumnId) const;
	void SetSortModeForColumn(const FName& ColumnId, EColumnSortMode::Type SortMode);
	void OnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type SortMode);

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Sorting actions

	// SortMode (HeaderMenu)
	bool HeaderMenu_SortMode_IsChecked(const FName ColumnId, const EColumnSortMode::Type InSortMode);
	bool HeaderMenu_SortMode_CanExecute(const FName ColumnId, const EColumnSortMode::Type InSortMode) const;
	void HeaderMenu_SortMode_Execute(const FName ColumnId, const EColumnSortMode::Type InSortMode);

	// SortMode (ContextMenu)
	bool ContextMenu_SortMode_IsChecked(const EColumnSortMode::Type InSortMode);
	bool ContextMenu_SortMode_CanExecute(const EColumnSortMode::Type InSortMode) const;
	void ContextMenu_SortMode_Execute(const EColumnSortMode::Type InSortMode);

	// SortByColumn (ContextMenu)
	bool ContextMenu_SortByColumn_IsChecked(const FName ColumnId);
	bool ContextMenu_SortByColumn_CanExecute(const FName ColumnId) const;
	void ContextMenu_SortByColumn_Execute(const FName ColumnId);

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Column visibility actions

	// ShowColumn
	bool CanShowColumn(const FName ColumnId) const;
	void ShowColumn(const FName ColumnId);

	// HideColumn
	bool CanHideColumn(const FName ColumnId) const;
	void HideColumn(const FName ColumnId);

	// ToggleColumnVisibility
	bool IsColumnVisible(const FName ColumnId);
	bool CanToggleColumnVisibility(const FName ColumnId) const;
	void ToggleColumnVisibility(const FName ColumnId);

	// ShowAllColumns (ContextMenu)
	bool ContextMenu_ShowAllColumns_CanExecute() const;
	void ContextMenu_ShowAllColumns_Execute();

	// MinMaxMedColumns (ContextMenu)
	bool ContextMenu_ShowMinMaxMedColumns_CanExecute() const;
	void ContextMenu_ShowMinMaxMedColumns_Execute();

	// ResetColumns (ContextMenu)
	bool ContextMenu_ResetColumns_CanExecute() const;
	void ContextMenu_ResetColumns_Execute();

	////////////////////////////////////////////////////////////////////////////////////////////////////

	FReply ShowAllTracks_OnClicked();
	FReply HideAllTracks_OnClicked();

	FReply LoadReportXML_OnClicked();

	////////////////////////////////////////////////////////////////////////////////////////////////////

	// Create memory graph tracks for selected LLM tag(s)
	bool CanCreateGraphTracksForSelectedMemTags() const;
	void CreateGraphTracksForSelectedMemTags();

	// Create memory graph tracks for filtered LLM tags
	bool CanCreateGraphTracksForFilteredMemTags() const;
	void CreateGraphTracksForFilteredMemTags();

	// Create all memory graph tracks
	bool CanCreateAllGraphTracks() const;
	void CreateAllGraphTracks();

	// Remove memory graph tracks for selected LLM tag(s)
	bool CanRemoveGraphTracksForSelectedMemTags() const;
	void RemoveGraphTracksForSelectedMemTags();

	// Remove all memory graph tracks
	bool CanRemoveAllGraphTracks() const;
	void RemoveAllGraphTracks();

	// Generate new color for selected LLM tag(s)
	bool CanGenerateColorForSelectedMemTags() const;
	void GenerateColorForSelectedMemTags();
	void SetColorToNode(const FMemTagNodePtr& MemTagNode, FLinearColor CustomColor, bool bSetRandomColor);
	FLinearColor GetEditableColor() const;
	void SetEditableColor(FLinearColor NewColor);
	void ColorPickerCancelled(FLinearColor OriginalColor);

	// Edit color for selected LLM tag(s)
	bool CanEditColorForSelectedMemTags() const;
	void EditColorForSelectedMemTags();

	////////////////////////////////////////////////////////////////////////////////////////////////////

	/**
	 * Ticks this widget.  Override in derived classes, but always call the parent implementation.
	 *
	 * @param  AllottedGeometry The space allotted for this widget
	 * @param  InCurrentTime  Current absolute real time
	 * @param  InDeltaTime  Real time passed since last tick
	 */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	/** A weak pointer to the Memory Insights window. */
	TWeakPtr<SMemoryProfilerWindow> ProfilerWindowWeakPtr;

	/** Table view model. */
	TSharedPtr<Insights::FTable> Table;

	/** The analysis session used to populate this widget. */
	TSharedPtr<const TraceServices::IAnalysisSession> Session;

	//////////////////////////////////////////////////
	// Tree View, Columns

	/** The tree widget which holds the LLM tag tree nodes. */
	TSharedPtr<STreeView<FMemTagNodePtr>> TreeView;

	/** Holds the tree view header row widget which display all columns in the tree view. */
	TSharedPtr<SHeaderRow> TreeViewHeaderRow;

	/** External scrollbar used to synchronize tree view position. */
	TSharedPtr<SScrollBar> ExternalScrollbar;

	//////////////////////////////////////////////////
	// Hovered Column, Hovered Node

	/** Name of the column currently being hovered by the mouse. */
	FName HoveredColumnId;

	/** A shared pointer to the node currently being hovered by the mouse. */
	FMemTagNodePtr HoveredNodePtr;

	/** Name of the node that should be drawn as highlighted. */
	FName HighlightedNodeName;

	//////////////////////////////////////////////////
	// MemTag Nodes

	/** An array of group nodes. */
	TArray<FMemTagNodePtr> GroupNodes;

	/** A filtered array of group nodes to be displayed in the tree widget. */
	TArray<FMemTagNodePtr> FilteredGroupNodes;

	/** The serial number of the memory tag list maintained by the MemorySharedState object (updated last time we have synced MemTagNodes with it). */
	uint32 LastMemoryTagListSerialNumber;

	/** All LLM tag nodes. */
	TSet<FMemTagNodePtr> MemTagNodes;

	/** All LLM tag nodes, stored as NodeId -> FMemTagNodePtr. */
	TMap<Insights::FMemoryTagId, FMemTagNodePtr> MemTagNodesIdMap;

	/** Currently expanded group nodes. */
	TSet<FMemTagNodePtr> ExpandedNodes;

	/** If true, the expanded nodes have been saved before applying a text filter. */
	bool bExpansionSaved;

	//////////////////////////////////////////////////
	// Search box and filters

	//bool bUseFiltering;

	/** The search box widget used to filter items displayed in the stats and groups tree. */
	TSharedPtr<SSearchBox> SearchBox;

	/** The text based filter. */
	TSharedPtr<FMemTagNodeTextFilter> TextFilter;

	/** The filter collection. */
	TSharedPtr<FMemTagNodeFilterCollection> Filters;

	/** Filter out the LLM tags having zero total instance count (aggregated stats). */
	bool bFilterOutZeroCountMemTags;

	/** Filter the LLM tags by tracker. */
	uint64 TrackersFilter;

	//////////////////////////////////////////////////
	// Grouping

	//bool bUseGrouping;

	TArray<TSharedPtr<EMemTagNodeGroupingMode>> GroupByOptionsSource;

	TSharedPtr<SComboBox<TSharedPtr<EMemTagNodeGroupingMode>>> GroupByComboBox;

	/** How we group the LLM tag nodes? */
	EMemTagNodeGroupingMode GroupingMode;

	//////////////////////////////////////////////////
	// Sorting

	//bool bUseSorting;

	/** All available sorters. */
	TArray<TSharedPtr<Insights::ITableCellValueSorter>> AvailableSorters;

	/** Current sorter. It is nullptr if sorting is disabled. */
	TSharedPtr<Insights::ITableCellValueSorter> CurrentSorter;

	/** Name of the column currently being sorted. Can be NAME_None if sorting is disabled (CurrentSorting == nullptr) or if a complex sorting is used (CurrentSorting != nullptr). */
	FName ColumnBeingSorted;

	/** How we sort the nodes? Ascending or Descending. */
	EColumnSortMode::Type ColumnSortMode;

	//////////////////////////////////////////////////

	TSharedPtr<SComboBox<TSharedPtr<Insights::FMemoryTracker>>> TrackerComboBox;

	FLinearColor EditableColorValue;

	//////////////////////////////////////////////////

	double StatsStartTime;
	double StatsEndTime;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
