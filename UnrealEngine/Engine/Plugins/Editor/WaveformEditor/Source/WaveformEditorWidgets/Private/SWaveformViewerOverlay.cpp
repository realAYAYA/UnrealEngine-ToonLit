// Copyright Epic Games, Inc. All Rights Reserved.

#include "SWaveformViewerOverlay.h"

#include "WaveformEditorTransportCoordinator.h"
#include "Widgets/SLeafWidget.h"

void SWaveformViewerOverlay::Construct(const FArguments& InArgs, TSharedRef<FWaveformEditorTransportCoordinator> InTransportCoordinator, TSharedRef<SWaveformTransformationsOverlay> InTransformationsOverlay)
{
	TransportCoordinator = InTransportCoordinator;
	TransformationsOverlay = InTransformationsOverlay;

	Style = InArgs._Style;
	
	check(Style);
	PlayheadColor = Style->PlayheadColor;
	PlayheadWidth = Style->PlayheadWidth;
	DesiredWidth = Style->DesiredWidth;
	DesiredHeight = Style->DesiredHeight;
}

FReply SWaveformViewerOverlay::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FReply InteractionReply = TransformationsOverlay->OnMouseButtonDown(MyGeometry, MouseEvent);

	if (!InteractionReply.IsEventHandled())
	{
		InteractionReply = TransportCoordinator->ReceiveMouseButtonDown(*this, MyGeometry, MouseEvent);
	}
	
	return InteractionReply;
}

FReply SWaveformViewerOverlay::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FReply InteractionReply = TransformationsOverlay->OnMouseWheel(MyGeometry, MouseEvent);

	if (!InteractionReply.IsEventHandled())
	{
		OnNewMouseDelta.ExecuteIfBound(MouseEvent.GetWheelDelta());
		InteractionReply = FReply::Handled();
	}

	return InteractionReply;

}

FReply SWaveformViewerOverlay::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return TransformationsOverlay->OnMouseMove(MyGeometry, MouseEvent);
}

FCursorReply SWaveformViewerOverlay::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	return TransformationsOverlay->OnCursorQuery(MyGeometry, CursorEvent);
}

void SWaveformViewerOverlay::OnStyleUpdated(const FWaveformEditorWidgetStyleBase* UpdatedStyle)
{
	check(UpdatedStyle);
	check(Style);

	if (UpdatedStyle != Style)
	{
		return;
	}

	PlayheadColor = Style->PlayheadColor;
	PlayheadWidth = Style->PlayheadWidth;
	DesiredWidth = Style->DesiredWidth;
	DesiredHeight = Style->DesiredHeight;
}

int32 SWaveformViewerOverlay::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = DrawPlayhead(AllottedGeometry, OutDrawElements, LayerId);

	return LayerId;
}

int32 SWaveformViewerOverlay::DrawPlayhead(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	const float PlayHeadX = AllottedGeometry.GetLocalSize().X * TransportCoordinator->GetPlayheadPosition();

	TArray<FVector2D> LinePoints;
	{
		LinePoints.AddUninitialized(2);
		LinePoints[0] = FVector2D(PlayHeadX, 0.0f);
		LinePoints[1] = FVector2D(PlayHeadX, AllottedGeometry.Size.Y);
	}

	FSlateDrawElement::MakeLines(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		LinePoints,
		ESlateDrawEffect::None,
		PlayheadColor.GetSpecifiedColor(),
		true, 
		PlayheadWidth
	);

	return ++LayerId;
}

FVector2D SWaveformViewerOverlay::ComputeDesiredSize(float) const
{
	return FVector2D(DesiredWidth, DesiredHeight);
}