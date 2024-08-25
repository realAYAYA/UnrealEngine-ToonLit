// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "IFixedSampledSequenceGridService.h"
#include "Fonts/SlateFontInfo.h"
#include "Math/Range.h"
#include "Misc/FrameRate.h"

DECLARE_MULTICAST_DELEGATE(FOnWaveformEditorGridUpdated)

class AUDIOWIDGETS_API FFixedSampledSequenceGridData : public IFixedSampledSequenceGridService
{
public:
	explicit FFixedSampledSequenceGridData(const uint32 InTotalFrames, const uint32 InSampleRateHz, const FSlateFontInfo& InTicksTimeFont, const float InGridSizePixels = 1.f);

	UE_DEPRECATED(5.4, "Constructor taking a const FSlateFontInfo* is deprecated, use the one taking a refrence instead")
	explicit FFixedSampledSequenceGridData(const uint32 InTotalFrames, const uint32 InSampleRateHz, const float InGridSizePixels = 1.f, const FSlateFontInfo* InTicksTimeFont = nullptr);


	void UpdateDisplayRange(const TRange<uint32> InDisplayRange);
	bool UpdateGridMetrics(const float InGridSizePixels);
	virtual const FFixedSampledSequenceGridMetrics GetGridMetrics() const override;
	void SetTicksTimeFont(const FSlateFontInfo& InNewFont);

	UE_DEPRECATED(5.4, "SetTicksTimeFont taking a const FSlateFontInfo* is deprecated, use the one taking a refrence instead")
	void SetTicksTimeFont(const FSlateFontInfo* InNewFont);

	
	const float SnapPositionToClosestFrame(const float InPixelPosition) const;

	FOnWaveformEditorGridUpdated OnGridMetricsUpdated;

private:
	FFixedSampledSequenceGridMetrics GridMetrics;
	uint32 TotalFrames = 0;
	TRange<uint32> DisplayRange;

	float GridSizePixels = 0.f;
	FSlateFontInfo TicksTimeFont;
	FFrameRate GridFrameRate;
};