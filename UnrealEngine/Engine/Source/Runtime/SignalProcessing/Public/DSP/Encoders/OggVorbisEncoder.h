// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/Encoders/IAudioEncoder.h"

#if !PLATFORM_TVOS

struct FOggVorbisEncoderPrivateState;

class SIGNALPROCESSING_API FOggVorbisEncoder : public Audio::IAudioEncoder
{
public:
	FOggVorbisEncoder(const FSoundQualityInfo& InInfo, int32 AverageBufferCallbackSize);

	// From IAudioEncoder: returns 0, since Ogg Vorbis is not built for networked streaming.
	virtual int32 GetCompressedPacketSize() const override;

protected:

	// From IAudioEncoder:
	virtual int64 SamplesRequiredPerEncode() const override;
	virtual bool StartFile(const FSoundQualityInfo& InQualityInfo, TArray<uint8>& OutFileStart) override;
	virtual bool EncodeChunk(const TArray<float>& InAudio, TArray<uint8>& OutBytes) override;
	virtual bool EndFile(TArray<uint8>& OutBytes) override;

private:
	FOggVorbisEncoder();

	int32 NumChannels;

	// Private, uniquely owned state.
	// This must be a raw pointer because it has a non-default destructor that isn't public.
	FOggVorbisEncoderPrivateState* PrivateState;
};
#endif // !PLATFORM_TVOS
