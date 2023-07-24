// Copyright Epic Games, Inc. All Rights Reserved.

#include "SCorrectivesViewer.h"

#include "PoseCorrectivesAsset.h"
#include "PoseCorrectivesEditorController.h"

#include "Animation/DebugSkelMeshComponent.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"


#define LOCTEXT_NAMESPACE "CorrectivesPoseEditor"

static const FName ColumnId_CorrectiveNameLabel("Corrective Name");
static const FName ColumnID_CorrectiveGroupLabel("Group");

void SCorrectiveListRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedRef<IPersonaPreviewScene>& InPreviewScene)
{
	Item = InArgs._Item;
	CorrectivesViewerPtr = InArgs._CorrectivesViewer;
	FilterText = InArgs._FilterText;
	PreviewScenePtr = InPreviewScene;

	check(Item.IsValid());

	SMultiColumnTableRow< TSharedPtr<FDisplayedCorrectiveInfo> >::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}

TSharedRef< SWidget > SCorrectiveListRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (ColumnName == ColumnId_CorrectiveNameLabel)
	{
		TSharedPtr< SInlineEditableTextBlock > InlineWidget;

		TSharedRef<SWidget> NameWidget =
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.0f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(InlineWidget, SInlineEditableTextBlock)
				.Text(this, &SCorrectiveListRow::GetName)
				.HighlightText(FilterText)
				.ToolTipText(LOCTEXT("CorrectiveName_ToolTip", "Modify Corrective Name - Make sure this name is unique."))
				.OnVerifyTextChanged(this, &SCorrectiveListRow::OnVerifyNameChanged)
				.OnTextCommitted(this, &SCorrectiveListRow::OnNameCommitted)
			];

		Item->OnRenameRequested.BindSP(InlineWidget.Get(), &SInlineEditableTextBlock::EnterEditingMode);

		return NameWidget;
	}
	else
	{	
		TSharedRef<SWidget> GroupWidget =
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SComboBox<TSharedPtr<FName>>)
					.OptionsSource(&CorrectivesViewerPtr.Pin()->GroupNames)
					.OnSelectionChanged_Lambda([this](TSharedPtr<FName> NewSelection, ESelectInfo::Type SelectInfo)
					{
						Item->Group = *NewSelection;
						CorrectivesViewerPtr.Pin()->PoseCorrectivesAssetPtr->UpdateGroupForCorrective(Item->Name, Item->Group);

					})
					.OnGenerateWidget_Lambda([](TSharedPtr<FName> Option)
					{
						return SNew(STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 12)).Text(FText::FromName(*Option));
					})
					[
						SNew(STextBlock).Text_Lambda([this](){ return FText::FromName(Item->Group); })
					]
			];

		return GroupWidget;
	}
}

FText SCorrectiveListRow::GetName() const
{
	return (FText::FromName(Item->Name));
}

void SCorrectiveListRow::OnNameCommitted(const FText& InText, ETextCommit::Type InCommitType) const
{
	// for now only allow enter
	// because it is important to keep the unique names per pose
	if (InCommitType == ETextCommit::OnEnter)
	{
		FScopedTransaction Transaction(LOCTEXT("Rename Corrective", "Rename Corrective"));
		CorrectivesViewerPtr.Pin()->PoseCorrectivesAssetPtr.Get()->Modify();

		FName NewName = FName(*InText.ToString());
		FName OldName = Item->Name;		

		if (CorrectivesViewerPtr.IsValid() && CorrectivesViewerPtr.Pin()->ModifyName(OldName, NewName))
		{
			Item->Name = NewName;
		}
	}
}

bool SCorrectiveListRow::OnVerifyNameChanged(const FText& InText, FText& OutErrorMessage)
{
	bool bVerifyName = false;

	FName NewName = FName(*InText.ToString());

	if (NewName == NAME_None)
	{
		OutErrorMessage = LOCTEXT("EmptyPoseName", "Correctives must have a name!");
	}

	if (CorrectivesViewerPtr.IsValid())
	{
		UPoseCorrectivesAsset* CorrectivesAsset = CorrectivesViewerPtr.Pin()->PoseCorrectivesAssetPtr.Get();
		if (CorrectivesAsset != nullptr)
		{
			if (CorrectivesAsset->FindCorrective(NewName))
			{
				OutErrorMessage = LOCTEXT("NameAlreadyUsedByTheSameAsset", "The name is used by another corrective within the same asset. Please choose another name.");
			}
			else
			{
				bVerifyName = true;
			}
		}
	}

	return bVerifyName;
}


//////////////////////////////////////////////////////////////////////////
// SPoseViewer

