// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialSlot.h"
#include "Slate/SDMStage.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

class SBox;
class SDMLayerEffectsView;
class SDMSlot;
class SDMStage;
class UDMMaterialLayerObject;
class UDMMaterialSlot;
class UDMMaterialStage;
class UMaterialFunctionInterface;
struct FDMMaterialLayerReference;
struct FSlateBrush;

DECLARE_DELEGATE_TwoParams(FDMOnStageSelected, const bool, const TSharedRef<SDMStage>& /** StageWidget */);
DECLARE_DELEGATE_TwoParams(FDMOnLayerLinkToggled, const bool, const TSharedRef<SDMStage>& /** StageWidget */);

/**
 * Material Slot Layer
 * 
 * Represents a single item in the layer view that can be dragged and re-arranged.
 * Displays the base and mask stages, along with enable and link buttons.
 */
class SDMSlotLayerItem : public STableRow<TSharedPtr<FDMMaterialLayerReference>>
{
public:
	SLATE_BEGIN_ARGS(SDMSlotLayerItem)
		: _PreviewSize(40)
		{}
		SLATE_ATTRIBUTE(bool, StageBaseEnabled)
		SLATE_ATTRIBUTE(bool, StageBaseSelected)
		SLATE_ATTRIBUTE(bool, StageMaskEnabled)
		SLATE_ATTRIBUTE(bool, StageMaskSelected)
		SLATE_ATTRIBUTE(int32, PreviewSize)
		SLATE_EVENT(FDMOnStageSelected, OnStageSelected)
		SLATE_EVENT(FDMOnLayerLinkToggled, OnLayerLinkToggled)
	SLATE_END_ARGS()

	virtual ~SDMSlotLayerItem() {}

	void Construct(const FArguments& InArgs, const TSharedPtr<SDMSlot>& InSlotWidget, const TSharedRef<STableViewBase>& InLayerView, 
		const TSharedPtr<FDMMaterialLayerReference>& InLayerReferenceItem);

	UDMMaterialLayerObject* GetLayer() const;

	int32 GetLayerItemIndex() const;
	bool IsRootLayer() const;

	FORCEINLINE const TSharedPtr<SDMStage>& GetBaseStageWidget() const { return BaseStageWidget; }
	FORCEINLINE const TSharedPtr<SDMStage>& GetMaskStageWidget() const { return MaskStageWidget; }

	FORCEINLINE const TSharedPtr<SDMStage>& GetSelectedStageWidget() const { return SelectedStageWidget; }

	FORCEINLINE bool AreEffectsExpanded() const { return bDisplayEffectsList; }

	void DeselectAllEffects();

protected:
	TWeakPtr<SDMSlot> SlotWidgetWeak;
	TWeakPtr<STableViewBase> LayerViewWeak;
	TSharedPtr<FDMMaterialLayerReference> LayerItem;

	TAttribute<bool> StageBaseEnabled;
	TAttribute<bool> StageBaseSelected;
	TAttribute<bool> StageMaskEnabled;
	TAttribute<bool> StageMaskSelected;
	TAttribute<int32> PreviewSize;
	FDMOnStageSelected OnStageSelected;
	FDMOnLayerLinkToggled OnLayerLinkToggled;

	TSharedPtr<SDMStage> BaseStageWidget;
	TSharedPtr<SDMStage> MaskStageWidget;

	TSharedPtr<SDMStage> SelectedStageWidget;

	TSharedPtr<SDMLayerEffectsView> EffectsList;
	bool bDisplayEffectsList;

	TSharedPtr<SBox> LayerHeaderTextContainer;

	TSharedRef<SWidget> CreateMainContent();
	TSharedRef<SWidget> CreateHeaderRowContent();

