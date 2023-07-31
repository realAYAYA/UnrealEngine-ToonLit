// Copyright Epic Games, Inc. All Rights Reserved.

#include "SInterchangeResultsBrowserWindow.h"

#include "Styling/AppStyle.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDocumentation.h"
#include "Modules/ModuleManager.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SSpacer.h"

#define LOCTEXT_NAMESPACE "InterchangeResultsBrowser"


namespace SResultsBrowserWidgetDefs
{
	const FName ColumnID_ResultType("ResultType");
	const FName ColumnID_Source("Source");
	const FName ColumnID_Dest("Dest");
	const FName ColumnID_AssetType("AssetType");
	const FName ColumnID_Message("Message");
}


SInterchangeResultsBrowserWindow::SInterchangeResultsBrowserWindow()
	: OwnerTab(nullptr),
	  bIsFiltered(false)
{
}


SInterchangeResultsBrowserWindow::~SInterchangeResultsBrowserWindow()
{
}


void SInterchangeResultsBrowserWindow::Construct(const FArguments& InArgs)
{
	SortByColumn = SResultsBrowserWidgetDefs::ColumnID_ResultType;
	SortMode = EColumnSortMode::Ascending;
	OnFilterChangedState = InArgs._OnFilterChangedState;
	bIsFiltered = InArgs._IsFiltered;
	ResultsContainer = InArgs._InterchangeResultsContainer;

	OwnerTab = InArgs._OwnerTab;
	check(OwnerTab.IsValid());

	TSharedRef<SHeaderRow> HeaderRowWidget = SNew(SHeaderRow);

	HeaderRowWidget->AddColumn(
		SHeaderRow::Column(SResultsBrowserWidgetDefs::ColumnID_ResultType)
		.SortMode(this, &SInterchangeResultsBrowserWindow::GetColumnSortMode, SResultsBrowserWidgetDefs::ColumnID_ResultType)
		.OnSort(this, &SInterchangeResultsBrowserWindow::OnColumnSortModeChanged)
		[
			SNew(SSpacer)
		]
		.FixedWidth(24.0f)
		);

	HeaderRowWidget->AddColumn(
		SHeaderRow::Column(SResultsBrowserWidgetDefs::ColumnID_Source)
		.DefaultLabel(LOCTEXT("ColumnLabel_SourceFile", "Source File"))
		.SortMode(this, &SInterchangeResultsBrowserWindow::GetColumnSortMode, SResultsBrowserWidgetDefs::ColumnID_Source)
		.OnSort(this, &SInterchangeResultsBrowserWindow::OnColumnSortModeChanged)
		.FillWidth(5.0f)
	);

	HeaderRowWidget->AddColumn(
		SHeaderRow::Column(SResultsBrowserWidgetDefs::ColumnID_Dest)
		.DefaultLabel(LOCTEXT("ColumnLabel_DestinationAsset", "Destination Asset"))
		.SortMode(this, &SInterchangeResultsBrowserWindow::GetColumnSortMode, SResultsBrowserWidgetDefs::ColumnID_Dest)
		.OnSort(this, &SInterchangeResultsBrowserWindow::OnColumnSortModeChanged)
		.FillWidth(5.0f)
	);

	HeaderRowWidget->AddColumn(
		SHeaderRow::Column(SResultsBrowserWidgetDefs::ColumnID_AssetType)
		.DefaultLabel(LOCTEXT("ColumnLabel_AssetType", "Asset Type"))
		.SortMode(this, &SInterchangeResultsBrowserWindow::GetColumnSortMode, SResultsBrowserWidgetDefs::ColumnID_AssetType)
		.OnSort(this, &SInterchangeResultsBrowserWindow::OnColumnSortModeChanged)
		.FillWidth(3.0f)
	);

	HeaderRowWidget->AddColumn(
		SHeaderRow::Column(SResultsBrowserWidgetDefs::ColumnID_Message)
		.DefaultLabel(LOCTEXT("ColumnLabel_Details", "Details"))
		.FillWidth(9.0f)
	);

	this->ChildSlot
	[
		SNew(SBorder)
		.Padding(FMargin(10.0f, 3.0f))
		.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			.Padding(10)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SCheckBox)
					.IsChecked(bIsFiltered ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
					.OnCheckStateChanged(this, &SInterchangeResultsBrowserWindow::OnFilterStateChanged)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("OnlyShowErrors", "Only display errors/warnings"))
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(2)
			[
				SNew(SSplitter)
				.Orientation(Orient_Vertical)
				+ SSplitter::Slot()
				.Value(0.4f)
				[
					SAssignNew(ListView, SListView<UInterchangeResult*>)
					.ItemHeight(30)
					.ListItemsSource(&FilteredResults)
					.HeaderRow(HeaderRowWidget)
					.SelectionMode(ESelectionMode::Single)
					.OnGenerateRow(this, &SInterchangeResultsBrowserWindow::OnGenerateRowForList)
					.OnSelectionChanged(this, &SInterchangeResultsBrowserWindow::OnSelectionChanged)
				]
				+ SSplitter::Slot()
				.Value(0.2f)
				[
					SNew(SBorder)
					.Padding(3)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					[
						SNew(SBox)
						.Padding(10)
						[
							SAssignNew(Description, STextBlock)
							.AutoWrapText(true)
						]
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(2)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(2)
				+ SUniformGridPanel::Slot(0, 0)
				[
					IDocumentation::Get()->CreateAnchor(FString("Engine/Content/Interchange/ErrorBrowser"))
				]
				+ SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("ErrorBrowser_Clear", "Clear"))
					.IsEnabled(false)
					.OnClicked(this, &SInterchangeResultsBrowserWindow::OnCloseDialog)
				]
				+ SUniformGridPanel::Slot(2, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("ErrorBrowser_Close", "Close"))
					.OnClicked(this, &SInterchangeResultsBrowserWindow::OnCloseDialog)
				]
			]
		]
	];

	Set(InArgs._InterchangeResultsContainer);
}