void SCorrectivesViewer::Construct(const FArguments& InArgs, const TSharedRef<FPoseCorrectivesEditorController>& InEditorController, const TSharedRef<IPersonaPreviewScene>& InPreviewScene)
{
	EditorControllerPtr = InEditorController;
	EditorControllerPtr.Pin()->SetCorrectivesViewer(SharedThis(this));
	PreviewScenePtr = InPreviewScene;
	PoseCorrectivesAssetPtr = EditorControllerPtr.Pin()->Asset;

	InPreviewScene->RegisterOnPreviewMeshChanged(FOnPreviewMeshChanged::CreateSP(this, &SCorrectivesViewer::OnPreviewMeshChanged));

	OnDelegateCorrectivesListChangedDelegateHandle = PoseCorrectivesAssetPtr->RegisterOnCorrectivesListChanged(UPoseCorrectivesAsset::FOnCorrectivesListChanged::CreateSP(this, &SCorrectivesViewer::OnCorrectivesAssetModified));
	BindCommands();

	ChildSlot
	[
		SNew(SSplitter)
		.Orientation(EOrientation::Orient_Horizontal)

		// Pose List
		+SSplitter::Slot()
		.Value(1)
		[
			SNew(SBox)
			.Padding(5)
			[
				SNew(SVerticalBox)

				 + SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 2)
				[
					SNew(SHorizontalBox)
					// Filter entry
					+ SHorizontalBox::Slot()
					.FillWidth(1)
					[
						SAssignNew(NameFilterBox, SSearchBox)
						.SelectAllTextWhenFocused(true)
						.OnTextChanged(this, &SCorrectivesViewer::OnFilterTextChanged)
						.OnTextCommitted(this, &SCorrectivesViewer::OnFilterTextCommitted)
					]
				]

				+ SVerticalBox::Slot()
				.FillHeight(1)
				.Padding(0, 2)
				[
					SAssignNew(CorrectivesListView, SCorrectivesListType)
					.ListItemsSource(&CorrectivesList)
					.OnGenerateRow(this, &SCorrectivesViewer::GenerateCorrectiveRow)
					.OnContextMenuOpening(this, &SCorrectivesViewer::OnGetContextMenuContent)
					.OnMouseButtonDoubleClick(this, &SCorrectivesViewer::OnListDoubleClick)
					.ItemHeight(22.0f)
					.HeaderRow
					(
						SNew(SHeaderRow)
						+ SHeaderRow::Column(ColumnId_CorrectiveNameLabel)
						.DefaultLabel(LOCTEXT("CorrectiveNameLabel", "Corrective Name"))

						+ SHeaderRow::Column(ColumnID_CorrectiveGroupLabel)
						.DefaultLabel(LOCTEXT("CorrectiveGroupLabel", "Group"))
						)
				]
			]
		]
	];

	CreateCorrectivesList();
}

FReply SCorrectivesViewer::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (UICommandList.IsValid() && UICommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SCorrectivesViewer::OnPreviewMeshChanged(class USkeletalMesh* OldPreviewMesh, class USkeletalMesh* NewPreviewMesh)
{
	CreateCorrectivesList(NameFilterBox->GetText().ToString());
}

void SCorrectivesViewer::OnFilterTextChanged(const FText& SearchText)
{
	FilterText = SearchText;
	CreateCorrectivesList(SearchText.ToString());
}

void SCorrectivesViewer::OnFilterTextCommitted(const FText& SearchText, ETextCommit::Type CommitInfo)
{
	OnFilterTextChanged(SearchText);
}

TSharedRef<ITableRow> SCorrectivesViewer::GenerateCorrectiveRow(TSharedPtr<FDisplayedCorrectiveInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable)
{
	check(InInfo.IsValid());

	return
		SNew(SCorrectiveListRow, OwnerTable, PreviewScenePtr.Pin().ToSharedRef())
		.Item(InInfo)
		.CorrectivesViewer(SharedThis(this))
		.FilterText(GetFilterText());
}

bool SCorrectivesViewer::IsCorrectiveSelected() const
{
	TArray<TSharedPtr<FDisplayedCorrectiveInfo>> SelectedRows = CorrectivesListView->GetSelectedItems();
	return SelectedRows.Num() > 0;
}

bool SCorrectivesViewer::IsSingleCorrectiveSelected() const
{
	TArray<TSharedPtr<FDisplayedCorrectiveInfo>> SelectedRows = CorrectivesListView->GetSelectedItems();
	return SelectedRows.Num() == 1;
}

void SCorrectivesViewer::OnDeleteCorrectives()
{
	TArray<TSharedPtr<FDisplayedCorrectiveInfo>> SelectedRows = CorrectivesListView->GetSelectedItems();

	FScopedTransaction Transaction(LOCTEXT("DeleteCorrectives", "Delete Correctives"));
	PoseCorrectivesAssetPtr.Get()->Modify();

	TArray<FName> PosesToDelete;
	for (int RowIndex = 0; RowIndex < SelectedRows.Num(); ++RowIndex)
	{
		FName CorrectiveName = SelectedRows[RowIndex]->Name;
		PoseCorrectivesAssetPtr.Get()->DeleteCorrective(CorrectiveName);
	}

	CreateCorrectivesList(NameFilterBox->GetText().ToString());
}

void SCorrectivesViewer::OnRenameCorrective()
{
	TArray<TSharedPtr< FDisplayedCorrectiveInfo>> SelectedRows = CorrectivesListView->GetSelectedItems();
	if (SelectedRows.Num() > 0)
	{
		TSharedPtr<FDisplayedCorrectiveInfo> SelectedRow = SelectedRows[0];
		if (SelectedRow.IsValid())
		{
			SelectedRow->OnRenameRequested.ExecuteIfBound();
		}
	}
}

bool SCorrectivesViewer::ModifyName(FName OldName, FName NewName)
{
	return EditorControllerPtr.Pin()->HandleCorrectiveRenamed(OldName, NewName);
}

void SCorrectivesViewer::BindCommands()
{
	// This should not be called twice on the same instance
	check(!UICommandList.IsValid());
	UICommandList = MakeShareable(new FUICommandList);
	FUICommandList& CommandList = *UICommandList;

	CommandList.MapAction(
		FGenericCommands::Get().Rename,
		FExecuteAction::CreateSP(this, &SCorrectivesViewer::OnRenameCorrective),
		FCanExecuteAction::CreateSP(this, &SCorrectivesViewer::IsSingleCorrectiveSelected));

	CommandList.MapAction(
		FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &SCorrectivesViewer::OnDeleteCorrectives),
		FCanExecuteAction::CreateSP(this, &SCorrectivesViewer::IsCorrectiveSelected));
}

