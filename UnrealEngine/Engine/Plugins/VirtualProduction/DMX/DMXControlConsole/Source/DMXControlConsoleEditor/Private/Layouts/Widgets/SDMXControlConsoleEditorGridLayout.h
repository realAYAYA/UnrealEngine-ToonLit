// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Layouts/Widgets/SDMXControlConsoleEditorLayout.h"

struct EVisibility;
class SScrollBox;
class SVerticalBox;
class UDMXControlConsoleEditorGlobalLayoutRow;
class UDMXControlConsoleFaderGroupController;


namespace UE::DMX::Private
{ 
	class SDMXControlConsoleEditorLayoutRowView;

	/** Draws the fader groups of a control console in a grid */
	class SDMXControlConsoleEditorGridLayout
		: public SDMXControlConsoleEditorLayout
	{
	public:
		SLATE_BEGIN_ARGS(SDMXControlConsoleEditorGridLayout)
		{}

		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, UDMXControlConsoleEditorGlobalLayoutBase* InLayout, UDMXControlConsoleEditorModel* InEditorModel);

	protected:
		//~ Begin SDMXControlConsoleEditorLayout interface
		virtual bool CanRefresh() const override;
		virtual void OnLayoutElementAdded() override;
		virtual void OnLayoutElementRemoved() override;
		//~ End SDMXControlConsoleEditorLayout interface

	private:
		/** Checks if the LayoutRowViews array contains a reference to the given */
		bool IsLayoutRowContained(UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow);

		/** Finds LayoutRowView by the given LayoutRow, if valid */
		TSharedPtr<SDMXControlConsoleEditorLayoutRowView> FindLayoutRowView(const UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow);

		/** Called when a LayoutRowView needs to be scrolled into view */
		void OnScrollIntoView(const UDMXControlConsoleFaderGroupController* FaderGroupController);

		/** Gets visibility for each LayoutRowView widget */
		EVisibility GetLayoutRowViewVisibility(TWeakObjectPtr<UDMXControlConsoleEditorGlobalLayoutRow> LayoutRow) const;

		/** Reference to the container widget of this DMX Control Console's Layout Rows slots */
		TSharedPtr<SVerticalBox> LayoutRowsVerticalBox;

		/** Reference to the horizontal ScrollBox widget */
		TSharedPtr<SScrollBox> HorizontalScrollBox;

		/** Reference to the vertical ScrollBox widget */
		TSharedPtr<SScrollBox> VerticalScrollBox;

		/** Array of weak references to the Layout Row widgets */
		TArray<TWeakPtr<SDMXControlConsoleEditorLayoutRowView>> LayoutRowViews;
	};
}
