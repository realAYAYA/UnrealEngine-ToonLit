// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IElectraTextureSample.h"
#include "RHI.h"
#include "RHIResources.h"
#include "ShaderCore.h"

#include "MediaVideoDecoderOutputPC.h"

#if !PLATFORM_WINDOWS
#error "Should only be used on Windows"
#endif

THIRD_PARTY_INCLUDES_START
#include "d3d12.h"
THIRD_PARTY_INCLUDES_END

class ELECTRASAMPLES_API FElectraTextureSample final
	: public IElectraTextureSampleBase
	, public IMediaTextureSampleConverter
{
public:

	/** Default constructor. */
	FElectraTextureSample()
		: SampleFormat(EMediaTextureSampleFormat::Undefined)
		, bCanUseSRGB(false)
	{ }

	void Initialize(FVideoDecoderOutput* VideoDecoderOutput);
	FVideoDecoderOutput *GetDecoderOutput() { return VideoDecoderOutput.Get(); }

	//
	// General Interface
	//

	virtual const void* GetBuffer() override;
	virtual uint32 GetStride() const override;

	virtual EMediaTextureSampleFormat GetFormat() const override
	{
		return SampleFormat;
	}

#if WITH_ENGINE
	virtual FRHITexture* GetTexture() const override;
#endif //WITH_ENGINE

	virtual IMediaTextureSampleConverter* GetMediaTextureSampleConverter() override;

#if !UE_SERVER
	virtual void ShutdownPoolable() override;
#endif

private:
	virtual float GetSampleDataScale(bool b10Bit) const override;

	virtual bool Convert(FTexture2DRHIRef & InDstTexture, const FConversionHints & Hints) override;
	virtual uint32 GetConverterInfoFlags() const
	{
		return ConverterInfoFlags_PreprocessOnly;
	}

	/** The sample format. */
	EMediaTextureSampleFormat SampleFormat;

	/** Destination Texture resource (from Rendering device) */
	mutable FTexture2DRHIRef Texture;

	/** True if texture format could support sRGB conversion in HW */
	bool bCanUseSRGB;

	/** Output data from video decoder. Baseclass holds reference */
	FVideoDecoderOutputPC* VideoDecoderOutputPC;
};

using FElectraTextureSamplePtr = TSharedPtr<FElectraTextureSample, ESPMode::ThreadSafe>;
using FElectraTextureSampleRef = TSharedRef<FElectraTextureSample, ESPMode::ThreadSafe>;

class FElectraTextureSamplePool : public TMediaObjectPool<FElectraTextureSample>
{
public:
	void PrepareForDecoderShutdown() {}
};

