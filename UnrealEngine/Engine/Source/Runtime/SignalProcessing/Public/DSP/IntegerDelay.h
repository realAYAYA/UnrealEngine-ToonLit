// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/Dsp.h"
#include "DSP/AlignedBlockBuffer.h"
#include "DSP/BufferVectorOperations.h"

namespace Audio
{

	// An adjustable delay line. Delays values are limited to integer values. 
	class SIGNALPROCESSING_API FIntegerDelay
	{
	public:

		// InMaxDelaySamples is the maximum supported delay.
		// InDelaySamples is the initial delay in samples.
		FIntegerDelay(int32 InMaxNumDelaySamples, int32 InNumDelaySamples, int32 InNumInternalBufferSamples = 256);

		// Destructor
		~FIntegerDelay();

		// Sets the current delay in samples. InDelay must be less than or equal to the InMaxDelaySamples set in the constructor.
		void SetDelayLengthSamples(int32 InNumDelaySamples);

		// Resets the delay line state, flushes buffer and resets read/write pointers.
		void Reset();

		// Returns the current delay length in samples.
		int32 GetNumDelaySamples() const;

		// Process InSamples, placing delayed versions in OutSamples.
		void ProcessAudio(const Audio::FAlignedFloatBuffer& InSamples, Audio::FAlignedFloatBuffer& OutSamples);

		// Retrieve a copy of the internal delay line.
		// InNum must be less than or equal to InMaxNumDelaySamples.
		void PeekDelayLine(int32 InNum, Audio::FAlignedFloatBuffer& OutSamples);

	private:
		// Process a block of audio. InNum is always less than or equal to NumInternalBufferSamples.
		void ProcessAudioBlock(const float* InSamples, const int32 InNum, float* OutSamples);

		// Maximum supported number of delay samples
		int32 MaxNumDelaySamples;

		// Current number of delay samples
		int32 NumDelaySamples;

		// Offset of output delayed samples relative to DelayLine block buffer
		int32 NumDelayLineOffsetSamples;

		// Offset to handle delay values which are not multiple of required buffer alignment.
		int32 NumBufferOffsetSamples;

		// Number of samples in an internal buffer
		int32 NumInternalBufferSamples;

		// Buffer for holding delayed data.
		TUniquePtr<FAlignedBlockBuffer> DelayLine;
	};
}
