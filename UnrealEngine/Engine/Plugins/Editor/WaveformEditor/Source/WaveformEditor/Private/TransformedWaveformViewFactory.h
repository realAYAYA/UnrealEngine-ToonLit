// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WaveformEditorSequenceDataProvider.h"
#include "Input/Reply.h"

class FNotifyHook;
class FSparseSampledSequenceTransportCoordinator;
class FWaveformEditorZoomController; 
class IDetailsView;
class STransformedWaveformViewPanel;
class SWaveformTransformationsOverlay;
class SWidget;
class USoundWave;
struct FGeometry;
struct FPointerEvent;
struct FTransformedWaveformView;

namespace TransformedWaveformViewFactory
{
	enum class EReceivedInteractionType
	{
		MouseButtonUp,
		MouseButtonDown,
		MouseMove,
		COUNT
	};
}

class WAVEFORMEDITOR_API FTransformedWaveformViewFactory
{	
public:
	static FTransformedWaveformViewFactory& Get();
	static void Create();

	FTransformedWaveformView GetTransformedView(TObjectPtr<USoundWave> SoundWaveToView, TSharedRef<FSparseSampledSequenceTransportCoordinator> TransportCoordinator, FNotifyHook* OwnerNotifyHook = nullptr, TSharedPtr<FWaveformEditorZoomController> InZoomController = nullptr);

private:
	FOnTransformationsPropertiesRequired SetUpOwnerNotifyHook(TObjectPtr<USoundWave> SoundWaveToView, FNotifyHook* InNotifyHook);
	TSharedPtr<IDetailsView> SetUpTransformationsPropertiesPropagator(TObjectPtr<USoundWave> SoundWaveToView, FNotifyHook* InNotifyHook);
	void SetUpWaveformPanelInteractions(TSharedRef<STransformedWaveformViewPanel> WaveformPanel, TSharedRef<FSparseSampledSequenceTransportCoordinator> TransportCoordinator, TSharedPtr<FWaveformEditorZoomController> InZoomController = nullptr, TSharedPtr<SWaveformTransformationsOverlay> InTransformationsOverlay = nullptr);
	FReply HandleTimeRulerInteraction(const TransformedWaveformViewFactory::EReceivedInteractionType MouseInteractionType, TSharedRef<FSparseSampledSequenceTransportCoordinator> TransportCoordinator, const TSharedRef<SWidget> TimeRulerWidget, const FPointerEvent& MouseEvent, const FGeometry& Geometry);
	
	static TUniquePtr<FTransformedWaveformViewFactory> Instance;

};