	/** Create components of the slot layer. */
	TSharedRef<SWidget> CreateStageBaseWidget(const bool bInteractable, const bool bInShowTextOverlays, TAttribute<FVector2D> InDesiredSize = FVector2D(40.0f), FDMOnStageUniformSizeChanged InOnUniformSizeChanged = nullptr);
	TSharedRef<SWidget> CreateStageMaskWidget(const bool bInteractable, const bool bInShowTextOverlays, TAttribute<FVector2D> InDesiredSize = FVector2D(40.0f), FDMOnStageUniformSizeChanged InOnUniformSizeChanged = nullptr);
	TSharedRef<SWidget> CreateHandleWidget();

	TSharedRef<SWidget> CreateLayerBypassButton();
	TSharedRef<SWidget> CreateTogglesWidget();
	TSharedRef<SWidget> CreateLayerBaseToggleButton();
	TSharedRef<SWidget> CreateLayerMaskToggleButton();
	TSharedRef<SWidget> CreateLayerLinkToggleButton();
	TSharedRef<SWidget> CreateEffectsToggleButton();

	UDMMaterialStage* GetBaseStage() const;
	UDMMaterialStage* GetMaskStage() const;

	UDMMaterialSlot* GetSlot() const;

	bool AreStagesLinked() const;
	bool IsBaseStageEnabled() const;
	bool IsMaskStageEnabled() const;

	int32 OnLayerItemPaintDropIndicator(EItemDropZone InItemDropZone, const FPaintArgs& Args, const FGeometry& AllottedGeometry, 
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const;
	TOptional<EItemDropZone> OnLayerItemCanAcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone, 
		TSharedPtr<FDMMaterialLayerReference> InSlotLayer) const;
	void OnLayerItemDragEnter(const FDragDropEvent& InDragDropEvent);
	void OnLayerItemDragLeave(const FDragDropEvent& InDragDropEvent);
	FReply OnLayerItemDragDetected(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent);
	FReply OnLayerItemAcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone, 
		TSharedPtr<FDMMaterialLayerReference> InSlotLayer);

	FText GetToolTipText() const;
	FText GetLayerHeaderText() const;
	FText GetLayerIndexText() const;
	FText GetBlendModeText() const;
	FText GetStageDescription() const;
	const FSlateBrush* GetRowHandleBrush() const;

	//~ Begin SWidget
	virtual FCursorReply OnCursorQuery(const FGeometry& InMyGeometry, const FPointerEvent& InCursorEvent) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;
	//~ End SWidget

	void OnStageClick(const FPointerEvent& InMouseEvent, const TSharedRef<SDMStage>& InStageWidget);

	bool OnStageCanAcceptDrop(const FDragDropEvent& InDragDropEvent, const TSharedRef<SDMStage>& InStageWidget) const;
	FReply OnStageAcceptDrop(const FDragDropEvent& InDragDropEvent, const TSharedRef<SDMStage>& InStageWidget);

	void SaveLayerPreviewSize(const float InNewSize, const TSharedRef<SDMStage>& InStageWidget);

	EVisibility GetEffectsListVisibility() const;

	TSharedRef<SWidget> CreateNewEffectMenu();

	FReply OnCreateLayerBypassButtonClicked();
	const FSlateBrush* GetCreateLayerBypassButtonImage() const;

	FReply OnBaseToggleButtonClicked();
	const FSlateBrush* GetBaseToggleButtonImage() const;

	FReply OnLayerMaskToggleButtonClicked();
	const FSlateBrush* GetLayerMaskToggleButtonImage() const;

	FReply OnLayerLinkToggleButton();
	const FSlateBrush* GetLayerLinkToggleButtonImage() const;
	EVisibility GetLayerLinkToggleButtonVisibility() const;

	FReply OnEffectsToggleButtonClicked();
	const FSlateBrush* GetEffectsToggleButtonImage() const;

	FVector2D GetStagePreviewSize() const;

	TSharedRef<SWidget> CreateLayerHeaderText() const;
	TSharedRef<SWidget> CreateLayerHeaderEditableText() const;
};
