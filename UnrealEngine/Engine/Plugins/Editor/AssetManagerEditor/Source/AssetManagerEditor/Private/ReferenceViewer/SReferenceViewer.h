// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "GraphEditor.h"
#include "AssetRegistry/AssetData.h"
#include "HistoryManager.h"
#include "CollectionManagerTypes.h"
#include "AssetManagerEditorModule.h"
#include "Containers/ArrayView.h"
#include "ReferenceViewer/ReferenceViewerSettings.h"
#include "ReferenceViewer/EdGraph_ReferenceViewer.h"
#include "ReferenceViewer/SReferenceViewerFilterBar.h"

class UEdGraph;

/**
 * 
 */
class SReferenceViewer : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SReferenceViewer ){}

	SLATE_END_ARGS()

	~SReferenceViewer();

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs );

	/**
	 * Sets a new root package name
	 *
	 * @param NewGraphRootIdentifiers	The root elements of the new graph to be generated
	 * @param ReferenceViewerParams		Different visualization settings, such as whether it should display the referencers or the dependencies of the NewGraphRootIdentifiers
	 */
	void SetGraphRootIdentifiers(const TArray<FAssetIdentifier>& NewGraphRootIdentifiers, const FReferenceViewerParams& ReferenceViewerParams = FReferenceViewerParams());

	/** Gets graph editor */
	TSharedPtr<SGraphEditor> GetGraphEditor() const { return GraphEditorPtr; }

	/** Called when the current registry source changes */
	void SetCurrentRegistrySource(const FAssetManagerEditorRegistrySource* RegistrySource);

	/**SWidget interface **/
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

private:

	/** Call after a structural change is made that causes the graph to be recreated */
	void RebuildGraph();

	/** Called to create context menu when right-clicking on graph */
	FActionMenuContent OnCreateGraphActionMenu(UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed);

	/** Called when a node is double clicked */
	void OnNodeDoubleClicked(class UEdGraphNode* Node);

	/** True if the user may use the history back button */
	bool IsBackEnabled() const;

	/** True if the user may use the history forward button */
	bool IsForwardEnabled() const;

	/** Handler for clicking the history back button */
	void BackClicked();

	/** Handler for clicking the history forward button */
	void ForwardClicked();

	/** Refresh the current view */
	void RefreshClicked();

	/** Handler for when the graph panel tells us to go back in history (like using the mouse thumb button) */
	void GraphNavigateHistoryBack();

	/** Handler for when the graph panel tells us to go forward in history (like using the mouse thumb button) */
	void GraphNavigateHistoryForward();

	/** Gets the tool tip text for the history back button */
	FText GetHistoryBackTooltip() const;

	/** Gets the tool tip text for the history forward button */
	FText GetHistoryForwardTooltip() const;

	/** Gets the text to be displayed in the address bar */
	FText GetAddressBarText() const;

	/** Gets the text to be displayed for warning/status updates */
	FText GetStatusText() const;

	/** Called when the path is being edited */
	void OnAddressBarTextChanged(const FText& NewText);

	/** Sets the new path for the viewer */
	void OnAddressBarTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);

	void OnApplyHistoryData(const FReferenceViewerHistoryData& History);

	void OnUpdateHistoryData(FReferenceViewerHistoryData& HistoryData) const;

	void OnUpdateFilterBar();
	
	void OnSearchDepthEnabledChanged( ECheckBoxState NewState );
	ECheckBoxState IsSearchDepthEnabledChecked() const;

	int32 GetSearchReferencerDepthCount() const;
	int32 GetSearchDependencyDepthCount() const;

	void OnSearchReferencerDepthCommitted(int32 NewValue);
	void OnSearchDependencyDepthCommitted(int32 NewValue);

	void OnSearchBreadthEnabledChanged( ECheckBoxState NewState );
	ECheckBoxState IsSearchBreadthEnabledChecked() const;

	void OnEnableCollectionFilterChanged(ECheckBoxState NewState);
	ECheckBoxState IsEnableCollectionFilterChecked() const;
	TSharedRef<SWidget> GenerateCollectionFilterItem(TSharedPtr<FName> InItem);
	void UpdateCollectionsComboList();
	void HandleCollectionFilterChanged(TSharedPtr<FName> Item, ESelectInfo::Type SelectInfo);
	FText GetCollectionFilterText() const;

	void OnShowSoftReferencesChanged();
	bool IsShowSoftReferencesChecked() const;
	void OnShowHardReferencesChanged();
	bool IsShowHardReferencesChecked() const;
	void OnShowEditorOnlyReferencesChanged();
	bool IsShowEditorOnlyReferencesChecked() const;

	void OnShowFilteredPackagesOnlyChanged();
	bool IsShowFilteredPackagesOnlyChecked() const;
	void UpdateIsPassingSearchFilterCallback();

	void OnCompactModeChanged();
	bool IsCompactModeChecked() const;

	void OnShowDuplicatesChanged();
	bool IsShowDuplicatesChecked() const;

	bool GetManagementReferencesVisibility() const;
	void OnShowManagementReferencesChanged();
	bool IsShowManagementReferencesChecked() const;
	void OnShowSearchableNamesChanged();
	bool IsShowSearchableNamesChecked() const;
	void OnShowCodePackagesChanged();
	bool IsShowCodePackagesChecked() const;

	int32 GetSearchBreadthCount() const;
	void OnSearchBreadthCommitted(int32 NewValue);

	TSharedRef<SWidget> GetShowMenuContent();

	void RegisterActions();
	void ShowSelectionInContentBrowser();
	void OpenSelectedInAssetEditor();
	void ReCenterGraph();
	void CopyReferencedObjects();
	void CopyReferencingObjects();
	void ShowReferencedObjects();
	void ShowReferencingObjects();
	void MakeCollectionWithReferencersOrDependencies(ECollectionShareType::Type ShareType, bool bReferencers);
	void ShowReferenceTree();
	void ViewSizeMap();
	void ViewAssetAudit();
	void ZoomToFit();
	bool CanZoomToFit() const;
	void OnFind();

	/** Handlers for searching */
	void HandleOnSearchTextChanged(const FText& SearchText);
	void HandleOnSearchTextCommitted(const FText& SearchText, ETextCommit::Type CommitType);

	void ReCenterGraphOnNodes(const TSet<UObject*>& Nodes);

	FString GetReferencedObjectsList() const;
	FString GetReferencingObjectsList() const;

	UObject* GetObjectFromSingleSelectedNode() const;
	void GetPackageNamesFromSelectedNodes(TSet<FName>& OutNames) const;
	bool HasExactlyOneNodeSelected() const;
	bool HasExactlyOnePackageNodeSelected() const;
	bool HasAtLeastOnePackageNodeSelected() const;
	bool HasAtLeastOneRealNodeSelected() const;

	void OnAssetRegistryChanged(const FAssetData& AssetData);
	void OnInitialAssetRegistrySearchComplete();
	EActiveTimerReturnType TriggerZoomToFit(double InCurrentTime, float InDeltaTime);
