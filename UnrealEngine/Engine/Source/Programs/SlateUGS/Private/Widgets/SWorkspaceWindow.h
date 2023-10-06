// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SWindow.h"
#include "Widgets/Input/SEditableTextBox.h"

class UGSTab;

class SWorkspaceWindow final : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SWorkspaceWindow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UGSTab* InTab);

	void SetWorkspaceTextBox(FText Text) const;

private:
	FReply OnOkClicked();
	FReply OnCancelClicked();

	FReply OnBrowseClicked();
	FString PreviousProjectPath;

	FReply OnNewClicked();

	bool bIsLocalFileSelected = true;
	TSharedPtr<SEditableTextBox> LocalFileTextBox = nullptr;
	FString WorkspacePathText;

	TSharedPtr<SEditableTextBox> WorkspaceNameTextBox = nullptr;

	UGSTab* Tab = nullptr;
};
