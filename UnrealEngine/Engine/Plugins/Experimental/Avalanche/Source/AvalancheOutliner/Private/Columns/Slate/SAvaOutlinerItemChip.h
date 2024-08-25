// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaOutlinerDefines.h"
#include "Delegates/DelegateCombinations.h"
#include "Framework/SlateDelegates.h"
#include "Misc/Optional.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Templates/SharedPointer.h"
#include "Widgets/Layout/SBorder.h"

class FAvaOutlinerView;
enum class EItemDropZone;
struct FTableRowStyle;

/**
 * Chip Widgets that represent an Item in a compact way in the Items Column
 */
class SAvaOutlinerItemChip : public SBorder
{
public:
	DECLARE_DELEGATE_RetVal_TwoParams(FReply, FOnItemChipClicked, const FAvaOutlinerItemPtr& InItem, const FPointerEvent& InMouseEvent);

	SLATE_BEGIN_ARGS(SAvaOutlinerItemChip) {}
		SLATE_EVENT(FOnItemChipClicked, OnItemChipClicked)
		SLATE_EVENT(FOnDragOver, OnValidDragOver)
		SLATE_STYLE_ARGUMENT(FTableRowStyle, ChipStyle)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs
		, const TSharedRef<IAvaOutlinerItem>& InItem
		, const TSharedPtr<FAvaOutlinerView>& InOutlinerView);

	//~ Begin SWidget
	virtual int32 OnPaint(const FPaintArgs& InArgs, const FGeometry& InAllottedGeometry, const FSlateRect& InCullingRect
		, FSlateWindowElementList& OutDrawElements, int32 LayerId
		, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void OnMouseCaptureLost(const FCaptureLostEvent& InCaptureLostEvent) override;
	virtual FReply OnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent) override;
	virtual void OnDragLeave(const FDragDropEvent& InDragDropEvent) override;
	//~ End SWidget

	bool IsSelected() const;
	const FSlateBrush* GetItemBackgroundBrush() const;
	const FSlateBrush* GetDropIndicatorBrush(EItemDropZone InItemDropZone) const;

private:
	bool IsPressed() const { return bIsPressed; }
	void Press();
	void Release();

	static EItemDropZone GetHoverZone(const FVector2f& InLocalPosition, const FVector2f& InLocalSize);

	FAvaOutlinerItemWeakPtr ItemWeak;

	TWeakPtr<FAvaOutlinerView> OutlinerViewWeak;

	TOptional<EItemDropZone> ItemDropZone;

	const FTableRowStyle* ChipStyle = nullptr;

	FOnItemChipClicked OnItemChipClicked;

	FOnDragOver OnValidDragOver;

	bool bIsPressed = false;
};
