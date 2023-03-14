// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/Osc.h"

namespace Audio
{
	// Bit crushing effect
	// https://en.wikipedia.org/wiki/Bitcrusher
	class SIGNALPROCESSING_API FBitCrusher
	{
	public:
		// Constructor
		FBitCrusher();

		// Destructor
		~FBitCrusher();

		// Initialize the equalizer
		void Init(const float InSampleRate, const int32 InNumChannels);

		// The amount to reduce the sample rate of the audio stream.
		void SetSampleRateCrush(const float InFrequency);

		// The amount to reduce the bit depth of the audio stream.
		void SetBitDepthCrush(const float InBitDepth);

		// Processes audio
		void ProcessAudioFrame(const float* InFrame, float* OutFrame);
		void ProcessAudio(const float* InBuffer, const int32 InNumSamples, float* OutBuffer);

		// Returns the maximum value given bit depths will be clamped to.
		static float GetMaxBitDepth();

	private:
		// i.e. 8 bit, etc. But can be float!
		float SampleRate;
		float BitDepth;
		float BitDelta;
		float ReciprocalBitDelta;

		// The current phase of the bit crusher
		float Phase;

		// The amount of phase to increment each sample
		float PhaseDelta;

		// Used to sample+hold the last output
		float LastOutput[2];

		int32 NumChannels;
	};

}
