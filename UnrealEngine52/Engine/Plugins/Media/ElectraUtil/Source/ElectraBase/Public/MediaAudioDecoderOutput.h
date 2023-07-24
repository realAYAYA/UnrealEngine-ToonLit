// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MediaDecoderOutput.h"

class IAudioDecoderOutput : public IDecoderOutput
{
public:
	enum class ESampleFormat
	{
		Undefined = 0,
		Double,
		Float,
		Int8,
		Int16,
		Int32
	};

	virtual ~IAudioDecoderOutput() {}

	virtual void Reserve(uint32 InBufferSizeBytes) = 0;

	virtual void Initialize(ESampleFormat InFormat, uint32 InNumChannels, uint32 InSampleRate, FTimespan InDuration, const FDecoderTimeStamp& InPts, uint32 InBufferSizeBytes) = 0;

	virtual const void* GetBuffer() const = 0;
	virtual uint32 GetUsedBufferBytes() const = 0;
	virtual uint32 GetReservedBufferBytes() const = 0;

	virtual uint32 GetChannels() const = 0;
	virtual uint32 GetFrames() const = 0;
	virtual ESampleFormat GetFormat() const = 0;

	virtual FDecoderTimeStamp GetTime() const = 0;
	virtual FTimespan GetDuration() const = 0;

	virtual uint32 GetSampleRate() const = 0;
};

using IAudioDecoderOutputPtr = TSharedPtr<IAudioDecoderOutput, ESPMode::ThreadSafe>;
