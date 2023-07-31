// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/Osc.h"

namespace Audio
{
	// Foldback distortion effect
	// https://en.wikipedia.org/wiki/Foldback_(power_supply_design)
	class SIGNALPROCESSING_API FFoldbackDistortion
	{
	public:
		// Constructor
		FFoldbackDistortion();

		// Destructor
		~FFoldbackDistortion();

		// Initialize the equalizer
		void Init(const float InSampleRate, const int32 InNumChannels);

		// Sets the foldback distortion threshold
		void SetThresholdDb(const float InThresholdDb);

		// Sets the input gain
		void SetInputGainDb(const float InInputGainDb);

		// Sets the output gain
		void SetOutputGainDb(const float InOutputGainDb);

		// Processes a single audio sample
		float ProcessAudioSample(const float InSample);

		// Processes a mono stream
		void ProcessAudioFrame(const float* InFrame, float* OutFrame);

		// Processes a stereo stream
		void ProcessAudio(const float* InBuffer, const int32 InNumSamples, float* OutBuffer);

	private:
		// Threshold to check before folding audio back on itself
		float Threshold;

		// Threshold times 2
		float Threshold2;

		// Threshold time 4
		float Threshold4;

		// Input gain used to force hitting the threshold
		float InputGain;

		// A final gain scaler to apply to the output
		float OutputGain;

		// How many channels we expect the audio intput to be.
		int32 NumChannels;
	};

}
