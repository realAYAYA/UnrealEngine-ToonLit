// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IMediaAudioSample.h"
#include "MediaObjectPool.h"
#include "Misc/Timespan.h"
#include "Templates/SharedPointer.h"

#include "MediaAudioDecoderOutput.h"

 
// IMediaAudioSample impl
// is used as audio decoder output
// contains decoded audio samples ready for playback along with corresponding metadata
// owns binary data so can be cached
class FElectraPlayerAudioSample :
	public IMediaAudioSample,
	public IMediaPoolable
{
public:

	FElectraPlayerAudioSample()
	{
	}

	~FElectraPlayerAudioSample()
	{
	}

	void Initialize(const IAudioDecoderOutputPtr & InDecoderOutput)
	{
		DecoderOutput = InDecoderOutput;
	}

	const void* GetBuffer() override
	{ return DecoderOutput ? DecoderOutput->GetBuffer() : nullptr; }

	uint32 GetChannels() const override
	{ return DecoderOutput ? DecoderOutput->GetChannels() : 0; }

	FTimespan GetDuration() const override
	{ return DecoderOutput ? DecoderOutput->GetDuration() : FTimespan::Zero(); }

	EMediaAudioSampleFormat GetFormat() const override
	{ 
		static_assert((int32)EMediaAudioSampleFormat::Undefined == (int32)IAudioDecoderOutput::ESampleFormat::Undefined, "check enums are identical");
		static_assert((int32)EMediaAudioSampleFormat::Double == (int32)IAudioDecoderOutput::ESampleFormat::Double, "check enums are identical");
		static_assert((int32)EMediaAudioSampleFormat::Float == (int32)IAudioDecoderOutput::ESampleFormat::Float, "check enums are identical");
		static_assert((int32)EMediaAudioSampleFormat::Int8 == (int32)IAudioDecoderOutput::ESampleFormat::Int8, "check enums are identical");
		static_assert((int32)EMediaAudioSampleFormat::Int16 == (int32)IAudioDecoderOutput::ESampleFormat::Int16, "check enums are identical");
		static_assert((int32)EMediaAudioSampleFormat::Int32 == (int32)IAudioDecoderOutput::ESampleFormat::Int32, "check enums are identical");

		return DecoderOutput ? (EMediaAudioSampleFormat)DecoderOutput->GetFormat() : EMediaAudioSampleFormat::Undefined;
	}

	uint32 GetFrames() const override
	{ return DecoderOutput ? DecoderOutput->GetFrames() : 0; }

	uint32 GetSampleRate() const override
	{ return DecoderOutput ? DecoderOutput->GetSampleRate() : 0; }

	FMediaTimeStamp GetTime() const override
	{
		if (!DecoderOutput)
		{
			return FMediaTimeStamp();
		}
		FDecoderTimeStamp TimeStamp = DecoderOutput->GetTime();
		return FMediaTimeStamp(TimeStamp.Time, TimeStamp.SequenceIndex);
	}

	uint32 GetMaxBufferBytes() const
	{ return DecoderOutput ? DecoderOutput->GetReservedBufferBytes() : 0; }
	
	virtual void InitializePoolable() override;
	virtual void ShutdownPoolable() override;

private:
	IAudioDecoderOutputPtr DecoderOutput;
};

using FElectraPlayerAudioSamplePtr = TSharedPtr<FElectraPlayerAudioSample, ESPMode::ThreadSafe>;
using FElectraPlayerAudioSamplePool = TMediaObjectPool<FElectraPlayerAudioSample>;
