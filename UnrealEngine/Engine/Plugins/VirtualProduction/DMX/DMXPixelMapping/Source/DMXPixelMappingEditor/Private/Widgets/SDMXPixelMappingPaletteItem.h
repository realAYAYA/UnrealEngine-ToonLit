// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DMXPixelMappingEditorCommon.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class STableViewBase;
class FDMXPixelMappingPaletteWidgetViewModel;
class FDMXPixelMappingComponentTemplate;
class FReply;

class SDMXPixelMappingHierarchyItemHeader
	: public STableRow<TSharedPtr<FDMXPixelMappingPaletteWidgetViewModel>>
{
public:
	SLATE_BEGIN_ARGS(SDMXPixelMappingHierarchyItemHeader) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef< STableViewBase >& InOwnerTableView, const TSharedPtr<FDMXPixelMappingPaletteWidgetViewModel>& InViewModel);
};

class SDMXPixelMappingHierarchyItemTemplate
	: public STableRow<TSharedPtr<FDMXPixelMappingPaletteWidgetViewModel>>
{
public:
	SLATE_BEGIN_ARGS(SDMXPixelMappingHierarchyItemTemplate) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef< STableViewBase >& InOwnerTableView, const TSharedPtr<FDMXPixelMappingPaletteWidgetViewModel>& InViewModel);

private:
	FReply OnDraggingWidget(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

private:
	TWeakPtr<FDMXPixelMappingPaletteWidgetViewModel> ViewModel;
};