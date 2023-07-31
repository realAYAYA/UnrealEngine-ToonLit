// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "Containers/UnrealString.h"
#include "Misc/IFilter.h"
#include "Framework/Views/TreeFilterHandler.h"
#include "Misc/TextFilter.h"

#include "ITraceObject.h"

class ITableRow;
class SFilterPresetList;
class SSessionSelector;
class SSearchBox;
struct IFilterPreset;
class ITraceObject;
class ISessionTraceFilterService;
class FEventFilterService;
struct FTraceObjectInfo;

class STraceDataFilterWidget : public SCompoundWidget
{
public:
	/** Default constructor. */
	STraceDataFilterWidget();

	/** Virtual destructor. */
	virtual ~STraceDataFilterWidget();

	SLATE_BEGIN_ARGS(STraceDataFilterWidget) {}
	SLATE_END_ARGS()

	/** Constructs this widget. */
	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
protected:
	/** Callback from SFilterPresetList, should save the current tree view filter state as the specified preset */
	void OnSavePreset(const TSharedPtr<IFilterPreset>& Preset);
	/** Callback from SFilterPresetList, should update filtering state according to currently active Filter Presets */
	void OnPresetChanged(const class SFilterPreset& Preset);
	/** Callback from SFilterPresetList, should highlight the tree view items encompassed by the specified preset */
	void OnHighlightPreset(const TSharedPtr<IFilterPreset>& Preset);

	void OnSearchboxTextChanged(const FText& FilterText);	
	void OnItemDoubleClicked(TSharedPtr<ITraceObject> InObject) const;

	/** Constructing various widgets */
	void ConstructTreeview();
	void ConstructSearchBoxFilter();
	void ConstructFilterHandler();

	/** Callback for whenever a different analysis session (store) has been retrieved */
	void SetCurrentAnalysisSession(uint32 SessionHandle, TSharedRef<const TraceServices::IAnalysisSession> AnalysisSession);
	bool HasValidFilterSession() const;
		
	void RefreshTreeviewData();
		
	/** Sets the treeview item expansion state recursively for the specified object */
	void SetExpansionRecursively(const TSharedPtr<ITraceObject>& InObject, bool bShouldExpandItem) const;
	void SetParentExpansionRecursively(const TSharedPtr<ITraceObject>& InObject, bool bShouldExpandItem) const;	

	/** Enumerates the (currently selected) treeview items and calls InFunction for each entry */
	void EnumerateSelectedItems(TFunction<void(TSharedPtr<ITraceObject> InItem)> InFunction) const;
	void EnumerateAllItems(TFunction<void(TSharedPtr<ITraceObject> InItem)> InFunction) const;
	bool EnumerateAllItems(TFunction<bool(TSharedPtr<ITraceObject> InItem)> InFunction) const;
	bool EnumerateSelectedItems(TFunction<bool(TSharedPtr<ITraceObject> InItem)> InFunction) const;

	/** Creates and adds a new treeview object entry, given the specified data */
	TSharedRef<ITraceObject> AddFilterableObject(const FTraceObjectInfo& Event, FString ParentName);
	
	/** Treeview callbacks */
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<ITraceObject> InItem, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedPtr<SWidget> OnContextMenuOpening() const;

	/** Presets dropdown button callback */
	TSharedRef<SWidget> MakeAddFilterMenu();

	/** (Re) storing item expansion / selection state */
	void SaveItemsExpansion();
	void SaveItemSelection();
	void RestoreItemsExpansion();	
	void RestoreItemSelection();
protected:
	/** Root level items used by treeview */
	TArray<TSharedPtr<ITraceObject>> RootItems;
	/** Dynamically generated array according to treeview filtering */
	TArray<TSharedPtr<ITraceObject>> TreeItems;
	/** Flat list of contained treeview items */
	TArray<TSharedPtr<ITraceObject>> FlatItems;
	/** Parent child relationships */
	TMap<TSharedPtr<ITraceObject>, TArray<TSharedPtr<ITraceObject>>> ParentToChild;
	TMap<TSharedPtr<ITraceObject>, TSharedPtr<ITraceObject>> ChildToParent;

	/** Analysis filter service for the current analysis session this window is currently representing */
	TSharedPtr<ISessionTraceFilterService> SessionFilterService;
	uint32 PreviousSessionHandle;
	/** Timestamp used for refreshing cached filter data */
	FDateTime SyncTimeStamp = 0;
	
	/** Wrapper for presets drop down button */
	TSharedPtr<SHorizontalBox> OptionsWidget;
	
	/** Treeview widget, represents the currently connected analysis session its filtering state */
	TSharedPtr<STreeView<TSharedPtr<ITraceObject>>> Treeview;
	TSharedPtr<TreeFilterHandler<TSharedPtr<ITraceObject>>> TreeviewFilterHandler;

	/** Flag indicating the treeview should be refreshed */
	bool bNeedsTreeRefresh;

	/** Search box for filtering the Treeview items */
	TSharedPtr<SSearchBox> SearchBoxWidget;
	/** The text filter used by the search box */
	TSharedPtr<TTextFilter<TSharedPtr<ITraceObject>>> SearchBoxWidgetFilter;

	/** Filter presets bar widget */
	TSharedPtr<SFilterPresetList> FilterPresetsListWidget;
	
	/** Widget used for selecting specific AnalysisSession the treeview should represent the filter state for */
	TSharedPtr<SSessionSelector> SessionSelector;

	/** Whether or not we a currently highlighting a preset */
	bool bHighlightingPreset;

	/** Cached state of which named entries were expanded / selected */
	TSet<FString> ExpandedObjectNames;
	TSet<FString> SelectedObjectNames;

	/** Inter-window cached state of last expansion state */
	static TSet<FString> LastExpandedObjectNames;
};
