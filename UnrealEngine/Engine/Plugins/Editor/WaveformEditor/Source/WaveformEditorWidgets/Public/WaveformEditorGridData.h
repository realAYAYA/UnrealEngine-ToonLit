// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "IFixedSampledSequenceGridService.h"
#include "Math/Range.h"
#include "Misc/FrameRate.h"

DECLARE_MULTICAST_DELEGATE(FOnWaveformEditorGridUpdated)

struct FSlateFontInfo;

class WAVEFORMEDITORWIDGETS_API FWaveformEditorGridData : public IFixedSampledSequenceGridService
{
public: 
	explicit FWaveformEditorGridData(const uint32 InTotalFrames, const uint32 InSampleRateHz, const float InGridSizePixels = 1.f, const FSlateFontInfo* InTicksTimeFont = nullptr);

	void UpdateDisplayRange(const TRange<uint32> InDisplayRange);
	bool UpdateGridMetrics(const float InGridSizePixels);
	virtual const FFixedSampledSequenceGridMetrics GetGridMetrics() const override;
	void SetTicksTimeFont(const FSlateFontInfo* InNewFont);
	const float SnapPositionToClosestFrame(const float InPixelPosition) const;

	FOnWaveformEditorGridUpdated OnGridMetricsUpdated;

private:
	FFixedSampledSequenceGridMetrics GridMetrics;
	uint32 TotalFrames = 0;
	TRange<uint32> DisplayRange;

	float GridSizePixels = 0.f;
	const FSlateFontInfo* TicksTimeFont = nullptr;
	FFrameRate GridFrameRate;
};