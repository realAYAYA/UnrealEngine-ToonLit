// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Input/DragAndDrop.h"
#include "Slate/Layers/SDMSlotLayerView.h"
#include "UObject/StrongObjectPtr.h"

class FWidgetRenderer;
class SDMSlotLayerItem;
class SWidget;
class UDMMaterialLayerObject;
class UDMMaterialStage;
class UTextureRenderTarget2D;

class FDMSlotLayerDragDropOperation : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FDMSlotLayerDragDropOperation, FDragDropOperation)

	FDMSlotLayerDragDropOperation(const TSharedRef<SDMSlotLayerItem>& InLayerItemWidget, const bool bInShouldDuplicate);

	FORCEINLINE TSharedPtr<SDMSlotLayerItem> GetLayerItemWidget() const { return LayerItemWidgetWeak.Pin(); }
	UDMMaterialLayerObject* GetLayer() const;

	FORCEINLINE bool IsValidDropLocation() { return bValidDropLocation; }
	FORCEINLINE void SetValidDropLocation(const bool bIsValid) { bValidDropLocation = bIsValid; }
	FORCEINLINE void SetToValidDropLocation() { bValidDropLocation = true; }
	FORCEINLINE void SetToInvalidDropLocation() { bValidDropLocation = false; }

	//~ Begin FDragDropOperation
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override;
	virtual FCursorReply OnCursorQuery() override;
	//~ End FDragDropOperation

protected:
	TWeakPtr<SDMSlotLayerItem> LayerItemWidgetWeak;
	bool bShouldDuplicate;

	bool bValidDropLocation = true;

	TSharedPtr<FWidgetRenderer> WidgetRenderer;

	TStrongObjectPtr<UTextureRenderTarget2D> TextureRenderTarget;
	FSlateBrush WidgetTextureBrush;

	EVisibility GetInvalidDropVisibility() const;
};
