// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/Dsp.h"
#include "DSP/Delay.h"

namespace Audio
{
	// Class which reads doppler-shifted tap from a delay buffer 
	class SIGNALPROCESSING_API FTapDelayPitchShifter
	{
	public:
		static constexpr float MinDelayLength = 10.0f;
		static constexpr float MaxDelayLength = 100.0f;
		static constexpr float MaxAbsPitchShiftInOctaves = 6.0f;
		
		// Constructor
		FTapDelayPitchShifter();

		// Virtual Destructor
		virtual ~FTapDelayPitchShifter();

		// Initialization of the internal delay with given sample rate, pitch shift in semitones, and delay length in milliseconds
		void Init(const float InSampleRate, const float InPitchShift, const float InDelayLength);

		// Sets the internal delay line length.
		void SetDelayLength(const float InDelayLength);

		// Sets the pitch shift in semitones
		void SetPitchShift(const float InPitchScaleSemitones);

		// Sets the pitch shift in terms of a sample rate ratio
		void SetPitchShiftRatio(const float InPitchShiftRatio);
		
		// Reads the next sample tap from the input delay buffer
		float ReadDopplerShiftedTapFromDelay(const Audio::FDelay& InDelayBuffer, const float ReadOffsetMilliseconds = 0.0f);

		// Process whole block of audio reading to and writing from delay line from input buffer
		void ProcessAudio(Audio::FDelay& InDelayBuffer, const float* InAudioBuffer, const int32 InNumFrames, float* OutAudioBuffer);
		
	private:
		void UpdatePhasorPhaseIncrement();
		
		float SampleRate = 0.0f;
		Audio::FExponentialEase CurrentDelayLength;
		float CurrentTargetDelayLength = 0.0f;
		float CurrentPitchShift = 0.0f;
		float CurrentPitchShiftRatio = 1.0f;
		float PhasorPhase = 0.0f;
		float PhasorPhaseIncrement = 0.0f;
	};

}
