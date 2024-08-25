// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HarmonixDsp/AudioBuffer.h"

#include "WaveformAnalyzer.generated.h"

USTRUCT(BlueprintType)
struct FHarmonixWaveformAnalyzerSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Settings")
	int32 NumBinsHeld = 1024;

	UPROPERTY(EditAnywhere, Category="Settings")
	int32 NumBinsPerSecond = 512;

	UPROPERTY(EditAnywhere, Category="Settings")
	int32 SmoothingDistance = 0;

	UPROPERTY(EditAnywhere, Category="Settings")
	float SmoothingFactor = 0;
};

USTRUCT(BlueprintType)
struct FHarmonixWaveformAnalyzerResults
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, Category="Results")
	TArray<float> WaveformRaw;

	UPROPERTY(BlueprintReadOnly, Category="Results")
	TArray<float> WaveformSmoothed;
};

namespace Harmonix::Dsp::AudioAnalysis
{
	class HARMONIXDSP_API FWaveformAnalyzer
	{
	public:
		explicit FWaveformAnalyzer(float InSampleRate);

		/**
		 * @brief Set the settings for this analyzer
		 * @param InSettings - The settings to use
		 */
		void SetSettings(const FHarmonixWaveformAnalyzerSettings& InSettings);

		/**
		 * @brief Reset the analyzer to its initial state
		 */
		void Reset();

		/**
		 * @brief Process a buffer and get the analyzer results
		 * @param InBuffer - The audio buffer to analyze
		 * @param InOutResults - The results from the analysis
		 */
		void Process(const TAudioBuffer<float>& InBuffer, FHarmonixWaveformAnalyzerResults& InOutResults);
		
	private:
		const float SampleRate;
		
		FCriticalSection SettingsGuard;
		FHarmonixWaveformAnalyzerSettings Settings;

		struct FState
		{
			int32 NumLeftoverFrames = 0;
			float LeftoverBinSumSquared = 0;
		};
		FState State;
	};
}
