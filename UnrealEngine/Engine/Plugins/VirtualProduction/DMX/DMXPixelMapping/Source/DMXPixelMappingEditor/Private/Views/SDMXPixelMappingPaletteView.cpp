// Copyright Epic Games, Inc. All Rights Reserved.

#include "Views/SDMXPixelMappingPaletteView.h"

#include "Toolkits/DMXPixelMappingToolkit.h"
#include "ViewModels/DMXPixelMappingPaletteViewModel.h"

#include "Widgets/Views/STreeView.h"
#include "Widgets/Layout/SScrollBorder.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Framework/Views/TreeFilterHandler.h"
#include "Misc/IFilter.h"

#define LOCTEXT_NAMESPACE "SDMXPixelMappingPaletteView"

void SDMXPixelMappingPaletteView::Construct(const FArguments& InArgs, const TSharedPtr<FDMXPixelMappingToolkit>& InToolkit)
{
	Toolkit = InToolkit;
	const TSharedPtr<FDMXPixelMappingPaletteViewModel>& PaletteViewModel = InToolkit->GetPaletteViewModel();

	WidgetFilter = MakeShared<WidgetViewModelTextFilter>(WidgetViewModelTextFilter::FItemToStringArray::CreateSP(this, &SDMXPixelMappingPaletteView::GetWidgetFilterStrings));

	FilterHandler = MakeShared<FFilterHandler>();
	FilterHandler->SetFilter(WidgetFilter.Get());
	FilterHandler->SetRootItems(&(PaletteViewModel->GetWidgetViewModels()), &TreeWidgetViewModels);
	FilterHandler->SetGetChildrenDelegate(FFilterHandler::FOnGetChildren::CreateRaw(this, &SDMXPixelMappingPaletteView::OnGetChildren));

	SAssignNew(TreeViewPtr, TreeView)
		.ItemHeight(1.f)
		.SelectionMode(ESelectionMode::Single)
		.OnGenerateRow(this, &SDMXPixelMappingPaletteView::OnGenerateWidgetTemplateItem)
		.OnGetChildren(FilterHandler.ToSharedRef(), &FFilterHandler::OnGetFilteredChildren)
		.TreeItemsSource(&TreeWidgetViewModels);

	FilterHandler->SetTreeView(TreeViewPtr.Get());
	
	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SScrollBorder, TreeViewPtr.ToSharedRef())
			[
				TreeViewPtr.ToSharedRef()
			]
		]
	];

	bRefreshRequested = true;
	PaletteViewModel->Update();
}

void SDMXPixelMappingPaletteView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (bRefreshRequested)
	{
		bRefreshRequested = false;
		FilterHandler->RefreshAndFilterTree();

		for (const FDMXPixelMappingPreviewWidgetViewModelPtr& Model : TreeWidgetViewModels)
		{
			TreeViewPtr->SetItemExpansion(Model, true);
		}
	}
}

void SDMXPixelMappingPaletteView::OnGetChildren(FDMXPixelMappingPreviewWidgetViewModelPtr Item, ViewModelsArray& Children)
{
	return Item->GetChildren(Children);
}

TSharedRef<ITableRow> SDMXPixelMappingPaletteView::OnGenerateWidgetTemplateItem(FDMXPixelMappingPreviewWidgetViewModelPtr Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return Item->BuildRow(OwnerTable);
}

#undef LOCTEXT_NAMESPACE
