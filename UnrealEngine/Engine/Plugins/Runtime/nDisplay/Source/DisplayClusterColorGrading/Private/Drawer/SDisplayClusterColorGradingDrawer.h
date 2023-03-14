// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorUndoClient.h"
#include "Styling/SlateTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

#include "DisplayClusterColorGradingDrawerState.h"
#include "SDisplayClusterColorGradingObjectList.h"
#include "SDisplayClusterColorGradingColorWheel.h"

class ADisplayClusterRootActor;
class FDisplayClusterOperatorStatusBarExtender;
class FDisplayClusterColorGradingDataModel;
class IDisplayClusterOperatorViewModel;
class IPropertyRowGenerator;
class SHorizontalBox;
class SDisplayClusterColorGradingColorWheelPanel;
class SDisplayClusterColorGradingDetailsPanel;

/** Color grading drawer widget, which displays a list of color gradable items, and the color wheel panel */
class SDisplayClusterColorGradingDrawer : public SCompoundWidget, public FEditorUndoClient
{
public:
	~SDisplayClusterColorGradingDrawer();

	SLATE_BEGIN_ARGS(SDisplayClusterColorGradingDrawer)
	{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, bool bInIsInDrawer);

	/** Refreshes the drawer's UI to match the current state of the level and active root actor, optionally preserving UI state */
	void Refresh(bool bPreserveDrawerState = false);

	//~ SWidget interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	//~ End SWidget interface

	//~ FEditorUndoClient interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	//~ End FEditorUndoClient interface

	/** Gets the state of the drawer UI */
	FDisplayClusterColorGradingDrawerState GetDrawerState() const;

	/** Sets the state of the drawer UI */
	void SetDrawerState(const FDisplayClusterColorGradingDrawerState& InDrawerState);

	/** Sets the state of the drawer UI to its default value, which is to have the nDisplay stage actor selected */
	void SetDrawerStateToDefault();

private:
	/** Creates the button used to dock the drawer in the operator panel */
	TSharedRef<SWidget> CreateDockInLayoutButton();

	/** Gets the display name of the color grading list at the specified index */
	FText GetColorGradingListName(int32 ListIndex) const;

	/** Gets the name of the current level the active root actor is in */
	FText GetCurrentLevelName() const;

	/** Gets the name of the active root actor */
	FText GetCurrentRootActorName() const;

	/** Binds a callback to the BlueprintCompiled delegate of the specified class */
	void BindBlueprintCompiledDelegate(const UClass* Class);

	/** Unbinds a callback to the BlueprintCompiled delegate of the specified class */
	void UnbindBlueprintCompiledDelegate(const UClass* Class);

	/** Refreshes all of the lists in the drawer, filling them with the current color gradable objects from the root actor and world  */
	void RefreshColorGradingLists();

	/** Refreshes the specified list, filling it with the current color gradable objects from the relevant source */
	void RefreshColorGradingList(int32 Index);

	/** Fills the specified list with any color gradable items found in the currently loaded level */
	void FillLevelColorGradingList(TArray<FDisplayClusterColorGradingListItemRef>& InOutList);

	/** Fills the specified list with any color gradable components of the active root actor */
	void FillRootActorColorGradingList(TArray<FDisplayClusterColorGradingListItemRef>& InOutList);

	/** Fills the specified list with any color gradable color correction regions in the currently loaded level */
	void FillColorCorrectionRegionColorGradingList(TArray<FDisplayClusterColorGradingListItemRef>& InOutList);

	/** Updates the color grading data model with the specified list of objects */
	void SetColorGradingDataModelObjects(const TArray<UObject*>& Objects);

	/** Fills the color grading group toolbar using the color grading data model */
	void FillColorGradingGroupToolBar();

	/** Gets whether the specified drawer mode is currently selected */
	ECheckBoxState IsDrawerModeSelected(EDisplayClusterColorGradingDrawerMode InDrawerMode) const;

	/** Gets the visibility state for the specified drawer mode */
	EVisibility GetDrawerModeVisibility(EDisplayClusterColorGradingDrawerMode InDrawerMode) const;

	/** Raised when the specified drawer mode checked state is changed */
	void OnDrawerModeSelected(ECheckBoxState State, EDisplayClusterColorGradingDrawerMode InDrawerMode);

	/** Gets the visibility state of the color grading group toolbar */
	EVisibility GetColorGradingGroupToolBarVisibility() const;

	/** Gets whether the color grading group at the specified index is currently selected */
	ECheckBoxState IsColorGradingGroupSelected(int32 GroupIndex) const;

	/** Raised when the user has selected the specified color grading group */
	void OnColorGradingGroupCheckedChanged(ECheckBoxState State, int32 GroupIndex);

	/** Raised when the editor replaces any UObjects with new instantiations, usually when actors have been recompiled from blueprints */
	void OnObjectsReplaced(const TMap<UObject*, UObject*>& OldToNewInstanceMap);

	/** Raised when an actor is added to the current level */
	void OnLevelActorAdded(AActor* Actor);

	/** Raised when an actor has been deleted from the currnent level */
	void OnLevelActorDeleted(AActor* Actor);

	/** Raised when the specified blueprint has been recompiled */
	void OnBlueprintCompiled(UBlueprint* Blueprint);

	/** Raised when the user has changed the active root actor selected in the nDisplay operator panel */
	void OnActiveRootActorChanged(ADisplayClusterRootActor* NewRootActor);
	
	/** Raised when the obejcts being displayed in the operator's details panel have changed */
	void OnDetailObjectsChanged(const TArray<UObject*>& Objects);

	/** Raised when the color grading data model has been generated */
	void OnColorGradingDataModelGenerated();

	/** Raised when the user has selected a new item in any of the drawer's list views */
	void OnListSelectionChanged(TSharedRef<SDisplayClusterColorGradingObjectList> SourceList, FDisplayClusterColorGradingListItemRef SelectedItem, ESelectInfo::Type SelectInfo);

	/** Raised when the "Dock in Layout" button has been clicked */
	FReply DockInLayout();

private:
	/** The operator panel's view model */
	TSharedPtr<IDisplayClusterOperatorViewModel> OperatorViewModel;

	/** A list of all color grading object list widgets being displayed in the drawer's list panel */
	TArray<TSharedPtr<SDisplayClusterColorGradingObjectList>> ColorGradingObjectListViews;

	/** A list of source lists for the color grading object list widgets */
	TArray<TArray<FDisplayClusterColorGradingListItemRef>> ColorGradingItemLists;

	TSharedPtr<SHorizontalBox> ColorGradingGroupToolBarBox;
	TSharedPtr<SDisplayClusterColorGradingColorWheelPanel> ColorWheelPanel;
	TSharedPtr<SDisplayClusterColorGradingDetailsPanel> DetailsPanel;

	/** The color grading data model for the currently selected objects */
	TSharedPtr<FDisplayClusterColorGradingDataModel> ColorGradingDataModel;

	/** The current mode that the drawer is in */
	EDisplayClusterColorGradingDrawerMode CurrentDrawerMode;

	/** Gets whether this widget is in a drawer or docked in a tab */
	bool bIsInDrawer = false;

	/** Indicates that the drawer should refresh itself on the next tick */
	bool bRefreshOnNextTick = false;

	/** Indicates if the color grading data model should update when a list item selection has changed */
	bool bUpdateDataModelOnSelectionChanged = true;

	/** Delegate handle for the OnActiveRootActorChanged delegate */
	FDelegateHandle ActiveRootActorChangedHandle;
};