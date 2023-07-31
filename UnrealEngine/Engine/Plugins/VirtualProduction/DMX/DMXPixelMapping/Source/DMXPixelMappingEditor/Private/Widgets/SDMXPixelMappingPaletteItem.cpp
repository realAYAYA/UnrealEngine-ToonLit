// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SDMXPixelMappingPaletteItem.h"

#include "DragDrop/DMXPixelMappingDragDropOp.h"
#include "Templates/DMXPixelMappingComponentTemplate.h"
#include "ViewModels/DMXPixelMappingPaletteViewModel.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Styling/AppStyle.h"


namespace UE::DMX::Private::SDMXPixelMappingPaletteItem
{
	constexpr FLinearColor NormalBGColor(0.f, 0.0f, 0.f, 0.f); // Fully transparent so it just shows the background
	constexpr FLinearColor HoveredBGColor(1.f, 1.f, 1.f, 0.2f); // White transparent
}

void SDMXPixelMappingHierarchyItemHeader::Construct(const FArguments& InArgs, const TSharedRef< STableViewBase >& InOwnerTableView, const TSharedPtr<FDMXPixelMappingPaletteWidgetViewModel>& InViewModel)
{
	STableRow<FDMXPixelMappingPreviewWidgetViewModelPtr>::Construct(
		STableRow<FDMXPixelMappingPreviewWidgetViewModelPtr>::FArguments()
		.Padding(1.0f)
		.Style(FAppStyle::Get(), "UMGEditor.PaletteHeader")
		.Content()
		[
			SNew(STextBlock)
			.Text(InViewModel->GetName())
		],
		InOwnerTableView);
}

void SDMXPixelMappingHierarchyItemTemplate::Construct(const FArguments& InArgs, const TSharedRef< STableViewBase >& InOwnerTableView, const TSharedPtr<FDMXPixelMappingPaletteWidgetViewModel>& InViewModel)
{
	using namespace UE::DMX::Private::SDMXPixelMappingPaletteItem;

	ViewModel = InViewModel;

	STableRow<FDMXPixelMappingPreviewWidgetViewModelPtr>::Construct(
		STableRow<FDMXPixelMappingPreviewWidgetViewModelPtr>::FArguments()
		.Padding(1.0f)
		.Style(FAppStyle::Get(), "UMGEditor.PaletteHeader")
		.ShowSelection(false)
		.OnDragDetected(this, &SDMXPixelMappingHierarchyItemTemplate::OnDraggingWidget)
		.Content()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
			.BorderBackgroundColor_Lambda([this]()
				{
					return IsHovered() ? HoveredBGColor : NormalBGColor;
				})
			[
				SNew(STextBlock)
				.Text(InViewModel->GetName())
			]
		],
		InOwnerTableView);

}

FReply SDMXPixelMappingHierarchyItemTemplate::OnDraggingWidget(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	check(ViewModel.IsValid());

	if (FDMXPixelMappingComponentTemplatePtr ComponentTemplatePtr = ViewModel.Pin()->GetTemplate())
	{
		const FVector2D DragOffset = FVector2D::ZeroVector;
		TArray<FDMXPixelMappingComponentTemplatePtr> TemplateArray = TArray<FDMXPixelMappingComponentTemplatePtr>({ ComponentTemplatePtr });

		return FReply::Handled().BeginDragDrop(FDMXPixelMappingDragDropOp::New(DragOffset, TemplateArray, nullptr));
	}

	return FReply::Unhandled();
}
