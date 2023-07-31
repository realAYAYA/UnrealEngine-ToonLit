// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/EnvelopeFollower.h"

namespace Audio
{
	enum EDynamicFilterType
	{
		Bell,
		LowShelf,
		HighShelf,
	};

	// combination of two state-variable filters, the first filtering a key signal, which drives gain in the second
	class SIGNALPROCESSING_API FDynamicStateVariableFilter
	{
	public:
		void Init(const float InSampleRate, const int32 InNumChannels);
		void Reset();

		void ProcessAudio(const float* InSamples, float* OutSamples, const int32 InNumSamples);
		void ProcessAudio(const float* InSamples, float* OutSamples, const float* KeySamples, const int32 InNumSamples);

		void SetFrequency(const float InFrequency);
		void SetQ(const float InQ);
		void SetGain(const float InGain);
		void SetFilterType(const EDynamicFilterType InFilterType);

		void SetAnalog(const bool bInAnalog);
		void SetAttackTime(const float InAttackTime);
		void SetReleaseTime(const float InReleaseTime);
		void SetThreshold(const float InThresholdDb);
		void SetEnvMode(const EPeakMode::Type InMode);
		void SetDynamicRange(const float InDynamicRange);
		void SetRatio(const float InRatio);
		void SetKnee(const float InKnee);

		float GetGainReduction();

	protected:
		float SampleRate = 48000.f;
		float OneOverSampleRate = 1.f / 48000.f;
		int32 NumChannels = 0;

		// human readable parameters
		EDynamicFilterType FilterType = EDynamicFilterType::Bell;

		float Frequency = 1000.f;
		float Q = 1.f;
		float OneOverQ = 1.f;
		float ThresholdDb = -12.f;
		float DynamicRangeDb = 0.f;
		float Ratio = 1.5f;
		float GainDb = 1.f;
		float Knee = 12.f;
		float HalfKnee = 6.f;

		// cached result of envelope follower
		float ScaledGainLinear = 1.f;

		// Cached Gain ratio, only needs to be updated when Ratio is changed 
		float SlopeFactor = 0.f;

		// whether coefficients need updating
		bool bNeedsUpdate = true;

		// state-variable coefficient formulas for the included filter modes can be found here:
		// https://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf
		struct FSVFCoefficients
		{
			// TanOmega
			float G = 0.f;

			// Feedback / 1/Q
			float K = 0.f;

			// Node Scalars
			float A0 = 1.f;
			float A1 = 0.f;
			float A2 = 0.f;
			float A3 = 0.f;

			// Output Scalars
			float M0 = 1.f;
			float M1 = 0.f;
			float M2 = 0.f;
		};

		struct FSVFState
		{
			// Nodes
			float V1 = 0.f;
			float V2 = 0.f;
			float V3 = 0.f;

			// Delays
			float Z1 = 0.f;
			float Z2 = 0.f;

			void ProcessSample(const FSVFCoefficients& Coeffs, const float InSample);
		};

		FInlineEnvelopeFollower Envelope;

		FSVFState KeyFilterState;
		// One per channel
		TArray<FSVFState> FilterState;

		FSVFCoefficients KeyVars;
		
		FSVFCoefficients OutputVars;
		
		// Points in the knee used for lagrangian interpolation
		TArray<FVector2D> KneePoints;
		
		void ProcessBell(const float* InSamples, float* OutSamples, const float* KeySamples, const int32 InNumSamples);
		void ProcessLowShelf(const float* InSamples, float* OutSamples, const float* KeySamples, const int32 InNumSamples);
		void ProcessHighShelf(const float* InSamples, float* OutSamples, const float* KeySamples, const int32 InNumSamples);
		
		void UpdateSettings();
		void UpdatePreFilterVariables();
		void UpdatePostFilterVariables();
		
		float CalcGain(const float KeySample);
		
		void ClearFilterDenormals();
	};
}
