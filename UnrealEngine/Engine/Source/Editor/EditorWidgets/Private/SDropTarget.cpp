// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDropTarget.h"

#include "Framework/Application/SlateApplication.h"
#include "Layout/Children.h"
#include "Layout/Clipping.h"
#include "Layout/Geometry.h"
#include "Math/Vector2D.h"
#include "Rendering/DrawElements.h"
#include "Rendering/RenderingCommon.h"
#include "SlotBase.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"

class FSlateRect;
class FWidgetStyle;


#define LOCTEXT_NAMESPACE "EditorWidgets"

void SDropTarget::Construct(const FArguments& InArgs)
{
	DroppedEvent = InArgs._OnDropped;
	AllowDropEvent = InArgs._OnAllowDrop;
	IsRecognizedEvent = InArgs._OnIsRecognized;

	bIsDragEventRecognized = false;
	bAllowDrop = false;
	bIsDragOver = false;

	ValidColor = InArgs._ValidColor;
	InvalidColor = InArgs._InvalidColor;

	VerticalImage = InArgs._VerticalImage;
	HorizontalImage = InArgs._HorizontalImage;

	ChildSlot
	[
		SNew(SOverlay)
		.Clipping(EWidgetClipping::ClipToBounds)
		+ SOverlay::Slot()
		[
			InArgs._Content.Widget
		]

		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.Visibility(this, &SDropTarget::GetDragOverlayVisibility)
			.BorderImage(InArgs._BackgroundImage)
			.BorderBackgroundColor(this, &SDropTarget::GetBackgroundBrightness)
		]
	];
}

FSlateColor SDropTarget::GetBackgroundBrightness() const
{
	return bAllowDrop ? ValidColor : InvalidColor;
}

EVisibility SDropTarget::GetDragOverlayVisibility() const
{
	if ( FSlateApplication::Get().IsDragDropping() )
	{
		if ( AllowDrop(FSlateApplication::Get().GetDragDroppingContent()) || (bIsDragOver && bIsDragEventRecognized) )
		{
			return EVisibility::HitTestInvisible;
		}
	}

	return EVisibility::Hidden;
}

FReply SDropTarget::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	// Handle the reply if we are allowed to drop, otherwise do not handle it.
	return AllowDrop(DragDropEvent.GetOperation()) ? FReply::Handled() : FReply::Unhandled();
}

bool SDropTarget::AllowDrop(TSharedPtr<FDragDropOperation> DragDropOperation) const
{
	bAllowDrop = OnAllowDrop(DragDropOperation);
	bIsDragEventRecognized = OnIsRecognized(DragDropOperation) || bAllowDrop;

	return bAllowDrop;
}

bool SDropTarget::OnAllowDrop(TSharedPtr<FDragDropOperation> DragDropOperation) const
{
	if ( AllowDropEvent.IsBound() )
	{
		return AllowDropEvent.Execute(DragDropOperation);
	}

	return false;
}

bool SDropTarget::OnIsRecognized(TSharedPtr<FDragDropOperation> DragDropOperation) const
{
	if ( IsRecognizedEvent.IsBound() )
	{
		return IsRecognizedEvent.Execute(DragDropOperation);
	}

	return false;
}

FReply SDropTarget::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	const bool bCurrentbAllowDrop = bAllowDrop;

	// We've dropped an asset so we are no longer being dragged over
	bIsDragEventRecognized = false;
	bIsDragOver = false;
	bAllowDrop = false;

	// if we allow drop, call a delegate to handle the drop
	if ( bCurrentbAllowDrop )
	{
		if ( DroppedEvent.IsBound() )
		{
			return DroppedEvent.Execute(MyGeometry, DragDropEvent);
		}

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SDropTarget::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	// initially we dont recognize this event
	bIsDragEventRecognized = false;
	bIsDragOver = true;
}

void SDropTarget::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	// No longer being dragged over
	bIsDragEventRecognized = false;
	// Disallow dropping if not dragged over.
	bAllowDrop = false;

	bIsDragOver = false;
}

int32 SDropTarget::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if ( GetDragOverlayVisibility().IsVisible() )
	{
		if ( bIsDragEventRecognized )
		{
			FSlateColor DashColor = bAllowDrop ? ValidColor : InvalidColor;

			int32 DashLayer = LayerId + 1;
			
			const float Inset = 3.0f;

			// Top
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				DashLayer,
				AllottedGeometry.ToPaintGeometry(FVector2D(Inset, 0), FVector2D(AllottedGeometry.GetLocalSize().X-Inset*2, HorizontalImage->ImageSize.Y)),
				HorizontalImage,
				ESlateDrawEffect::None,
				DashColor.GetColor(InWidgetStyle));

			// Bottom
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				DashLayer,
				AllottedGeometry.ToPaintGeometry(FVector2D(Inset, AllottedGeometry.GetLocalSize().Y - HorizontalImage->ImageSize.Y), FVector2D(AllottedGeometry.Size.X-Inset * 2, HorizontalImage->ImageSize.Y)),
				HorizontalImage,
				ESlateDrawEffect::None,
				DashColor.GetColor(InWidgetStyle));

			// Left
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				DashLayer,
				AllottedGeometry.ToPaintGeometry(FVector2D(0, Inset), FVector2D(VerticalImage->ImageSize.X, AllottedGeometry.GetLocalSize().Y-Inset * 2)),
				VerticalImage,
				ESlateDrawEffect::None,
				DashColor.GetColor(InWidgetStyle));

			// Right
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				DashLayer,
				AllottedGeometry.ToPaintGeometry(FVector2D(AllottedGeometry.GetLocalSize().X - VerticalImage->ImageSize.X, Inset), FVector2D(VerticalImage->ImageSize.X, AllottedGeometry.GetLocalSize().Y-Inset * 2)),
				VerticalImage,
				ESlateDrawEffect::None,
				DashColor.GetColor(InWidgetStyle));

			return DashLayer;
		}
	}

	return LayerId;
}

#undef LOCTEXT_NAMESPACE
