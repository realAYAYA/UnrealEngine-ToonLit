// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSourceControlSubmit.h"

#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
#include "ISourceControlProvider.h"
#include "ISourceControlModule.h"
#include "SourceControlHelpers.h"
#include "SSourceControlCommon.h"
#include "Modules/ModuleManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SErrorText.h"
#include "Widgets/Input/SCheckBox.h"
#include "UObject/UObjectHash.h"
#include "Styling/AppStyle.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Virtualization/VirtualizationSystem.h"
#include "Logging/MessageLog.h"
#include "RevisionControlStyle/RevisionControlStyle.h"
#include "Bookmarks/BookmarkScoped.h"
#include "HAL/IConsoleManager.h"
#include "Algo/AllOf.h"

#if SOURCE_CONTROL_WITH_SLATE

#define LOCTEXT_NAMESPACE "SSourceControlSubmit"

// This is useful for source control that do not support changelist (Git/SVN) or when the submit widget is not created from the changelist window. If a user
// commits/submits this way, then edits the submit description but cancels, the description will be remembered in memory for the next time he tries to submit.
static FText GSavedChangeListDescription;

bool TryToVirtualizeFilesToSubmit(const TArray<FString>& FilesToSubmit, FText& Description, FText& OutFailureMsg)
{
	using namespace UE::Virtualization;

	{
		TArray<FText> PayloadErrors;
		TArray<FText> DescriptionTags;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
		ISourceControlModule::Get().GetOnPreSubmitFinalize().Broadcast(FilesToSubmit, DescriptionTags, PayloadErrors);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	IVirtualizationSystem& System = IVirtualizationSystem::Get();
	if (!System.IsEnabled())
	{
		return true;
	}

	EVirtualizationOptions VirtualizationOptions = EVirtualizationOptions::None;

	FVirtualizationResult Result = System.TryVirtualizePackages(FilesToSubmit, VirtualizationOptions);
	if (Result.WasSuccessful())
	{
		FTextBuilder NewDescription;
		NewDescription.AppendLine(Description);

		for (const FText& Line : Result.DescriptionTags)
		{
			NewDescription.AppendLine(Line);
		}

		Description = NewDescription.ToText();

		return true;
	}
	else if (System.AllowSubmitIfVirtualizationFailed())
	{
		for (const FText& Error : Result.Errors)
		{
			FMessageLog("SourceControl").Warning(Error);
		}

		// Even though the virtualization process had problems we should continue submitting
		return true;
	}
	else
	{
		for (const FText& Error : Result.Errors)
		{
			FMessageLog("SourceControl").Error(Error);
		}

		OutFailureMsg = LOCTEXT("SCC_Virtualization_Failed", "Failed to virtualize the files being submitted!");

		return false;
	}
}

namespace SSourceControlSubmitWidgetDefs
{
	const FName ColumnID_CheckBoxLabel("CheckBox");
	const FName ColumnID_IconLabel("Icon");
	const FName ColumnID_AssetLabel("Asset");
	const FName ColumnID_FileLabel("File");

	const float CheckBoxColumnWidth = 23.0f;
	const float IconColumnWidth = 21.0f;
}


void SSourceControlSubmitListRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	SourceControlSubmitWidgetPtr = InArgs._SourceControlSubmitWidget;
	Item = InArgs._Item;

	SMultiColumnTableRow<TSharedPtr<FFileTreeItem>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}


TSharedRef<SWidget> SSourceControlSubmitListRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	// Create the widget for this item
	TSharedPtr<SSourceControlSubmitWidget> SourceControlSubmitWidget = SourceControlSubmitWidgetPtr.Pin();
	if (SourceControlSubmitWidget.IsValid())
	{
		return SourceControlSubmitWidget->GenerateWidgetForItemAndColumn(Item, ColumnName);
	}

	// Packages dialog no longer valid; return a valid, null widget.
	return SNullWidget::NullWidget;
}


SSourceControlSubmitWidget::~SSourceControlSubmitWidget()
{
	// If the user cancel the submit, save the changelist. If the user submitted, ChangeListDescriptionTextCtrl was cleared).
	GSavedChangeListDescription = ChangeListDescriptionTextCtrl->GetText();
}

