// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorUndoClient.h"
#include "Engine/EngineTypes.h"
#include "Widgets/SCompoundWidget.h"

enum class ECheckBoxState : uint8;
enum class EDMXControlConsoleEditorViewMode : uint8;
enum class EDMXControlConsoleLayoutMode : uint8;
struct FSlateIcon;
class FUICommandList;
class IDetailsView;
class SDMXControlConsoleEditorFixturePatchVerticalBox;
class SDMXControlConsoleEditorPortSelector;
class SDockTab;
class SHorizontalBox;
class SScrollBox;
class SSearchBox;
class UDMXControlConsoleData;
class UDMXControlConsoleEditorLayouts;
class UDMXControlConsoleEditorModel;

namespace UE::DMXControlConsoleEditor::Layout::Private { class SDMXControlConsoleEditorLayout; }


/** Widget for the DMX Control Console */
class SDMXControlConsoleEditorView
	: public SCompoundWidget
	, public FSelfRegisteringEditorUndoClient
{
public:
	SLATE_BEGIN_ARGS(SDMXControlConsoleEditorView)
	{}

	SLATE_END_ARGS()

	/** Destructor */
	~SDMXControlConsoleEditorView();

	/** Constructs the widget */
	void Construct(const FArguments& InArgs);

	/** Gets DMX Control Console Editor Model instance reference */
	UDMXControlConsoleEditorModel& GetEditorConsoleModel() const;

	/** Gets current DMX Control Console Data */
	UDMXControlConsoleData* GetControlConsoleData() const;

	/** Gets current DMX Control Console Layouts */
	UDMXControlConsoleEditorLayouts* GetControlConsoleLayouts() const;

protected:
	//~ Begin SWidget interface
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	//~ End of SWidget interface
	
	//~ Begin FSelfRegisteringEditorUndoClient interface
	virtual bool MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const override;
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	//~ End FSelfRegisteringEditorUndoClient interface

private:
	/** Registers commands for this view */
	void RegisterCommands();

	/** Generates the toolbar for this view */
	TSharedRef<SWidget> GenerateToolbar();

	/** Generates a widget to select the current control mode */
	TSharedRef<SWidget> GenerateControlModeMenuWidget();

	/** Generates a widget to select the current view mode */
	TSharedRef<SWidget> GenerateViewModeMenuWidget();

	/** Generates a widget for selection options */
	TSharedRef<SWidget> GenerateSelectionMenuWidget();

	/** Generates a widget to select the current layout mode */
	TSharedRef<SWidget> GenerateLayoutModeMenuWidget();

	/** Restores global search filter text from Constrol Console Data */
	void RestoreGlobalFilter();

	/** Requests to update the Details Views on the next tick */
	void RequestUpdateDetailsViews();

	/** Updates the Details Views */
	void ForceUpdateDetailsViews();

	/** Updates Control Console layout, according to current layout mode */
	void UpdateLayout();

	/** Updates FixturePatchVerticalBox widget */
	void UpdateFixturePatchVerticalBox();

	/** Called when the search text changed */
	void OnSearchTextChanged(const FText& SearchText);

	/** Called to get filtered Elements auto-selection state */
	ECheckBoxState IsFilteredElementsAutoSelectChecked() const;

	/** Called to set filtered Elements auto-selection state */
	void OnFilteredElementsAutoSelectStateChanged(ECheckBoxState CheckBoxState);

	/** Called when a Fader Groups view mode is selected */
	void OnFaderGroupsViewModeSelected(const EDMXControlConsoleEditorViewMode ViewMode) const;

	/** Called when a Faders view mode is selected */
	void OnFadersViewModeSelected(const EDMXControlConsoleEditorViewMode ViewMode) const;

	/** Called when a Layout mode is selected */
	void OnLayoutModeSelected(const EDMXControlConsoleLayoutMode LayoutMode) const;

	/** True if the current layout mode matches the given one */
	bool IsCurrentLayoutMode(const EDMXControlConsoleLayoutMode LayoutMode) const;

	/** True if the current layout widget's type name matches the given one */
	bool IsCurrentLayoutWidgetType(const FName& InWidgetTypeName) const;

	/** Called when a Selection option is selected */
	void OnSelectAll(bool bOnlyMatchingFilter = false) const;

	/** Called when Control Console gets cleared */
	void OnClearAll();

	/** Called when Port selection changes */
	void OnSelectedPortsChanged();

	/** Called when a Property has changed in current Control Console Data */
	void OnControlConsoleDataPropertyChanged(const FPropertyChangedEvent& PropertyChangedEvent);

	/** Called when the browse to asset button was clicked */
	void OnBrowseToAssetClicked();

	/** Called when a console was loaded */
	void OnConsoleLoaded();

	/** Called when a console was saved */
	void OnConsoleSaved();

	/** Called when a console was refreshed */
	void OnConsoleRefreshed();

	/** Called when the DMX Library in use changed */
	void OnDMXLibraryChanged();

	/** Called when the active tab in the editor changes */
	void OnActiveTabChanged(TSharedPtr<SDockTab> PreviouslyActive, TSharedPtr<SDockTab> NewlyActivated);

	/** Searches this widget's parents to see if it's a child of InDockTab */
	bool IsWidgetInTab(TSharedPtr<SDockTab> InDockTab, TSharedPtr<SWidget> InWidget) const;

	/** Gets the text for the send dmx button */
	FText GetSendDMXButtonText() const;

	/** Gets the icon for the send dmx button */
	FSlateIcon GetSendDMXButtonIcon() const;

	/** Gets Details Views section visibility */
	EVisibility GetDetailViewsSectionVisibility() const;

	/** Reference to Control Console current layout widget */
	TSharedPtr<UE::DMXControlConsoleEditor::Layout::Private::SDMXControlConsoleEditorLayout> Layout;

	/** Reference to layout container box */
	TSharedPtr<SHorizontalBox> LayoutBox;

	/** Reference to FixturePatchRows widgets container */
	TSharedPtr<SDMXControlConsoleEditorFixturePatchVerticalBox> FixturePatchVerticalBox;

	/** Reference to Control Console searchbox used for global filtering */
	TSharedPtr<SSearchBox> GlobalFilterSearchBox;

	/** Widget to handle Port selection */
	TSharedPtr<SDMXControlConsoleEditorPortSelector> PortSelector;

	/** Shows DMX Control Console Data's details */
	TSharedPtr<IDetailsView> ControlConsoleDataDetailsView;

	/** Shows details of the current selected Fader Groups */
	TSharedPtr<IDetailsView> FaderGroupsDetailsView;

	/** Shows details of the current selected Faders */
	TSharedPtr<IDetailsView> FadersDetailsView;

	/** Delegate handle bound to the FGlobalTabmanager::OnActiveTabChanged delegate */
	FDelegateHandle OnActiveTabChangedDelegateHandle;

	/** Timer handle in use while updating details views is requested but not carried out yet */
	FTimerHandle UpdateDetailsViewTimerHandle;

	/** Command list for the Control Console Editor View */
	TSharedPtr<FUICommandList> CommandList;
};
