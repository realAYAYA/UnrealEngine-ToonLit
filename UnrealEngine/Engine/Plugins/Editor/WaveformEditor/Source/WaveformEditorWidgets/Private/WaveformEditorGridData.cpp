// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveformEditorGridData.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericPlatformMath.h"
#include "Misc/FrameRate.h"
#include "WaveformEditorRenderData.h"

FWaveformEditorGridData::FWaveformEditorGridData(TSharedRef<FWaveformEditorRenderData> InRenderData, const FSlateFontInfo* InTicksTimeFont /* = nullptr*/)
	: RenderData(InRenderData)
	, TicksTimeFont(InTicksTimeFont)
{
}

void FWaveformEditorGridData::UpdateDisplayRange(const TRange<float> InDisplayRange)
{
	DisplayRange = InDisplayRange;
	UpdateGridMetrics(GridPixelWidth);
}

bool FWaveformEditorGridData::UpdateGridMetrics(const float InGridPixelWidth)
{
	if (GridPixelWidth != InGridPixelWidth)
	{
		GridPixelWidth = InGridPixelWidth;
	}
	
	const float WaveformDurationSeconds = RenderData->GetOriginalWaveformDurationInSeconds();
	const double StartTime = WaveformDurationSeconds * DisplayRange.GetLowerBoundValue();
	const double DisplayedDuration = WaveformDurationSeconds * DisplayRange.Size<double>();
	const double PixelsPerSecond = GridPixelWidth / DisplayedDuration;
	double MajorGridStepSeconds = 0.0;
	float MinTicksPixelSpacing = 30.0f;
	
	if (TicksTimeFont)
	{
		TSharedRef<FSlateFontMeasure> FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		FString LongestDisplayedTimeString = FString::Printf(TEXT("%.3f"), WaveformDurationSeconds * DisplayRange.GetUpperBoundValue());
		FVector2D MaxTextSize = FontMeasureService->Measure(LongestDisplayedTimeString, *TicksTimeFont);
		MinTicksPixelSpacing = MaxTextSize.X;
	}

	const FFrameRate FrameRate(RenderData->GetSampleRate(), 1);

	if (!FrameRate.ComputeGridSpacing(PixelsPerSecond, MajorGridStepSeconds, GridMetrics.NumMinorGridDivisions, MinTicksPixelSpacing + 5.f))
	{
		return false;
	}

	const double GridOffset = FGenericPlatformMath::Fmod(StartTime, MajorGridStepSeconds);
	GridMetrics.FirstMajorTickX = (0.f - GridOffset) * PixelsPerSecond;
	GridMetrics.MajorGridXStep = MajorGridStepSeconds * PixelsPerSecond;
	GridMetrics.StartTime = StartTime;
	GridMetrics.PixelsPerSecond = PixelsPerSecond;

	if (OnGridMetricsUpdated.IsBound())
	{
		OnGridMetricsUpdated.Broadcast(GridMetrics);
	}

	return true;
}

const FWaveEditorGridMetrics FWaveformEditorGridData::GetGridMetrics() const
{
	return GridMetrics;
}

void FWaveformEditorGridData::SetTicksTimeFont(const FSlateFontInfo* InNewFont)
{
	TicksTimeFont = InNewFont;
}
