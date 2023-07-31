// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SWindow.h"

class UGSTab;

class SWorkspaceWindow : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SWorkspaceWindow) {}
		SLATE_ARGUMENT(UGSTab*, Tab)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply OnBrowseClicked();

	FReply OnOkClicked();
	FReply OnCancelClicked();

	bool bIsLocalFileSelected = true;
	TSharedPtr<SEditableTextBox> LocalFileText = nullptr;
	FString WorkspacePathText;

	UGSTab* Tab;
};
