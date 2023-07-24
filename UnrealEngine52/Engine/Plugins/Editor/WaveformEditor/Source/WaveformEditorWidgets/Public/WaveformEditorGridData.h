// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "Math/Range.h"

struct FSlateFontInfo;
class FWaveformEditorRenderData;

struct WAVEFORMEDITORWIDGETS_API FWaveEditorGridMetrics
{
	int32 NumMinorGridDivisions = 0;
	double PixelsPerSecond = 0;
	double FirstMajorTickX = 0;
	double MajorGridXStep = 0;
	double StartTime = 0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGridMetricsUpdated, const FWaveEditorGridMetrics& /* New Grid Metrics */);

class WAVEFORMEDITORWIDGETS_API FWaveformEditorGridData
{
public: 
	explicit FWaveformEditorGridData(TSharedRef<FWaveformEditorRenderData> InRenderData, const FSlateFontInfo* InTicksTimeFont = nullptr);

	FOnGridMetricsUpdated OnGridMetricsUpdated;

	void UpdateDisplayRange(const TRange<float> InDisplayRange);
	bool UpdateGridMetrics(const float InGridPixelWidth);
	const FWaveEditorGridMetrics GetGridMetrics() const;
	void SetTicksTimeFont(const FSlateFontInfo* InNewFont);

private:
	FWaveEditorGridMetrics GridMetrics;
	TSharedPtr<FWaveformEditorRenderData> RenderData = nullptr;
	TRange<float> DisplayRange = TRange<float>::Inclusive(0.f, 1.f);

	float GridPixelWidth = 0.f;
	const FSlateFontInfo* TicksTimeFont = nullptr;
};