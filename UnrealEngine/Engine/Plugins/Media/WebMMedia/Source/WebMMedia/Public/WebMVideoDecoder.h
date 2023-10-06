// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_WEBM_LIBS

#include "Templates/SharedPointer.h"
#include "MediaShaders.h"

THIRD_PARTY_INCLUDES_START
#include <vpx/vpx_decoder.h>
#include <vpx/vp8dx.h>
THIRD_PARTY_INCLUDES_END

class IWebMSamplesSink;
class FWebMMediaTextureSample;
class FWebMMediaTextureSamplePool;
struct FWebMFrame;

class WEBMMEDIA_API FWebMVideoDecoder
{
public:
	FWebMVideoDecoder(IWebMSamplesSink& InSamples);
	~FWebMVideoDecoder();

public:
	bool Initialize(const char* CodecName);
	void DecodeVideoFramesAsync(const TArray<TSharedPtr<FWebMFrame>>& VideoFrames);
	bool IsBusy() const;

private:
	struct FConvertParams
	{
		TSharedPtr<FWebMMediaTextureSample, ESPMode::ThreadSafe> VideoSample;
		const vpx_image_t* Image;
	};

	vpx_codec_ctx_t Context;
	TUniquePtr<FWebMMediaTextureSamplePool> VideoSamplePool;
	TRefCountPtr<FRHITexture2D> DecodedY;
	TRefCountPtr<FRHITexture2D> DecodedU;
	TRefCountPtr<FRHITexture2D> DecodedV;
	FGraphEventRef VideoDecodingTask;
	IWebMSamplesSink& Samples;
	bool bTexturesCreated;
	bool bIsInitialized;

	void ConvertYUVToRGBAndSubmit(const FConvertParams& Params);
	void DoDecodeVideoFrames(const TArray<TSharedPtr<FWebMFrame>>& VideoFrames);
	void CreateTextures(const vpx_image_t* Image);
	void Close();
};

#endif // WITH_WEBM_LIBS