void SInterchangeResultsBrowserWindow::Set(UInterchangeResultsContainer* Data)
{
	ResultsContainer = Data;
	RepopulateItems();
}


void SInterchangeResultsBrowserWindow::RepopulateItems()
{
	FilteredResults.Empty();

	if (ResultsContainer != nullptr)
	{
		for (UInterchangeResult* Item : ResultsContainer->GetResults())
		{
			if (Item->GetResultType() != EInterchangeResultType::Success || !bIsFiltered)
			{
				FilteredResults.Add(Item);
			}
		}

		RequestSort();
	}
}


void SInterchangeResultsBrowserWindow::CloseErrorBrowser()
{
	if (TSharedPtr<SDockTab> OwnerTabPin = OwnerTab.Pin())
	{
		OwnerTabPin->RequestCloseTab();
	}
	OwnerTab = nullptr;
}


TSharedRef<ITableRow> SInterchangeResultsBrowserWindow::OnGenerateRowForList(UInterchangeResult* InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedRef<ITableRow> Row =
		SNew(SInterchangeResultsBrowserListRow, OwnerTable)
		.InterchangeResultsBrowserWidget(SharedThis(this))
		.Item(InItem);

	return Row;
}


EColumnSortMode::Type SInterchangeResultsBrowserWindow::GetColumnSortMode(const FName ColumnId) const
{
	if (SortByColumn != ColumnId)
	{
		return EColumnSortMode::None;
	}

	return SortMode;
}


void SInterchangeResultsBrowserWindow::OnColumnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type InSortMode)
{
	SortByColumn = ColumnId;
	SortMode = InSortMode;

	RequestSort();
}


void SInterchangeResultsBrowserWindow::OnFilterStateChanged(ECheckBoxState NewState)
{
	bIsFiltered = (NewState == ECheckBoxState::Checked);
	OnFilterChangedState.ExecuteIfBound(bIsFiltered);
	RepopulateItems();
}


