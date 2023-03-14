// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SWindow.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"

#include "UGSCore/Workspace.h"

#include "SCheckBoxList.h"

class UGSTab;

class SSyncFilterWindow : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SSyncFilterWindow) {}
		SLATE_ARGUMENT(UGSTab*, Tab)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply OnShowCombinedFilterClicked();
	FReply OnCustomViewSyntaxClicked();
	FReply OnSaveClicked();
	FReply OnCancelClicked();

	void ConstructSyncFilters();
	void ConstructCustomSyncViewTextBoxes();

	UGSTab* Tab;
	TArray<UGSCore::FWorkspaceSyncCategory> WorkspaceCategoriesCurrent;
	TArray<UGSCore::FWorkspaceSyncCategory> WorkspaceCategoriesAll;

	TSharedPtr<SCheckBoxList> SyncFiltersCurrent;
	TSharedPtr<SCheckBoxList> SyncFiltersAll;
	TSharedPtr<SMultiLineEditableTextBox> CustomSyncViewCurrent;
	TSharedPtr<SMultiLineEditableTextBox> CustomSyncViewAll;

};
