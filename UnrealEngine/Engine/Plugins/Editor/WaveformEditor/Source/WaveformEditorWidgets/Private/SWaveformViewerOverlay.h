// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "WaveformEditorSlateTypes.h"
#include "WaveformEditorTransportCoordinator.h"
#include "SWaveformTransformationsOverlay.h"
#include "Widgets/SLeafWidget.h"

DECLARE_DELEGATE_OneParam(FOnNewMouseDelta, const float /* new delta */)

class SWaveformViewerOverlay : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SWaveformViewerOverlay)
	{
	}

	SLATE_STYLE_ARGUMENT(FWaveformViewerOverlayStyle, Style)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FWaveformEditorTransportCoordinator> InTransportCoordinator, TSharedRef<SWaveformTransformationsOverlay> InTransformationsOverlay);

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

	void OnStyleUpdated(const FWaveformEditorWidgetStyleBase* UpdatedStyle);

	FOnNewMouseDelta OnNewMouseDelta;

private:
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;

	int32 DrawPlayhead(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;

	TSharedPtr<FWaveformEditorTransportCoordinator> TransportCoordinator = nullptr;
	TSharedPtr<SWaveformTransformationsOverlay> TransformationsOverlay = nullptr;

	const FWaveformViewerOverlayStyle* Style = nullptr;

	FSlateColor PlayheadColor = FLinearColor(255.f, 0.1f, 0.2f, 1.f);
	float PlayheadWidth = 1.0;
	float DesiredWidth = 0.f;
	float DesiredHeight = 0.f;
};