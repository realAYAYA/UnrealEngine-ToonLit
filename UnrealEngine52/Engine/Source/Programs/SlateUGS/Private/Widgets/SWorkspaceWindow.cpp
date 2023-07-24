// Copyright Epic Games, Inc. All Rights Reserved.

#include "SWorkspaceWindow.h"

#include "Styling/AppStyle.h"
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"

#include "UGSTab.h"
#include "SGameSyncTab.h"
#include "SNewWorkspaceWindow.h"
#include "SPopupTextWindow.h"
#include "SPrimaryButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SSpacer.h"

#define LOCTEXT_NAMESPACE "UGSWorkspaceWindow"

void SWorkspaceWindow::Construct(const FArguments& InArgs, UGSTab* InTab)
{
	Tab = InTab;

	SWindow::Construct(SWindow::FArguments()
	.Title(LOCTEXT("WindowTitle", "Open Project"))
	.SizingRule(ESizingRule::FixedSize)
	.ClientSize(FVector2D(600, 300))
	[
		SNew(SBox)
		.Padding(10.0f, 10.0f)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(20.0f, 20.0f)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SCheckBox).Style(FAppStyle::Get(), "RadioButton")
						.IsChecked_Lambda([this] () { return bIsLocalFileSelected ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda( [this] (ECheckBoxState InState) { bIsLocalFileSelected = (bIsLocalFileSelected || InState == ECheckBoxState::Checked); } )
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("LocalFileText", "Local File"))
					]
				]
				+SVerticalBox::Slot()
				.Padding(0.0f, 10.0f)
				[
					SNew(SHorizontalBox)
					.IsEnabled_Lambda([this]() { return bIsLocalFileSelected; })
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.FillWidth(1)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("FileText", "File:"))
					]
					+SHorizontalBox::Slot()
					.Padding(10.0f, 0.0f)
					.FillWidth(7)
					[
						SAssignNew(LocalFileTextBox, SEditableTextBox)
						.HintText(LOCTEXT("FilePathHint", "Path/To/ProjectFile.uproject")) // Todo: Make hint text use backslash for Windows, forward slash for Unix
			 			.OnTextChanged_Lambda([this](const FText& InText)
						{
							WorkspacePathText = InText.ToString();
						})
					]
					+SHorizontalBox::Slot()
					.FillWidth(2)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(LOCTEXT("BrowseText", "Browse..."))
						.OnClicked(this, &SWorkspaceWindow::OnBrowseClicked)
					]
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(20.0f, 10.0f)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SCheckBox).Style(FAppStyle::Get(), "RadioButton")
						.IsChecked_Lambda([this] () { return bIsLocalFileSelected ? ECheckBoxState::Unchecked : ECheckBoxState::Checked; })
						.OnCheckStateChanged_Lambda( [this] (ECheckBoxState InState) { bIsLocalFileSelected = (!bIsLocalFileSelected && InState == ECheckBoxState::Checked); } )
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("WorkspaceText", "Workspace"))
					]
				]
				+SVerticalBox::Slot()
				.Padding(0.0f, 10.0f)
				[
					SNew(SVerticalBox)
					.IsEnabled_Lambda([this]() { return !bIsLocalFileSelected; })
					+SVerticalBox::Slot()
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.HAlign(HAlign_Right)
						.FillWidth(1)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("NameText", "Name:"))
						]
						+SHorizontalBox::Slot()
						.Padding(10.0f, 0.0f)
						.FillWidth(5)
						[
							SAssignNew(WorkspaceNameTextBox, SEditableTextBox)
							.HintText(LOCTEXT("NameHint", "WorkspaceName"))
						]
						+SHorizontalBox::Slot()
						.FillWidth(2)
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.Text(LOCTEXT("NewText", "New..."))
							.OnClicked(this, &SWorkspaceWindow::OnNewClicked)
						]
						+SHorizontalBox::Slot()
						.Padding(10.0f, 0.0f, 0.0f, 0.0f)
						.FillWidth(2)
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.Text(LOCTEXT("BrowseText", "Browse..."))
						]
					]
					+SVerticalBox::Slot()
					.Padding(0.0f, 10.0f)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.HAlign(HAlign_Right)
						.FillWidth(1)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("FileText", "File:"))
						]
						+SHorizontalBox::Slot()
						.Padding(10.0f, 0.0f)
						.FillWidth(7)
						[
							SNew(SEditableTextBox)
							.HintText(LOCTEXT("WorkspacePathHint", "/Relative/Path/To/ProjectFile.uproject"))
						]
						+SHorizontalBox::Slot()
						.FillWidth(2)
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.Text(LOCTEXT("BrowseText", "Browse..."))
						]
					]
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(20.0f, 10.0f)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.Padding(10.0f, 0.0f)
				.FillWidth(6)
				[
					SNew(SSpacer)
				]
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				.FillWidth(2)
				[
					SNew(SPrimaryButton)
					.Text(LOCTEXT("OkText", "Ok"))
					.OnClicked(this, &SWorkspaceWindow::OnOkClicked)
				]
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				.Padding(10.0f, 0.0f, 0.0f, 0.0f)
				.FillWidth(2)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("CancelText", "Cancel"))
					.OnClicked(this, &SWorkspaceWindow::OnCancelClicked)
				]
			]
		]
	]);
}

void SWorkspaceWindow::SetWorkspaceTextBox(FText Text) const
{
	WorkspaceNameTextBox->SetText(Text);
}

FReply SWorkspaceWindow::OnOkClicked()
{
	bool bIsWorkspaceValid = Tab->OnWorkspaceChosen(WorkspacePathText);
	if (bIsWorkspaceValid)
	{
		FSlateApplication::Get().FindWidgetWindow(AsShared())->RequestDestroyWindow();
	}
	else
	{
		// Todo: report other errors (no workspace associated with the file)
		FText WorkspaceTitle = FText::FromString("Error");
		FText Error = FText::FromString("Project file does not exist, try again");
		FSlateApplication::Get().AddModalWindow(SNew(SPopupTextWindow).TitleText(WorkspaceTitle).BodyText(Error), SharedThis(this), false);
	}

	return FReply::Handled();
}

FReply SWorkspaceWindow::OnCancelClicked()
{
	FSlateApplication::Get().FindWidgetWindow(AsShared())->RequestDestroyWindow();
	return FReply::Handled();
}

FReply SWorkspaceWindow::OnBrowseClicked()
{
	TArray<FString> OutOpenFilenames;
	FDesktopPlatformModule::Get()->OpenFileDialog(
		FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared()),
		LOCTEXT("OpenDialogTitle", "Open Unreal Project").ToString(),
		PreviousProjectPath,
		TEXT(""),
		TEXT("Unreal Project Files (*.uproject)|*.uproject"),
		EFileDialogFlags::None,
		OutOpenFilenames
	);

	if (!OutOpenFilenames.IsEmpty())
	{
		PreviousProjectPath = OutOpenFilenames[0];
		LocalFileTextBox->SetText(FText::FromString(FPaths::ConvertRelativePathToFull(PreviousProjectPath)));
	}

	return FReply::Handled();
}

FReply SWorkspaceWindow::OnNewClicked()
{
	FSlateApplication::Get().AddModalWindow(SNew(SNewWorkspaceWindow, SharedThis(this), Tab), SharedThis(this), false);

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
