// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HarmonixDsp/AudioBuffer.h"
#include "HarmonixDsp/AudioAnalysis/OutputSettings.h"

#include "VuMeterAnalyzer.generated.h"

USTRUCT(BlueprintType)
struct FHarmonixVuMeterAnalyzerSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Settings")
	float AvgWindowMs = 1.0f;

	UPROPERTY(EditAnywhere, Category="Settings")
	float PeakHoldMs = 1000.0f;

	UPROPERTY(EditAnywhere, Category="Settings")
	FHarmonixAudioAnalyzerOutputSettings OutputSettings{};
};

USTRUCT(BlueprintType)
struct FHarmonixVuMeterAnalyzerChannelValues
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Results")
	float LevelMeanSquared = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Results")
	float PeakSquared = 0.0f;
};

USTRUCT(BlueprintType)
struct FHarmonixVuMeterAnalyzerResults
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Results")
	float MonoPeakDecibels = -96.0f;

	UPROPERTY(BlueprintReadOnly, Category="Results")
	TArray<FHarmonixVuMeterAnalyzerChannelValues> ChannelValues;

	UPROPERTY(BlueprintReadOnly, Category="Results")
	FHarmonixVuMeterAnalyzerChannelValues MonoValues{};
};

namespace Harmonix::Dsp::AudioAnalysis
{
	class HARMONIXDSP_API FVuMeterAnalyzer
	{
	public:
		explicit FVuMeterAnalyzer(float InSampleRate);

		/**
		 * @brief Set the settings for this analyzer
		 * @param InSettings - The settings to use
		 */
		void SetSettings(const FHarmonixVuMeterAnalyzerSettings& InSettings);

		/**
		 * @brief Reset the analyzer to its initial state
		 */
		void Reset();

		/**
		 * @brief Process a buffer and get the analyzer results
		 * @param InBuffer - The audio buffer to analyze
		 * @param InOutResults - The results from the analysis
		 */
		void Process(const TAudioBuffer<float>& InBuffer, FHarmonixVuMeterAnalyzerResults& InOutResults);
		
	private:
		struct FChannelState
    	{
    		FHarmonixVuMeterAnalyzerChannelValues LatestValues{};
    		float RiseTargetSquared = 0.0f;
    		float PeakHoldRemainingMs = 0.0f;
    	};

		static void UpdateChannel(
			FChannelState& ChannelState,
			FHarmonixVuMeterAnalyzerChannelValues& ResultsValues,
			float PeakSquared,
			float MeanSquared,
			float ElapsedMs,
			const FHarmonixVuMeterAnalyzerSettings& SettingsToUse);

		const float SampleRate;
		
		FCriticalSection SettingsGuard;
		FHarmonixVuMeterAnalyzerSettings Settings;
		
		TArray<FChannelState> ChannelStates;
		FChannelState MonoState;
	};
}