void SSourceControlSubmitWidget::Construct(const FArguments& InArgs)
{
	ParentFrame = InArgs._ParentWindow.Get();
	SortByColumn = SSourceControlSubmitWidgetDefs::ColumnID_AssetLabel;
	SortMode = EColumnSortMode::Ascending;
	if (!InArgs._Description.Get().IsEmpty())
	{
		// If a description is provided, override the last one saved in memory.
		GSavedChangeListDescription = InArgs._Description.Get();
	}
	bAllowSubmit = InArgs._AllowSubmit.Get();
	bAllowDiffAgainstDepot = InArgs._AllowDiffAgainstDepot.Get();

	const bool bDescriptionIsReadOnly = !InArgs._AllowDescriptionChange.Get();
	const bool bAllowUncheckFiles = InArgs._AllowUncheckFiles.Get();
	const bool bAllowKeepCheckedOut = InArgs._AllowKeepCheckedOut.Get();
	const bool bShowChangelistValidation = !InArgs._ChangeValidationResult.Get().IsEmpty();
	const bool bAllowSaveAndClose = InArgs._AllowSaveAndClose.Get();

	for (const auto& Item : InArgs._Items.Get())
	{
		ListViewItems.Add(MakeShareable(new FFileTreeItem(Item)));
	}

	TSharedRef<SHeaderRow> HeaderRowWidget = SNew(SHeaderRow);

	if (bAllowUncheckFiles)
	{
		HeaderRowWidget->AddColumn(
			SHeaderRow::Column(SSourceControlSubmitWidgetDefs::ColumnID_CheckBoxLabel)
			[
				SNew(SCheckBox)
				.IsChecked(this, &SSourceControlSubmitWidget::GetToggleSelectedState)
				.OnCheckStateChanged(this, &SSourceControlSubmitWidget::OnToggleSelectedCheckBox)
			]
			.FixedWidth(SSourceControlSubmitWidgetDefs::CheckBoxColumnWidth)
		);
	}

	HeaderRowWidget->AddColumn(
		SHeaderRow::Column(SSourceControlSubmitWidgetDefs::ColumnID_IconLabel)
		[
			SNew(SSpacer)
		]
		.SortMode(this, &SSourceControlSubmitWidget::GetColumnSortMode, SSourceControlSubmitWidgetDefs::ColumnID_IconLabel)
		.OnSort(this, &SSourceControlSubmitWidget::OnColumnSortModeChanged)
		.FixedWidth(SSourceControlSubmitWidgetDefs::IconColumnWidth)
	);

	HeaderRowWidget->AddColumn(
		SHeaderRow::Column(SSourceControlSubmitWidgetDefs::ColumnID_AssetLabel)
		.DefaultLabel(LOCTEXT("AssetColumnLabel", "Asset"))
		.SortMode(this, &SSourceControlSubmitWidget::GetColumnSortMode, SSourceControlSubmitWidgetDefs::ColumnID_AssetLabel)
		.OnSort(this, &SSourceControlSubmitWidget::OnColumnSortModeChanged)
		.FillWidth(5.0f)
	);

	HeaderRowWidget->AddColumn(
		SHeaderRow::Column(SSourceControlSubmitWidgetDefs::ColumnID_FileLabel)
		.DefaultLabel(LOCTEXT("FileColumnLabel", "File"))
		.SortMode(this, &SSourceControlSubmitWidget::GetColumnSortMode, SSourceControlSubmitWidgetDefs::ColumnID_FileLabel)
		.OnSort(this, &SSourceControlSubmitWidget::OnColumnSortModeChanged)
		.FillWidth(7.0f)
	);

	TSharedPtr<SVerticalBox> Contents;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SAssignNew(Contents, SVerticalBox)
		]
	];

	// Build contents of dialog
	Contents->AddSlot()
	.AutoHeight()
	.Padding(5)
	[
		SNew(STextBlock)
		.Text(NSLOCTEXT("SourceControl.SubmitPanel", "ChangeListDesc", "Changelist Description"))
	];

	Contents->AddSlot()
	.FillHeight(.5f)
	.Padding(FMargin(5, 0, 5, 5))
	[
		SNew(SBox)
		.WidthOverride(520)
		[
			SAssignNew(ChangeListDescriptionTextCtrl, SMultiLineEditableTextBox)
			.SelectAllTextWhenFocused(!bDescriptionIsReadOnly)
			.Text(GSavedChangeListDescription)
			.AutoWrapText(true)
			.IsReadOnly(bDescriptionIsReadOnly)
		]
	];

	Contents->AddSlot()
	.Padding(FMargin(5, 0))
	[
		SNew(SBorder)
		[
			SAssignNew(ListView, SListView<TSharedPtr<FFileTreeItem>>)
			.ItemHeight(20)
			.ListItemsSource(&ListViewItems)
			.OnGenerateRow(this, &SSourceControlSubmitWidget::OnGenerateRowForList)
			.OnContextMenuOpening(this, &SSourceControlSubmitWidget::OnCreateContextMenu)
			.OnMouseButtonDoubleClick(this, &SSourceControlSubmitWidget::OnDiffAgainstDepotSelected)
			.HeaderRow(HeaderRowWidget)
			.SelectionMode(ESelectionMode::Multi)
		]
	];

	if (!bDescriptionIsReadOnly)
	{
		Contents->AddSlot()
		.AutoHeight()
		.Padding(FMargin(5, 5, 5, 0))
		[
			SNew( SBorder)
			.Visibility(this, &SSourceControlSubmitWidget::IsWarningPanelVisible)
			.Padding(5)
			[
				SNew( SErrorText )
				.ErrorText(ChangeListDescriptionTextCtrl->GetText().IsEmpty() ? 
					NSLOCTEXT("SourceControl.SubmitPanel", "ChangeListDescWarning", "Changelist description is required to submit") :
					NSLOCTEXT("SourceControl.SubmitPanel", "Error", "Error!")) // Other errors exist and a better mechanism should be built in to display the right error. 
			]
		];
	}

	if (bShowChangelistValidation)
	{
		const FString ChangelistResultText = InArgs._ChangeValidationResult.Get();
		const FString ChangelistResultWarningsText = InArgs._ChangeValidationWarnings.Get();
		const FString ChangelistResultErrorsText = InArgs._ChangeValidationErrors.Get();

		const FName ChangelistSuccessIconName = TEXT("Icons.SuccessWithColor.Large");
		const FName ChangelistWarningsIconName = TEXT("Icons.WarningWithColor.Large");
		const FName ChangelistErrorsIconName = TEXT("Icons.ErrorWithColor.Large");

		if (bAllowSubmit)
		{
			Contents->AddSlot()
			.AutoHeight()
			.Padding(FMargin(5))
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush(ChangelistSuccessIconName))
				]
				+SHorizontalBox::Slot()
				[
					SNew(SMultiLineEditableTextBox)
					.Text(FText::FromString(ChangelistResultText))
					.AutoWrapText(true)
					.IsReadOnly(true)
				]
			];
		}
		else
		{
			Contents->AddSlot()
			.AutoHeight()
			.Padding(FMargin(5))
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				[
					SNew(SMultiLineEditableTextBox)
					.Text(FText::FromString(ChangelistResultText))
					.AutoWrapText(true)
					.IsReadOnly(true)
				]
			];

			if (!ChangelistResultErrorsText.IsEmpty())
			{
				Contents->AddSlot()
				.Padding(FMargin(5))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush(ChangelistErrorsIconName))
					]
					+SHorizontalBox::Slot()
					[
						SNew(SMultiLineEditableTextBox)
						.Text(FText::FromString(ChangelistResultErrorsText))
						.AutoWrapText(true)
						.IsReadOnly(true)
					]
				];
			}

			if (!ChangelistResultWarningsText.IsEmpty())
			{
				Contents->AddSlot()
				.Padding(FMargin(5))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush(ChangelistWarningsIconName))
					]
					+SHorizontalBox::Slot()
					[
						SNew(SMultiLineEditableTextBox)
						.Text(FText::FromString(ChangelistResultWarningsText))
						.AutoWrapText(true)
						.IsReadOnly(true)
					]
				];
			}
		}
	}

	if (bAllowKeepCheckedOut)
	{
		Contents->AddSlot()
		.AutoHeight()
		.Padding(5)
		[
			SNew(SWrapBox)
			.UseAllottedSize(true)
			+SWrapBox::Slot()
			.Padding(0.0f, 0.0f, 16.0f, 0.0f)
			[
				SNew(SCheckBox)
				.OnCheckStateChanged( this, &SSourceControlSubmitWidget::OnCheckStateChanged_KeepCheckedOut)
				.IsChecked( this, &SSourceControlSubmitWidget::GetKeepCheckedOut )
				.IsEnabled( this, &SSourceControlSubmitWidget::CanCheckOut )
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SourceControl.SubmitPanel", "KeepCheckedOut", "Keep Files Checked Out") )
				]
			]
		];
	}

	const float AdditionalTopPadding = (bAllowKeepCheckedOut ? 0.0f : 5.0f);

	TSharedPtr<SUniformGridPanel> SubmitSaveCancelButtonGrid;
	int32 ButtonSlotId = 0;

	Contents->AddSlot()
	.AutoHeight()
	.HAlign(HAlign_Right)
	.VAlign(VAlign_Bottom)
	.Padding(0.0f, AdditionalTopPadding, 0.0f, 5.0f)
	[
		SAssignNew(SubmitSaveCancelButtonGrid, SUniformGridPanel)
		.SlotPadding(FAppStyle::GetMargin("StandardDialog.SlotPadding"))
		.MinDesiredSlotWidth(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
		.MinDesiredSlotHeight(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
		+SUniformGridPanel::Slot(ButtonSlotId++, 0)
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
			.IsEnabled(this, &SSourceControlSubmitWidget::IsSubmitEnabled)
			.Text( NSLOCTEXT("SourceControl.SubmitPanel", "OKButton", "Submit") )
			.OnClicked(this, &SSourceControlSubmitWidget::SubmitClicked)
		]
	];

	if (bAllowSaveAndClose)
	{
		SubmitSaveCancelButtonGrid->AddSlot(ButtonSlotId++, 0)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
				.Text(NSLOCTEXT("SourceControl.SubmitPanel", "Save", "Save"))
				.ToolTipText(NSLOCTEXT("SourceControl.SubmitPanel", "Save_Tooltip", "Save the description and close without submitting."))
				.OnClicked(this, &SSourceControlSubmitWidget::SaveAndCloseClicked)
			];
	}

	SubmitSaveCancelButtonGrid->AddSlot(ButtonSlotId++, 0)
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
			.Text( NSLOCTEXT("SourceControl.SubmitPanel", "CancelButton", "Cancel") )
			.OnClicked(this, &SSourceControlSubmitWidget::CancelClicked)
		];

	RequestSort();

	DialogResult = ESubmitResults::SUBMIT_CANCELED;
	KeepCheckedOut = ECheckBoxState::Unchecked;

	ParentFrame.Pin()->SetWidgetToFocusOnActivate(ChangeListDescriptionTextCtrl);
}

