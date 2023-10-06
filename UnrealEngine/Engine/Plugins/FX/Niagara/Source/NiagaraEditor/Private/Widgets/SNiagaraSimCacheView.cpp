// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNiagaraSimCacheView.h"
#include "ViewModels/NiagaraSimCacheViewModel.h"

#include "CoreMinimal.h"

#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SHeaderRow.h"

#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "SNiagaraSimCacheTreeView.h"

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
		
		if(!SimCacheViewModel.Get()->IsCacheValid())
		{
			return SNullWidget::NullWidget;
		}

		const int32 InstanceIndex = *RowIndexPtr;
		
		if (InColumnName == NAME_Instance)
		{
			return SNew(STextBlock)
				.Text(FText::AsNumber(InstanceIndex));
		}
		
		return SNew(STextBlock)
			.Text(SimCacheViewModel->GetComponentText(InColumnName, InstanceIndex));
		
	}

	TSharedPtr<int32>						RowIndexPtr;
	TSharedPtr<FNiagaraSimCacheViewModel>   SimCacheViewModel;
};

void SNiagaraSimCacheView::Construct(const FArguments& InArgs)
{
	SimCacheViewModel = InArgs._SimCacheViewModel;

	SimCacheViewModel->OnViewDataChanged().AddSP(this, &SNiagaraSimCacheView::OnViewDataChanged);
	SimCacheViewModel->OnSimCacheChanged().AddSP(this, &SNiagaraSimCacheView::OnSimCacheChanged);
	SimCacheViewModel->OnBufferChanged().AddSP(this, &SNiagaraSimCacheView::OnBufferChanged);

	HeaderRowWidget = SNew(SHeaderRow);

	UpdateColumns(true);
	UpdateRows(false);

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
		//// Main Spreadsheet View
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
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
					.ConsumeMouseWheel(EConsumeMouseWheel::Always)
					.AllowOverscroll(EAllowOverscroll::No)
					.HeaderRow(HeaderRowWidget)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				VerticalScrollBar
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

void SNiagaraSimCacheView::GenerateColumns()
{
	//  Give columns a width to prevent them from being shrunk when filtering. 
	constexpr float ManualWidth = 125.0f;
	HeaderRowWidget->ClearColumns();

	if(!SimCacheViewModel->IsCacheValid())
	{
		return;
	}
	
	// Generate instance count column
	HeaderRowWidget->AddColumn(
		SHeaderRow::Column(NAME_Instance)
		.DefaultLabel(FText::FromName(NAME_Instance))
		.HAlignHeader(EHorizontalAlignment::HAlign_Center)
		.VAlignHeader(EVerticalAlignment::VAlign_Fill)
		.HAlignCell(EHorizontalAlignment::HAlign_Center)
		.VAlignCell(EVerticalAlignment::VAlign_Fill)
		.ManualWidth(ManualWidth)
		.SortMode(EColumnSortMode::None)
	);
	
	// Generate a column for each component
	for (const FNiagaraSimCacheViewModel::FComponentInfo& ComponentInfo : SimCacheViewModel->GetCurrentComponentInfos())
	{
		HeaderRowWidget->AddColumn(
			SHeaderRow::Column(ComponentInfo.Name)
			.DefaultLabel(FText::FromName(ComponentInfo.Name))
			.HAlignHeader(EHorizontalAlignment::HAlign_Center)
			.VAlignHeader(EVerticalAlignment::VAlign_Fill)
			.HAlignCell(EHorizontalAlignment::HAlign_Center)
			.VAlignCell(EVerticalAlignment::VAlign_Fill)
			.FillWidth(1.0f)
			.ManualWidth(ManualWidth)
			.ShouldGenerateWidget(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &SNiagaraSimCacheView::GetShouldGenerateWidget, ComponentInfo.Name)))
			.SortMode(EColumnSortMode::None)
		);
	}

}

void SNiagaraSimCacheView::UpdateColumns(const bool bReset)
{
	if(bReset)
	{
		GenerateColumns();
	}
	
	HeaderRowWidget->RefreshColumns();
	
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

void SNiagaraSimCacheView::OnSimCacheChanged()
{
	UpdateRows(true);
	UpdateColumns(true);
}

void SNiagaraSimCacheView::OnViewDataChanged(const bool bFullRefresh)
{
	UpdateRows(true);
	if (bFullRefresh)
	{
		UpdateColumns(false);
	}
}

void SNiagaraSimCacheView::OnBufferChanged()
{
	UpdateRows(true);
	UpdateColumns(true);
}

bool SNiagaraSimCacheView::GetShouldGenerateWidget(FName Name)
{
	TArray<FString> FilterArray;
	FilterArray = SimCacheViewModel->GetComponentFilters();

	if(!SimCacheViewModel->IsComponentFilterActive())
	{
		return true;
	}

	FString ColumnName = Name.ToString();

	// Always display the instance column
	if(ColumnName.Equals(NAME_Instance.ToString()))
	{
		return true;
	}

	auto ComponentFilterPred = [&ColumnName](const FString& ComponentFilter)
	{
		return ColumnName.Equals(ComponentFilter);
	};
	
	return FilterArray.ContainsByPredicate(ComponentFilterPred);
}

#undef LOCTEXT_NAMESPACE
