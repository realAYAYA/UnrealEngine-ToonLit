// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/InputDataTypes/AvaUserInputDataTypeBase.h"

class SAvaUserInputDialog : public SCompoundWidget
{
	SLATE_DECLARE_WIDGET_API(SAvaUserInputDialog, SCompoundWidget, AVALANCHEEDITORCORE_API)

public:
	static AVALANCHEEDITORCORE_API bool CreateModalDialog(const TSharedPtr<SWidget>& InParent, const FText& InTitle, const FText& InPrompt,
		const TSharedRef<FAvaUserInputDataTypeBase>& InInputType);

	SLATE_BEGIN_ARGS(SAvaUserInputDialog) {}
		SLATE_ARGUMENT(FText, Prompt)
	SLATE_END_ARGS()

	AVALANCHEEDITORCORE_API void Construct(const FArguments& InArgs, const TSharedRef<FAvaUserInputDataTypeBase>& InInputType);

	AVALANCHEEDITORCORE_API TSharedPtr<FAvaUserInputDataTypeBase> GetInputType() const;

	AVALANCHEEDITORCORE_API bool WasAccepted() const;

private:
	TSharedPtr<FAvaUserInputDataTypeBase> InputType;
	bool bAccepted = false;

	FReply OnOkayClicked();

	FReply OnCancelClicked();

	void OnUserCommit();

	void Close(bool bInAccepted);
};
