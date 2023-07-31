// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Layout/Margin.h"
#include "Input/CursorReply.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

enum class EDMXPixelMappingTransformDirection : uint8
{
	CenterRight,
	BottomRight,
	BottomLeft,
	BottomCenter,

	MAX
};

enum class EDMXPixelMappingTransformAction : uint8
{
	None,
	Primary,
	Secondary
};

class SDMXPixelMappingDesignerView;
class UDMXPixelMappingBaseComponent;
class FScopedTransaction;

/**
 * Most of the logic copied from a private class Engine/Source/Editor/UMGEditor/Private/Designer/STransformHandle.h
 * This is a small dot widget for control the size of the selected widget
 */
class SDMXPixelMappingTransformHandle
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SDMXPixelMappingTransformHandle) 
		: _Offset(FVector2D(0, 0))
	{}
		SLATE_ATTRIBUTE(FVector2D, Offset)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<SDMXPixelMappingDesignerView> InDesignerView, EDMXPixelMappingTransformDirection InTransformDirection, TAttribute<FVector2D> InOffset);

	// SWidget interface
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	// End SWidget interface

	EDMXPixelMappingTransformDirection GetTransformDirection() const { return TransformDirection; }
	FVector2D GetOffset() const { return Offset.Get(); }

protected:
	EVisibility GetHandleVisibility() const;

	bool CanResize(UDMXPixelMappingBaseComponent* BaseComponent, const FVector2D& Direction) const { return true; }
	void Resize(UDMXPixelMappingBaseComponent* BaseComponent, const FVector2D& Direction, const FVector2D& Amount);

	EDMXPixelMappingTransformAction ComputeActionAtLocation(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const;

protected:
	FVector2D ComputeDragDirection(EDMXPixelMappingTransformDirection InTransformDirection) const;
	FVector2D ComputeOrigin(EDMXPixelMappingTransformDirection InTransformDirection) const;

protected:
	EDMXPixelMappingTransformDirection TransformDirection;
	EDMXPixelMappingTransformAction Action;

	FVector2D DragDirection;
	FVector2D DragOrigin;

	FVector2D MouseDownPosition;
	FMargin StartingOffsets;

	TSharedPtr<FScopedTransaction> ScopedTransaction;

	TWeakPtr<SDMXPixelMappingDesignerView> DesignerViewWeakPtr;

	TAttribute<FVector2D> Offset;
};
