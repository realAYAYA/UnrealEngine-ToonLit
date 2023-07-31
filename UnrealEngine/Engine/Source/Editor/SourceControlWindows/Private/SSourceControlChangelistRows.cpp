// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSourceControlChangelistRows.h"

#include "ISourceControlModule.h"
#include "UncontrolledChangelistsModule.h"
#include "SourceControlOperations.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Algo/Transform.h"

#define LOCTEXT_NAMESPACE "SourceControlChangelistRow"

FName SourceControlFileViewColumn::Icon::Id() { return TEXT("Icon"); }
FText SourceControlFileViewColumn::Icon::GetDisplayText() {return LOCTEXT("Name_Icon", "Source Control Status"); }
FText SourceControlFileViewColumn::Icon::GetToolTipText() { return LOCTEXT("Icon_Column_Tooltip", "Displays the asset/file status"); }

FName SourceControlFileViewColumn::Name::Id() { return TEXT("Name"); }
FText SourceControlFileViewColumn::Name::GetDisplayText() { return LOCTEXT("Name_Column", "Name"); }
FText SourceControlFileViewColumn::Name::GetToolTipText() { return LOCTEXT("Name_Column_Tooltip", "Displays the asset/file name"); }

FName SourceControlFileViewColumn::Path::Id() { return TEXT("Path"); }
FText SourceControlFileViewColumn::Path::GetDisplayText() { return LOCTEXT("Path_Column", "Path"); }
FText SourceControlFileViewColumn::Path::GetToolTipText() { return LOCTEXT("Path_Column_Tooltip", "Displays the asset/file path"); }

FName SourceControlFileViewColumn::Type::Id() { return TEXT("Type"); }
FText SourceControlFileViewColumn::Type::GetDisplayText() { return LOCTEXT("Type_Column", "Type"); }
FText SourceControlFileViewColumn::Type::GetToolTipText() { return LOCTEXT("Type_Column_Tooltip", "Displays the asset type"); }

FName SourceControlFileViewColumn::LastModifiedTimestamp::Id() { return TEXT("LastModifiedTimestamp"); }
FText SourceControlFileViewColumn::LastModifiedTimestamp::GetDisplayText() {return LOCTEXT("LastModifiedTimestamp_Column", "Last Saved"); }
FText SourceControlFileViewColumn::LastModifiedTimestamp::GetToolTipText() { return LOCTEXT("LastMofiedTimestamp_Column_Tooltip", "Displays the last time the file/asset was saved on user hard drive"); }

FName SourceControlFileViewColumn::CheckedOutByUser::Id() { return TEXT("CheckedOutByUser"); }
FText SourceControlFileViewColumn::CheckedOutByUser::GetDisplayText() { return LOCTEXT("CheckedOutByUser_Column", "User"); }
FText SourceControlFileViewColumn::CheckedOutByUser::GetToolTipText() { return LOCTEXT("CheckedOutByUser_Column_Tooltip", "Displays the other user(s) that checked out the file/asset, if any"); }


void SChangelistTableRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwner)
{
	TreeItem = static_cast<FChangelistTreeItem*>(InArgs._TreeItemToVisualize.Get());
	OnPostDrop = InArgs._OnPostDrop;

	const FSlateBrush* IconBrush = (TreeItem != nullptr) ?
		FAppStyle::GetBrush(TreeItem->ChangelistState->GetSmallIconName()) :
		FAppStyle::GetBrush("SourceControl.Changelist");

	SetToolTipText(GetChangelistDescriptionText());

	STableRow<FChangelistTreeItemPtr>::Construct(
		STableRow<FChangelistTreeItemPtr>::FArguments()
		.Style(FAppStyle::Get(), "TableView.Row")
		.Content()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot() // Icon
			.AutoWidth()
			[
				SNew(SImage)
				.Image(IconBrush)
			]
			+SHorizontalBox::Slot() // Changelist number.
			.Padding(2, 0, 0, 0)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(this, &SChangelistTableRow::GetChangelistText)
				.HighlightText(InArgs._HighlightText)
			]
			+SHorizontalBox::Slot() // Files count.
			.Padding(4, 0, 4, 0)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(FText::Format(INVTEXT("({0})"), TreeItem->GetFileCount()))
			]
			+SHorizontalBox::Slot() // Description.
			.Padding(2, 0, 0, 0)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(this, &SChangelistTableRow::GetChangelistDescriptionText)
				.HighlightText(InArgs._HighlightText)
			]
		], InOwner);
}