/** Corvus: Called to create a context menu when right-clicking on an item */
TSharedPtr<SWidget> SSourceControlSubmitWidget::OnCreateContextMenu()
{
	FMenuBuilder MenuBuilder(true, NULL);

	MenuBuilder.BeginSection("Source Control", NSLOCTEXT("SourceControl.SubmitWindow.Menu", "SourceControlSectionHeader", "Revision Control"));
	{
		if (SSourceControlSubmitWidget::CanDiffAgainstDepot())
		{
			MenuBuilder.AddMenuEntry(
				NSLOCTEXT("SourceControl.SubmitWindow.Menu", "DiffAgainstDepot", "Diff Against Depot"),
				NSLOCTEXT("SourceControl.SubmitWindow.Menu", "DiffAgainstDepotTooltip", "Look at differences between your version of the asset and that in revision control."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.Actions.Diff"),
				FUIAction(
					FExecuteAction::CreateSP(this, &SSourceControlSubmitWidget::OnDiffAgainstDepot),
					FCanExecuteAction::CreateSP(this, &SSourceControlSubmitWidget::CanDiffAgainstDepot)
				)
			);
		}

		if (AllowRevert())
		{
			MenuBuilder.AddMenuEntry(
				NSLOCTEXT("SourceControl.SubmitWindow.Menu", "Revert", "Revert"),
				NSLOCTEXT("SourceControl.SubmitWindow.Menu", "RevertTooltip", "Revert the selected assets to their original state from revision control."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.Actions.Revert"),
				FUIAction(
					FExecuteAction::CreateSP(this, &SSourceControlSubmitWidget::OnRevert),
					FCanExecuteAction::CreateSP(this, &SSourceControlSubmitWidget::CanRevert)
				)
			);
		}
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

bool SSourceControlSubmitWidget::CanDiffAgainstDepot() const
{
	bool bCanDiff = false;
	if (bAllowDiffAgainstDepot)
	{
		const auto& SelectedItems = ListView->GetSelectedItems();
		if (SelectedItems.Num() == 1)
		{
			bCanDiff = SelectedItems[0]->CanDiff();
		}
	}
	return bCanDiff;
}

void SSourceControlSubmitWidget::OnDiffAgainstDepot()
{
	const auto& SelectedItems = ListView->GetSelectedItems();
	if (SelectedItems.Num() == 1)
	{
		OnDiffAgainstDepotSelected(SelectedItems[0]);
	}
}

void SSourceControlSubmitWidget::OnDiffAgainstDepotSelected(TSharedPtr<FFileTreeItem> InSelectedItem)
{
	if (bAllowDiffAgainstDepot)
	{
		FString PackageName;
		if (FPackageName::TryConvertFilenameToLongPackageName(InSelectedItem->GetFileName().ToString(), PackageName))
		{
			TArray<FAssetData> Assets;
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			AssetRegistryModule.Get().GetAssetsByPackageName(*PackageName, Assets);
			if (Assets.Num() == 1)
			{
				const FAssetData& AssetData = Assets[0];
				UObject* CurrentObject = AssetData.GetAsset();
				if (CurrentObject)
				{
					const FString AssetName = AssetData.AssetName.ToString();
					FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
					AssetToolsModule.Get().DiffAgainstDepot(CurrentObject, PackageName, AssetName);
				}
			}
		}
	}
}

bool SSourceControlSubmitWidget::AllowRevert() const
{
	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("SourceControl.Revert.EnableFromSubmitWidget")))
	{
		return CVar->GetBool();
	}
	else
	{
		return false;
	}
}

bool SSourceControlSubmitWidget::CanRevert() const
{
	const auto& SelectedItems = ListView->GetSelectedItems();
	if (SelectedItems.Num() > 0)
	{
		return Algo::AllOf(SelectedItems, [](const FFileTreeItemPtr& SelectedItem)
			{
				return SelectedItem->CanRevert();
			}
		);
	}
	return false;
}

void SSourceControlSubmitWidget::OnRevert()
{
	const auto& SelectedItems = ListView->GetSelectedItems();
	if (SelectedItems.Num() < 1)
	{
		return;
	}

	auto RemoveItemsFromListView = [this](TArray<FString>& ItemsToRemove)
	{
		ListViewItems.RemoveAll([&ItemsToRemove](const FFileTreeItemPtr& ListViewItem) -> bool
			{
				return ItemsToRemove.ContainsByPredicate([&ListViewItem](const FString& ItemToRemove) -> bool
					{
						return ItemToRemove == ListViewItem->GetFileName().ToString();
					}
				);
			}
		);
	};

	TArray<FString> PackagesToRevert;
	TArray<FString> FilesToRevert;
	for (const auto& SelectedItem : SelectedItems)
	{
		if (FPackageName::IsPackageFilename(SelectedItem->GetFileName().ToString()))
		{
			PackagesToRevert.Add(SelectedItem->GetFileName().ToString());
		}
		else
		{
			FilesToRevert.Add(SelectedItem->GetFileName().ToString());
		}
	}

	{
		FBookmarkScoped BookmarkScoped;
		bool bAnyReverted = false;
		if (PackagesToRevert.Num() > 0)
		{
			bAnyReverted = SourceControlHelpers::RevertAndReloadPackages(PackagesToRevert, /*bRevertAll=*/false, /*bReloadWorld=*/true);
			RemoveItemsFromListView(PackagesToRevert);
		}
		if (FilesToRevert.Num() > 0)
		{
			bAnyReverted |= SourceControlHelpers::RevertFiles(FilesToRevert);
			RemoveItemsFromListView(FilesToRevert);
		}
		
		if (bAnyReverted)
		{
			if (ListViewItems.IsEmpty())
			{
				DialogResult = ESubmitResults::SUBMIT_CANCELED;
				ParentFrame.Pin()->RequestDestroyWindow();
			}
			else
			{
				ListView->RebuildList();
			}
		}
	}
}

FReply SSourceControlSubmitWidget::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	// Pressing escape returns as if the user clicked cancel
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		return CancelClicked();
	}

	return FReply::Unhandled();
}

