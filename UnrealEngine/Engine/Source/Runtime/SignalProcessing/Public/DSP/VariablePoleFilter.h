// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/Filter.h"

namespace Audio
{
	enum class EFilterOrder : uint32
	{
		TwoPole = 1,
		FourPole,
		SixPole,
		EightPole,
	};

	class SIGNALPROCESSING_API FVariablePoleFilter
	{
	public:
		int32 NumFilters = 0;
		int32 NumChannels = 0;


		EBiquadFilter::Type Type = EBiquadFilter::Lowpass;
		FVariablePoleFilter() {};

		void Init(EFilterOrder InOrder
				  , const float InSampleRate
				  , const int32 InNumChannels
				  , const float InFrequency
				  , const EBiquadFilter::Type InType
				  , const float InBandwidth = 2.f
				  , const float InGain = 0.f);

		void SetParams(const EBiquadFilter::Type InFilterType, const float InCutoffFrequency, const float InBandwidth = 2.f, const float InGain = 0.f);

		void ProcessAudioFrame(const float* InFrame, float* OutFrame);
		void ProcessAudioBuffer(const float* InFrame, float* OutFrame, const int32 NumSamples);

		void Reset();

		const TArray<FBiquadFilter>& GetFilters() const { return Filters; }

	private:
		TArray<FBiquadFilter> Filters;
	};
}