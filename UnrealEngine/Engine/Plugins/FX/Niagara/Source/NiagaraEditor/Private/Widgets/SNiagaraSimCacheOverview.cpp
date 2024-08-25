// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNiagaraSimCacheOverview.h"

#include "SNiagaraSimCacheTreeView.h"

#define LOCTEXT_NAMESPACE "NiagaraSimCacheOverview"

class SNiagaraSimCacheBufferItem : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraSimCacheBufferItem) {}
		SLATE_ARGUMENT(TSharedPtr<FNiagaraSimCacheOverviewItem>, Item)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Item = InArgs._Item;

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

	TSharedPtr<FNiagaraSimCacheOverviewItem> Item;
};


void SNiagaraSimCacheOverview::OnSimCacheChanged()
{
	if(BufferListView.IsValid())
	{
		BufferListView->RebuildList();
	}
}

void SNiagaraSimCacheOverview::Construct(const FArguments& InArgs)
{
	ViewModel = InArgs._SimCacheViewModel;

	SAssignNew(TreeViewWidget, SNiagaraSimCacheTreeView)
	.SimCacheViewModel(ViewModel);

	SAssignNew(BufferListView, SListView<TSharedRef<FNiagaraSimCacheOverviewItem>>)
	.ListItemsSource(ViewModel->GetBufferEntries())
	.OnGenerateRow(this, &SNiagaraSimCacheOverview::OnGenerateRowForItem)
	.OnSelectionChanged(this, &SNiagaraSimCacheOverview::OnListSelectionChanged);

	ViewModel.Get()->OnSimCacheChanged().AddSP(this, &SNiagaraSimCacheOverview::OnSimCacheChanged);

	const float MinSplitterSlotSize = 30.0f; 

	ChildSlot
	[
		SNew(SSplitter)
		.Orientation(Orient_Vertical)
		+SSplitter::Slot()
		.Value(0.2f)
		.MinSize(MinSplitterSlotSize)
		[

			// Cache Buffers
			SNew(SSplitter)
			.Orientation(Orient_Vertical)
			+SSplitter::Slot()
			.Resizable(false)
			.SizeRule(SSplitter::SizeToContent)
			[
				// Header
				SNew(SBorder)
				.BorderImage(FAppStyle::GetNoBrush())
				.Padding(5.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("CacheBufferSelection", "Cache Buffer Selection"))
				]
			]
			+SSplitter::Slot()
			.Resizable(false)
			[
				// List View
				BufferListView.ToSharedRef()
			]
		]
		+SSplitter::Slot()
		.Value(0.8f)
		.MinSize(MinSplitterSlotSize)
		[
			// Component Details
			SNew(SSplitter)
			.Orientation(Orient_Vertical)
			+SSplitter::Slot()
			.Resizable(false)
			.SizeRule(SSplitter::SizeToContent)
			[
				// Header
				SNew(SBorder)
				.BorderImage(FAppStyle::GetNoBrush())
				.Padding(5.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ComponentTreeView", "Component Details"))
				]
			]
			+SSplitter::Slot()
			.Resizable(false)
			[
				// Tree View
				TreeViewWidget.ToSharedRef()
			]
			
		]
	];
}

TSharedRef<ITableRow> SNiagaraSimCacheOverview::OnGenerateRowForItem(TSharedRef<FNiagaraSimCacheOverviewItem> Item,
	const TSharedRef<STableViewBase>& Owner)
{
	static const char* ItemStyles[] =
	{
		"NiagaraEditor.SimCache.SystemItem",
		"NiagaraEditor.SimCache.EmitterItem",
		"NiagaraEditor.SimCache.ComponentItem",
		"NiagaraEditor.SimCache.DataInterfaceItem"
	};

	ENiagaraSimCacheOverviewItemType StyleType = Item->GetType();
	
	return SNew(STableRow<TSharedRef<FNiagaraSimCacheOverviewItem>>, Owner)
	.Style(FNiagaraEditorStyle::Get(), ItemStyles[static_cast<int32>(StyleType)])
	.Padding(1.0f)
	[
		SNew(SNiagaraSimCacheBufferItem)
		.Item(Item)
	];
}

void SNiagaraSimCacheOverview::OnListSelectionChanged(TSharedPtr<FNiagaraSimCacheOverviewItem> Item, ESelectInfo::Type)
{
	if (Item.IsValid())
	{
		ViewModel->SetEmitterIndex(Item->GetBufferIndex(), Item->GetDataInterface());
	}
}

#undef LOCTEXT_NAMESPACE