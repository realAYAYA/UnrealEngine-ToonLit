// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaRundownSubListStartPage.h"

#include "Rundown/AvaRundownEditor.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SAvaRundownSubListStartPage"

void SAvaRundownSubListStartPage::Construct(const FArguments& InArgs, TSharedPtr<FAvaRundownEditor> InRundownEditor)
{
	RundownEditorWeak = InRundownEditor;

	ChildSlot
	[
		SNew(SBox)
		.Padding(10.f, 10.f)
		.HAlign(EHorizontalAlignment::HAlign_Center)
		.VAlign(EVerticalAlignment::VAlign_Top)
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("AddSubListTooltip", "Add Page View"))
			.OnClicked(this, &SAvaRundownSubListStartPage::OnCreateSubListClicked)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AddSubList", "Add Page View"))
			]
		]
	];
}

FReply SAvaRundownSubListStartPage::OnCreateSubListClicked()
{
	TSharedPtr<FAvaRundownEditor> RundownEditor = RundownEditorWeak.Pin();

	if (RundownEditor.IsValid())
	{
		UAvaRundown* Rundown = RundownEditor->GetRundown();

		if (IsValid(Rundown))
		{
			Rundown->AddSubList();

			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE
