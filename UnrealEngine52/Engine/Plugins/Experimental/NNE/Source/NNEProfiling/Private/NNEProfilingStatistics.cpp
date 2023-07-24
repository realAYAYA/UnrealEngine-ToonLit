// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEProfilingStatistics.h"
#include "GenericPlatform/GenericPlatformMath.h"

namespace UE::NNEProfiling::Internal
{
	void FStatisticsEstimator_CalculateMinMax(float& OutMin, float& OutMax, const TCircularBuffer<float>& InDataBuffer, const bool bInIsBufferFull, const uint32 InBufferIdx)
	{
		const uint32 PtrEnd = (bInIsBufferFull) ? InDataBuffer.Capacity() : InBufferIdx;

		OutMin = FLT_MAX;
		OutMax = FLT_MIN;
		for (uint32 Index = 0; Index < PtrEnd; ++Index)
		{
			const float CurrentElement = InDataBuffer[Index];
			if (CurrentElement < OutMin)
			{
				OutMin = CurrentElement;
			}
			if (CurrentElement > OutMax)
			{
				OutMax = CurrentElement;
			}
		}
	}

	FStatistics::FStatistics(const int InNumberSamples, const float InAverage, const float InStdDev, const float InMin, const float InMax)
		: NumberSamples(InNumberSamples)
		, Average(InAverage)
		, StdDev(InStdDev)
		, Min(InMin)
		, Max(InMax)
	{

	}

	FStatisticsEstimator::FStatisticsEstimator(const uint32 InSizeRollingWindow)
		: EpsilonFloat(0.000001f)
		, BufferIdx(0)
		, bIsBufferFull(false)
		, LastSample(0.f)
		, DataBuffer(InSizeRollingWindow, 0)
	{

	}

	void FStatisticsEstimator::StoreSample(const float InRunTime)
	{
		LastSample = InRunTime;

		DataBuffer[BufferIdx] = LastSample;

		if (BufferIdx == DataBuffer.Capacity() && !bIsBufferFull)
		{
			bIsBufferFull = true;
		}
		BufferIdx = DataBuffer.GetNextIndex(BufferIdx);
	}

	void FStatisticsEstimator::ResetStats()
	{
		for (uint32 Index = 0; Index < DataBuffer.Capacity(); ++Index)
		{
			DataBuffer[Index] = 0;
		}
		bIsBufferFull = false;
		BufferIdx = 0;
		LastSample = 0;
	}

	float FStatisticsEstimator::GetLastSample() const
	{
		return LastSample;
	}

	FStatistics FStatisticsEstimator::GetStats() const
	{
		const uint32 count = (bIsBufferFull) ? DataBuffer.Capacity() : BufferIdx;
		const float Mean = CalculateMean();
		const float StdDev = CalculateStdDev(Mean);
		float Min, Max;
		FStatisticsEstimator_CalculateMinMax(Min, Max, DataBuffer, bIsBufferFull, BufferIdx);
		// Return aggregated data
		return FStatistics(count, Mean, StdDev, Min, Max);
	}

	float FStatisticsEstimator::CalculateMean() const
	{
		uint32 PtrEnd = (bIsBufferFull) ? DataBuffer.Capacity() : BufferIdx;
		float AccumulatorMean = 0;
		for (uint32 Index = 0; Index < PtrEnd; ++Index)
		{
			AccumulatorMean += DataBuffer[Index];
		}
		// Return mean
		return AccumulatorMean / (static_cast<float>(PtrEnd) + EpsilonFloat);
	}

	float FStatisticsEstimator::CalculateStdDev(const float InMean) const
	{
		const uint32 PtrEnd = (bIsBufferFull) ? DataBuffer.Capacity() : BufferIdx;

		float AccumulatorStdDev = 0;
		for (uint32 Index = 0; Index < PtrEnd; ++Index)
		{
			AccumulatorStdDev += (DataBuffer[Index] - InMean) * (DataBuffer[Index] - InMean);
		}
		// Return std deviation
		return FMath::Sqrt(AccumulatorStdDev / (static_cast<float>(PtrEnd) + EpsilonFloat));
	}
} // UE::NNEProfiling::Internal