void SInterchangeResultsBrowserWindow::RequestSort()
{
	if (SortByColumn == SResultsBrowserWidgetDefs::ColumnID_ResultType)
	{
		if (SortMode == EColumnSortMode::Ascending)
		{
			FilteredResults.Sort([](const UInterchangeResult& A, const UInterchangeResult& B) {
				return static_cast<int>(A.GetResultType()) < static_cast<int>(B.GetResultType()); });
		}
		else if (SortMode == EColumnSortMode::Descending)
		{
			FilteredResults.Sort([](const UInterchangeResult& A, const UInterchangeResult& B) {
				return static_cast<int>(A.GetResultType()) >= static_cast<int>(B.GetResultType()); });
		}
	}
	else if (SortByColumn == SResultsBrowserWidgetDefs::ColumnID_Source)
	{
		if (SortMode == EColumnSortMode::Ascending)
		{
			FilteredResults.Sort([](const UInterchangeResult& A, const UInterchangeResult& B) {
				return A.SourceAssetName < B.SourceAssetName; });
		}
		else if (SortMode == EColumnSortMode::Descending)
		{
			FilteredResults.Sort([](const UInterchangeResult& A, const UInterchangeResult& B) {
				return A.SourceAssetName >= B.SourceAssetName; });
		}
	}
	else if (SortByColumn == SResultsBrowserWidgetDefs::ColumnID_Dest)
	{
		if (SortMode == EColumnSortMode::Ascending)
		{
			FilteredResults.Sort([](const UInterchangeResult& A, const UInterchangeResult& B) {
				return A.DestinationAssetName < B.DestinationAssetName; });
		}
		else if (SortMode == EColumnSortMode::Descending)
		{
			FilteredResults.Sort([](const UInterchangeResult& A, const UInterchangeResult& B) {
				return A.DestinationAssetName >= B.DestinationAssetName; });
		}
	}
	else if (SortByColumn == SResultsBrowserWidgetDefs::ColumnID_AssetType)
	{
		if (SortMode == EColumnSortMode::Ascending)
		{
			FilteredResults.Sort([](const UInterchangeResult& A, const UInterchangeResult& B) {
				FString NameA = A.AssetType ? A.AssetType->GetName() : FString();
				FString NameB = B.AssetType ? B.AssetType->GetName() : FString();
				return NameA < NameB;
				});
		}
		else if (SortMode == EColumnSortMode::Descending)
		{
			FilteredResults.Sort([](const UInterchangeResult& A, const UInterchangeResult& B) {
				FString NameA = A.AssetType ? A.AssetType->GetName() : FString();
				FString NameB = B.AssetType ? B.AssetType->GetName() : FString();
				return NameA >= NameB;
				});
		}
	}

	ListView->RequestListRefresh();
}



void SInterchangeResultsBrowserWindow::OnSelectionChanged(UInterchangeResult* InItem, ESelectInfo::Type SelectInfo)
{
	if (InItem != nullptr)
	{
		Description->SetText(InItem->GetText());
	}
	else
	{
		Description->SetText(FText());
	}
}




void SInterchangeResultsBrowserListRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	InterchangeResultsBrowserWidgetPtr = InArgs._InterchangeResultsBrowserWidget;
	Item = InArgs._Item;

	SMultiColumnTableRow<UInterchangeResult*>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}


static FName GetImageFromResultType(EInterchangeResultType ResultType)
{
	switch (ResultType)
	{
	case EInterchangeResultType::Success: return FName("InterchangeResultsBrowser.ResultType.Success");
	case EInterchangeResultType::Warning: return FName("InterchangeResultsBrowser.ResultType.Warning");
	case EInterchangeResultType::Error: return FName("InterchangeResultsBrowser.ResultType.Error");
	}

	return FName();
}


TSharedRef<SWidget> SInterchangeResultsBrowserListRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (TSharedPtr<SInterchangeResultsBrowserWindow> InterchangeResultsBrowserWidget = InterchangeResultsBrowserWidgetPtr.Pin())
	{
		const FMargin RowPadding(8, 2, 2, 0);

		if (ColumnName == SResultsBrowserWidgetDefs::ColumnID_ResultType)
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(RowPadding)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(FSlateIconFinder::FindIcon(GetImageFromResultType(Item->GetResultType())).GetOptionalIcon())
				];
		}
		else if (ColumnName == SResultsBrowserWidgetDefs::ColumnID_Source)
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(RowPadding)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FPaths::GetCleanFilename(Item->SourceAssetName)))
					.ToolTipText(FText::FromString(Item->SourceAssetName))
				];
		}
		else if (ColumnName == SResultsBrowserWidgetDefs::ColumnID_Dest)
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(RowPadding)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FPackageName::ObjectPathToObjectName(Item->DestinationAssetName)))
					.ToolTipText(FText::FromString(Item->DestinationAssetName))
				];
		}
		else if (ColumnName == SResultsBrowserWidgetDefs::ColumnID_AssetType)
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(RowPadding)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Item->AssetType ? Item->AssetType->GetName() : FString()))
				];
		}
		else if (ColumnName == SResultsBrowserWidgetDefs::ColumnID_Message)
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(RowPadding)
				[
					SNew(STextBlock)
					.Text(Item->GetText())
					.ToolTipText(Item->GetText())
				];
		}
	}

	return SNullWidget::NullWidget;
}


#undef LOCTEXT_NAMESPACE
