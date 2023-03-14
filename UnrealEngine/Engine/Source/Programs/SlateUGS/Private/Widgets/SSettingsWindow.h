// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SWindow.h"

#include "SCheckBoxList.h"

class UGSTab;

namespace UGSCore
{
	struct FUserSettings;
}

class SSettingsWindow : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SSettingsWindow) {}
		SLATE_ARGUMENT(UGSTab*, Tab)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** Handle the check box enable or disabling for AfterSync settings */
	void HandleBuildChanged(ECheckBoxState InCheck);
	ECheckBoxState HandleGetBuildChecked() const;

	void HandleRunChanged(ECheckBoxState InCheck);
	ECheckBoxState HandleGetRunChecked() const;

	void HandleOpenSolutionChanged(ECheckBoxState InCheck);
	ECheckBoxState HandleGetOpenSolutionChecked() const;

	FReply OnOkClicked();
	FReply OnCancelClicked();

	UGSTab* Tab;
	TSharedPtr<UGSCore::FUserSettings> UserSettings;
};
