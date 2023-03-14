// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveformEditorSlateTypes.h"

#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"

namespace WaveformEditorStylesSharedParams
{
	const FLazyName BackgroundBrushName = TEXT("WhiteBrush");
	const FLazyName HandleBrushName = TEXT("Sequencer.Timeline.VanillaScrubHandleDown");
	const FLinearColor PlayheadColor = FLinearColor(255.f, 0.1f, 0.2f, 1.f);
	const FLinearColor RulerTicksColor = FLinearColor(1.f, 1.f, 1.f, 0.9f);
	const FLinearColor WaveformBackGroundColor = FLinearColor(0.02f, 0.02f, 0.02f, 1.f);
	const float ViewerHeight = 720.f;
	const float ViewerWidth = 1280.f;
}

const FName FWaveformViewerStyle::TypeName("FWaveformViewerStyle");

FWaveformViewerStyle::FWaveformViewerStyle()
	: WaveformColor(FLinearColor::White)
	, WaveformLineThickness(1.f)
	, MajorGridLineColor(FLinearColor::Black)
	, MinorGridLineColor(FLinearColor(0.f, 0.f, 0.f, 0.5f))
	, ZeroCrossingLineColor(FLinearColor::Black)
	, ZeroCrossingLineThickness(1.f)
	, SampleMarkersSize(2.5f)
	, WaveformBackgroundColor(WaveformEditorStylesSharedParams::WaveformBackGroundColor)
	, BackgroundBrush(*FAppStyle::GetBrush(WaveformEditorStylesSharedParams::BackgroundBrushName))
	, DesiredWidth(WaveformEditorStylesSharedParams::ViewerWidth)
	, DesiredHeight(WaveformEditorStylesSharedParams::ViewerHeight)
{
}

const FWaveformViewerStyle& FWaveformViewerStyle::GetDefault()
{
	static FWaveformViewerStyle Default;
	return Default;
}

void FWaveformViewerStyle::GetResources(TArray< const FSlateBrush* >& OutBrushes) const
{
	OutBrushes.Add(&BackgroundBrush);
}

const FName FWaveformViewerOverlayStyle::TypeName("FWaveformViewerOverlayStyle");

FWaveformViewerOverlayStyle::FWaveformViewerOverlayStyle()
	: PlayheadColor(WaveformEditorStylesSharedParams::PlayheadColor)
	, PlayheadWidth(1.0f)
	, DesiredWidth(WaveformEditorStylesSharedParams::ViewerWidth)
	, DesiredHeight(WaveformEditorStylesSharedParams::ViewerHeight)
{
}

const FWaveformViewerOverlayStyle& FWaveformViewerOverlayStyle::GetDefault()
{
	static FWaveformViewerOverlayStyle Default;
	return Default;
}


const FName FWaveformEditorTimeRulerStyle::TypeName("FWaveformEditorTimeRulerStyle");

FWaveformEditorTimeRulerStyle::FWaveformEditorTimeRulerStyle()
	: HandleWidth(15.f)
	, HandleColor(WaveformEditorStylesSharedParams::PlayheadColor)
	, HandleBrush(*FAppStyle::GetBrush(WaveformEditorStylesSharedParams::HandleBrushName))
	, TicksColor(WaveformEditorStylesSharedParams::RulerTicksColor)
	, TicksTextColor(WaveformEditorStylesSharedParams::RulerTicksColor)
	, TicksTextFont(FAppStyle::GetFontStyle("Regular"))
	, TicksTextOffset(5.f)
	, BackgroundColor(FLinearColor::Black)
	, BackgroundBrush(*FAppStyle::GetBrush(WaveformEditorStylesSharedParams::BackgroundBrushName))
	, DesiredWidth(WaveformEditorStylesSharedParams::ViewerWidth)
	, DesiredHeight(30.f)
{
}

const FWaveformEditorTimeRulerStyle& FWaveformEditorTimeRulerStyle::GetDefault()
{
	static FWaveformEditorTimeRulerStyle Default;
	return Default;
}

void FWaveformEditorTimeRulerStyle::GetResources(TArray< const FSlateBrush* >& OutBrushes) const
{
	OutBrushes.Add(&HandleBrush);
	OutBrushes.Add(&BackgroundBrush);
}

