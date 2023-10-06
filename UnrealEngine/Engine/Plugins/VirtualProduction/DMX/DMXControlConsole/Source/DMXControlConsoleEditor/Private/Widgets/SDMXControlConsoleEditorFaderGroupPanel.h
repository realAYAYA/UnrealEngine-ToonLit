// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
class SDMXControlConsoleEditorFaderGroupView;
class UDMXControlConsoleFaderGroup;


/** Base Fader Group UI widget */
class SDMXControlConsoleEditorFaderGroupPanel
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMXControlConsoleEditorFaderGroupPanel)
	{}

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs, const TWeakPtr<SDMXControlConsoleEditorFaderGroupView>& InFaderGroupView);

private:
	/** Gets reference to the Fader Group */
	UDMXControlConsoleFaderGroup* GetFaderGroup() const;

	/** Gets current FaderGroupName */
	FText OnGetFaderGroupNameText() const;

	/** Gets current FID as text, if valid */
	FText OnGetFaderGroupFIDText() const;

	/** Gets current Universe ID range as text */
	FText OnGetFaderGroupUniverseText() const;

	/** Gets current Address range as text */
	FText OnGetFaderGroupAddressText() const;

	/** Called when the fader name changes */
	void OnFaderGroupNameCommitted(const FText& NewName, ETextCommit::Type InCommit);

	/** Shows/Modifies Fader Group Name */
	TSharedPtr<SEditableTextBox> FaderGroupNameTextBox;

	/** Weak Reference to this Fader Group Row */
	TWeakPtr<SDMXControlConsoleEditorFaderGroupView> FaderGroupView;
};
