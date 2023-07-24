// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSourceControlFileDialog.h"

#include "Framework/Application/SlateApplication.h"
#include "SSourceControlChangelistRows.h"
#include "Styling/AppStyle.h"
#include "SWarningOrErrorBox.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBorder.h"

#define LOCTEXT_NAMESPACE "SSourceControlFileDialog"

void SSourceControlFileDialog::Construct(const FArguments& InArgs)
{
	Reset();
	SetMessage(InArgs._Message);
	SetWarning(InArgs._Warning);
	SetFiles(InArgs._Files);

	FileTreeView =	SNew(STreeView<FChangelistTreeItemPtr>)
					.ItemHeight(24.0f)
					.TreeItemsSource(&FileTreeNodes)
					.OnGenerateRow(this, &SSourceControlFileDialog::OnGenerateRow)
					.OnGetChildren(this, &SSourceControlFileDialog::OnGetFileChildren)
					.HeaderRow
					(
						SNew(SHeaderRow)
						+ SHeaderRow::Column("Icon")
						.DefaultLabel(FText::GetEmpty())
						.FillSized(18)

						+ SHeaderRow::Column("Name")
						.DefaultLabel(LOCTEXT("Name", "Name"))
						.FillWidth(0.2f)

						+ SHeaderRow::Column("Path")
						.DefaultLabel(LOCTEXT("Path", "Path"))
						.FillWidth(0.6f)

						+ SHeaderRow::Column("Type")
						.DefaultLabel(LOCTEXT("Type", "Type"))
						.FillWidth(0.2f)
					);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		.Padding(FMargin(16))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(0.1)
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock)
				.Text(this, &SSourceControlFileDialog::GetMessage)
				.AutoWrapText(true)
			]
			+ SVerticalBox::Slot()
			.FillHeight(0.8)
			[
				SNew(SScrollBorder, FileTreeView.ToSharedRef())
				[
					FileTreeView.ToSharedRef()
				]
			]
			+ SVerticalBox::Slot()
			.Padding(0, 16.0f, 0, 0)
			.AutoHeight()
			[
				SNew(SWarningOrErrorBox)
				.Visibility(this, &SSourceControlFileDialog::GetWarningVisibility)
				.Message(this, &SSourceControlFileDialog::GetWarning)
			]
			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 16.0f, 0.0f, 0.0f)
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Bottom)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(5, 0)
				[
					SAssignNew(ProceedButton, SButton)
					.ButtonStyle(&FAppStyle::Get(), "PrimaryButton")
					.TextStyle(&FAppStyle::Get(), "PrimaryButtonText")
					.Text(LOCTEXT("Proceed", "Proceed"))
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.OnClicked(this, &SSourceControlFileDialog::OnProceedClicked)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(5, 0)
				[
					SAssignNew(CancelButton, SButton)
					.ButtonStyle(&FAppStyle::Get(), "Button")
					.TextStyle(&FAppStyle::Get(), "ButtonText")
					.Text(LOCTEXT("Cancel", "Cancel"))
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.OnClicked(this, &SSourceControlFileDialog::OnCancelClicked)
				]
			]
		]
	];
}

void SSourceControlFileDialog::SetMessage(const FText& InMessage)
{
	Message = InMessage;
}

void SSourceControlFileDialog::SetWarning(const FText& InWarning)
{
	Warning = InWarning;
}

void SSourceControlFileDialog::SetFiles(const TArray<FSourceControlStateRef>& InFiles)
{
	for (const TSharedRef<ISourceControlState>& FileState : InFiles)
	{
		FileTreeNodes.Add(MakeShared<FFileTreeItem>(FileState, true));
	}

	if (FileTreeView.IsValid())
	{
		FileTreeView->RequestTreeRefresh();
	}
}

TSharedRef<ITableRow> SSourceControlFileDialog::OnGenerateRow(TSharedPtr<IChangelistTreeItem> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return	SNew(SFileTableRow, OwnerTable)
			.TreeItemToVisualize(InItem);
}

void SSourceControlFileDialog::Reset()
{
	bIsProceedButtonPressed = false;
}

void SSourceControlFileDialog::SetWindow(TSharedPtr<SWindow> InWindow)
{
	Window = MoveTemp(InWindow);
}

FReply SSourceControlFileDialog::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if ((InKeyEvent.GetKey() == EKeys::Enter && ProceedButton.IsValid()))
	{
		return OnProceedClicked();
	}
	else if ((InKeyEvent.GetKey() == EKeys::Escape && CancelButton.IsValid()))
	{
		return OnCancelClicked();
	}

	return FReply::Unhandled();
}

FText SSourceControlFileDialog::GetMessage() const
{
	return Message;
}

FText SSourceControlFileDialog::GetWarning() const
{
	return Warning;
}

EVisibility SSourceControlFileDialog::GetWarningVisibility() const
{
	return (Warning.IsEmpty()) ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
}

FReply SSourceControlFileDialog::OnProceedClicked()
{
	bIsProceedButtonPressed = true;
	CloseDialog();

	return FReply::Handled();
}

FReply SSourceControlFileDialog::OnCancelClicked()
{
	bIsProceedButtonPressed = false;
	CloseDialog();

	return FReply::Handled();
}

void SSourceControlFileDialog::CloseDialog()
{
	if (Window.IsValid())
	{
		Window->RequestDestroyWindow();
	}
}

#undef LOCTEXT_NAMESPACE
