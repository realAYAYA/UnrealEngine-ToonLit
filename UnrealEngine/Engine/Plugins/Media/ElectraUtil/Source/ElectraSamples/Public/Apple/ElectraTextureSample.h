// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

#include "MediaSamples.h"
#include "IElectraTextureSample.h"
#include "IMediaTextureSampleConverter.h"
#include "MediaVideoDecoderOutputApple.h"

#import <AVFoundation/AVFoundation.h>
#import <VideoToolbox/VideoToolbox.h>

class FElectraMediaTexConvApple;


class ELECTRASAMPLES_API FElectraTextureSample final
	: public IElectraTextureSampleBase
    , public IMediaTextureSampleConverter
{
public:
	FElectraTextureSample(const TWeakPtr<FElectraMediaTexConvApple, ESPMode::ThreadSafe> & InTexConv)
		: Texture(nullptr)
		, TexConv(InTexConv)
	{
	}

	virtual ~FElectraTextureSample()
	{
	}

public:
	void Initialize(FVideoDecoderOutput* InVideoDecoderOutput);

	//~ IMediaTextureSample interface

	virtual const void* GetBuffer() override;

	virtual EMediaTextureSampleFormat GetFormat() const override;

	virtual uint32 GetStride() const override;

	virtual FRHITexture* GetTexture() const override
	{
		return Texture;
	}

	virtual IMediaTextureSampleConverter* GetMediaTextureSampleConverter() override;

private:
	/** The sample's texture resource. */
	TRefCountPtr<FRHITexture2D> Texture;

	/** Output data from video decoder. */
	FVideoDecoderOutputApple* VideoDecoderOutputApple;

	TWeakPtr<FElectraMediaTexConvApple, ESPMode::ThreadSafe> TexConv;

	virtual uint32 GetConverterInfoFlags() const override;
    virtual bool Convert(FTexture2DRHIRef & InDstTexture, const FConversionHints & Hints) override;
};


using FElectraTextureSamplePtr  = TSharedPtr<FElectraTextureSample, ESPMode::ThreadSafe>;
using FElectraTextureSampleRef  = TSharedRef<FElectraTextureSample, ESPMode::ThreadSafe>;


class ELECTRASAMPLES_API FElectraMediaTexConvApple
{
public:
    FElectraMediaTexConvApple();
    ~FElectraMediaTexConvApple();

#if WITH_ENGINE
    void ConvertTexture(FTexture2DRHIRef & InDstTexture, CVImageBufferRef InImageBufferRef, bool bFullRange, EMediaTextureSampleFormat Format, const FMatrix44f& YUVMtx, const FMatrix44d& GamutToXYZMtx, UE::Color::EEncoding EncodingType, float NormalizationFactor);
#endif

private:
#if WITH_ENGINE
    /** The Metal texture cache for unbuffered texture uploads. */
    CVMetalTextureCacheRef MetalTextureCache;
#endif
};


class ELECTRASAMPLES_API FElectraTextureSamplePool : public TMediaObjectPool<FElectraTextureSample, FElectraTextureSamplePool>
{
	using ParentClass = TMediaObjectPool<FElectraTextureSample, FElectraTextureSamplePool>;
	using TextureSample = FElectraTextureSample;

public:
	FElectraTextureSamplePool()
		: TMediaObjectPool<TextureSample, FElectraTextureSamplePool>(this)
		, TexConv(new FElectraMediaTexConvApple())
	{}

	TextureSample *Alloc() const
	{
		return new TextureSample(TexConv);
	}

	void PrepareForDecoderShutdown()
	{
	}

private:
	TSharedPtr<FElectraMediaTexConvApple, ESPMode::ThreadSafe> TexConv;
};

