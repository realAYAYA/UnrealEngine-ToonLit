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
#include "Insights/ViewModels/TimerGroupingAndSorting.h"
#include "Insights/ViewModels/TimerNode.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

class SFrameTrack;
class FMenuBuilder;
class FTimingGraphTrack;
class FUICommandList;

namespace TraceServices
{
	class IAnalysisSession;
}

namespace Insights
{
	class FTable;
	class FTableColumn;
	class ITableCellValueSorter;

	class FTimerAggregator;
	class SAsyncOperationStatus;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

/** The filter collection - used for updating the list of timer nodes. */
typedef TFilterCollection<const FTimerNodePtr&> FTimerNodeFilterCollection;

/** The text based filter - used for updating the list of timer nodes. */
typedef TTextFilter<const FTimerNodePtr&> FTimerNodeTextFilter;

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * A custom widget used to display the list of timers.
 */
class STimersView : public SCompoundWidget
{
public:
	/** Default constructor. */
	STimersView();

	/** Virtual destructor. */
	virtual ~STimersView();

	SLATE_BEGIN_ARGS(STimersView) {}
	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 * @param InArgs - The declaration data for this widget
	 */
	void Construct(const FArguments& InArgs);

	TSharedPtr<Insights::FTable> GetTable() const { return Table; }

	void Reset();

	/**
	 * Rebuilds the tree (if necessary).
	 * @param bResync - If true, it forces a resync with list of timers from Analysis, even if the list did not changed since last sync.
	 */
	void RebuildTree(bool bResync);

	void ResetStats();
	void UpdateStats(double StartTime, double EndTime);

	FTimerNodePtr GetTimerNode(uint32 TimerId) const;
	void SelectTimerNode(uint32 TimerId);

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	ETraceFrameType GetFrameTypeMode() { return ModeFrameType; }

	void OnTimingViewTrackListChanged();

	void ToggleTimingViewMainGraphEventSeries(FTimerNodePtr TimerNode) const;

private:
	void InitCommandList();

	void UpdateTree();

	void FinishAggregation();
	void ApplyAggregation(TraceServices::ITable<TraceServices::FTimingProfilerAggregatedStats>* AggregatedStatsTable);

	/** Called when the session has changed. */
	void InsightsManager_OnSessionChanged();

	/** Called when the analysis was completed. */
	void InsightsManager_OnSessionAnalysisCompleted();

	/**
	 * Populates OutSearchStrings with the strings that should be used in searching.
	 *
	 * @param GroupOrStatNodePtr - the group and stat node to get a text description from.
	 * @param OutSearchStrings   - an array of strings to use in searching.
	 *
	 */
	void HandleItemToStringArray(const FTimerNodePtr& GroupOrStatNodePtr, TArray<FString>& OutSearchStrings) const;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Tree View - Context Menu

	TSharedPtr<SWidget> TreeView_GetMenuContent();
	void TreeView_BuildSortByMenu(FMenuBuilder& MenuBuilder);
	void TreeView_BuildViewColumnMenu(FMenuBuilder& MenuBuilder);
	void TreeView_BuildExportMenu(FMenuBuilder& MenuBuilder);

	bool ContextMenu_CopyToClipboard_CanExecute() const;
	void ContextMenu_CopyToClipboard_Execute();

	bool ContextMenu_Export_CanExecute() const;
	void ContextMenu_Export_Execute();

	void AddTimerNodeRecursive(FTimerNodePtr InNode, TSet<uint32>& InOutIncludedTimers) const;

	bool ContextMenu_ExportTimingEventsSelection_CanExecute() const;
	void ContextMenu_ExportTimingEventsSelection_Execute() const;

	bool ContextMenu_ExportTimingEvents_CanExecute() const;
	void ContextMenu_ExportTimingEvents_Execute() const;

	bool ContextMenu_ExportThreads_CanExecute() const;
	void ContextMenu_ExportThreads_Execute() const;

	bool ContextMenu_ExportTimers_CanExecute() const;
	void ContextMenu_ExportTimers_Execute() const;

	bool OpenSaveTextFileDialog(const FString& InDialogTitle, const FString& InDefaultFile, FString& OutFilename) const;
	class IFileHandle* OpenExportFile(const TCHAR* InFilename) const;

	bool ContextMenu_OpenSource_CanExecute() const;
	void ContextMenu_OpenSource_Execute() const;

	bool ContextMenu_FindInstance_CanExecute() const;
	void ContextMenu_FindInstance_Execute(bool bFindMax) const;

