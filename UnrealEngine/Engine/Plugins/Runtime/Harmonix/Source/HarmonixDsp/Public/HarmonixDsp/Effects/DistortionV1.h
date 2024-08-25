// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "HarmonixDsp/AudioBuffer.h"
#include "HarmonixDsp/Effects/BiquadFilter.h"
#include "HarmonixDsp/Effects/FirFilter.h"
#include "HarmonixDsp/Effects/Settings/DistortionSettings.h"

namespace Harmonix::Dsp::Effects
{

	class HARMONIXDSP_API FDistortionV1
	{
	public:
		FDistortionV1(int32 sampleRate = 48000);

		void Process(TAudioBuffer<float>& InOutBuffer);

		void SetInputGainDb(float GainDb);
		void SetOutputGainDb(float GainDb);
		void SetType(EDistortionTypeV1 Type);
		void SetupFilter(int32 Index, const FBiquadFilterSettings& settings);
		void SetupFilter(int32 Index, const FDistortionFilterSettings& Settings);
		void SetOversample(bool Oversample, int32 RenderBufferSizeFrames);
		void SetSampleRate(int32 SampleRate);

		static const int32 kNumFilters = 3;
		static const int32 kMaxChannels = 8;

	private:
		void ProcessChannel(int32 Channel, float* InOutSamples, int32 NumFrames);

		EDistortionTypeV1    Type;
		float                InputGain;
		float                OutputGain;
		float                DCAdjust;
		bool                 FilterEnabled[kNumFilters];
		bool                 FilterPreClip[FDistortionV1::kNumFilters];
		FBiquadFilterCoefs   FilterCoefs[kNumFilters];
		TBiquadFilter<double>        Filter[kNumFilters][kMaxChannels];
		FFirFilter32         OversampleFilterUp[kMaxChannels];
		FFirFilter32         OversampleFilterDown[kMaxChannels];
		bool                 DoOversampling;
		TAudioBuffer<float>  UpsampleBuffer;
		int32                  SampleRate;
		static const int32     kNumFilterTaps = 32;
		static const float   kOversamplingFilterTaps[kNumFilterTaps];
	};

};
