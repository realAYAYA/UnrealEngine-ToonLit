// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/BufferVectorOperations.h"
#include "DSP/Delay.h"

namespace Audio
{
	class SIGNALPROCESSING_API FFlanger
	{
	public: 
		FFlanger();
		~FFlanger();

		void Init(const float InSampleRate);

		void SetModulationRate(const float InModulationRate);
		void SetModulationDepth(const float InModulationDepth);
		void SetCenterDelay(const float InCenterDelay);
		void SetMixLevel(const float InMixLevel);

		void ProcessAudio(const FAlignedFloatBuffer& InBuffer, const int32 InNumSamples, FAlignedFloatBuffer& OutBuffer);
	protected: 
		static const float MaxDelaySec;
		static const float MaxModulationRate;
		static const float MaxCenterDelay;

		// LFO parameters
		// LFO frequency 
		float ModulationRate = 0.5f;
		// Modulation depth is clamped to CenterDelayMsec to avoid clipping
		float ModulationDepth = 0.5f;
		float CenterDelayMsec = 0.5f;

		// Balance between original and delayed signal 
		// (Should be between 0 and 1.0; 
		// 0.5 is equal amounts of each and 
		// > 0.5 is more delayed signal than non-delayed signal)
		float MixLevel = 0.5f;
		float DelayedSignalLevel = 0.5f;
		float NonDelayedSignalLevel = 0.5f;

		// Internal delay buffer 
		FDelay DelayBuffer;
		// Scratch buffer used for accumulating delay samples per block
		FAlignedFloatBuffer ScratchBuffer;

		// Delay sample generated from LFO per block
		float DelaySample = 0.0f;

		// Internal LFO for delay amount
		FSinOsc2DRotation LFO;

		// The audio sample rate, Init() must be called to initialize this
		float SampleRate = 48000.0f;
	};
}