	bool ContextMenu_FindInstanceInSelection_CanExecute() const;
	void ContextMenu_FindInstanceInSelection_Execute(bool bFindMax) const;

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
	void TreeView_OnGetChildren(FTimerNodePtr InParent, TArray<FTimerNodePtr>& OutChildren);

	/** Called by STreeView when selection has changed. */
	void TreeView_OnSelectionChanged(FTimerNodePtr SelectedItem, ESelectInfo::Type SelectInfo);

	FTimerNodePtr GetSingleSelectedTimerNode() const;

	/** Called by STreeView when a tree item is double clicked. */
	void TreeView_OnMouseButtonDoubleClick(FTimerNodePtr TreeNode);

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Tree View - Table Row

	/** Called by STreeView to generate a table row for the specified item. */
	TSharedRef<ITableRow> TreeView_OnGenerateRow(FTimerNodePtr TreeNode, const TSharedRef<STableViewBase>& OwnerTable);

	bool TableRow_ShouldBeEnabled(FTimerNodePtr NodePtr) const;

	void TableRow_SetHoveredCell(TSharedPtr<Insights::FTable> TablePtr, TSharedPtr<Insights::FTableColumn> ColumnPtr, FTimerNodePtr NodePtr);
	EHorizontalAlignment TableRow_GetColumnOutlineHAlignment(const FName ColumnId) const;

	FText TableRow_GetHighlightText() const;
	FName TableRow_GetHighlightedNodeName() const;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Filtering

	/** Populates the group and stat tree with items based on the current data. */
	void ApplyFiltering();

	void FilterOutZeroCountTimers_OnCheckStateChanged(ECheckBoxState NewRadioState);
	ECheckBoxState FilterOutZeroCountTimers_IsChecked() const;

	TSharedRef<SWidget> GetToggleButtonForTimerType(const ETimerNodeType InNodeType);
	void FilterByTimerType_OnCheckStateChanged(ECheckBoxState NewRadioState, const ETimerNodeType InNodeType);
	ECheckBoxState FilterByTimerType_IsChecked(const ETimerNodeType InNodeType) const;

	bool SearchBox_IsEnabled() const;
	void SearchBox_OnTextChanged(const FText& InFilterText);

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Grouping

	void CreateGroups();
	void CreateGroupByOptionsSources();

	void GroupBy_OnSelectionChanged(TSharedPtr<ETimerGroupingMode> NewGroupingMode, ESelectInfo::Type SelectInfo);

	TSharedRef<SWidget> GroupBy_OnGenerateWidget(TSharedPtr<ETimerGroupingMode> InGroupingMode) const;

	FText GroupBy_GetSelectedText() const;

	FText GroupBy_GetSelectedTooltipText() const;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Frame Type

	void CreateModeOptionsSources();

	void Mode_OnSelectionChanged(TSharedPtr<ETraceFrameType> NewGroupingMode, ESelectInfo::Type SelectInfo);

	TSharedRef<SWidget> Mode_OnGenerateWidget(TSharedPtr<ETraceFrameType> InGroupingMode) const;

	FText Mode_GetSelectedText() const;
	
	FText Mode_GetText(ETraceFrameType InFrameType) const;

	FText Mode_GetSelectedTooltipText() const;

	FText Mode_GetTooltipText(ETraceFrameType InFrameType) const;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Sorting

	static const FName GetDefaultColumnBeingSorted();
	static const EColumnSortMode::Type GetDefaultColumnSortMode();

	void CreateSortings();

	void UpdateCurrentSortingByColumn();
	void SortTreeNodes();
	void SortTreeNodesRec(FTimerNode& Node, const Insights::ITableCellValueSorter& Sorter);

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
	bool IsColumnVisible(const FName ColumnId) const;
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

	void ToggleTimingViewEventFilter(FTimerNodePtr TimerNode) const;

	void TreeView_BuildPlotTimerMenu(FMenuBuilder& MenuBuilder);

	void TreeView_FindMenu(FMenuBuilder& MenuBuilder);

	TSharedPtr<FTimingGraphTrack> GetTimingViewMainGraphTrack() const;
	TSharedPtr<SFrameTrack> GetFrameTrack() const;

	void ToggleGraphInstanceSeries(TSharedRef<FTimingGraphTrack> GraphTrack, FTimerNodeRef NodePtr) const;
	bool IsInstanceSeriesInTimingViewMainGraph(FTimerNodePtr TimerNode) const;
	void ToggleTimingViewMainGraphEventInstanceSeries(FTimerNodePtr TimerNode) const;

