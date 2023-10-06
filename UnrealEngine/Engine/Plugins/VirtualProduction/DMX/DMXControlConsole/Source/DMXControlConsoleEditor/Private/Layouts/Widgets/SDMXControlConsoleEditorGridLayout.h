// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Layouts/Widgets/SDMXControlConsoleEditorLayout.h"

struct EVisibility;
class FReply;
class SDMXControlConsoleEditorLayoutRowView;
class SScrollBox;
class SVerticalBox;
class UDMXControlConsoleFaderGroup;
class UDMXControlConsoleEditorGlobalLayoutRow;


namespace UE::DMXControlConsoleEditor::Layout::Private
{ 
	/** A widget to describe control console grid layout sorting */
	class SDMXControlConsoleEditorGridLayout
		: public SDMXControlConsoleEditorLayout
	{
	public:
		SLATE_BEGIN_ARGS(SDMXControlConsoleEditorGridLayout)
		{}

		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, UDMXControlConsoleEditorGlobalLayoutBase* InLayout);

	protected:
		//~ Begin SDMXControlConsoleEditorLayout interface
		virtual bool CanRefresh() const override;
		virtual void OnLayoutElementAdded() override;
		virtual void OnLayoutElementRemoved() override;
		//~ End SDMXControlConsoleEditorLayout interface

	private:
		/** Checks if LayoutRowViews array contains a reference to the given */
		bool IsLayoutRowContained(UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow);

		/** Finds LayoutRowView by the given LayoutRow, if valid */
		TSharedPtr<SDMXControlConsoleEditorLayoutRowView> FindLayoutRowView(const UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow);

		/** Called to add the first Fader Group */
		FReply OnAddFirstFaderGroup();

		/** Called when a LayoutRowView needs to be scrolled into view */
		void OnScrollIntoView(const UDMXControlConsoleFaderGroup* FaderGroup);

		/** Gets visibility for each LayoutRowView widget */
		EVisibility GetLayoutRowViewVisibility(TWeakObjectPtr<UDMXControlConsoleEditorGlobalLayoutRow> LayoutRow) const;

		/** Gets add button visibility */
		EVisibility GetAddButtonVisibility() const;

		/** Reference to the container widget of this DMX Control Console's Layout Rows slots */
		TSharedPtr<SVerticalBox> LayoutRowsVerticalBox;

		/** Reference to horizontal ScrollBox widget */
		TSharedPtr<SScrollBox> HorizontalScrollBox;

		/** Reference to vertical ScrollBox widget */
		TSharedPtr<SScrollBox> VerticalScrollBox;

		/** Array of weak references to Layout Row widgets */
		TArray<TWeakPtr<SDMXControlConsoleEditorLayoutRowView>> LayoutRowViews;
	};
}
