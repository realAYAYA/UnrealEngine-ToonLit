// Copyright Epic Games, Inc. All Rights Reserved.

#include "SGMEResourceList.h"

#include "SGMEResourceItem.h"
#include "SlateOptMacros.h"
#include "ViewModels/GMEResourceItemViewModel.h"
#include "ViewModels/GMEResourceListViewModel.h"
#include "Widgets/Layout/SSeparator.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

#define LOCTEXT_NAMESPACE "SGMEResourceList"

SGMEResourceList::~SGMEResourceList()
{
	if (ViewModel.IsValid())
	{
		ViewModel->OnChanged().RemoveAll(this);
	}
}

void SGMEResourceList::Construct(const FArguments& InArgs, TSharedRef<FGMEResourceListViewModel> InViewModel)
{
	constexpr float Padding = 2.0f;
	
	ViewModel = InViewModel;
	ViewModel->OnChanged().AddSPLambda(
		this,
		[this]()
		{
			Refresh();
		});

	SAssignNew(ResourceListWidget, SListView<TSharedPtr<IGMETreeNodeViewModel>>)
		.ListItemsSource(&ResourceItems)
		.OnGenerateRow(this, &SGMEResourceList::OnGenerateRow)
		.ScrollbarVisibility(EVisibility::Visible)
		.SelectionMode(ESelectionMode::None);

	Refresh();

	TWeakPtr<SGMEResourceList> ThisWeak = SharedThis(this);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("EditorViewportToolBar.Background"))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(Padding)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("TotalCount", "Total:"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(Padding)
				[
					SNew(STextBlock)
					.Text_Lambda([ThisWeak]()
					{
						if (ThisWeak.IsValid())
						{
							return FText::AsNumber(ThisWeak.Pin()->ResourceItems.Num());
						}
						return FText::AsNumber(0);
					})
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(Padding)
				[
					SNew(SSeparator)
					.Orientation(Orient_Vertical)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(Padding)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("TotalMemory", "Memory:"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(Padding)
				[
					SNew(STextBlock)
					.Text_Lambda([ThisWeak]()
					{
						if (ThisWeak.IsValid())
						{
							float Memory = 0.0f;
							for (TSharedPtr<IGMETreeNodeViewModel>& ChildItem : ThisWeak.Pin()->ResourceItems)
							{
								if (TSharedPtr<FGMEResourceItemViewModel> ResourceItem = StaticCastSharedPtr<FGMEResourceItemViewModel>(ChildItem))
								{
									Memory += ResourceItem->GetMemoryUsage();
								}								
							}
							return FText::Format(LOCTEXT("TotalMemoryValue", "{0}mb"), FText::AsNumber(Memory));
						}
						return FText::AsNumber(0);
					})
				]
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(1, 1, 1, Padding)
		[
			ResourceListWidget.ToSharedRef()
		]
	];
}

TSharedRef<ITableRow> SGMEResourceList::OnGenerateRow(
	TSharedPtr<IGMETreeNodeViewModel> InViewModel
	, const TSharedRef<STableViewBase>& InOwnerTable)
{
	check(InViewModel.IsValid());

	return SNew(SGMEResourceItem, InOwnerTable, StaticCastSharedPtr<FGMEResourceItemViewModel>(InViewModel).ToSharedRef());
}

void SGMEResourceList::Refresh()
{
	ResourceItems.Reset();
	ViewModel->GetChildren(ResourceItems);
	
	ResourceListWidget->RequestListRefresh();
}

#undef LOCTEXT_NAMESPACE

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
