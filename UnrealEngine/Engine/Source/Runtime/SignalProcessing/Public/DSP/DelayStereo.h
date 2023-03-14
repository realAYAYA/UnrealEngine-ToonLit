// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/Delay.h"
#include "DSP/Filter.h"

namespace Audio
{
	// The different stereo delay modes
	namespace EStereoDelayMode
	{
		enum Type
		{
			Normal,
			Cross,
			PingPong,

			NumStereoDelayModes
		};
	}

	class SIGNALPROCESSING_API FDelayStereo
	{
	public:
		FDelayStereo();
		~FDelayStereo();

		// Initializes the stereo delay with given sample rate and default max delay length
		void Init(const float InSampleRate, const int32 InNumChannels, const float InDelayLengthSec = 2.0f);

		// Resets the stereo delay state
		void Reset();

		// Process a single frame of audio
		void ProcessAudioFrame(const float* InFrame, float* OutFrame);

		// Process a buffer of audio
		void ProcessAudio(const float* InBuffer, const int32 InNumSamples, float* OutBuffer);

		// Sets which delay stereo mode to use
		void SetMode(const EStereoDelayMode::Type InMode);

		// Gets the current stereo dealy mode
		EStereoDelayMode::Type GetMode() const { return DelayMode; }

		// Sets the delay time in msec. 
		void SetDelayTimeMsec(const float InDelayTimeMsec);

		// Sets the feedback amount
		void SetFeedback(const float InFeedback);

		// Sets the delay ratio (scales difference between left and right stereo delays)
		void SetDelayRatio(const float InDelayRatio);

		// Sets the amount of the effect to mix in the output
		void SetWetLevel(const float InWetLevel);

		// Sets the amount of the effect to mix in the output
		void SetDryLevel(const float InDryLevel);

		// Sets whether or not the filter is enabled
		void SetFilterEnabled(bool bInEnabled);

		// Sets the filter settings
		void SetFilterSettings(EBiquadFilter::Type InFilterType, const float InCutoffFrequency, const float InQ);

	protected:
		// Updates the delays based on recent parameters
		void UpdateDelays();

		// Delay lines per channel
		TArray<FDelay> Delays;

		// Biquad filter per channel to feed the delay output through
		TArray<FBiquadFilter> BiquadFilters;

		// What mode the stereo delay is in
		EStereoDelayMode::Type DelayMode  = EStereoDelayMode::Normal;

		// Amount of delay time in msec	
		float DelayTimeMsec = 0.0f;

		// How much delay feedback to use
		float Feedback = 0.0f;

		// How much to shift the delays from each other
		float DelayRatio = 0.0f;

		// The amount of wet level on the output
		float WetLevel = 0.0f;

		// The amount of dry level on the output
		float DryLevel = 1.0f;

		// Filter data
		float FilterFreq = 20000.0f;
		float FilterQ = 2.0f;
		EBiquadFilter::Type FilterType = EBiquadFilter::Lowpass;

		// The number of channels to use (will sum mono)
		int32 NumChannels = 0;

		// If the delay has started processing yet
		bool bIsInit = true;

		bool bIsFilterEnabled = false;
	};

}