TSharedRef<SWidget> SSourceControlSubmitWidget::GenerateWidgetForItemAndColumn(TSharedPtr<FFileTreeItem> Item, const FName ColumnID) const
{
	check(Item.IsValid());

	const FMargin RowPadding(3, 0, 0, 0);

	TSharedPtr<SWidget> ItemContentWidget;

	if (ColumnID == SSourceControlSubmitWidgetDefs::ColumnID_CheckBoxLabel)
	{
		ItemContentWidget = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(RowPadding)
			[
				SNew(SCheckBox)
				.IsChecked(Item.Get(), &FFileTreeItem::GetCheckBoxState)
				.OnCheckStateChanged(Item.Get(), &FFileTreeItem::SetCheckBoxState)
			];
	}
	else if (ColumnID == SSourceControlSubmitWidgetDefs::ColumnID_IconLabel)
	{
		ItemContentWidget = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(FRevisionControlStyleManager::Get().GetBrush(Item->GetIconName()))
				.ToolTipText(Item->GetIconTooltip())
			];
	}
	else if (ColumnID == SSourceControlSubmitWidgetDefs::ColumnID_AssetLabel)
	{
		ItemContentWidget = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(RowPadding)
			[
				SNew(STextBlock)
				.Text(Item->GetAssetName())
			];
	}
	else if (ColumnID == SSourceControlSubmitWidgetDefs::ColumnID_FileLabel)
	{
		ItemContentWidget = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(RowPadding)
			[
				SNew(STextBlock)
				.Text(Item->GetPackageName())
				.ToolTipText(Item->GetFileName())
			];
	}

	return ItemContentWidget.ToSharedRef();
}


