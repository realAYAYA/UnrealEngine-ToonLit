// Copyright Epic Games, Inc. All Rights Reserved.

#include "DragDrop/DMSlotLayerDragDropOperation.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/Layers/SDMSlotLayerItem.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SOverlay.h"

FDMSlotLayerDragDropOperation::FDMSlotLayerDragDropOperation(const TSharedRef<SDMSlotLayerItem>& InLayerItemWidget, const bool bInShouldDuplicate)
	: LayerItemWidgetWeak(InLayerItemWidget), bShouldDuplicate(bInShouldDuplicate)
{
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	WidgetRenderer = MakeShared<FWidgetRenderer>(true);

	const FVector2D DrawSize = InLayerItemWidget->GetTickSpaceGeometry().GetLocalSize();
	TextureRenderTarget = TStrongObjectPtr<UTextureRenderTarget2D>(WidgetRenderer->DrawWidget(InLayerItemWidget, DrawSize));

	WidgetTextureBrush.SetImageSize(DrawSize);
	WidgetTextureBrush.SetResourceObject(TextureRenderTarget.Get());

	bCreateNewWindow = false;
	Construct();
}

TSharedPtr<SWidget> FDMSlotLayerDragDropOperation::GetDefaultDecorator() const
{
	static const FLinearColor InvalidLocationColor = FLinearColor(1.0f, 0.0f, 0.0f, 0.05f);

	return 
		SNew(SOverlay)
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SImage)
			.ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.1f))
			.Image(&WidgetTextureBrush)
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SColorBlock)
			.Color(InvalidLocationColor)
			.Visibility_Raw(this, &FDMSlotLayerDragDropOperation::GetInvalidDropVisibility)
		];
}

FCursorReply FDMSlotLayerDragDropOperation::OnCursorQuery()
{
	return FCursorReply::Cursor(EMouseCursor::GrabHandClosed);
}

EVisibility FDMSlotLayerDragDropOperation::GetInvalidDropVisibility() const
{
	return bValidDropLocation ? EVisibility::Hidden : EVisibility::SelfHitTestInvisible;
}

UDMMaterialLayerObject* FDMSlotLayerDragDropOperation::GetLayer() const
{
	if (TSharedPtr<SDMSlotLayerItem> LayerItemWidget = LayerItemWidgetWeak.Pin())
	{
		return LayerItemWidget->GetLayer();
	}

	return nullptr;
}
