// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_WEBM_LIBS

#include "Templates/SharedPointer.h"
#include "MediaShaders.h"

class IWebMSamplesSink;
class FWebMMediaTextureSample;
class FWebMMediaTextureSamplePool;
struct FWebMFrame;
struct vpx_image;
struct vpx_codec_ctx;

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
		const vpx_image* Image;
	};

	vpx_codec_ctx* Context;
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
	void CreateTextures(const vpx_image* Image);
	void Close();
};

#endif // WITH_WEBM_LIBS