ECheckBoxState SSourceControlSubmitWidget::GetToggleSelectedState() const
{
	// Default to a Checked state
	ECheckBoxState PendingState = ECheckBoxState::Checked;

	// Iterate through the list of selected items
	for (const TSharedPtr<FFileTreeItem>& Item : ListViewItems)
	{
		if (Item->GetCheckBoxState() == ECheckBoxState::Unchecked)
		{
			// If any item in the list is Unchecked, then represent the entire set of highlighted items as Unchecked,
			// so that the first (user) toggle of ToggleSelectedCheckBox consistently Checks all items
			PendingState = ECheckBoxState::Unchecked;
			break;
		}
	}

	return PendingState;
}


void SSourceControlSubmitWidget::OnToggleSelectedCheckBox(ECheckBoxState InNewState)
{
	for (const TSharedPtr<FFileTreeItem>& Item : ListViewItems)
	{
		Item->SetCheckBoxState(InNewState);
	}

	ListView->RequestListRefresh();
}


void SSourceControlSubmitWidget::FillChangeListDescription(FChangeListDescription& OutDesc)
{
	OutDesc.Description = ChangeListDescriptionTextCtrl->GetText();

	OutDesc.FilesForAdd.Empty();
	OutDesc.FilesForSubmit.Empty();

	for (const TSharedPtr<FFileTreeItem>& Item : ListViewItems)
	{
		if (Item->GetCheckBoxState() == ECheckBoxState::Checked)
		{
			if (Item->CanCheckIn())
			{
				OutDesc.FilesForSubmit.Add(Item->GetFileName().ToString());
			}
			else if (Item->NeedsAdding())
			{
				OutDesc.FilesForAdd.Add(Item->GetFileName().ToString());
			}
		}
	}
}


