// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNiagaraSimCacheView.h"
#include "ViewModels/NiagaraSimCacheViewModel.h"
#include "NiagaraSimCache.h"

#include "CoreMinimal.h"

#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SHeaderRow.h"

#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"

#define LOCTEXT_NAMESPACE "NiagaraSimCacheView"

static const FName NAME_Instance("Instance");

class SSimCacheDataBufferRowWidget : public SMultiColumnTableRow<TSharedPtr<int32>>
{
public:
	SLATE_BEGIN_ARGS(SSimCacheDataBufferRowWidget) {}
		SLATE_ARGUMENT(TSharedPtr<int32>, RowIndexPtr)
		SLATE_ARGUMENT(TSharedPtr<FNiagaraSimCacheViewModel>, SimCacheViewModel)
	SLATE_END_ARGS()

	

	/** Construct function for this widget */
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		RowIndexPtr = InArgs._RowIndexPtr;
		SimCacheViewModel = InArgs._SimCacheViewModel;

		SMultiColumnTableRow<TSharedPtr<int32>>::Construct(
			FSuperRowType::FArguments()
			.Style(FAppStyle::Get(), "DataTableEditor.CellListViewRow"),
			InOwnerTableView
		);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override
	{
		const int32 InstanceIndex = *RowIndexPtr;
		if (InColumnName == NAME_Instance)
		{
			return SNew(STextBlock)
				.Text(FText::AsNumber(InstanceIndex));
		}
		else
		{
			return SNew(STextBlock)
				.Text(SimCacheViewModel->GetComponentText(InColumnName, InstanceIndex));
		}
	}

	TSharedPtr<int32>						RowIndexPtr;
	TSharedPtr<FNiagaraSimCacheViewModel>   SimCacheViewModel;
};

void SNiagaraSimCacheView::Construct(const FArguments& InArgs)
{
	SimCacheViewModel = InArgs._SimCacheViewModel;

	SimCacheViewModel->OnViewDataChanged().AddSP(this, &SNiagaraSimCacheView::OnViewDataChanged);

	HeaderRowWidget = SNew(SHeaderRow);

	UpdateColumns(false);
	UpdateRows(false);
	UpdateBufferSelectionList();

	TSharedRef<SScrollBar> HorizontalScrollBar =
		SNew(SScrollBar)
		.AlwaysShowScrollbar(true)
		.Thickness(12.0f)
		.Orientation(Orient_Horizontal);
	TSharedRef<SScrollBar> VerticalScrollBar =
		SNew(SScrollBar)
		.AlwaysShowScrollbar(true)
		.Thickness(12.0f)
		.Orientation(Orient_Vertical);
	
	// Widget
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			// Select cache buffer
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(10.0f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CacheBufferSelection", "Cache Buffer Selection"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				SNew(SComboBox<TSharedPtr<FBufferSelectionInfo>>)
				.OptionsSource(&BufferSelectionList)
				.OnGenerateWidget(this, &SNiagaraSimCacheView::BufferSelectionGenerateWidget)
				.OnSelectionChanged(this, &SNiagaraSimCacheView::BufferSelectionChanged)
				.InitiallySelectedItem(BufferSelectionList[0])
				[
					SNew(STextBlock)
					.Text(this, &SNiagaraSimCacheView::GetBufferSelectionText)
				]
			]
			// Component Filter
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(10.0f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ComponentFilter", "Component Filter"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				SNew(SEditableTextBox)
				.OnTextChanged(this, &SNiagaraSimCacheView::OnComponentFilterChange)
				.MinDesiredWidth(100)
			]
		]
		//// Main Spreadsheet View
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SScrollBox)
					.Orientation(Orient_Horizontal)
					.ExternalScrollbar(HorizontalScrollBar)
					+ SScrollBox::Slot()
					[
						SAssignNew(ListViewWidget, SListView<TSharedPtr<int32>>)
						.ListItemsSource(&RowItems)
						.OnGenerateRow(this, &SNiagaraSimCacheView::MakeRowWidget)
						.Visibility(EVisibility::Visible)
						.SelectionMode(ESelectionMode::Single)
						.ExternalScrollbar(VerticalScrollBar)
						.HeaderRow(HeaderRowWidget)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					VerticalScrollBar
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			HorizontalScrollBar
		]
	];
};

TSharedRef<ITableRow> SNiagaraSimCacheView::MakeRowWidget(const TSharedPtr<int32> RowIndexPtr, const TSharedRef<STableViewBase>& OwnerTable) const
{
	return
		SNew(SSimCacheDataBufferRowWidget, OwnerTable)
		.RowIndexPtr(RowIndexPtr)
		.SimCacheViewModel(SimCacheViewModel);
}

