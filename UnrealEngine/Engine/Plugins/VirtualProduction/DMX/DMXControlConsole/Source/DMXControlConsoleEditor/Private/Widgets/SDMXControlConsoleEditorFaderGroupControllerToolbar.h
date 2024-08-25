// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/SDMXControlConsoleEditorExpandArrowButton.h"

class SFilterSearchBox;
class UDMXControlConsoleEditorModel;
class UDMXControlConsoleFaderGroupController;


namespace UE::DMX::Private
{
	class FDMXControlConsoleFaderGroupControllerModel;
	class SDMXControlConsoleEditorFaderGroupControllerComboBox;

	/** Toolbar widget for the Fader Group view */
	class SDMXControlConsoleEditorFaderGroupControllerToolbar
		: public SCompoundWidget
	{
	public:
		DECLARE_DELEGATE_RetVal(bool, FDMXFaderGroupControllerToolbarRetValDelegate)

		SLATE_BEGIN_ARGS(SDMXControlConsoleEditorFaderGroupControllerToolbar)
			{}
			/** Executed when the Fader Group Controller View is expanded */
			SLATE_EVENT(FDMXControleConsolEditorExpandArrowButtonDelegate, OnExpanded)

			/** Executed to get wheter expanded view mdode is enabled or not */
			SLATE_EVENT(FDMXFaderGroupControllerToolbarRetValDelegate, IsExpandedViewModeEnabled)

		SLATE_END_ARGS()

		/** Constructs the widget */
		void Construct(const FArguments& InArgs, const TWeakPtr<FDMXControlConsoleFaderGroupControllerModel>& InFaderGroupControllerModel, UDMXControlConsoleEditorModel* InEditorModel);

		/** Generates the Fader Group Controller settings menu widget content */
		TSharedRef<SWidget> GenerateSettingsMenuWidget();

		/** Gets a reference to this widget's ExpandArrow button */
		TSharedPtr<SDMXControlConsoleEditorExpandArrowButton> GetExpandArrowButton() const { return ExpandArrowButton; }

	private:
		/** Gets reference to the Fader Group Controller */
		UDMXControlConsoleFaderGroupController* GetFaderGroupController() const;

		/** Generates a menu widget for the Fader Group Controller info panel */
		TSharedRef<SWidget> GenerateFaderGroupControllerInfoMenuWidget();

		/** Restores the search filter text from the Fader Group Controller */
		void RestoreFaderGroupControllerFilter();

		/** Called when the search text changed */
		void OnSearchTextChanged(const FText& SearchText);

		/** Called to generate the Fader Group Controller Info Panel */
		void OnGetInfoPanel();

		/** Called to select all Element Controllers in the Fader Group Controller */
		void OnSelectAllElementControllers() const;

		/** Called when the duplicate option is selected */
		void OnDuplicateFaderGroupController() const;

		/** Gets wheter the duplicate option is allowed or not */
		bool CanDuplicateFaderGroupController() const;

		/** Called when the remove option is selected */
		void OnRemoveFaderGroupController() const;

		/** Gets wheter the remove option is allowed or not */
		bool CanRemoveFaderGroupController() const;

		/** Called when the group option is selected */
		void OnGroupFaderGroupControllers() const;

		/** Called when the ungroup option is selected */
		void OnUngroupFaderGroupControllers() const;

		/** Gets wheter the selected Fader Group Controllers can be grouped or not */
		bool CanGroupFaderGroupControllers() const;

		/** Gets wheter the selected Fader Group Controllers can be ungrouped or not */
		bool CanUngroupFaderGroupControllers() const;

		/** Called when the reset option is selected */
		void OnResetFaderGroupController() const;

		/** Called when the lock option is selected */
		void OnLockFaderGroupController(bool bLock) const;

		/** Gets the visibility for the toolbar sections visible only in expanded view mode */
		EVisibility GetExpandedViewModeVisibility() const;

		/** Gets the visibility for the toolbar serch box */
		EVisibility GetSearchBoxVisibility() const;

		/** Expander arrow button for showing/hiding the Element Controller views */
		TSharedPtr<SDMXControlConsoleEditorExpandArrowButton> ExpandArrowButton;

		/** Reference to the Fader Group Controller toolbar searchbox used for filtering */
		TSharedPtr<SFilterSearchBox> ToolbarSearchBox;

		/** A ComboBox for showing all active Fixture Patches in the current DMX Library */
		TSharedPtr<SDMXControlConsoleEditorFaderGroupControllerComboBox> ControllerComboBox;

		/** Weak Reference to the Fader Group Controller model */
		TWeakPtr<FDMXControlConsoleFaderGroupControllerModel> WeakFaderGroupControllerModel;

		/** Weak reference to the Control Console editor model */
		TWeakObjectPtr<UDMXControlConsoleEditorModel> EditorModel;

		// Slate Arguments
		FDMXFaderGroupControllerToolbarRetValDelegate IsExpandedViewModeEnabledDelegate;
	};
}
