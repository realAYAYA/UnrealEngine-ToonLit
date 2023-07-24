// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNiagaraSimCacheTreeView.h"

#include "NiagaraSimCacheViewModel.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"

#define LOCTEXT_NAMESPACE "NiagaraSimCacheTreeView"

class SNiagaraSimCacheTreeItem : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraSimCacheTreeItem) {}
		SLATE_ARGUMENT(TSharedPtr<FNiagaraSimCacheTreeItem>, Item)
		SLATE_ARGUMENT(TWeakPtr<SNiagaraSimCacheTreeView>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Item = InArgs._Item;
		Owner = InArgs._Owner;
	
		RefreshContent();
	}

	void RefreshContent() 
	{
		ChildSlot
		.Padding(2.0f)
		[
			Item->GetRowWidget()
		];
	}

	TSharedPtr<FNiagaraSimCacheTreeItem> Item;
	TWeakPtr<SNiagaraSimCacheTreeView> Owner;
};

//// Filter Widget /////


void SNiagaraSimCacheTreeViewFilterWidget::Construct(const FArguments& InArgs, TWeakPtr<FNiagaraSimCacheTreeItem> InTreeItem,
	TWeakPtr<SNiagaraSimCacheTreeView> InTreeView)
{
	WeakTreeItem = InTreeItem;
	WeakTreeView = InTreeView;

	ChildSlot
	.HAlign(HAlign_Center)
	.Padding(3.0f)
	[
		// Filter controls
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f)
		[
			// Clear All
			SNew(SButton)
			.Text(LOCTEXT("ClearAll", "Clear All"))
			.OnClicked(this, &SNiagaraSimCacheTreeViewFilterWidget::OnClearAllReleased)
			.IsEnabled(this, &SNiagaraSimCacheTreeViewFilterWidget::IsFilterActive)
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f)
		[
			// Select All
			SNew(SButton)
			.Text(LOCTEXT("SelectAll", "Select All"))
			.OnClicked(this, &SNiagaraSimCacheTreeViewFilterWidget::OnSelectAllReleased)
			.IsEnabled(this, &SNiagaraSimCacheTreeViewFilterWidget::IsFilterActive)
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f)
		[

			// Filter Toggle
			SNew(SCheckBox)
			.Type(ESlateCheckBoxType::ToggleButton)
			.HAlign(HAlign_Right)
			//.ToolTip(LOCTEXT("ToggleFiltering", "Toggle Filtering"))
			.Style(FNiagaraEditorStyle::Get(),"NiagaraEditor.SimCache.FilterToggleStyle")
			.OnCheckStateChanged(this, &SNiagaraSimCacheTreeViewFilterWidget::OnCheckStateChanged)
			.IsChecked(this, &SNiagaraSimCacheTreeViewFilterWidget::GetFilterState)
			.Padding(FMargin(3.0f, 2.0f))
			[
					
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.Filter"))
					
			]
		]
	];
}

void SNiagaraSimCacheTreeViewFilterWidget::OnCheckStateChanged(ECheckBoxState InState)
{
	TSharedPtr<SNiagaraSimCacheTreeView> TreeView = WeakTreeView.Pin();

	if(TreeView.IsValid())
	{
		const bool bCurrentlyActive = InState == ECheckBoxState::Checked;
		TreeView->SetFilterActive(bCurrentlyActive);
	}
	
}

