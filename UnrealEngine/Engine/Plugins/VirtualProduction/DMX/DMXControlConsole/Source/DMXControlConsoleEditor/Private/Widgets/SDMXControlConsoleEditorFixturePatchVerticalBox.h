// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class FReply;
class UDMXControlConsoleEditorModel;


namespace UE::DMX::Private
{
	class SDMXControlConsoleFixturePatchList;

	/** A container for the Fixture Patch List widget */
	class SDMXControlConsoleEditorFixturePatchVerticalBox
		: public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDMXControlConsoleEditorFixturePatchVerticalBox)
			{}

		SLATE_END_ARGS()

		/** Constructs the widget */
		void Construct(const FArguments& InArgs, UDMXControlConsoleEditorModel* InEditorModel);

		/** Refreshes the widget */
		void ForceRefresh();

	protected:
		//~ Begin SWidget interface
		virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override { return FReply::Handled(); }
		//~ End SWidget interface

	private:
		/** Generates a toolbar for the FixturePatchList widget */
		TSharedRef<SWidget> GenerateFixturePatchListToolbar();

		/** Creates a menu for the Add Patch combo button */
		TSharedRef<SWidget> CreateAddPatchMenu();

		/** Creates a menu for the Add Empty combo button */
		TSharedRef<SWidget> CreateAddEmptyMenu();

		/** Called on Add All Patches button clicked to generate Fader Group Controllers form a Library */
		FReply OnAddAllPatchesClicked();

		/** Returns true if the 'Add Patch' buttons are enabled */
		bool IsAddPatchesButtonEnabled() const;

		/** Returns true if the 'Add Empty' button is enabled */
		bool IsAddEmptyButtonEnabled() const;

		/** Gets the visibility for the FixturePatchList toolbar  */
		EVisibility GetFixturePatchListToolbarVisibility() const;

		/** Reference to the FixturePatchList widget */
		TSharedPtr<SDMXControlConsoleFixturePatchList> FixturePatchList;

		/** Weak reference to the Control Console editor model */
		TWeakObjectPtr<UDMXControlConsoleEditorModel> EditorModel;
	};
}