void SNiagaraSimCacheView::UpdateColumns(const bool bRefresh)
{
	// TODO: Hide columns rather than regenerating everything?
	HeaderRowWidget->ClearColumns();

	HeaderRowWidget->AddColumn(
		SHeaderRow::Column(NAME_Instance)
		.DefaultLabel(FText::FromName(NAME_Instance))
		.HAlignHeader(EHorizontalAlignment::HAlign_Center)
		.VAlignHeader(EVerticalAlignment::VAlign_Fill)
		.HAlignCell(EHorizontalAlignment::HAlign_Center)
		.VAlignCell(EVerticalAlignment::VAlign_Fill)
		.SortMode(EColumnSortMode::None)
	);



	const bool bFilterActive = ComponentFilterArray.Num() > 0;
	for (const FNiagaraSimCacheViewModel::FComponentInfo& ComponentInfo : SimCacheViewModel->GetComponentInfos())
	{
		if (bFilterActive)
		{
			const FString ComponentInfoString = ComponentInfo.Name.ToString();
			const bool bPassedFilter = ComponentFilterArray.ContainsByPredicate(
				[ComponentInfoString = ComponentInfo.Name.ToString()](const FString& ComponentFilter)
			{
				return ComponentInfoString.Contains(ComponentFilter);
			}
			);
			if (bPassedFilter == false)
			{
				continue;
			}
		}

		HeaderRowWidget->AddColumn(
			SHeaderRow::Column(ComponentInfo.Name)
			.DefaultLabel(FText::FromName(ComponentInfo.Name))
			.HAlignHeader(EHorizontalAlignment::HAlign_Center)
			.VAlignHeader(EVerticalAlignment::VAlign_Fill)
			.HAlignCell(EHorizontalAlignment::HAlign_Center)
			.VAlignCell(EVerticalAlignment::VAlign_Fill)
			.SortMode(EColumnSortMode::None)
		);
	}

	HeaderRowWidget->ResetColumnWidths();
	HeaderRowWidget->RefreshColumns();
	if (bRefresh && ListViewWidget)
	{
		ListViewWidget->RequestListRefresh();
	}
}

void SNiagaraSimCacheView::UpdateRows(const bool bRefresh)
{
	RowItems.Reset(SimCacheViewModel->GetNumInstances());
	for (int32 i = 0; i < SimCacheViewModel->GetNumInstances(); ++i)
	{
		RowItems.Emplace(MakeShared<int32>(i));
	}

	if (bRefresh && ListViewWidget)
	{
		ListViewWidget->RequestListRefresh();
	}
}

void SNiagaraSimCacheView::UpdateBufferSelectionList()
{
	BufferSelectionList.Empty();

	if (SimCacheViewModel->IsCacheValid())
	{
		BufferSelectionList.Emplace(MakeShared<FBufferSelectionInfo>(INDEX_NONE, LOCTEXT("SystemInstance", "System Instance")));
		for (int32 i = 0; i < SimCacheViewModel->GetNumEmitterLayouts(); ++i)
		{
			BufferSelectionList.Emplace(
				MakeShared<FBufferSelectionInfo>(i, FText::Format(LOCTEXT("EmitterFormat", "Emitter - {0}"), FText::FromName(SimCacheViewModel->GetEmitterLayoutName(i))))
			);
		}
	}
	else
	{
		BufferSelectionList.Emplace(MakeShared<FBufferSelectionInfo>(INDEX_NONE, LOCTEXT("InvalidCache", "Invalid Cache")));
	}
}

TSharedRef<SWidget> SNiagaraSimCacheView::BufferSelectionGenerateWidget(TSharedPtr<FBufferSelectionInfo> InItem)
{
	return
		SNew(STextBlock)
		.Text(InItem->Value);
}

void SNiagaraSimCacheView::BufferSelectionChanged(TSharedPtr<FBufferSelectionInfo> NewSelection, ESelectInfo::Type SelectInfo)
{
	if(!NewSelection.IsValid() || NewSelection->Key == SimCacheViewModel->GetEmitterIndex())
	{
		return;
	}
	
	SimCacheViewModel->SetEmitterIndex(NewSelection->Key);
	UpdateColumns(false);
	UpdateRows(true);
}

FText SNiagaraSimCacheView::GetBufferSelectionText() const
{
	const int32 EmitterIndex = SimCacheViewModel->GetEmitterIndex();
	for (const TSharedPtr<FBufferSelectionInfo>& SelectionInfo : BufferSelectionList)
	{
		if (SelectionInfo->Key == EmitterIndex)
		{
			return SelectionInfo->Value;
		}
	}
	return FText::GetEmpty();
}

void SNiagaraSimCacheView::OnComponentFilterChange(const FText& InFilter)
{
	ComponentFilterArray.Empty();
	InFilter.ToString().ParseIntoArray(ComponentFilterArray, TEXT(","));
	UpdateColumns(true);
}

void SNiagaraSimCacheView::OnSimCacheChanged(const FAssetData& InAsset)
{
	FSlateApplication::Get().DismissAllMenus();

	if (UNiagaraSimCache* SimCache = Cast<UNiagaraSimCache>(InAsset.GetAsset()))
	{
		SimCacheViewModel->Initialize(SimCache);
		UpdateRows(true);
		UpdateColumns(true);
		UpdateBufferSelectionList();
	}
}

void SNiagaraSimCacheView::OnViewDataChanged(const bool bFullRefresh)
{
	UpdateRows(true);
	if (bFullRefresh)
	{
		UpdateColumns(true);
		UpdateBufferSelectionList();
	}
}

#undef LOCTEXT_NAMESPACE