void SChangelistTableRow::PopulateSearchString(const FChangelistTreeItem& Item, TArray<FString>& OutStrings)
{
	OutStrings.Emplace(Item.GetDisplayText().ToString()); // The changelist number
	OutStrings.Emplace(GetChangelistDescription(Item));   // The changelist description.
}

FText SChangelistTableRow::GetChangelistText() const
{
	return TreeItem->GetDisplayText();
}

FText SChangelistTableRow::GetChangelistDescriptionText() const
{
	return FText::FromString(GetChangelistDescription(*TreeItem));
}

FString SChangelistTableRow::GetChangelistDescription(const FChangelistTreeItem& Item)
{
	FString DescriptionString = Item.GetDescriptionText().ToString();
	// Here we'll both remove \r\n (when edited from the dialog) and \n (when we get it from the SCC)
	DescriptionString.ReplaceInline(TEXT("\r"), TEXT(""));
	DescriptionString.ReplaceInline(TEXT("\n"), TEXT(" "));
	DescriptionString.TrimEndInline();
	return DescriptionString;
}

FReply SChangelistTableRow::OnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent)
{
	TSharedPtr<FSCCFileDragDropOp> DropOperation = InDragDropEvent.GetOperationAs<FSCCFileDragDropOp>();
	if (DropOperation.IsValid())
	{
		FSourceControlChangelistPtr DestChangelist = TreeItem->ChangelistState->GetChangelist();
		check(DestChangelist.IsValid());

		// NOTE: The UI don't show 'source controlled files' and 'uncontrolled files' at the same time. User cannot select and drag/drop both file types at the same time.
		if (!DropOperation->Files.IsEmpty())
		{
			TArray<FString> Files;
			Algo::Transform(DropOperation->Files, Files, [](const FSourceControlStateRef& State) { return State->GetFilename(); });

			SSourceControlCommon::ExecuteChangelistOperationWithSlowTaskWrapper(LOCTEXT("Dropping_Files_On_Changelist", "Moving file(s) to the selected changelist..."), [&]()
			{
				ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
				SourceControlProvider.Execute(ISourceControlOperation::Create<FMoveToChangelist>(), DestChangelist, Files, EConcurrency::Synchronous, FSourceControlOperationComplete::CreateLambda(
					[](const TSharedRef<ISourceControlOperation>& Operation, ECommandResult::Type InResult)
					{
						if (InResult == ECommandResult::Succeeded)
						{
							SSourceControlCommon::DisplaySourceControlOperationNotification(LOCTEXT("Drop_Files_On_Changelist_Succeeded", "File(s) successfully moved to the selected changelist."), SNotificationItem::CS_Success);
						}
						else if (InResult == ECommandResult::Failed)
						{
							SSourceControlCommon::DisplaySourceControlOperationNotification(LOCTEXT("Drop_Files_On_Changelist_Failed", "Failed to move the file(s) to the selected changelist."), SNotificationItem::CS_Fail);
						}
					}));
			});
		}
		else if (!DropOperation->UncontrolledFiles.IsEmpty())
		{
			// NOTE: This function does several operations that can fails but we don't get feedback.
			SSourceControlCommon::ExecuteUncontrolledChangelistOperationWithSlowTaskWrapper(LOCTEXT("Dropping_Uncontrolled_Files_On_Changelist", "Moving uncontrolled file(s) to the selected changelist..."), 
				[&DropOperation, &DestChangelist]()
			{
				FUncontrolledChangelistsModule::Get().MoveFilesToControlledChangelist(DropOperation->UncontrolledFiles, DestChangelist, SSourceControlCommon::OpenConflictDialog);
					
				// TODO: Fix MoveFilesToControlledChangelist() to report the possible errors and display a notification.
			});

			OnPostDrop.ExecuteIfBound();
		}
	}

	return FReply::Handled();
}


void SUncontrolledChangelistTableRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwner)
{
	TreeItem = static_cast<FUncontrolledChangelistTreeItem*>(InArgs._TreeItemToVisualize.Get());
	OnPostDrop = InArgs._OnPostDrop;

		const FSlateBrush* IconBrush = (TreeItem != nullptr) ?
			FAppStyle::GetBrush(TreeItem->UncontrolledChangelistState->GetSmallIconName()) :
			FAppStyle::GetBrush("SourceControl.Changelist");

	SetToolTipText(GetChangelistText());

	STableRow<FChangelistTreeItemPtr>::Construct(
		STableRow<FChangelistTreeItemPtr>::FArguments()
		.Style(FAppStyle::Get(), "TableView.Row")
		.Content()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(IconBrush)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SUncontrolledChangelistTableRow::GetChangelistText)
				.HighlightText(InArgs._HighlightText)
			]
			+SHorizontalBox::Slot() // Files/Offline file count.
			.Padding(4, 0, 4, 0)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(FText::Format(INVTEXT("({0})"), TreeItem->GetFileCount()))
			]
		], InOwner);
}

void SUncontrolledChangelistTableRow::PopulateSearchString(const FUncontrolledChangelistTreeItem& Item, TArray<FString>& OutStrings)
{
	OutStrings.Emplace(Item.GetDisplayText().ToString());
}

FText SUncontrolledChangelistTableRow::GetChangelistText() const
{
	return TreeItem->GetDisplayText();
}


FReply SUncontrolledChangelistTableRow::OnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent)
{
	TSharedPtr<FSCCFileDragDropOp> Operation = InDragDropEvent.GetOperationAs<FSCCFileDragDropOp>();

	if (Operation.IsValid())
	{
		SSourceControlCommon::ExecuteUncontrolledChangelistOperationWithSlowTaskWrapper(LOCTEXT("Drag_File_To_Uncontrolled_Changelist", "Moving file(s) to the selected uncontrolled changelists..."),
			[this, &Operation]()
		{
			FUncontrolledChangelistsModule::Get().MoveFilesToUncontrolledChangelist(Operation->Files, Operation->UncontrolledFiles, TreeItem->UncontrolledChangelistState->Changelist);
		});

		OnPostDrop.ExecuteIfBound();
	}

	return FReply::Handled();
}


void SFileTableRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwner)
{
	TreeItem = static_cast<FFileTreeItem*>(InArgs._TreeItemToVisualize.Get());

	HighlightText = InArgs._HighlightText;

	FSuperRowType::FArguments Args = FSuperRowType::FArguments()
		.OnDragDetected(InArgs._OnDragDetected)
		.ShowSelection(true);
	FSuperRowType::Construct(Args, InOwner);
}

