// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "libav_Decoder_Common.h"

class ILibavDecoderAAC : public ILibavDecoder
{
public:
	// Checks if the AAC decoder is available.
	static LIBAV_API bool IsAvailable();
	
	// Create an instance of the decoder. If not available nullptr is returned.
	static LIBAV_API TSharedPtr<ILibavDecoderAAC, ESPMode::ThreadSafe> Create(const TArray<uint8>& InCodecSpecificData);
	
	virtual ~ILibavDecoderAAC() = default;

	struct FInputAU : public ILibavDecoder::FInputAccessUnit
	{
	};

	struct FOutputInfo
	{
		uint64 ChannelMask = 0;
		int64 PTS = 0;
		int64 UserValue = 0;
		int32 NumChannels = 0;
		int32 NumSamples = 0;
		int32 SampleRate = 0;
	};

	virtual EDecoderError DecodeAccessUnit(const FInputAU& InInputAccessUnit) = 0;
	virtual EDecoderError SendEndOfData() = 0;
	virtual void Reset() = 0;
	virtual EOutputStatus HaveOutput(FOutputInfo& OutInfo) = 0;
	virtual bool GetOutputAsS16(int16* OutInterleavedBuffer, int32 OutBufferSizeInBytes) = 0;
};