FReply SNiagaraSimCacheTreeViewFilterWidget::OnClearAllReleased()
{
	TSharedPtr<SNiagaraSimCacheTreeView> TreeView = WeakTreeView.Pin();

	if(TreeView.IsValid())
	{
		TreeView->ClearFilterSelection();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SNiagaraSimCacheTreeViewFilterWidget::OnSelectAllReleased()
{
	TSharedPtr<SNiagaraSimCacheTreeView> TreeView = WeakTreeView.Pin();

	if(TreeView.IsValid())
	{
		TreeView->SelectAll();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

ECheckBoxState SNiagaraSimCacheTreeViewFilterWidget::GetFilterState() const
{
	return IsFilterActive() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

bool SNiagaraSimCacheTreeViewFilterWidget::IsFilterActive() const
{
	TSharedPtr<SNiagaraSimCacheTreeView> TreeView = WeakTreeView.Pin();

	if(TreeView.IsValid())
	{
		return TreeView->IsFilterActive();
	}

	return false;
}

//// Visibility Widget //////

void SSimCacheTreeViewVisibilityWidget::Construct(const FArguments& InArgs, TWeakPtr<FNiagaraSimCacheTreeItem> InTreeItem, TWeakPtr<SNiagaraSimCacheTreeView> InWeakTreeView)
{
	WeakTreeItem = InTreeItem;
	WeakTreeView = InWeakTreeView;

	ChildSlot
	.HAlign(HAlign_Center)
	.Padding(1.0f)
	[
		SNew(SCheckBox)
		.OnCheckStateChanged(this, &SSimCacheTreeViewVisibilityWidget::OnCheckStateChanged)
		.IsChecked(this, &SSimCacheTreeViewVisibilityWidget::GetCheckedState)
		.IsEnabled(this, &SSimCacheTreeViewVisibilityWidget::IsFilterActive)
	];
	
}
	
void SSimCacheTreeViewVisibilityWidget::OnCheckStateChanged(ECheckBoxState NewState)
{
	TSharedPtr<SNiagaraSimCacheTreeView> TreeView = WeakTreeView.Pin();
	TSharedPtr<FNiagaraSimCacheTreeItem> TreeItem = WeakTreeItem.Pin();
	
	if(TreeView.IsValid() && TreeItem.IsValid())
	{
		TreeView->VisibilityButtonClicked(TreeItem.ToSharedRef());
	}
}

ECheckBoxState SSimCacheTreeViewVisibilityWidget::GetCheckedState() const
{
	return IsInFilter()? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

bool SSimCacheTreeViewVisibilityWidget::IsInFilter() const
{
	TSharedPtr<SNiagaraSimCacheTreeView> TreeView = WeakTreeView.Pin();
	TSharedPtr<FNiagaraSimCacheTreeItem> TreeItem = WeakTreeItem.Pin();

	if(TreeView.IsValid() && TreeItem.IsValid())
	{
		return TreeView->IsItemInFilter(TreeItem);
	}

	return false;
}

bool SSimCacheTreeViewVisibilityWidget::IsFilterActive() const
{
	const TSharedPtr<SNiagaraSimCacheTreeView> TreeView = WeakTreeView.Pin();

	if(TreeView.IsValid())
	{
		return TreeView->IsFilterActive();
	}

	return false;
}

bool SSimCacheTreeViewVisibilityWidget::IsItemSelected() const
{
	const TSharedPtr<SNiagaraSimCacheTreeView> TreeView = WeakTreeView.Pin();
	const TSharedPtr<FNiagaraSimCacheTreeItem> TreeItem = WeakTreeItem.Pin();

	if(TreeView.IsValid() && TreeItem.IsValid())
	{
		return TreeView->IsItemSelected(TreeItem.ToSharedRef());
	}

	return false;
}

///// Tree View Widget

void SNiagaraSimCacheTreeView::Construct(const FArguments& InArgs)
{
	constexpr float ItemHeight = 50.0f;

	ViewModel = InArgs._SimCacheViewModel;

	ViewModel->OnBufferChanged().AddSP(this, &SNiagaraSimCacheTreeView::OnBufferChanged);

	ViewModel->BuildEntries(SharedThis(this));

	TArray<TSharedRef<FNiagaraSimCacheTreeItem>>* RootEntries = ViewModel->GetCurrentRootEntries();

	
	TreeView = SNew(STreeView<TSharedRef<FNiagaraSimCacheTreeItem>>)
	.ItemHeight(ItemHeight)
	.SelectionMode(ESelectionMode::Multi)
	.TreeItemsSource(ViewModel->GetCurrentRootEntries())
	.OnGenerateRow(this, &SNiagaraSimCacheTreeView::OnGenerateRow)
	.OnGetChildren(this, &SNiagaraSimCacheTreeView::OnGetChildren);

	if(RootEntries && !RootEntries->IsEmpty())
	{
		TreeView->SetItemExpansion((*RootEntries)[0], true);
	}


	ChildSlot
	[
		TreeView.ToSharedRef()
	];
}


TSharedRef<ITableRow> SNiagaraSimCacheTreeView::OnGenerateRow(TSharedRef<FNiagaraSimCacheTreeItem> Item,
                                                              const TSharedRef<STableViewBase>& OwnerTable)
{
	static const char* ItemStyles[] =
	{
		"NiagaraEditor.SimCache.SystemItem",
		"NiagaraEditor.SimCache.EmitterItem",
		"NiagaraEditor.SimCache.ComponentItem"
	};

	ENiagaraSimCacheOverviewItemType StyleType = Item->GetType();
	
	return SNew(STableRow<TSharedRef<FNiagaraSimCacheTreeItem>>, OwnerTable)
	.Style(FNiagaraEditorStyle::Get(), ItemStyles[(int32)StyleType])
	[
		SNew(SNiagaraSimCacheTreeItem)
		.Item(Item)
		.Owner(SharedThis(this))
	];
}

void SNiagaraSimCacheTreeView::OnGetChildren(TSharedRef<FNiagaraSimCacheTreeItem> InItem,
	TArray<TSharedRef<FNiagaraSimCacheTreeItem>>& OutChildren)
{
	OutChildren = InItem.Get().Children;
}

void SNiagaraSimCacheTreeView::OnBufferChanged()
{
	TreeView->RequestTreeRefresh();
	SelectionForFilter.Empty();
	ViewModel->SetComponentFilters(TArray<FString>());
	TreeView->SetItemExpansion((*ViewModel->GetCurrentRootEntries())[0], true);
}

void SNiagaraSimCacheTreeView::RecursiveAddToSelectionFilter(TArray<TSharedRef<FNiagaraSimCacheTreeItem>>& ArrayToAdd)
{
	for(const TSharedRef<FNiagaraSimCacheTreeItem>& Item : ArrayToAdd)
	{
		if(Item->Children.Num() > 0)
		{
			RecursiveAddToSelectionFilter(Item->Children);
		}
	}
	SelectionForFilter.Append(ArrayToAdd);
}

void SNiagaraSimCacheTreeView::RecursiveRemoveFromSelectionFilter(TArray<TSharedRef<FNiagaraSimCacheTreeItem>>& ArrayToRemove)
{
	for(TSharedRef<FNiagaraSimCacheTreeItem> Item : ArrayToRemove)
	{
		if(Item->Children.Num() > 0)
		{
			RecursiveRemoveFromSelectionFilter(Item->Children);
		}
		SelectionForFilter.Remove(Item);
	}
}

void SNiagaraSimCacheTreeView::UpdateSelectionFilter(TSharedRef<FNiagaraSimCacheTreeItem> ClickedItem)
{
	TArray<TSharedRef<FNiagaraSimCacheTreeItem>> SelectedItems = TreeView->GetSelectedItems();
	SelectedItems.AddUnique(ClickedItem);

	SelectedItems.RemoveAll([](const TSharedRef<FNiagaraSimCacheTreeItem> Item){ return Item->GetType() != ENiagaraSimCacheOverviewItemType::Component; });

	if(IsItemInFilter(ClickedItem))
	{
		RecursiveRemoveFromSelectionFilter(SelectedItems);
	}
	else
	{
		RecursiveAddToSelectionFilter(SelectedItems);
	}

	UpdateStringFilters();
}

bool SNiagaraSimCacheTreeView::IsFilterActive() const
{
	return ViewModel->IsComponentFilterActive();
}

bool SNiagaraSimCacheTreeView::IsItemInFilter(TSharedPtr<FNiagaraSimCacheTreeItem> InItem) const
{
	return ViewModel->GetComponentFilters().Contains(InItem->GetFilterName());
}

void SNiagaraSimCacheTreeView::SetFilterActive(bool bNewActive)
{
	ViewModel->SetComponentFilterActive(bNewActive);
}

void SNiagaraSimCacheTreeView::VisibilityButtonClicked(TSharedRef<FNiagaraSimCacheTreeItem> InItem)
{
	UpdateSelectionFilter(InItem);
}

bool SNiagaraSimCacheTreeView::IsItemSelected(TSharedRef<FNiagaraSimCacheTreeItem> InItem)
{
	return TreeView->GetSelectedItems().Contains(InItem);
}

void SNiagaraSimCacheTreeView::ClearFilterSelection()
{
	SelectionForFilter.Empty();
	ViewModel->SetComponentFilters(TArray<FString>());
}

void SNiagaraSimCacheTreeView::UpdateStringFilters()
{
	TArray<FString> StringFilters;
	for(TSharedRef<FNiagaraSimCacheTreeItem> FilterItem : SelectionForFilter)
	{
		if(FilterItem->GetType() == ENiagaraSimCacheOverviewItemType::Component)
		{
			StringFilters.AddUnique(FilterItem->GetFilterName());
		}
	}

	ViewModel->SetComponentFilters(StringFilters);
}

void SNiagaraSimCacheTreeView::SelectAll()
{
	RecursiveAddToSelectionFilter(*ViewModel->GetCurrentRootEntries());

	UpdateStringFilters();
}

#undef LOCTEXT_NAMESPACE
