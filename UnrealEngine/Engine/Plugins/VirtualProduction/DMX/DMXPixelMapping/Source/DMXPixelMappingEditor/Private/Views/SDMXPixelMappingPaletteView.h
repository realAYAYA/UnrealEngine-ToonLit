// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DMXPixelMappingEditorCommon.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Misc/TextFilter.h"

template<typename ItemType>
class STreeView;

template<typename ItemType>
class TreeFilterHandler;

class ITableRow;
class STableViewBase;

class SDMXPixelMappingPaletteView
	: public SCompoundWidget
{
public:
	using ViewModelsArray = TArray<FDMXPixelMappingPreviewWidgetViewModelPtr>;
	using TreeView = STreeView<FDMXPixelMappingPreviewWidgetViewModelPtr>;
	using FTreeViewPtr = TSharedPtr<TreeView>;
	using FFilterHandler = TreeFilterHandler<FDMXPixelMappingPreviewWidgetViewModelPtr>;
	using WidgetViewModelTextFilter = TTextFilter<FDMXPixelMappingPreviewWidgetViewModelPtr>;

public:

	SLATE_BEGIN_ARGS(SDMXPixelMappingPaletteView) { }
	SLATE_END_ARGS()

public:
	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The construction arguments.
	 */
	void Construct(const FArguments& InArgs, const TSharedPtr<FDMXPixelMappingToolkit>& InToolkit);

	//~ Begin SWidget interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime);
	//~ End SWidget interface

private:

	void OnGetChildren(FDMXPixelMappingPreviewWidgetViewModelPtr Item, ViewModelsArray& Children);

	TSharedRef<ITableRow> OnGenerateWidgetTemplateItem(FDMXPixelMappingPreviewWidgetViewModelPtr Item, const TSharedRef<STableViewBase>& OwnerTable);

	void GetWidgetFilterStrings(FDMXPixelMappingPreviewWidgetViewModelPtr WidgetViewModel, TArray<FString>& OutStrings) {}

private:
	TWeakPtr<FDMXPixelMappingToolkit> Toolkit;

	FTreeViewPtr TreeViewPtr;

	TSharedPtr<FFilterHandler> FilterHandler;

	TSharedPtr<WidgetViewModelTextFilter> WidgetFilter;

	ViewModelsArray TreeWidgetViewModels;

	bool bRefreshRequested;
};
