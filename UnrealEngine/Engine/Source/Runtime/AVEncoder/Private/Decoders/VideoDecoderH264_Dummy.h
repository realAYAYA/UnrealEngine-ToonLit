// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#define AVENCODER_VIDEO_DECODER_AVAILABLE_H264_DUMMY		// enable dummy h264 decoder

#ifdef AVENCODER_VIDEO_DECODER_AVAILABLE_H264_DUMMY

#include "VideoDecoderFactory.h"


namespace AVEncoder
{

class FVideoDecoderH264_Dummy : public FVideoDecoder
{
public:
	// register decoder with video decoder factory
	static void Register(FVideoDecoderFactory& InFactory);

	virtual bool Setup(const FInit& InInit) override;
	virtual void Shutdown() override;

	virtual EDecodeResult Decode(const FVideoDecoderInput* InInput) override;


	// query whether or not encoder is supported and available
//	static bool GetIsAvailable(FVideoEncoderInputImpl& InInput, FVideoEncoderInfo& OutEncoderInfo);


private:
	FVideoDecoderH264_Dummy();
	virtual ~FVideoDecoderH264_Dummy();
	uint8_t YOffset = 0;
	bool bIsInitialized = false;
};

} // namespace AVEncoder

#endif // AVENCODER_VIDEO_DECODER_AVAILABLE_H264_DUMMY
