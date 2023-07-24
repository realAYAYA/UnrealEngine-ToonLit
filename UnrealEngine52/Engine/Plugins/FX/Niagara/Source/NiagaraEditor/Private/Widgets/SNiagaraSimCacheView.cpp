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

bool SNiagaraSimCacheView::IsStringFilterEnabled() const
{
	return !SimCacheViewModel->IsComponentFilterActive();
}

void SNiagaraSimCacheView::Construct(const FArguments& InArgs)
{
	SimCacheViewModel = InArgs._SimCacheViewModel;

	SimCacheViewModel->OnViewDataChanged().AddSP(this, &SNiagaraSimCacheView::OnViewDataChanged);
	SimCacheViewModel->OnSimCacheChanged().AddSP(this, &SNiagaraSimCacheView::OnSimCacheChanged);
	SimCacheViewModel->OnBufferChanged().AddSP(this, &SNiagaraSimCacheView::OnBufferChanged);

	HeaderRowWidget = SNew(SHeaderRow);

	UpdateColumns(true);
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
				.IsEnabled(this, &SNiagaraSimCacheView::IsStringFilterEnabled)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				SNew(SEditableTextBox)
				.OnTextChanged(this, &SNiagaraSimCacheView::OnComponentFilterChange)
				.MinDesiredWidth(100)
				.IsEnabled(this, &SNiagaraSimCacheView::IsStringFilterEnabled)
			]
		]
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
	
	TArray<FString> FilterArray;

	// There are two filtering methods, from the tree view and from the spreadsheet view.
	// The spreadsheet view uses a filter string input by the user. The tree view uses component item selection.
	const bool bUseComponentFilter = SimCacheViewModel->IsComponentFilterActive();

	FilterArray = bUseComponentFilter ? SimCacheViewModel->GetComponentFilters() : StringFilterArray;

	// If the component filter is active, always apply the filter. If using the string filter, only apply it if it's not empty.
	const bool bApplyFilter = bUseComponentFilter || !FilterArray.IsEmpty();

	// If the columns are newly generated, and there are no filters to apply they are already up to date.
	const bool bColumnsUpToDate = bReset && !bApplyFilter;

	FString ColumnName;

	auto ComponentFilterPred = [&ColumnName](const FString& ComponentFilter)
	{
		return ColumnName.Equals(ComponentFilter);
	};

	auto StringFilterPred = [&ColumnName](const FString& StringFilter)
	{
		return ColumnName.Contains(StringFilter);
	};
	
	if(!bColumnsUpToDate)
	{
		for (SHeaderRow::FColumn Column : HeaderRowWidget->GetColumns())
		{
			ColumnName = Column.DefaultText.Get().ToString();

			// Never filter instance count column.
			if(ColumnName.Equals(NAME_Instance.ToString()))
			{
				continue;
			}

			bool bPassedFilter = true;

			if(bApplyFilter)
			{
				bPassedFilter = bUseComponentFilter ? FilterArray.ContainsByPredicate(ComponentFilterPred) : FilterArray.ContainsByPredicate(StringFilterPred);
			}
			
			HeaderRowWidget->SetShowGeneratedColumn(Column.ColumnId, bPassedFilter);
		}

		HeaderRowWidget->RefreshColumns();
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
	InFilter.ToString().ParseIntoArray(StringFilterArray, TEXT(","));
	UpdateColumns(false);
}

void SNiagaraSimCacheView::OnSimCacheChanged()
{
	UpdateRows(true);
	UpdateColumns(true);
	UpdateBufferSelectionList();
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

#undef LOCTEXT_NAMESPACE
