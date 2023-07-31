// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Widgets/SCompoundWidget.h"

class FWaveformEditorGridData;
class FWaveformEditorRenderData;
class FWaveformEditorStyle;
class FWaveformEditorTransportCoordinator;
class FWaveformEditorZoomController;
class SWaveformEditorTimeRuler;
class SWaveformTransformationsOverlay;
class SWaveformViewer;
class SWaveformViewerOverlay;
enum class EWaveformEditorDisplayUnit;

class WAVEFORMEDITORWIDGETS_API SWaveformPanel : public SCompoundWidget
{
public: 

	SLATE_BEGIN_ARGS(SWaveformPanel) {}
		SLATE_DEFAULT_SLOT(FArguments, InArgs)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FWaveformEditorRenderData> InRenderData, TSharedRef<FWaveformEditorTransportCoordinator> InTransportCoordinator, TSharedRef<FWaveformEditorZoomController> InZoomManager, TSharedPtr<SWaveformTransformationsOverlay> InWaveformTransformationsOverlay = nullptr);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private: 
	void CreateLayout();

	void SetUpWaveformViewer(TSharedRef<FWaveformEditorRenderData> InRenderData, TSharedRef<FWaveformEditorTransportCoordinator> InTransportCoordinator, TSharedRef<FWaveformEditorZoomController> InZoomManager, TSharedRef<FWaveformEditorGridData> InGridData);
	void SetUpWaveformViewerOverlay(TSharedRef<FWaveformEditorTransportCoordinator> InTransportCoordinator, TSharedRef<FWaveformEditorZoomController> InZoomManager);
	void SetUpTimeRuler(TSharedRef<FWaveformEditorRenderData> InRenderData, TSharedRef<FWaveformEditorTransportCoordinator> InTransportCoordinator, TSharedRef<FWaveformEditorGridData> InGridData);
	void SetUpGridData(TSharedRef<FWaveformEditorRenderData> InRenderData, TSharedRef<FWaveformEditorTransportCoordinator> InTransportCoordinator);

	void UpdateDisplayUnit(const EWaveformEditorDisplayUnit InDisplayUnit);

	TSharedPtr<FWaveformEditorGridData> GridData = nullptr;
	TSharedPtr<SWaveformEditorTimeRuler> TimeRuler = nullptr;
	TSharedPtr<SWaveformViewer> WaveformViewer = nullptr;
	TSharedPtr<SWaveformTransformationsOverlay> WaveformTransformationsOverlay = nullptr;
	TSharedPtr<SWaveformViewerOverlay> WaveformViewerOverlay = nullptr;

	float CachedPixelWidth = 0.f;

	EWaveformEditorDisplayUnit DisplayUnit;

	FWaveformEditorStyle* WaveformEditorStyle;
};