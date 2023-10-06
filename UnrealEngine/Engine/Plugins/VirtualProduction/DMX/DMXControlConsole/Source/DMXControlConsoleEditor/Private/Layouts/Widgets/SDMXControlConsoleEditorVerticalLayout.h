// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Layouts/Widgets/SDMXControlConsoleEditorLayout.h"

struct EVisibility;
class FReply;
class SDMXControlConsoleEditorFaderGroupView;
class SScrollBox;
class SVerticalBox;
class UDMXControlConsoleFaderGroup;


namespace UE::DMXControlConsoleEditor::Layout::Private
{ 
	/** Model for control console grid layout */
	class SDMXControlConsoleEditorVerticalLayout
		: public SDMXControlConsoleEditorLayout
	{
	public:
		SLATE_BEGIN_ARGS(SDMXControlConsoleEditorVerticalLayout)
		{}

		SLATE_END_ARGS()

		/** Constructs the widget */
		void Construct(const FArguments& InArgs, UDMXControlConsoleEditorGlobalLayoutBase* InLayout);

	protected:
		//~ Begin SDMXControlConsoleEditorLayout interface
		virtual bool CanRefresh() const override;
		virtual void OnLayoutElementAdded() override;
		virtual void OnLayoutElementRemoved() override;
		//~ End SDMXControlConsoleEditorLayout interface

	private:
		/** Checks if FaderGroups array contains a reference to the given */
		bool IsFaderGroupContained(UDMXControlConsoleFaderGroup* FaderGroup);

		/** Called when the first fader group should be added */
		FReply OnAddFirstFaderGroup();

		/** Called when a FaderGroupView needs to be scrolled into view */
		void OnScrollIntoView(const UDMXControlConsoleFaderGroup* FaderGroup);

		/** Gets the visibility for each FaderGroupView widget in this row */
		EVisibility GetFaderGroupViewVisibility(TWeakObjectPtr<UDMXControlConsoleFaderGroup> FaderGroup) const;

		/** Gets the add button visibility */
		EVisibility GetAddButtonVisibility() const;

		/** The widget containing the FaderGroupViews */
		TSharedPtr<SVerticalBox> FaderGroupsVerticalBox;

		/** The vertical ScrollBox widget */
		TSharedPtr<SScrollBox> VerticalScrollBox;

		/** Array of weak references to the Fader Group widgets */
		TArray<TWeakPtr<SDMXControlConsoleEditorFaderGroupView>> FaderGroupViews;
	};
}
