// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/Range.h"

namespace WaveformDrawingUtils
{
	typedef TRange<int16> SampleRange;

	void WAVEFORMEDITORWIDGETS_API GetBinnedPeaksFromWaveformRawData(TArray<SampleRange>& OutWaveformPeaks, const uint32 NBins, const int16* RawDataPtr, const uint32 NSamples, const uint32 SampleRate, const uint16 NChannels, const float StartTime, const float EndTime);
}