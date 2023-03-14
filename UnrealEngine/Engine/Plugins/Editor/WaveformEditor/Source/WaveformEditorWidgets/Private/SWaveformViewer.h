// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "WaveformDrawingUtils.h"
#include "WaveformEditorGridData.h"
#include "WaveformEditorSlateTypes.h"
#include "WaveformEditorTransportCoordinator.h"
#include "Widgets/SLeafWidget.h"

class FWaveformEditorRenderData;
struct FWaveformEditorWidgetStyleBase;
struct FWaveformViewerStyle;

struct FSamplesBinCoordinates
{
	FVector2D PointA;
	FVector2D PointB;
};

struct FChannelSlotBoundaries
{
	explicit FChannelSlotBoundaries(const uint16 ChannelToDraw, const uint16 TotalNumChannels, const FGeometry& InAllottedGeometry)
	{
		Height = InAllottedGeometry.GetLocalSize().Y / TotalNumChannels;
		float ChannelSlotMidPoint = Height / 2.f;

		Top = Height * ChannelToDraw;
		Center = Top + ChannelSlotMidPoint;
		Bottom = Top + Height;
	}

	float Top;
	float Center;
	float Bottom;
	float Height;
};

class SWaveformViewer : public SLeafWidget
{
public:

	SLATE_BEGIN_ARGS(SWaveformViewer) 
	{
	}

	SLATE_STYLE_ARGUMENT(FWaveformViewerStyle, Style)
		
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FWaveformEditorRenderData> InRenderData, TSharedRef<FWaveformEditorTransportCoordinator> InTransportCoordinator);
	
	void OnRenderDataUpdated();
	void OnDisplayRangeUpdated(const TRange<float> NewDisplayRange);
	void UpdateGridMetrics(const FWaveEditorGridMetrics& InMetrics);
	void OnStyleUpdated(const FWaveformEditorWidgetStyleBase* UpdatedStyle);

private:
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void GenerateWaveformBinsCoordinates(TArray<FSamplesBinCoordinates>& OutDrawCoordinates, const TArray<WaveformDrawingUtils::SampleRange>& InWaveformPeaks, const FGeometry& InAllottedGeometry, const float VerticalZoomFactor = 1.f);
	void GenerateWaveformSamplesCoordinates(TArray<FVector2D>& OutDrawCoordinates, const FGeometry& InAllottedGeometry, const float VerticalZoomFactor = 1.f);
	void DrawGridLines(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const;

	TRange<float> DisplayRange;
	bool bForceRedraw = false;

	TArrayView<const int16> SampleData;
	FWaveEditorGridMetrics GridMetrics;

	const FWaveformViewerStyle* Style = nullptr;
	FSlateBrush BackgroundBrush;
	FSlateColor BackgroundColor = FLinearColor(0.02f, 0.02f, 0.02f, 1.f);
	FSlateColor WaveformColor = FLinearColor::White;
	FSlateColor MajorGridLineColor = FLinearColor::Black;
	FSlateColor MinorGridLineColor = FLinearColor(0.f, 0.f, 0.f, 0.5f);
	FSlateColor ZeroCrossingLineColor = FLinearColor::Black;
	float ZeroCrossingLineThickness = 1.f;
	float DesiredHeight = 0.f;
	float DesiredWidth = 0.f;
	float SampleMarkersSize = 2.5f;
	float WaveformLineThickness = 1.f;
	
	const float MaxDistanceFromChannelBoundaries = 2.f;
	const float MaxWaveformHeight = 0.9f;
	const float MinWaveformBinHeight = 0.1f;
	const float MinScaledSamplePeakValue = 0.001f;

	uint32 CachedPixelWidth = 0; 
	float CachedPixelHeight = 0.f;
	TArray<WaveformDrawingUtils::SampleRange> CachedPeaks;
	TArray<FSamplesBinCoordinates> CachedBinsDrawCoordinates;
	TArray<FVector2D> CachedSampleDrawCoordinates;

	TSharedPtr<FWaveformEditorTransportCoordinator> TransportCoordinator = nullptr;
	TSharedPtr<FWaveformEditorRenderData> RenderData = nullptr;

	enum class EWaveformDrawMode {
		BinnedPeaks = 0,
		WaveformLine
	} WaveformDrawMode;
};