// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SAvaUserInputDialog.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SAvaUserInputDialog"

bool SAvaUserInputDialog::CreateModalDialog(const TSharedPtr<SWidget>& InParent, const FText& InTitle,
	const FText& InPrompt, const TSharedRef<FAvaUserInputDataTypeBase>& InInputType)
{
	TSharedPtr<SWindow> ParentWindow = InParent.IsValid() ? FSlateApplication::Get().FindWidgetWindow(InParent.ToSharedRef()) : nullptr;

	static const FText DefaultTitle = LOCTEXT("DefaultTitle", "User Input Required");
	static const FText DefaultPrompt = LOCTEXT("DefaultPrompt", "Value requested:");

	TSharedRef<SAvaUserInputDialog> InputDialog = SNew(SAvaUserInputDialog, InInputType)
		.Prompt(InPrompt.IsEmpty() ? DefaultPrompt : InPrompt);

	TSharedRef<SWindow> Window = SNew(SWindow)
		.SizingRule(ESizingRule::Autosized)
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		.Title(InTitle.IsEmpty() ? DefaultTitle : InTitle)
		[
			InputDialog
		];

	FSlateApplication::Get().AddModalWindow(Window, InParent, /* Slow Task */ false);

	return InputDialog->WasAccepted();
}

void SAvaUserInputDialog::PrivateRegisterAttributes(struct FSlateAttributeDescriptor::FInitializer& InInitializer)
{
}

void SAvaUserInputDialog::Construct(const FArguments& InArgs, const TSharedRef<FAvaUserInputDataTypeBase>& InInputType)
{
	InputType = InInputType;
	InputType->OnCommit.BindSP(this, &SAvaUserInputDialog::OnUserCommit);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.HAlign(EHorizontalAlignment::HAlign_Left)
		.Padding(15.f)
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(InArgs._Prompt)
		]
		+ SVerticalBox::Slot()
		.HAlign(EHorizontalAlignment::HAlign_Left)
		.Padding(15.f, 0.f, 15.f, 15.f)
		.AutoHeight()
		[
			InputType->CreateInputWidget()
		]
		+ SVerticalBox::Slot()
		.HAlign(EHorizontalAlignment::HAlign_Center)
		.Padding(15.f, 0.f, 15.f, 15.f)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 0.f, 5.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Accept", "Accept"))
				.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
				.OnClicked(this, &SAvaUserInputDialog::OnOkayClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Cancel", "Cancel"))
				.ButtonStyle(FAppStyle::Get(), "Button")
				.OnClicked(this, &SAvaUserInputDialog::OnCancelClicked)
			]
		]
	];
}

TSharedPtr<FAvaUserInputDataTypeBase> SAvaUserInputDialog::GetInputType() const
{
	return InputType;
}

bool SAvaUserInputDialog::WasAccepted() const
{
	return bAccepted;
}

FReply SAvaUserInputDialog::OnOkayClicked()
{
	Close(true);

	return FReply::Handled();
}

FReply SAvaUserInputDialog::OnCancelClicked()
{
	Close(false);

	return FReply::Handled();
}

void SAvaUserInputDialog::OnUserCommit()
{
	Close(true);
}

void SAvaUserInputDialog::Close(bool bInAccepted)
{
	bAccepted = bInAccepted;

	TSharedPtr<SWindow> CurrentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());

	if (CurrentWindow.IsValid())
	{
		CurrentWindow->RequestDestroyWindow();
	}
}

#undef LOCTEXT_NAMESPACE