bool SSourceControlSubmitWidget::WantToKeepCheckedOut()
{
	return KeepCheckedOut == ECheckBoxState::Checked ? true : false;
}

void SSourceControlSubmitWidget::ClearChangeListDescription()
{
	ChangeListDescriptionTextCtrl->SetText(FText());
}

FReply SSourceControlSubmitWidget::SubmitClicked()
{
	DialogResult = ESubmitResults::SUBMIT_ACCEPTED;
	ParentFrame.Pin()->RequestDestroyWindow();

	return FReply::Handled();
}

FReply SSourceControlSubmitWidget::CancelClicked()
{
	DialogResult = ESubmitResults::SUBMIT_CANCELED;
	ParentFrame.Pin()->RequestDestroyWindow();

	return FReply::Handled();
}

FReply SSourceControlSubmitWidget::SaveAndCloseClicked()
{
	DialogResult = ESubmitResults::SUBMIT_SAVED;
	ParentFrame.Pin()->RequestDestroyWindow();

	return FReply::Handled();
}

bool SSourceControlSubmitWidget::IsSubmitEnabled() const
{
	return bAllowSubmit && !ChangeListDescriptionTextCtrl->GetText().IsEmpty() && ListViewItems.Num() > 0;
}


EVisibility SSourceControlSubmitWidget::IsWarningPanelVisible() const
{
	return IsSubmitEnabled() ? EVisibility::Collapsed : EVisibility::Visible;
}


