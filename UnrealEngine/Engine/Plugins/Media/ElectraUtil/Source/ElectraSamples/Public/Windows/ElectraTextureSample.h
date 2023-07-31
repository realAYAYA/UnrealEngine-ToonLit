// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IMediaTextureSample.h"
#include "IMediaTextureSampleConverter.h"
#include "MediaObjectPool.h"
#include "Misc/Timespan.h"
#include "Templates/SharedPointer.h"
#include "Templates/RefCounting.h"
#include "RHI.h"
#include "RHIResources.h"
#include "ShaderCore.h"

#include "MediaVideoDecoderOutputPC.h"

#if !PLATFORM_WINDOWS
#error "Should only be used on Windows"
#endif

class ELECTRASAMPLES_API FElectraTextureSample final
	: public IMediaTextureSample
	, public IMediaPoolable
	, public IMediaTextureSampleConverter
{
public:

	/** Default constructor. */
	FElectraTextureSample()
		: SampleFormat(EMediaTextureSampleFormat::Undefined)
	{ }

	void Initialize(FVideoDecoderOutput* VideoDecoderOutput);
	FVideoDecoderOutput *GetDecoderOutput() { return VideoDecoderOutput.Get(); }

	//
	// General Interface
	//

	virtual const void* GetBuffer() override;
	virtual uint32 GetStride() const override;

	virtual FIntPoint GetDim() const override;
	virtual FIntPoint GetOutputDim() const override;

	virtual FMediaTimeStamp GetTime() const override;
	virtual FTimespan GetDuration() const override;

	virtual double GetAspectRatio() const override
	{
		return VideoDecoderOutput->GetAspectRatio();
	}

	virtual EMediaOrientation GetOrientation() const override
	{
		return (EMediaOrientation)VideoDecoderOutput->GetOrientation();
	}

	virtual EMediaTextureSampleFormat GetFormat() const override
	{
		return SampleFormat;
	}

	virtual bool IsCacheable() const override
	{
		return true;
	}

	virtual bool IsOutputSrgb() const override
	{
		return true;
	}

#if WITH_ENGINE
	virtual FRHITexture* GetTexture() const override
	{
		return Texture;
	}
#endif //WITH_ENGINE

	IMFSample* GetMFSample();

#if !UE_SERVER
	virtual void InitializePoolable() override;
	virtual void ShutdownPoolable() override;
#endif

	virtual IMediaTextureSampleConverter* GetMediaTextureSampleConverter() override;

private:
	virtual bool Convert(FTexture2DRHIRef & InDstTexture, const FConversionHints & Hints) override;
	virtual uint32 GetConverterInfoFlags() const
	{
		return ConverterInfoFlags_PreprocessOnly;
	}

	/** The sample format. */
	EMediaTextureSampleFormat SampleFormat;

	/** Destination Texture resource (from Rendering device) */
	FTexture2DRHIRef Texture;

	/** Output data from video decoder. */
	TSharedPtr<FVideoDecoderOutputPC, ESPMode::ThreadSafe> VideoDecoderOutput;
};

using FElectraTextureSamplePtr = TSharedPtr<FElectraTextureSample, ESPMode::ThreadSafe>;
using FElectraTextureSampleRef = TSharedRef<FElectraTextureSample, ESPMode::ThreadSafe>;

class FElectraTextureSamplePool : public TMediaObjectPool<FElectraTextureSample>
{
public:
	void PrepareForDecoderShutdown() {}
};

