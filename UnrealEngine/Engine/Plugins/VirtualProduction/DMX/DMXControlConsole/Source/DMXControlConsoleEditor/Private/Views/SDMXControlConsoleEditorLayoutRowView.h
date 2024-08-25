// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Layouts/DMXControlConsoleEditorGlobalLayoutRow.h"
#include "Widgets/SCompoundWidget.h"

class SHorizontalBox;
class UDMXControlConsoleEditorModel;
class UDMXControlConsoleFaderGroup;
class UDMXControlConsoleFaderGroupController;


namespace UE::DMX::Private
{
	class FDMXControlConsoleFaderGroupControllerModel;
	class SDMXControlConsoleEditorFaderGroupControllerView;

	/** A widget which displays a collection of Fader Group Controllers */
	class SDMXControlConsoleEditorLayoutRowView
		: public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDMXControlConsoleEditorLayoutRowView)
			{}

		SLATE_END_ARGS()

		/** Constructs the widget */
		void Construct(const FArguments& InArgs, UDMXControlConsoleEditorGlobalLayoutRow* InLayoutRow, UDMXControlConsoleEditorModel* InEditorModel);

		/** Gets the Layout Row this row is based on */
		UDMXControlConsoleEditorGlobalLayoutRow* GetLayoutRow() const { return LayoutRow.Get(); }

		/** Finds the matching Fader Group Controller view by the given Fader Group Controller, if valid */
		TSharedPtr<SDMXControlConsoleEditorFaderGroupControllerView> FindFaderGroupControllerView(const UDMXControlConsoleFaderGroupController* FaderGroupController) const;

	protected:
		//~ Begin SWidget interface
		virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
		//~ End of SWidget interface

	private:
		/** Refreshes layout row */
		void Refresh();

		/** Should be called when a Fader Group Controller was added to the Layout Row this view displays */
		void OnFaderGroupControllerAdded();

		/** Adds a Fader Group Controller slot widget */
		void AddFaderGroupController(UDMXControlConsoleFaderGroupController* FaderGroupController);

		/** Should be called when a Fader Group Controller was deleted from the Layout Row this view displays */
		void OnFaderGroupControllerRemoved();

		/** Checks if the FaderGroupControllerViews array contains a reference to the given Fader Group Controller */
		bool ContainsFaderGroupController(const UDMXControlConsoleFaderGroupController* FaderGroupController);

		/** Gets visibility for each Fader Group Controller view in this row */
		EVisibility GetFaderGroupControllerViewVisibility(TSharedPtr<FDMXControlConsoleFaderGroupControllerModel> FaderGroupControllerModel) const;

		/** Reference to the container widget of this Layout Row's Fader Group Controller slots  */
		TSharedPtr<SHorizontalBox> FaderGroupControllersHorizontalBox;

		/** Array of weak references to Fader Group Controller views */
		TArray<TWeakPtr<SDMXControlConsoleEditorFaderGroupControllerView>> FaderGroupControllerViews;

		/** Weak Reference to this Layout Row */
		TWeakObjectPtr<UDMXControlConsoleEditorGlobalLayoutRow> LayoutRow;

		/** Weak reference to the Control Console editor model */
		TWeakObjectPtr<UDMXControlConsoleEditorModel> EditorModel;
	};
}
