// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveformDrawingUtils.h"

void WaveformDrawingUtils::GetBinnedPeaksFromWaveformRawData(TArray<SampleRange>& OutWaveformPeaks, const uint32 NumBins, const int16* RawDataPtr, const uint32 NumSamples, const uint32 SampleRate, const uint16 NChannels, const float StartTime, const float EndTime)
{
	uint32 NumPeaks = NumBins * NChannels;
	OutWaveformPeaks.SetNumUninitialized(NumPeaks);

	double TimePerBin = (EndTime - StartTime) / NumBins;
	double IterationStartTime = StartTime;
	uint32 FirstBinnedFrame = FMath::RoundToInt(IterationStartTime * SampleRate);

	for (uint32 Peak = 0; Peak < NumPeaks; Peak += NChannels)
	{
		uint32 LastBinnedFrame = FMath::RoundToInt((IterationStartTime + TimePerBin) * SampleRate);

		for (uint16 Channel = 0; Channel < NChannels; ++Channel)
		{
			int16 MaxSampleValue = TNumericLimits<int16>::Min();
			int16 MinSampleValue = TNumericLimits<int16>::Max();

			for (uint32 Frame = FirstBinnedFrame; Frame < LastBinnedFrame; ++Frame)
			{
				const uint32 SampleIndex = Frame * NChannels + Channel;
				check(SampleIndex < NumSamples);
				int16 SampleValue = RawDataPtr[SampleIndex];

				MinSampleValue = FMath::Min(MinSampleValue, SampleValue);
				MaxSampleValue = FMath::Max(MaxSampleValue, SampleValue);

			}

			OutWaveformPeaks[Peak + Channel] = SampleRange(MinSampleValue, MaxSampleValue);
		}

		IterationStartTime += TimePerBin;
		FirstBinnedFrame = LastBinnedFrame;
	}
}
