// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralStatistics.h"
#include "GenericPlatform/GenericPlatformMath.h"

DEFINE_LOG_CATEGORY_STATIC(LogNNIStats, Display, All);



/* FNeuralStatisticsEstimator auxiliary functions
 *****************************************************************************/

void FNeuralStatisticsEstimator_CalculateMinMax(float& OutMin, float& OutMax, const TCircularBuffer<float>& InDataBuffer, const bool bInIsBufferFull,
	const uint32 InBufferIdx)
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



/* FNeuralStatistics constructor
 *****************************************************************************/

FNeuralStatistics::FNeuralStatistics(const int InNumberSamples, const float InAverage, const float InStdDev, const float InMin, const float InMax)
	: NumberSamples(InNumberSamples)
	, Average(InAverage)
	, StdDev(InStdDev)
	, Min(InMin)
	, Max(InMax)
{}



/* FNeuralNetworkInferenceQATimer public functions
 *****************************************************************************/

FNeuralStatisticsEstimator::FNeuralStatisticsEstimator(const uint32 InSizeRollingWindow) 
	: EpsilonFloat(0.000001f)
	, BufferIdx(0)
	, bIsBufferFull(false)
	, LastSample(0.f)
	, DataBuffer(InSizeRollingWindow, 0)
{
}
	
void FNeuralStatisticsEstimator::StoreSample(const float InRunTime) 
{
	LastSample = InRunTime;

	DataBuffer[BufferIdx] = LastSample;

	if(BufferIdx == DataBuffer.Capacity() && !bIsBufferFull)
	{
		bIsBufferFull = true;
	}
	BufferIdx = DataBuffer.GetNextIndex(BufferIdx);
}

void FNeuralStatisticsEstimator::ResetStats()
{
	for (uint32 Index = 0; Index < DataBuffer.Capacity(); ++Index)
	{
		DataBuffer[Index] = 0;
	}
	bIsBufferFull = false;
	BufferIdx = 0;
	LastSample = 0;
}

float FNeuralStatisticsEstimator::GetLastSample() const
{
	return LastSample;
}

FNeuralStatistics FNeuralStatisticsEstimator::GetStats() const
{
	const uint32 count = (bIsBufferFull) ? DataBuffer.Capacity() : BufferIdx;
	const float Mean = CalculateMean();
	const float StdDev = CalculateStdDev(Mean);
	float Min, Max;
	FNeuralStatisticsEstimator_CalculateMinMax(Min, Max, DataBuffer, bIsBufferFull, BufferIdx);
	// Return aggregated data
	return FNeuralStatistics(count, Mean, StdDev, Min, Max);
}

float FNeuralStatisticsEstimator::CalculateMean() const
{
	uint32 PtrEnd = (bIsBufferFull) ? DataBuffer.Capacity(): BufferIdx;
	float AccumulatorMean = 0;
	for (uint32 Index = 0; Index < PtrEnd; ++Index)
	{
		AccumulatorMean += DataBuffer[Index];
	}
	// Return mean
	return AccumulatorMean / (static_cast<float>(PtrEnd) + EpsilonFloat);
}

float FNeuralStatisticsEstimator::CalculateStdDev(const float InMean) const
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