	void ToggleGraphFrameStatsSeries(TSharedRef<FTimingGraphTrack> GraphTrack, FTimerNodeRef NodePtr, ETraceFrameType FrameType) const;
	bool IsFrameStatsSeriesInTimingViewMainGraph(FTimerNodePtr TimerNode, ETraceFrameType FrameType) const;
	void ToggleTimingViewMainGraphEventFrameStatsSeries(FTimerNodePtr TimerNode, ETraceFrameType FrameType) const;

	bool IsSeriesInFrameTrack(FTimerNodePtr TimerNode, ETraceFrameType FrameType) const;
	void ToggleFrameTrackSeries(FTimerNodePtr TimerNode, ETraceFrameType FrameType) const;

	////////////////////////////////////////////////////////////////////////////////////////////////////

	/**
	 * Ticks this widget.  Override in derived classes, but always call the parent implementation.
	 *
	 * @param  AllottedGeometry The space allotted for this widget
	 * @param  InCurrentTime  Current absolute real time
	 * @param  InDeltaTime  Real time passed since last tick
	 */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void OpenSourceFileInIDE(FTimerNodePtr InNode) const;

	void SaveSettings();
	void SaveVisibleColumnsSettings();
	void LoadSettings();
	void LoadVisibleColumnsSettings();

	void SetTimingViewFrameType();

private:
	/** Table view model. */
	TSharedPtr<Insights::FTable> Table;

	/** The analysis session used to populate this widget. */
	TSharedPtr<const TraceServices::IAnalysisSession> Session;

	TSharedPtr<FUICommandList> CommandList;

	//////////////////////////////////////////////////
	// Tree View, Columns

	/** The tree widget which holds the list of groups and timers corresponding with each group. */
	TSharedPtr<STreeView<FTimerNodePtr>> TreeView;

	/** Holds the tree view header row widget which display all columns in the tree view. */
	TSharedPtr<SHeaderRow> TreeViewHeaderRow;

	/** External scrollbar used to synchronize tree view position. */
	TSharedPtr<SScrollBar> ExternalScrollbar;

	//////////////////////////////////////////////////
	// Hovered Column, Hovered Timer Node

	/** Name of the column currently being hovered by the mouse. */
	FName HoveredColumnId;

	/** A shared pointer to the timer node currently being hovered by the mouse. */
	FTimerNodePtr HoveredNodePtr;

	/** Name of the timer that should be drawn as highlighted. */
	FName HighlightedNodeName;

	//////////////////////////////////////////////////
	// Timer Nodes

	/** An array of group and timer nodes generated from the metadata. */
	TArray<FTimerNodePtr> GroupNodes;

	/** A filtered array of group and timer nodes to be displayed in the tree widget. */
	TArray<FTimerNodePtr> FilteredGroupNodes;

	/** All timer nodes. An index in this array is a TimerId. */
	TArray<FTimerNodePtr> TimerNodes;

	/** Currently expanded group nodes. */
	TSet<FTimerNodePtr> ExpandedNodes;

	/** If true, the expanded nodes have been saved before applying a text filter. */
	bool bExpansionSaved;

	//////////////////////////////////////////////////
	// Search box and filters

	//bool bUseFiltering;

	/** The search box widget used to filter items displayed in the tree. */
	TSharedPtr<SSearchBox> SearchBox;

	/** The text based filter. */
	TSharedPtr<FTimerNodeTextFilter> TextFilter;

	/** The filter collection. */
	TSharedPtr<FTimerNodeFilterCollection> Filters;

	/** Holds the visibility of each timer type. */
	bool bTimerTypeIsVisible[static_cast<int>(ETimerNodeType::InvalidOrMax)];

	/** Filter out the timers having zero total instance count (aggregated stats). */
	bool bFilterOutZeroCountTimers;

	//////////////////////////////////////////////////
	// Grouping

	//bool bUseGrouping;

	TArray<TSharedPtr<ETimerGroupingMode>> GroupByOptionsSource;

	TSharedPtr<SComboBox<TSharedPtr<ETimerGroupingMode>>> GroupByComboBox;

	/** How we group the timers? */
	ETimerGroupingMode GroupingMode;

	//////////////////////////////////////////////////
	// Frame Type

	TArray<TSharedPtr<ETraceFrameType>> ModeOptionsSource;

	TSharedPtr<SComboBox<TSharedPtr<ETraceFrameType>>> ModeComboBox;

	ETraceFrameType ModeFrameType;

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

	TSharedRef<Insights::FTimerAggregator> Aggregator;
	TSharedPtr<Insights::SAsyncOperationStatus> AsyncOperationStatus;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
