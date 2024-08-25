// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMDefs.h"
#include "SlateMaterialBrush.h"
#include "Types/WidgetMouseEventsDelegate.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/SCompoundWidget.h"

class UDMMaterialComponent;
class UDMMaterialSlot;
enum class EDMMaterialLayerStage : uint8;

class SDMSlotPreview : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SDMSlotPreview)
		: _PreviewSize(FVector2D(48.0f))
		{}
		SLATE_ATTRIBUTE(FVector2D, PreviewSize)
		SLATE_EVENT(FPointerEventHandler, OnMouseButtonDown)
	SLATE_END_ARGS()

public:
	SDMSlotPreview();
	virtual ~SDMSlotPreview() override;

	void Construct(const FArguments& InArgs, UDMMaterialSlot* InSlot, EDMMaterialLayerStage InLayerStage);

	//~ Begin SWidget
	virtual void Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& InMyGeometry, const FPointerEvent& InCursorEvent) const override;
	virtual FReply OnMouseMove(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;
	//~ End SWidget

protected:
	TWeakObjectPtr<UDMMaterialSlot> SlotWeak;
	TWeakObjectPtr<UMaterialInterface> PreviewMaterialWeak;
	EDMMaterialLayerStage LayerStage;
	FSlateMaterialBrush Brush;

	TAttribute<FVector2D> PreviewSize;

	FVector2D CachedCursorLocation;
	float CachedPreviewSize;

	void OnSlotUpdated(UDMMaterialComponent* InComponent, EDMUpdateType InUpdateType);

	TOptional<FVector2D> GetPreviewSizeInner() const;
	TOptional<FVector2D> GetPreviewSizeOuter() const;
};
