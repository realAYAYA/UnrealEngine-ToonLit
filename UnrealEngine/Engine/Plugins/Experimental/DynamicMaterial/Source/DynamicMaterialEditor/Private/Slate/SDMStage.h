// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class FAssetDragDropOp;
class FDMStageDragDropOperation;
class UDMMaterialStage;
class SDMStage;
struct FOptionalSize;

DECLARE_DELEGATE_TwoParams(FDMOnStageClick, const FPointerEvent&, const TSharedRef<SDMStage>& /** StageWidget */);
DECLARE_DELEGATE_TwoParams(FDMOnStageUniformSizeChanged, const float /** NewSize */, const TSharedRef<SDMStage>& /** StageWidget */);

DECLARE_DELEGATE_OneParam(FDMOnStageDragEnter, const FDragDropEvent&);
DECLARE_DELEGATE_OneParam(FDMOnStageDragLeave, const FDragDropEvent&);
DECLARE_DELEGATE_RetVal_OneParam(FReply, FDMOnStageDragDetected, const FDragDropEvent&);
DECLARE_DELEGATE_RetVal_OneParam(FReply, FDMOnStageDrop, const FDragDropEvent&);

/** Delegate signature for querying whether this FDragDropEvent will be handled by the drop target. */
DECLARE_DELEGATE_RetVal_TwoParams(bool, FDMOnStageCanAcceptDrop, const FDragDropEvent&, const TSharedRef<SDMStage>& /** StageWidget */);
/** Delegate signature for handling the drop of FDragDropEvent onto target. */
DECLARE_DELEGATE_RetVal_TwoParams(FReply, FDMOnStageAcceptDrop, const FDragDropEvent&, const TSharedRef<SDMStage>& /** StageWidget */);
/** Delegate signature for painting drop indicators. */
DECLARE_DELEGATE_RetVal_SevenParams(int32, FDMOnStagePaintDropIndicator, const FPaintArgs&, const FGeometry&, const FSlateRect&, FSlateWindowElementList&, int32, const FWidgetStyle&, bool);

class SDMStage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMStage)
		: _StageIsMask(false)
		, _Interactable(false)
		, _ShowTextOverlays(false)
		, _DesiredSize(FVector2D(48.0f))
		, _MinUniformSize(32)
		, _MaxUniformSize(128)
	{}
		SLATE_ATTRIBUTE(bool, StageEnabled)
		SLATE_ATTRIBUTE(bool, StageSelected)
		SLATE_ARGUMENT(bool, StageIsMask)
		SLATE_ATTRIBUTE(bool, Interactable)
		SLATE_ATTRIBUTE(bool, ShowTextOverlays)
		SLATE_ATTRIBUTE(FVector2D, DesiredSize)
		SLATE_ARGUMENT(float, MinUniformSize)
		SLATE_ARGUMENT(float, MaxUniformSize)
		SLATE_EVENT(FDMOnStageUniformSizeChanged, OnUniformSizeChanged)
		SLATE_EVENT(FDMOnStageClick, OnClick)

		/** High Level DragAndDrop */
		/**
		 * Handle this event to determine whether a drag and drop operation can be executed on top of the target row widget.
		 * Most commonly, this is used for previewing re-ordering and re-organization operations in lists or trees.
		 * e.g. A user is dragging one item into a different spot in the list or tree.
		 *      This delegate will be called to figure out if we should give visual feedback on whether an item will
		 *      successfully drop into the list.
		 */
		SLATE_EVENT(FDMOnStageCanAcceptDrop, OnCanAcceptDrop)
		/**
		 * Perform a drop operation onto the target row widget
		 * Most commonly used for executing a re-ordering and re-organization operations in lists or trees.
		 * e.g. A user was dragging one item into a different spot in the list; they just dropped it.
		 *      This is our chance to handle the drop by reordering items and calling for a list refresh.
		 */
		SLATE_EVENT(FDMOnStageAcceptDrop, OnAcceptDrop)
		/**
		 * Used for painting drop indicators
		 */
		SLATE_EVENT(FDMOnStagePaintDropIndicator, OnPaintDropIndicator)
	SLATE_END_ARGS()

	virtual ~SDMStage() override;

	void Construct(const FArguments& InArgs, UDMMaterialStage* InStage);

	FORCEINLINE UDMMaterialStage* GetStage() const { return StageWeak.Get(); }

	FORCEINLINE bool IsStageSelected() const { return bStageSelected.Get(); }
	FORCEINLINE bool IsStageEnabled() const { return bStageEnabled.Get(); }
	FORCEINLINE bool IsStageMask() const { return bIsMask; }

	void SetStageSelected(const bool bInStageSelected) { bStageSelected = bInStageSelected; }

	FVector2D GetPreviewSize() const { return DesiredSize.Get(); }

	//~ Begin SWidget
	virtual FCursorReply OnCursorQuery(const FGeometry& InMyGeometry, const FPointerEvent& InCursorEvent) const override;
	virtual FReply OnMouseMove(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnDragDetected(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void OnDragEnter(FGeometry const& InMyGeometry, FDragDropEvent const& InDragDropEvent) override;
	virtual void OnDragLeave(FDragDropEvent const& InDragDropEvent) override;
	virtual FReply OnDragOver(const FGeometry& InMyGeometry, const FDragDropEvent& InDragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& InMyGeometry, const FDragDropEvent& InDragDropEvent) override;
	//~ End SWidget

protected:
	TWeakObjectPtr<UDMMaterialStage> StageWeak;

	TAttribute<bool> bStageEnabled;
	TAttribute<bool> bStageSelected;
	bool bIsMask;
	TAttribute<bool> bInteractable;
	TAttribute<bool> bShowTextOverlays;
	TAttribute<FVector2D> DesiredSize;
	float MinUniformSize;
	float MaxUniformSize;
	FDMOnStageUniformSizeChanged OnUniformSizeChanged;
	FDMOnStageClick OnClick;

	FDMOnStageCanAcceptDrop OnCanAcceptDropEvent;
	FDMOnStageAcceptDrop OnAcceptDropEvent;
	FDMOnStagePaintDropIndicator OnPaintDropIndicatorEvent;

	FVector2D CachedCursorLocation;
	float CachedPreviewSize;

	TSharedRef<SWidget> CreateTextBlockBackground();

	EVisibility GetBorderVisibility() const;
	const FSlateBrush* GetBorderBrush() const;
	const FSlateBrush* GetOverlayBorderBrush() const;

	FText GetStageName() const;

	void HandleStageDragDropOperation(FDMStageDragDropOperation& StageDragDropOperation);
	void HandleAssetDragDropOperation(FAssetDragDropOp& AssetDragDropOperation);

	bool IsToolTipInteractable() const;

	FOptionalSize GetStagePreviewDesiredWidth() const;
	FOptionalSize GetStagePreviewDesiredHeight() const;

	FSlateColor GetBorderBrushBackgroundColor() const;
	EVisibility GetOverlayBorderBrushVisibility() const;

	EVisibility GetTextBlockBackgroundVisibility() const;
};
