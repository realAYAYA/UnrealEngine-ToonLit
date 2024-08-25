// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Input/DragAndDrop.h"
#include "Slate/Layers/SDMSlotLayerView.h"
#include "UObject/StrongObjectPtr.h"

class FWidgetRenderer;
class SDMLayerEffectsItem;
class SWidget;
class UDMMaterialStage;
class UTextureRenderTarget2D;

class FDMLayerEffectsDragDropOperation : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FDMLayerEffectsDragDropOperation, FDragDropOperation)

	FDMLayerEffectsDragDropOperation(const TSharedRef<SDMLayerEffectsItem>& InLayerItemWidget, const bool bInShouldDuplicate);

	FORCEINLINE bool IsValidDropLocation() { return bValidDropLocation; }
	FORCEINLINE void SetValidDropLocation(const bool bIsValid) { bValidDropLocation = bIsValid; }
	FORCEINLINE void SetToValidDropLocation() { bValidDropLocation = true; }
	FORCEINLINE void SetToInvalidDropLocation() { bValidDropLocation = false; }

	TSharedPtr<SDMLayerEffectsItem> GetLayerItemWidget() const { return LayerItemWidgetWeak.Pin(); }

	//~ Begin FDragDropOperation
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override;
	virtual FCursorReply OnCursorQuery() override;
	//~ End FDragDropOperation

protected:
	TWeakPtr<SDMLayerEffectsItem> LayerItemWidgetWeak;
	bool bShouldDuplicate;

	bool bValidDropLocation = true;

	TSharedPtr<FWidgetRenderer> WidgetRenderer;

	TStrongObjectPtr<UTextureRenderTarget2D> TextureRenderTarget;
	FSlateBrush WidgetTextureBrush;

	EVisibility GetInvalidDropVisibility() const;
};
