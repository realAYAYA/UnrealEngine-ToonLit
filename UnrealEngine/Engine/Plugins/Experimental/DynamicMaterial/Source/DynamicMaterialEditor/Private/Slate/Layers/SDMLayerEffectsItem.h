// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialSlot.h"
#include "Slate/SDMStage.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

class SBox;
class SDMSlot;
class SDMStage;
class UDMMaterialEffect;
class UDMMaterialSlot;
class UDMMaterialStage;
struct FSlateBrush;

struct FDMEffectsLayerItem
{
	TWeakObjectPtr<UDMMaterialEffect> MaterialEffectWeak;

	FDMEffectsLayerItem(UDMMaterialEffect* InMaterialEffect);
};

/**
 * Material Slot Layer Effects Item
 */
class SDMLayerEffectsItem : public STableRow<TSharedPtr<FDMEffectsLayerItem>>
{
public:
	DECLARE_DELEGATE_TwoParams(FDMOnEffectSelected, const bool, const TSharedRef<SDMLayerEffectsItem>& /** EffectsItemWidget */);

	SLATE_BEGIN_ARGS(SDMLayerEffectsItem)
	{}
		SLATE_EVENT(FDMOnEffectSelected, OnEffectSelected)
	SLATE_END_ARGS()

	virtual ~SDMLayerEffectsItem() override = default;

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InEffectsView, const TSharedPtr<FDMEffectsLayerItem>& InLayerItem);

	const TSharedPtr<FDMEffectsLayerItem>& GetLayerItem() const { return LayerItem; }

	FORCEINLINE bool IsEffectSelected() const { return bEffectSelected; }
	FORCEINLINE void SetEffectSelected(const bool bInSelected) { bEffectSelected = bInSelected; }

protected:
	TWeakPtr<STableViewBase> EffectsViewWeak;
	TSharedPtr<FDMEffectsLayerItem> LayerItem;

	FDMOnEffectSelected OnEffectSelected;

	bool bEffectSelected;

	TSharedRef<SWidget> CreateMainContent();

	TSharedRef<SWidget> CreateLayerBypassButton();
	TSharedRef<SWidget> CreateLayerRemoveButton();

	int32 OnLayerItemPaintDropIndicator(EItemDropZone InItemDropZone, const FPaintArgs& Args, const FGeometry& AllottedGeometry, 
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, 
		bool bParentEnabled) const;
	TOptional<EItemDropZone> OnLayerItemCanAcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone, 
		TSharedPtr<FDMEffectsLayerItem> InSlotLayer) const;
	void OnLayerItemDragEnter(const FDragDropEvent& InDragDropEvent);
	void OnLayerItemDragLeave(const FDragDropEvent& InDragDropEvent);
	FReply OnLayerItemDragDetected(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent);
	FReply OnLayerItemAcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone, TSharedPtr<FDMEffectsLayerItem> InSlotLayer);

	FText GetToolTipText() const;
	FText GetLayerHeaderText() const;

	//~ Begin SWidget
	virtual FCursorReply OnCursorQuery(const FGeometry& InMyGeometry, const FPointerEvent& InCursorEvent) const override;
	//~ End SWidget

	void OnEffectClick(const FPointerEvent& InMouseEvent, const TSharedRef<SDMStage>& InStageWidget);

	bool OnEffectCanAcceptDrop(const FDragDropEvent& InDragDropEvent, const TSharedRef<SDMStage>& InStageWidget) const;
	FReply OnEffectAcceptDrop(const FDragDropEvent& InDragDropEvent, const TSharedRef<SDMStage>& InStageWidget);

	FReply OnLayerBypassButtonClick();
	const FSlateBrush* GetLayerBypassButtonImage() const;

	FReply OnLayerRemoveButtonClick();
};
