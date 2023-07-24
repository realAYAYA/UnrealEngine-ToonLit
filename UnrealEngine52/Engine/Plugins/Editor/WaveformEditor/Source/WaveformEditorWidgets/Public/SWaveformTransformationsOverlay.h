// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SConstraintCanvas.h"

class FWaveformEditorTransportCoordinator;
class IWaveformTransformationRenderer;
class SOverlay;
class SWaveformTransformationRenderLayer;

using FTransformationLayerRenderInfo = TPair<TSharedPtr<IWaveformTransformationRenderer>, TPair<float,float>>;

class WAVEFORMEDITORWIDGETS_API SWaveformTransformationsOverlay : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SWaveformTransformationsOverlay) {}
		SLATE_DEFAULT_SLOT(FArguments, InArgs)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TArrayView< const FTransformationLayerRenderInfo> InTransformationRenderers, TSharedRef<FWaveformEditorTransportCoordinator> InTransportCoordinator);
	void OnLayerChainGenerated(FTransformationLayerRenderInfo* FirstLayerPtr, const int32 NLayers);
	void UpdateLayerConstraints();
	void OnNewWaveformDisplayRange(const TRange<float> NewDisplayRange);

	FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	
private:
	void CreateLayout();
	void UpdateAnchors();
	
	TSharedPtr<SOverlay> MainOverlayPtr;
	TArray<TSharedPtr<SWaveformTransformationRenderLayer>> TransformationLayers;
	TArray<SConstraintCanvas::FSlot*> LayersSlots;
	TArrayView<const FTransformationLayerRenderInfo> TransformationRenderers;
	TSharedPtr<FWaveformEditorTransportCoordinator> TransportCoordinator = nullptr;

	typedef FReply(SWidget::*WidgetMouseInputFunction)(const FGeometry&, const FPointerEvent&);
	
	FReply RouteMouseInput(WidgetMouseInputFunction InputFunction, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	FCursorReply RouteCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const; 
};