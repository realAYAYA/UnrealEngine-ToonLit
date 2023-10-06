// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/CircularBuffer.h"

namespace UE::NNEProfiling::Internal
{
	struct NNEPROFILING_API FStatistics
	{
		const uint32 NumberSamples;
		const float Average;
		const float StdDev;
		const float Min;
		const float Max;

		FStatistics(const int InNumberSamples, const float InAverage, const float InStdDev, const float InMin, const float InMax);
	};

	class NNEPROFILING_API FStatisticsEstimator
	{
	public:

		FStatisticsEstimator(const uint32 InSizeRollingWindow = 1024);

		void StoreSample(const float InRunTime);
		void ResetStats();

		float GetLastSample() const;
		FStatistics GetStats() const;

	private:

		const float EpsilonFloat;
		uint32 BufferIdx;
		bool bIsBufferFull;
		float LastSample;
		TCircularBuffer<float> DataBuffer;

		float CalculateMean() const;
		float CalculateStdDev(const float InMean) const;
	};
} // UE::NNEProfiling::Internal