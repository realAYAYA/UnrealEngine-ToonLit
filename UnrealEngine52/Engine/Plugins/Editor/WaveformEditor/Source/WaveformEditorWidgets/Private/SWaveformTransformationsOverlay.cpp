// Copyright Epic Games, Inc. All Rights Reserved.

#include "SWaveformTransformationsOverlay.h"

#include "Widgets/SOverlay.h"
#include "WaveformEditorTransportCoordinator.h"
#include "SWaveformTransformationRenderLayer.h"

void SWaveformTransformationsOverlay::Construct(const FArguments& InArgs, TArrayView<const FTransformationLayerRenderInfo> InTransformationRenderers, TSharedRef<FWaveformEditorTransportCoordinator> InTransportCoordinator)
{
	TransformationRenderers = InTransformationRenderers;
	TransportCoordinator = InTransportCoordinator;
	CreateLayout();
}

void SWaveformTransformationsOverlay::CreateLayout()
{
	const int32 NumLayers = TransformationRenderers.Num();
	TransformationLayers.Empty();
	TransformationLayers.SetNumZeroed(NumLayers);
	LayersSlots.Empty();
	LayersSlots.SetNumUninitialized(NumLayers);

	ChildSlot
	[
		SAssignNew(MainOverlayPtr, SOverlay)
	];

	for (int32 i = 0; i < NumLayers; ++i)
	{
		const FTransformationLayerRenderInfo& LayerRenderInfo = TransformationRenderers[i];

		if (LayerRenderInfo.Key)
		{
			float LeftAnchor = TransportCoordinator->ConvertAbsoluteRatioToZoomed(LayerRenderInfo.Value.Key);
			float RightAnchor = TransportCoordinator->ConvertAbsoluteRatioToZoomed(LayerRenderInfo.Value.Value);

			SConstraintCanvas::FSlot* SlotPtr = nullptr;

			TSharedPtr<SConstraintCanvas> ConstraintCanvasPtr = SNew(SConstraintCanvas)
				+ SConstraintCanvas::Slot()
				.Anchors(FAnchors(LeftAnchor, 0.f, RightAnchor, 1.f))
				.Expose(SlotPtr)
				[
					SAssignNew(TransformationLayers[i], SWaveformTransformationRenderLayer, LayerRenderInfo.Key.ToSharedRef())
				];

			MainOverlayPtr->AddSlot()
			[
				ConstraintCanvasPtr.ToSharedRef()
			];

			LayersSlots[i] = SlotPtr;

		}
		else
		{
			TransformationLayers[i] = nullptr;
			LayersSlots[i] = nullptr;
		}
	}
}


void SWaveformTransformationsOverlay::UpdateAnchors()
{
	check(TransformationRenderers.Num() == LayersSlots.Num())
	
	for (int32 i = 0; i < TransformationRenderers.Num(); ++i)
	{
		const FTransformationLayerRenderInfo& Layer = TransformationRenderers[i];

		if (Layer.Key)
		{
			float LeftAnchor = TransportCoordinator->ConvertAbsoluteRatioToZoomed(Layer.Value.Key);
			float RightAnchor = TransportCoordinator->ConvertAbsoluteRatioToZoomed(Layer.Value.Value);

			LayersSlots[i]->SetAnchors(FAnchors(LeftAnchor, 0.f, RightAnchor, 1.f));
		}
	}
}

FReply SWaveformTransformationsOverlay::RouteMouseInput(WidgetMouseInputFunction InputFunction, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FReply TransformationInteraction = FReply::Unhandled();

	//input is propagated from topmost transformation to lowermost
	for (int32 i = TransformationLayers.Num() - 1; i >= 0; i--)
	{
		const TSharedPtr<SWaveformTransformationRenderLayer> LayerWidget = TransformationLayers[i];

		if (LayerWidget)
		{
			TransformationInteraction = (LayerWidget.Get()->*InputFunction)(LayerWidget->GetTickSpaceGeometry(), MouseEvent);

			if (TransformationInteraction.IsEventHandled())
			{
				return TransformationInteraction;
			}
		}
	}
		
	return TransformationInteraction;
}

void SWaveformTransformationsOverlay::OnLayerChainGenerated(FTransformationLayerRenderInfo* FirstLayerPtr, const int32 NLayers)
{
	TransformationRenderers = MakeArrayView(FirstLayerPtr, NLayers);
	CreateLayout();
}

void SWaveformTransformationsOverlay::UpdateLayerConstraints()
{
	UpdateAnchors();
}

void SWaveformTransformationsOverlay::OnNewWaveformDisplayRange(const TRange<float> NewDisplayRange)
{
	UpdateAnchors();
}

FReply SWaveformTransformationsOverlay::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return RouteMouseInput(&SWidget::OnMouseButtonDown, MyGeometry, MouseEvent);
}

FReply SWaveformTransformationsOverlay::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return RouteMouseInput(&SWidget::OnMouseButtonUp, MyGeometry, MouseEvent);
}

FReply SWaveformTransformationsOverlay::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return RouteMouseInput(&SWidget::OnMouseMove, MyGeometry, MouseEvent);
}

FReply SWaveformTransformationsOverlay::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return RouteMouseInput(&SWidget::OnMouseWheel, MyGeometry, MouseEvent);
}

FCursorReply SWaveformTransformationsOverlay::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	return RouteCursorQuery(MyGeometry, CursorEvent);
}

FCursorReply SWaveformTransformationsOverlay::RouteCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	FCursorReply CursorReply = FCursorReply::Unhandled();

	for (int32 i = TransformationLayers.Num() - 1; i >= 0; i--)
	{
		const TSharedPtr<SWaveformTransformationRenderLayer> LayerWidget = TransformationLayers[i];

		if (LayerWidget)
		{
			CursorReply = LayerWidget->OnCursorQuery(LayerWidget->GetTickSpaceGeometry(), CursorEvent);

			if (CursorReply.IsEventHandled())
			{
				return CursorReply;
			}
		}
	}

	return CursorReply;
}