// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/Range.h"

namespace TimeSeriesDrawingUtils
{
	/**
	* Groups samples evenly into a number of desired bins.
	* Each bin contains the min and max values of the grouped samples.
	*
	* @param OutBins			TArray where the bins will be written to
	* @param NumDesiredBins		Number of output bins
	* @param RawDataPtr			Ptr to the beginning of the samples
	* @param TotalNumSamples	The total number of samples of the input time series
	* @param NChannels			Number of interleaved channels in the time series
	* @param StartRatio			The ratio of the total number of frames (in a range of 0-1) at which grouping should start
	* @param EndRatio			The ratio of the total number of frames (in a range of 0-1) at which grouping should end.
	*
	*/
	template<typename SamplesType>
	void GroupInterleavedSamplesIntoMinMaxBins(TArray<TRange<SamplesType>>& OutBins, const uint32 NumDesiredBins, const SamplesType* RawDataPtr, const uint32 TotalNumSamples, const uint16 NChannels = 1, const double StartRatio = 0.0, double EndRatio = 1.0)
	{
		check(StartRatio >= 0.f && StartRatio < EndRatio)
		check(EndRatio <= 1.f)

		uint32 NumPeaks = NumDesiredBins * NChannels;
		OutBins.SetNumUninitialized(NumPeaks);
		const uint32 NumFrames = TotalNumSamples / NChannels;

		double FramesPerBin = ((EndRatio - StartRatio) * NumFrames) / NumDesiredBins;
		double IterationStartFrame = StartRatio * NumFrames;
		uint32 FirstBinnedFrame = FMath::RoundToInt(StartRatio * NumFrames);

		for (uint32 Peak = 0; Peak < NumPeaks; Peak += NChannels)
		{
			uint32 LastBinnedFrame = IterationStartFrame + FramesPerBin;

			for (uint16 Channel = 0; Channel < NChannels; ++Channel)
			{
				SamplesType MaxSampleValue = TNumericLimits<SamplesType>::Min();
				SamplesType MinSampleValue = TNumericLimits<SamplesType>::Max();

				for (uint32 Frame = FirstBinnedFrame; Frame < LastBinnedFrame; ++Frame)
				{
					const uint32 SampleIndex = Frame * NChannels + Channel;
					check(SampleIndex < TotalNumSamples);
					int16 SampleValue = RawDataPtr[SampleIndex];

					MinSampleValue = FMath::Min(MinSampleValue, SampleValue);
					MaxSampleValue = FMath::Max(MaxSampleValue, SampleValue);

				}

				OutBins[Peak + Channel] = TRange<SamplesType>(MinSampleValue, MaxSampleValue);
			}

			IterationStartFrame += FramesPerBin;
			FirstBinnedFrame = LastBinnedFrame;
		}
	}

	/**
	* Groups samples of a time series into an equal number of desired bins.
	* Each bin contains the min and max values of the grouped samples.
	* 
	* @param OutBins			TArray where the bins will be written to 
	* @param NumDesiredBins		Number of output bins
	* @param RawDataPtr			Ptr to the beginning of the samples 
	* @param TotalNumSamples	The total number of samples of the input time series 
	* @param SampleRate			SampleRate of the time series 
	* @param NChannels			Number of interleaved channels in the time series
	* @param StartTime			The time at which grouping should start
	* @param EndTime 			The time at which grouping should end. 
	*							
	* Note: With a negative EndTime, the method will calculate automatically the EndTime by doing 
	* TotalNumSamples / (SampleRate * NChannels)
	*							
	*/
	template<typename SamplesType>
	void GroupInterleavedSampledTSIntoMinMaxBins(TArray<TRange<SamplesType>>& OutBins, const uint32 NumDesiredBins, const SamplesType* RawDataPtr, const uint32 TotalNumSamples, const uint32 SampleRate, const uint16 NChannels = 1, const float StartTime = 0.f, float EndTime = -1.f)
	{
		const double TotalTime = TotalNumSamples / ((float) SampleRate * NChannels);
		const double StartRatio = StartTime / TotalTime;
		const double EndRatio = EndTime >= 0.f ? FMath::Clamp(EndTime / TotalTime, StartRatio, 1.0) : 1.0;

		GroupInterleavedSamplesIntoMinMaxBins(OutBins, NumDesiredBins, RawDataPtr, TotalNumSamples, NChannels, StartRatio, EndRatio);
	}
}