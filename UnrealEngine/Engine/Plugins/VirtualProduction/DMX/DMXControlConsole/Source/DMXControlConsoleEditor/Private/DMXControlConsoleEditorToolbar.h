// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

enum class ECheckBoxState : uint8;
enum class EDMXControlConsoleEditorViewMode : uint8;
enum class EDMXControlConsoleLayoutMode : uint8;
struct FCustomTextFilterData;
class FExtender;
struct FSlateIcon;
class FToolBarBuilder;
class SDMXControlConsoleEditorPortSelector;
class SFilterSearchBox;
class SWidget;
class SWindow;


namespace UE::DMX::Private
{
	class FDMXControlConsoleEditorToolkit;

	/** Custom toolbar for the DMX Control Console editor */
	class FDMXControlConsoleEditorToolbar :
		public TSharedFromThis<FDMXControlConsoleEditorToolbar>
	{
	public:
		/** Constructor */
		FDMXControlConsoleEditorToolbar(TSharedPtr<FDMXControlConsoleEditorToolkit> InToolkit);

		/** Builds the toolbar */
		void BuildToolbar(TSharedPtr<FExtender> Extender);

		/** Gets the toolbar's global filter searchbox widget */
		const TSharedPtr<SFilterSearchBox>& GetFilterSearchBox() const { return GlobalFilterSearchBox; }

	private:
		/** Callback, raised when the menu extender requests to build the toolbar */
		void BuildToolbarCallback(FToolBarBuilder& ToolbarBuilder);

		/** Generates a play options menu widget */
		TSharedRef<SWidget> GeneratePlayOptionsMenuWidget();

		/** Generates a widget for the clear options */
		TSharedRef<SWidget> GenerateClearMenuWidget();

		/** Generates a widget to select the current control mode */
		TSharedRef<SWidget> GenerateControlModeMenuWidget();

		/** Generates a widget to select the current view mode */
		TSharedRef<SWidget> GenerateViewModeMenuWidget();

		/** Generates a widget for the selection options */
		TSharedRef<SWidget> GenerateSelectionMenuWidget();

		/** Generates a widget to select the current layout mode */
		TSharedRef<SWidget> GenerateLayoutModeMenuWidget();

		/** Restores the global search filter text from Constrol Console Data */
		void RestoreGlobalFilter();

		/** Called when the search text has changed */
		void OnSearchTextChanged(const FText& SearchText);

		/** Called when the save search button is clicked */
		void OnSaveSearchButtonClicked(const FText& InSearchText);

		/** Called to create a custom filter from the filtering data */
		void OnCreateCustomTextFilter(const FCustomTextFilterData& InFilterData, bool bApplyFilter);

		/** Called when the cancel button in the custom filter window is clicked */
		void OnCancelCustomFilterWindowClicked();

		/** Called when the Port selection has changed */
		void OnSelectedPortsChanged();

		/** Called to get the filtered Elements auto-selection state */
		ECheckBoxState IsFilteredElementsAutoSelectChecked() const;

		/** Called to set the filtered Elements auto-selection state */
		void OnFilteredElementsAutoSelectStateChanged(ECheckBoxState CheckBoxState);

		/** Called when a Fader Groups view mode is selected */
		void OnFaderGroupsViewModeSelected(const EDMXControlConsoleEditorViewMode ViewMode) const;

		/** Called when a Faders view mode is selected */
		void OnFadersViewModeSelected(const EDMXControlConsoleEditorViewMode ViewMode) const;

		/** Called when a Layout mode is selected */
		void OnLayoutModeSelected(const EDMXControlConsoleLayoutMode LayoutMode) const;

		/** True if the current layout mode matches the given one */
		bool IsCurrentLayoutMode(const EDMXControlConsoleLayoutMode LayoutMode) const;

		/** Called when a Selection option is selected */
		void OnSelectAll(bool bOnlyMatchingFilter = false) const;

		/** Called when the Control Console gets cleared */
		void OnClearAll();

		/** Gets the text for the send dmx button */
		FText GetSendDMXButtonText() const;

		/** Gets the icon for the send dmx button */
		FSlateIcon GetSendDMXButtonIcon() const;

		/** Reference to the Control Console's searchbox used for global filtering */
		TSharedPtr<SFilterSearchBox> GlobalFilterSearchBox;

		/** Widget to handle the Port selection */
		TSharedPtr<SDMXControlConsoleEditorPortSelector> PortSelector;

		/** Weak reference to the window containing the custom text filter dialog */
		TWeakPtr<SWindow> WeakCustomTextFilterWindow;

		/** Weak reference to the Control Console editor toolkit */
		TWeakPtr<FDMXControlConsoleEditorToolkit> WeakToolkit;
	};
}
