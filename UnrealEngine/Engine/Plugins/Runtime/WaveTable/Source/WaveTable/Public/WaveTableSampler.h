// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "DSP/Filter.h"
#include "HAL/Platform.h"
#include "WaveTableSettings.h"


namespace WaveTable
{
	class WAVETABLE_API FWaveTableSampler
	{
	public:
		// Interpolation mode when sampling values between indices
		enum class EInterpolationMode
		{
			// No interpolation (Stepped)
			None,

			// Linear interpolation
			Linear,

			// Cubic interpolation
			Cubic,

			// Takes the maximum value between two points (EXPENSIVE, but good for caching/drawn curves).
			MaxValue,

			COUNT
		};

		// Mode of interpolation between last value in table and subsequent input index when sampling single values.
		enum class ESingleSampleMode
		{
			// Interpolates last value to zero (0.0f) in table if sampled index is beyond last position
			Zero = 0,

			// Interpolates last value to unit (1.0f) in table if sampled index is beyond last position
			Unit = 1,

			// Holds last value in table if index is beyond last position
			Hold,

			// Interpolates last value and first value in table if sampled index is beyond last position
			Loop,
		};

		struct WAVETABLE_API FSettings
		{
			float Amplitude = 1.0f;
			float Offset = 0.0f;

			// How many times to read through the incoming
			// buffer as a ratio to a single output buffer
			float Freq = 1.0f;

			// Offset from beginning to end of array [0.0, 1.0]
			float Phase = 0.0f;

			EInterpolationMode InterpolationMode = EInterpolationMode::Linear;
		};

		FWaveTableSampler();
		FWaveTableSampler(FSettings&& InSettings);

		// Interpolates and converts values in the given table for each provided index in the index-to-samples TArrayView (an array of sub-sample, floating point, indices)
		static void Interpolate(TArrayView<const float> InTableView, TArrayView<float> InOutIndexToSamplesView, EInterpolationMode InterpMode = EInterpolationMode::Linear);

		float Process(TArrayView<const float> InTableView, float& OutSample, ESingleSampleMode InMode = ESingleSampleMode::Zero);
		float Process(TArrayView<const float> InTableView, TArrayView<float> OutSamplesView);
		float Process(TArrayView<const float> InTableView, TArrayView<const float> InFreqModulator, TArrayView<const float> InPhaseModulator, TArrayView<const float> InSyncTriggers, TArrayView<float> OutSamplesView);

		void Reset();

		const FSettings& GetSettings() const;

		void SetInterpolationMode(EInterpolationMode InMode);
		void SetFreq(float InFreq);
		void SetPhase(float InPhase);

	private:
		void ComputeIndexFrequency(TArrayView<const float> InTableView, TArrayView<const float> InFreqModulator, TArrayView<const float> InSyncTriggers, TArrayView<float> OutIndicesView);
		void ComputeIndexPhase(TArrayView<const float> InTableView, TArrayView<const float> InPhaseModulator, TArrayView<float> OutIndicesView);

		float LastIndex = 0.0f;

		TArray<float> PhaseModScratch;
		FSettings Settings;
	};
} // namespace WaveTable