TSharedRef<SWidget> SFileTableRow::GenerateWidgetForColumn(const FName& ColumnId)
{
	if (ColumnId == SourceControlFileViewColumn::Icon::Id())
	{
		return SNew(SBox)
			.WidthOverride(16) // Small Icons are usually 16x16
			.HAlign(HAlign_Center)
			[
				SSourceControlCommon::GetSCCFileWidget(TreeItem->FileState, TreeItem->IsShelved())
			];
	}
	else if (ColumnId == SourceControlFileViewColumn::Name::Id())
	{
		return SNew(STextBlock)
			.Text(this, &SFileTableRow::GetDisplayName)
			.ToolTipText(this, &SFileTableRow::GetDisplayName)
			.HighlightText(HighlightText);
	}
	else if (ColumnId == SourceControlFileViewColumn::Path::Id())
	{
		return SNew(STextBlock)
			.Text(this, &SFileTableRow::GetDisplayPath)
			.ToolTipText(this, &SFileTableRow::GetFilename)
			.HighlightText(HighlightText);
	}
	else if (ColumnId == SourceControlFileViewColumn::Type::Id())
	{
		return SNew(STextBlock)
			.Text(this, &SFileTableRow::GetDisplayType)
			.ToolTipText(this, &SFileTableRow::GetDisplayType)
			.ColorAndOpacity(this, &SFileTableRow::GetDisplayColor)
			.HighlightText(HighlightText);
	}
	else if (ColumnId == SourceControlFileViewColumn::LastModifiedTimestamp::Id())
	{
		return SNew(STextBlock)
			.ToolTipText(this, &SFileTableRow::GetLastModifiedTimestamp)
			.Text(this, &SFileTableRow::GetLastModifiedTimestamp);
	}
	else if (ColumnId == SourceControlFileViewColumn::CheckedOutByUser::Id())
	{
		return SNew(STextBlock)
			.ToolTipText(this, &SFileTableRow::GetCheckedOutByUser)
			.Text(this, &SFileTableRow::GetCheckedOutByUser);
	}
	else
	{
		return SNullWidget::NullWidget;
	}
}

void SFileTableRow::PopulateSearchString(const FFileTreeItem& Item, TArray<FString>& OutStrings)
{
	OutStrings.Emplace(Item.GetAssetName().ToString()); // Name
	OutStrings.Emplace(Item.GetAssetPath().ToString()); // Path
	OutStrings.Emplace(Item.GetAssetType().ToString()); // Type
	OutStrings.Emplace(Item.GetLastModifiedTimestamp().ToString());
	OutStrings.Emplace(Item.GetCheckedOutByUser().ToString());
}

FText SFileTableRow::GetDisplayName() const
{
	return TreeItem->GetAssetName();
}

FText SFileTableRow::GetFilename() const
{
	return TreeItem->GetFileName();
}

FText SFileTableRow::GetDisplayPath() const
{
	return TreeItem->GetAssetPath();
}

FText SFileTableRow::GetDisplayType() const
{
	return TreeItem->GetAssetType();
}

FSlateColor SFileTableRow::GetDisplayColor() const
{
	return TreeItem->GetAssetTypeColor();
}

FText SFileTableRow::GetLastModifiedTimestamp() const
{
	return TreeItem->GetLastModifiedTimestamp();
}

FText SFileTableRow::GetCheckedOutByUser() const
{
	return TreeItem->GetCheckedOutByUser();
}

void SFileTableRow::OnDragEnter(FGeometry const& InGeometry, FDragDropEvent const& InDragDropEvent)
{
	TSharedPtr<FDragDropOperation> DragOperation = InDragDropEvent.GetOperation();
	DragOperation->SetCursorOverride(EMouseCursor::SlashedCircle);
}

void SFileTableRow::OnDragLeave(FDragDropEvent const& InDragDropEvent)
{
	TSharedPtr<FDragDropOperation> DragOperation = InDragDropEvent.GetOperation();
	DragOperation->SetCursorOverride(EMouseCursor::None);
}


void SShelvedFilesTableRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	TreeItem = static_cast<FShelvedChangelistTreeItem*>(InArgs._TreeItemToVisualize.Get());

	STableRow<FChangelistTreeItemPtr>::Construct(
		STableRow<FChangelistTreeItemPtr>::FArguments()
		.Content()
		[
			SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(5, 0, 0, 0)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("SourceControl.ShelvedChangelist"))
				]
				+SHorizontalBox::Slot()
				.Padding(2.0f, 1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(TreeItem->GetDisplayText())
					.HighlightText(InArgs._HighlightText)
				]
		],
		InOwnerTableView);
}