private:

	TSharedRef<SWidget> MakeToolBar();


	/** The manager that keeps track of history data for this browser */
	FReferenceViewerHistoryManager HistoryManager;

	TSharedPtr<SGraphEditor> GraphEditorPtr;

	TSharedPtr<FUICommandList> ReferenceViewerActions;
	TSharedPtr<SSearchBox> SearchBox;
	TSharedPtr<SWidget> ReferencerCountBox;
	TSharedPtr<SWidget> DependencyCountBox;
	TSharedPtr<SWidget> BreadthLimitBox;

	TSharedPtr< SReferenceViewerFilterBar > FilterWidget;

	UEdGraph_ReferenceViewer* GraphObj;

	UReferenceViewerSettings* Settings;

	/** The temporary copy of the path text when it is actively being edited. */
	FText TemporaryPathBeingEdited;

	/** Combo box for collections filter options */
	TSharedPtr<SComboBox<TSharedPtr<FName>>> CollectionsCombo;

	/** List of collection filter options */
	TArray<TSharedPtr<FName>> CollectionsComboList;

	/**
	 * Whether to visually show to the user the option of "Search Depth Limit" or hide it and fix it to a default value:
	 * - If 0 or negative, it will show to the user the option of "Search Depth Limit".
	 * - If >0, it will hide that option and fix the Depth value to this value.
	 */
	int32 FixAndHideSearchDepthLimit;
	/**
	 * Whether to visually show to the user the option of "Search Breadth Limit" or hide it and fix it to a default value:
	 * - If 0 or negative, it will show to the user the option of "Search Breadth Limit".
	 * - If >0, it will hide that option and fix the Breadth value to this value.
	 */
	int32 FixAndHideSearchBreadthLimit;
	/** Whether to visually show to the user the option of "Collection Filter" */
	bool bShowCollectionFilter;
	/** Whether to visually show to the user the options of "Show Soft/Hard/Management References" */
	bool bShowShowReferencesOptions;
	/** Whether to visually show to the user the option of "Show Searchable Names" */
	bool bShowShowSearchableNames;
	/** Whether to visually show to the user the option of "Show C++ Packages" */
	bool bShowShowCodePackages;
	/** Whether to visually show to the user the option of "Show Filtered Packages Only" */
	bool bShowShowFilteredPackagesOnly;
	/** True if our view is out of date due to asset registry changes */
	bool bDirtyResults;
	/** Whether to visually show to the user the option of "Compact Mode" */
	bool bShowCompactMode;

	/** A recursion check so as to avoid the rebuild of the graph if we are currently rebuilding the filters */
	bool bRebuildingFilters;

	/** Used to delay graph rebuilding during spinbox slider interaction */
	bool bNeedsGraphRebuild;
	double SliderDelayLastMovedTime = 0.0;
	double GraphRebuildSliderDelay = 0.25;

	/** Handle to know if dirty */
	FDelegateHandle AssetRefreshHandle;
};

enum class EDependencyPinCategory
{
	LinkEndPassive = 0,
	LinkEndActive = 1,
	LinkEndMask = LinkEndActive,

	LinkTypeNone = 0,
	LinkTypeUsedInGame = 2,
	LinkTypeHard = 4,
	LinkTypeMask = LinkTypeHard | LinkTypeUsedInGame,
};
ENUM_CLASS_FLAGS(EDependencyPinCategory);

extern EDependencyPinCategory ParseDependencyPinCategory(FName PinCategory);
extern FLinearColor GetColor(EDependencyPinCategory Category);
extern FName GetName(EDependencyPinCategory Category);
