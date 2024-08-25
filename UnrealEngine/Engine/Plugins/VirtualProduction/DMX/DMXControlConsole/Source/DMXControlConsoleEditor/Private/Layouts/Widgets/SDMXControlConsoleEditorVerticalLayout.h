// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Layouts/Widgets/SDMXControlConsoleEditorLayout.h"

struct EVisibility;
class SScrollBox;
class SVerticalBox;
class UDMXControlConsoleFaderGroupController;


namespace UE::DMX::Private
{ 
	class SDMXControlConsoleEditorFaderGroupControllerView;

	/** Draws the fader groups of a control console vertically */
	class SDMXControlConsoleEditorVerticalLayout
		: public SDMXControlConsoleEditorLayout
	{
	public:
		SLATE_BEGIN_ARGS(SDMXControlConsoleEditorVerticalLayout)
		{}

		SLATE_END_ARGS()

		/** Constructs the widget */
		void Construct(const FArguments& InArgs, UDMXControlConsoleEditorGlobalLayoutBase* InLayout, UDMXControlConsoleEditorModel* InEditorModel);

	protected:
		//~ Begin SDMXControlConsoleEditorLayout interface
		virtual bool CanRefresh() const override;
		virtual void OnLayoutElementAdded() override;
		virtual void OnLayoutElementRemoved() override;
		//~ End SDMXControlConsoleEditorLayout interface

	private:
		/** Checks if the Fader Group Controllers array contains a reference to the given Controller */
		bool IsFaderGroupControllerContained(UDMXControlConsoleFaderGroupController* FaderGroupController);

		/** Called when a Fader Group Controller view needs to be scrolled into view */
		void OnScrollIntoView(const UDMXControlConsoleFaderGroupController* FaderGroupController);

		/** Gets the visibility for each Fader Group Controller view in this layout */
		EVisibility GetFaderGroupControllerViewVisibility(TWeakObjectPtr<UDMXControlConsoleFaderGroupController> FaderGroupController) const;

		/** The widget containing the Fader Group Controller views */
		TSharedPtr<SVerticalBox> FaderGroupControllersVerticalBox;

		/** The vertical ScrollBox widget */
		TSharedPtr<SScrollBox> VerticalScrollBox;

		/** Array of weak references to the Fader Group Controller views */
		TArray<TWeakPtr<SDMXControlConsoleEditorFaderGroupControllerView>> FaderGroupControllerViews;
	};
}
