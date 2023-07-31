// Copyright Epic Games, Inc. All Rights Reserved.

#include "SWaveformPanel.h"

#include "SWaveformEditorTimeRuler.h"
#include "SWaveformTransformationsOverlay.h"
#include "SWaveformViewer.h"
#include "SWaveformViewerOverlay.h"
#include "WaveformEditorDisplayUnit.h"
#include "WaveformEditorGridData.h"
#include "WaveformEditorRenderData.h"
#include "WaveformEditorStyle.h"
#include "WaveformEditorTransportCoordinator.h"
#include "WaveformEditorZoomController.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"

void SWaveformPanel::Construct(const FArguments& InArgs, TSharedRef<FWaveformEditorRenderData> InRenderData, TSharedRef<FWaveformEditorTransportCoordinator> InTransportCoordinator, TSharedRef<FWaveformEditorZoomController> InZoomManager, TSharedPtr<SWaveformTransformationsOverlay> InWaveformTransformationsOverlay)
{
	DisplayUnit = EWaveformEditorDisplayUnit::Seconds;

	WaveformEditorStyle = &FWaveformEditorStyle::Get();
	check(WaveformEditorStyle);

	SetUpGridData(InRenderData, InTransportCoordinator);
	SetUpWaveformViewer(InRenderData, InTransportCoordinator, InZoomManager, GridData.ToSharedRef());

	if (InWaveformTransformationsOverlay)
	{
		WaveformTransformationsOverlay = InWaveformTransformationsOverlay;
	}
	
	SetUpWaveformViewerOverlay(InTransportCoordinator, InZoomManager);
	SetUpTimeRuler(InRenderData, InTransportCoordinator, GridData.ToSharedRef());
	CreateLayout();
}

void SWaveformPanel::CreateLayout()
{
	check(TimeRuler);
	check(WaveformViewer);
	check(WaveformViewerOverlay);

	TSharedPtr<SOverlay> WaveformView = SNew(SOverlay);
	WaveformView->AddSlot()
	[
		WaveformViewer.ToSharedRef()
	];

	if (WaveformTransformationsOverlay)
	{
		WaveformView->AddSlot()
		[
			WaveformTransformationsOverlay.ToSharedRef()
		];
	}

	WaveformView->AddSlot()
	[
		WaveformViewerOverlay.ToSharedRef()
	];

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			TimeRuler.ToSharedRef()
		]
		+ SVerticalBox::Slot()
		[
			WaveformView.ToSharedRef()
		]
	];
}

void SWaveformPanel::SetUpTimeRuler(TSharedRef<FWaveformEditorRenderData> InRenderData, TSharedRef<FWaveformEditorTransportCoordinator> InTransportCoordinator, TSharedRef<FWaveformEditorGridData> InGridData)
{
	FWaveformEditorTimeRulerStyle* TimeRulerStyle = &WaveformEditorStyle->GetRegisteredWidgetStyle<FWaveformEditorTimeRulerStyle>("WaveformEditorRuler.Style").Get();
	check(TimeRulerStyle);

	TimeRuler = SNew(SWaveformEditorTimeRuler, InTransportCoordinator, InRenderData).DisplayUnit(DisplayUnit).Style(TimeRulerStyle);
	TimeRulerStyle->OnStyleUpdated.AddSP(TimeRuler.ToSharedRef(), &SWaveformEditorTimeRuler::OnStyleUpdated);
	InGridData->OnGridMetricsUpdated.AddSP(TimeRuler.ToSharedRef(), &SWaveformEditorTimeRuler::UpdateGridMetrics);
	TimeRuler->OnTimeUnitMenuSelection.AddSP(this, &SWaveformPanel::UpdateDisplayUnit);
}

void SWaveformPanel::SetUpWaveformViewerOverlay(TSharedRef<FWaveformEditorTransportCoordinator> InTransportCoordinator, TSharedRef<FWaveformEditorZoomController> InZoomManager)
{
	FWaveformViewerOverlayStyle* OverlayStyle = &WaveformEditorStyle->GetRegisteredWidgetStyle<FWaveformViewerOverlayStyle>("WaveformViewerOverlay.Style").Get();
	check(OverlayStyle);

	WaveformViewerOverlay = SNew(SWaveformViewerOverlay, InTransportCoordinator, WaveformTransformationsOverlay.ToSharedRef()).Style(OverlayStyle);
	OverlayStyle->OnStyleUpdated.AddSP(WaveformViewerOverlay.ToSharedRef(), &SWaveformViewerOverlay::OnStyleUpdated);
	WaveformViewerOverlay->OnNewMouseDelta.BindSP(InZoomManager, &FWaveformEditorZoomController::ZoomByDelta);
}

void SWaveformPanel::SetUpWaveformViewer(TSharedRef<FWaveformEditorRenderData> InRenderData, TSharedRef<FWaveformEditorTransportCoordinator> InTransportCoordinator, TSharedRef<FWaveformEditorZoomController> InZoomManager, TSharedRef<FWaveformEditorGridData> InGridData)
{
	FWaveformViewerStyle* WaveViewerStyle = &WaveformEditorStyle->GetRegisteredWidgetStyle<FWaveformViewerStyle>("WaveformViewer.Style").Get();
	check(WaveViewerStyle);
	
	WaveformViewer = SNew(SWaveformViewer, InRenderData, InTransportCoordinator).Style(WaveViewerStyle);
	WaveViewerStyle->OnStyleUpdated.AddSP(WaveformViewer.ToSharedRef(), &SWaveformViewer::OnStyleUpdated);
	InZoomManager->OnZoomRatioChanged.AddSP(InTransportCoordinator, &FWaveformEditorTransportCoordinator::OnZoomLevelChanged);
	InGridData->OnGridMetricsUpdated.AddSP(WaveformViewer.ToSharedRef(), &SWaveformViewer::UpdateGridMetrics);
}

void SWaveformPanel::SetUpGridData(TSharedRef<FWaveformEditorRenderData> InRenderData, TSharedRef<FWaveformEditorTransportCoordinator> InTransportCoordinator)
{
	GridData = MakeShared<FWaveformEditorGridData>(InRenderData);
	InTransportCoordinator->OnDisplayRangeUpdated.AddSP(GridData.ToSharedRef(), &FWaveformEditorGridData::UpdateDisplayRange);

	const ISlateStyle* WaveEditorStyle = FSlateStyleRegistry::FindSlateStyle("WaveformEditorStyle");
	if (ensure(WaveEditorStyle))
	{
		const FWaveformEditorTimeRulerStyle& RulerStyle = WaveEditorStyle->GetWidgetStyle<FWaveformEditorTimeRulerStyle>("WaveformEditorRuler.Style");
		GridData->SetTicksTimeFont(&RulerStyle.TicksTextFont);
	}
}

void SWaveformPanel::UpdateDisplayUnit(const EWaveformEditorDisplayUnit InDisplayUnit)
{
	DisplayUnit = InDisplayUnit;
	TimeRuler->UpdateDisplayUnit(DisplayUnit);
}

void SWaveformPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	const float PaintedWidth = AllottedGeometry.GetLocalSize().X;

	if (PaintedWidth != CachedPixelWidth)
	{
		GridData->UpdateGridMetrics(PaintedWidth);
		CachedPixelWidth = PaintedWidth;
	}
}