void SSourceControlSubmitWidget::OnCheckStateChanged_KeepCheckedOut(ECheckBoxState InState)
{
	KeepCheckedOut = InState;
}


ECheckBoxState SSourceControlSubmitWidget::GetKeepCheckedOut() const
{
	return KeepCheckedOut;
}


bool SSourceControlSubmitWidget::CanCheckOut() const
{
	const ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	return SourceControlProvider.UsesCheckout();
}


TSharedRef<ITableRow> SSourceControlSubmitWidget::OnGenerateRowForList(TSharedPtr<FFileTreeItem> SubmitItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedRef<ITableRow> Row =
	SNew(SSourceControlSubmitListRow, OwnerTable)
		.SourceControlSubmitWidget(SharedThis(this))
		.Item(SubmitItem);

	return Row;
}


EColumnSortMode::Type SSourceControlSubmitWidget::GetColumnSortMode(const FName ColumnId) const
{
	if (SortByColumn != ColumnId)
	{
		return EColumnSortMode::None;
	}

	return SortMode;
}


void SSourceControlSubmitWidget::OnColumnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type InSortMode)
{
	SortByColumn = ColumnId;
	SortMode = InSortMode;

	RequestSort();
}


void SSourceControlSubmitWidget::RequestSort()
{
	// Sort the list of root items
	SortTree();

	ListView->RequestListRefresh();
}


void SSourceControlSubmitWidget::SortTree()
{
	if (SortByColumn == SSourceControlSubmitWidgetDefs::ColumnID_AssetLabel)
	{
		if (SortMode == EColumnSortMode::Ascending)
		{
			ListViewItems.Sort([](const TSharedPtr<FFileTreeItem>& A, const TSharedPtr<FFileTreeItem>& B) {
				return A->GetAssetName().ToString() < B->GetAssetName().ToString(); });
		}
		else if (SortMode == EColumnSortMode::Descending)
		{
			ListViewItems.Sort([](const TSharedPtr<FFileTreeItem>& A, const TSharedPtr<FFileTreeItem>& B) {
				return A->GetAssetName().ToString() >= B->GetAssetName().ToString(); });
		}
	}
	else if (SortByColumn == SSourceControlSubmitWidgetDefs::ColumnID_FileLabel)
	{
		if (SortMode == EColumnSortMode::Ascending)
		{
			ListViewItems.Sort([](const TSharedPtr<FFileTreeItem>& A, const TSharedPtr<FFileTreeItem>& B) {
				return A->GetPackageName().ToString() < B->GetPackageName().ToString(); });
		}
		else if (SortMode == EColumnSortMode::Descending)
		{
			ListViewItems.Sort([](const TSharedPtr<FFileTreeItem>& A, const TSharedPtr<FFileTreeItem>& B) {
				return A->GetPackageName().ToString() >= B->GetPackageName().ToString(); });
		}
	}
	else if (SortByColumn == SSourceControlSubmitWidgetDefs::ColumnID_IconLabel)
	{
		if (SortMode == EColumnSortMode::Ascending)
		{
			ListViewItems.Sort([](const TSharedPtr<FFileTreeItem>& A, const TSharedPtr<FFileTreeItem>& B) {
				return A->GetIconName().ToString() < B->GetIconName().ToString(); });
		}
		else if (SortMode == EColumnSortMode::Descending)
		{
			ListViewItems.Sort([](const TSharedPtr<FFileTreeItem>& A, const TSharedPtr<FFileTreeItem>& B) {
				return A->GetIconName().ToString() >= B->GetIconName().ToString(); });
		}
	}
}

#undef LOCTEXT_NAMESPACE

#endif // SOURCE_CONTROL_WITH_SLATE