TSharedPtr<SWidget> SCorrectivesViewer::OnGetContextMenuContent() const
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, UICommandList);

	MenuBuilder.BeginSection("CorrectivesAction", LOCTEXT("SelectedItems", "Selected Item Actions"));
	MenuBuilder.AddMenuEntry(FGenericCommands::Get().Delete, NAME_None, LOCTEXT("DeleteCorrectiveButtonLabel", "Delete"), LOCTEXT("DeleteCorrectiveButtonTooltip", "Delete the selected corrective(s)"));
	MenuBuilder.AddMenuEntry(FGenericCommands::Get().Rename, NAME_None, LOCTEXT("RenameCorrectiveButtonLabel", "Rename"), LOCTEXT("RenameCorrectiveButtonTooltip", "Renames the selected corrective"));
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SCorrectivesViewer::OnListDoubleClick(TSharedPtr<FDisplayedCorrectiveInfo> InItem)
{
	if(InItem.IsValid())
	{
		EditorControllerPtr.Pin()->HandleEditCorrective(InItem->Name);
		CorrectivesListView->ClearHighlightedItems();
		CorrectivesListView->SetItemHighlighted(InItem, true);
	}
}

void SCorrectivesViewer::CreateCorrectivesList(const FString& SearchText)
{
	CorrectivesList.Empty();
	PopulateGroupsList();

	if (PoseCorrectivesAssetPtr.IsValid())
	{
		TArray<FName> CorrectiveNames = PoseCorrectivesAssetPtr->GetCorrectiveNames();
		if (CorrectiveNames.Num() > 0)
		{
			bool bDoFiltering = !SearchText.IsEmpty();

			for (const FName& CorrectiveName : CorrectiveNames)
			{
				if (bDoFiltering && !CorrectiveName.ToString().Contains(SearchText))
				{
					continue; // Skip items that don't match our filter
				}

				FName GroupName = PoseCorrectivesAssetPtr->FindCorrective(CorrectiveName)->GroupName;
				const TSharedRef<FDisplayedCorrectiveInfo> Info = FDisplayedCorrectiveInfo::Make(CorrectiveName, GroupName);
				CorrectivesList.Add(Info);
			}
		}
	}

	CorrectivesListView->RequestListRefresh();
}

SCorrectivesViewer::~SCorrectivesViewer()
{
	if (PreviewScenePtr.IsValid())
	{
		PreviewScenePtr.Pin()->UnregisterOnPreviewMeshChanged(this);
	}

	if (PoseCorrectivesAssetPtr.IsValid())
	{
		PoseCorrectivesAssetPtr->UnregisterOnCorrectivesListChanged(OnDelegateCorrectivesListChangedDelegateHandle);
	}
}

void SCorrectivesViewer::HighlightCorrective(const FName& CorrectiveName)
{
	for (const auto& CorrectiveItem : CorrectivesList)
	{
		if (CorrectiveItem->Name == CorrectiveName)
		{
			CorrectivesListView->SetItemHighlighted(CorrectiveItem, true);
			return;
		}
	}
}

void SCorrectivesViewer::ClearHighlightedItems()
{
	CorrectivesListView->ClearHighlightedItems();
}

void SCorrectivesViewer::PopulateGroupsList()
{
	GroupNames.Reset();
	for (const FName& Name : PoseCorrectivesAssetPtr->GetGroupNames())
	{
		GroupNames.Add(MakeShareable(new FName(Name)));
	}

	CorrectivesListView->RequestListRefresh();
}

void SCorrectivesViewer::OnCorrectivesAssetModified()
{
	CreateCorrectivesList(NameFilterBox->GetText().ToString());
}


#undef LOCTEXT_NAMESPACE
