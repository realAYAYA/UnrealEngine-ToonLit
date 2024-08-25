// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
class UDMXControlConsoleFaderGroupController;


namespace UE::DMX::Private
{
	class FDMXControlConsoleFaderGroupControllerModel;

	/** A widget to display an info panel for the Fader Group Controller view */
	class SDMXControlConsoleEditorFaderGroupControllerPanel
		: public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDMXControlConsoleEditorFaderGroupControllerPanel)
			{}

		SLATE_END_ARGS()

		/** Constructs the widget */
		void Construct(const FArguments& InArgs, const TWeakPtr<FDMXControlConsoleFaderGroupControllerModel>& InFaderGroupControllerModel);

	private:
		/** Gets reference to the Fader Group Controller */
		UDMXControlConsoleFaderGroupController* GetFaderGroupController() const;

		/** Gets current FaderGroupControllerName */
		FText OnGetFaderGroupControllerNameText() const;

		/** Gets current FID as text, if valid */
		FText OnGetFaderGroupControllerFIDText() const;

		/** Gets current Universe ID range as text */
		FText OnGetFaderGroupControllerUniverseText() const;

		/** Gets current Address range as text */
		FText OnGetFaderGroupControllerAddressText() const;

		/** Called when the Fader Group Controller name changes */
		void OnFaderGroupControllerNameCommitted(const FText& NewName, ETextCommit::Type InCommit);

		/** Shows/Modifies Fader Group Controller Name */
		TSharedPtr<SEditableTextBox> FaderGroupControllerNameTextBox;

		/** Weak Reference to the Fader Group Controller model */
		TWeakPtr<FDMXControlConsoleFaderGroupControllerModel> WeakFaderGroupControllerModel;
	};
}
