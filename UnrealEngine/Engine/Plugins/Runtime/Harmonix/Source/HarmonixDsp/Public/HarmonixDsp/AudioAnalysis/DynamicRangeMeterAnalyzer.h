// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HarmonixDsp/AudioBuffer.h"
#include "HarmonixDsp/Effects/BiquadFilter.h"

namespace Harmonix::Dsp::AudioAnalysis
{
	class HARMONIXDSP_API FDynamicRangeMeterAnalyzer
	{
	public:
		struct FSettings
		{
			float SampleRate = 48000.0f;
			float HighRisingAlpha = 1.0f;
			float HighFallingAlpha = 0.4f;
			float LowRisingAlpha = 0.001f;
			float LowFallingAlpha = 1.0f;
			int32 WindowSize = 480;
			bool ResetRequested = false;
			bool FilterResult = false;
			float FilterCutoff = 30.0f;
		};
	
		struct FResults
		{
			float HighEnvelope = 0.0f;
			float LowEnvelope = 0.0f;
			float LevelDecibels = -96.0f;
			float MonoPeakDecibels = -96.0f;
			float MonoPeakHighEnvelopeDecibels = -96.0f;
			bool RequestReset = false;
		};

		FDynamicRangeMeterAnalyzer();

		/**
		 * @brief Set the settings for this analyzer
		 * @param InSettings - The settings to use
		 */
		void SetSettings(const FSettings& InSettings);

		/**
		 * @brief Reset the analyzer to its initial state
		 */
		void Reset();

		/**
		 * @brief Process a buffer and get the analyzer results
		 * @param InBuffer - The audio buffer to analyze
		 * @param InOutResults - The results from the analysis
		 */
		void Process(const TAudioBuffer<float>& InBuffer, FResults& InOutResults);
		
	private:
		FCriticalSection SettingsGuard;
		FSettings Settings;

		struct FState
		{
			float EnvLast = 0.0f;
			float HighLast = 0.0f;
			float LowLast = 1.0f;
			float RmsSum = 0.0f;
			bool HasHadFirstSettle = false;
			TArray<TDynamicStridePtr<float>> ChannelPointers;
			Effects::TMultipassBiquadFilter<double, 3> Filter;
			Effects::FBiquadFilterCoefs FilterCoefs;
			bool FilterEnabled = false;
			float FilterCutoff = 1000.0f;
			TArray<float> RmsWindow;
			int32 RmsNextWrite = 0;
		};

		FState State;
	};
}