void SShelvedFilesTableRow::PopulateSearchString(const FShelvedChangelistTreeItem& Item, TArray<FString>& OutStrings)
{
	OutStrings.Emplace(Item.GetDisplayText().ToString());
}


void SOfflineFileTableRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwner)
{
	TreeItem = static_cast<FOfflineFileTreeItem*>(InArgs._TreeItemToVisualize.Get());
	HighlightText = InArgs._HighlightText;

	FSuperRowType::FArguments Args = FSuperRowType::FArguments().ShowSelection(true);
	FSuperRowType::Construct(Args, InOwner);
}

TSharedRef<SWidget> SOfflineFileTableRow::GenerateWidgetForColumn(const FName& ColumnId)
{
	if (ColumnId == SourceControlFileViewColumn::Icon::Id())
	{
		return SNew(SBox)
			.WidthOverride(16) // Small Icons are usually 16x16
			.HAlign(HAlign_Center)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush(FName("SourceControl.OfflineFile_Small")))
			];
	}
	else if (ColumnId == SourceControlFileViewColumn::Name::Id())
	{
		return SNew(STextBlock)
			.ToolTipText(this, &SOfflineFileTableRow::GetDisplayName)
			.Text(this, &SOfflineFileTableRow::GetDisplayName)
			.HighlightText(HighlightText);
	}
	else if (ColumnId == SourceControlFileViewColumn::Path::Id())
	{
		return SNew(STextBlock)
			.Text(this, &SOfflineFileTableRow::GetDisplayPath)
			.ToolTipText(this, &SOfflineFileTableRow::GetFilename)
			.HighlightText(HighlightText);
	}
	else if (ColumnId == SourceControlFileViewColumn::Type::Id())
	{
		return SNew(STextBlock)
			.Text(this, &SOfflineFileTableRow::GetDisplayType)
			.ToolTipText(this, &SOfflineFileTableRow::GetDisplayType)
			.ColorAndOpacity(this, &SOfflineFileTableRow::GetDisplayColor)
			.HighlightText(HighlightText);
	}
	else if (ColumnId == SourceControlFileViewColumn::LastModifiedTimestamp::Id())
	{
		return SNew(STextBlock)
			.ToolTipText(this, &SOfflineFileTableRow::GetLastModifiedTimestamp)
			.Text(this, &SOfflineFileTableRow::GetLastModifiedTimestamp);
	}
	else if (ColumnId == SourceControlFileViewColumn::CheckedOutByUser::Id())
	{
		return SNew(STextBlock)
			.Text(FText::GetEmpty());
	}
	else
	{
		return SNullWidget::NullWidget;
	}
}

void SOfflineFileTableRow::PopulateSearchString(const FOfflineFileTreeItem& Item, TArray<FString>& OutStrings)
{
	OutStrings.Emplace(Item.GetDisplayName().ToString()); // Name
	OutStrings.Emplace(Item.GetDisplayPath().ToString()); // Path
	OutStrings.Emplace(Item.GetDisplayType().ToString()); // Type
	OutStrings.Emplace(Item.GetLastModifiedTimestamp().ToString());
}

FText SOfflineFileTableRow::GetDisplayName() const
{
	return TreeItem->GetDisplayName();
}

FText SOfflineFileTableRow::GetFilename() const
{
	return TreeItem->GetPackageName();
}

FText SOfflineFileTableRow::GetDisplayPath() const
{
	return TreeItem->GetDisplayPath();
}

FText SOfflineFileTableRow::GetDisplayType() const
{
	return TreeItem->GetDisplayType();
}

FSlateColor SOfflineFileTableRow::GetDisplayColor() const
{
	return TreeItem->GetDisplayColor();
}

FText SOfflineFileTableRow::GetLastModifiedTimestamp() const
{
	return TreeItem->GetLastModifiedTimestamp();
}


#undef LOCTEXT_NAMESPACE
