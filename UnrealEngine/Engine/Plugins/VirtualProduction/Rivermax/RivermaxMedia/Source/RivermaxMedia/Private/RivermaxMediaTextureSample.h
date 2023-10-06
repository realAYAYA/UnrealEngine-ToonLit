// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaTextureSample.h"

#include "IMediaSamples.h"
#include "MediaShaders.h"
#include "RivermaxMediaSource.h"
#include "Templates/PimplPtr.h"
#include "Templates/SharedPointer.h"



namespace UE::RivermaxMedia
{

class FRivermaxMediaTextureSampleConverter;
class FRivermaxMediaPlayer;

/** Used to setup the configuration of a sample */
struct FSampleConfigurationArgs
{
	/** Player that produced the sample */
	TSharedPtr<FRivermaxMediaPlayer> Player;

	/** Width of the sample in pixels */
	uint32 Width = 0;

	/** Height of the sample in pixels */
	uint32 Height = 0;

	/** Pixel format of the sample */
	ERivermaxMediaSourcePixelFormat SampleFormat = ERivermaxMediaSourcePixelFormat::RGB_10bit;

	/** Timestamp of the sample */
	FTimespan Time = 0;

	/** Frame rate at which samples are created */
	FFrameRate FrameRate = {24, 1};

	/** Whether sample is in SRGB space and needs to be converted to linear */
	bool bInIsSRGBInput = false;
};

/**
 * Implements a media texture sample for RivermaxMediaPlayer.  
 */
class FRivermaxMediaTextureSample : public IMediaTextureSample, public TSharedFromThis<FRivermaxMediaTextureSample>
{
public:

	FRivermaxMediaTextureSample();

	//~ Begin IMediaTextureSample interface
	const void* GetBuffer() override;
	virtual FIntPoint GetDim() const override;
	virtual FTimespan GetDuration() const override;
	virtual EMediaTextureSampleFormat GetFormat() const override;
	virtual FIntPoint GetOutputDim() const override;
	virtual uint32 GetStride() const override;
	virtual FMediaTimeStamp GetTime() const override;
	virtual bool IsCacheable() const override;
	virtual const FMatrix& GetYUVToRGBMatrix() const override;
	virtual bool IsOutputSrgb() const override;

#if WITH_ENGINE
	virtual IMediaTextureSampleConverter* GetMediaTextureSampleConverter() override;
	virtual FRHITexture* GetTexture() const override;
#endif
	//~ End IMediaTextureSample interface

	bool ConfigureSample(const FSampleConfigurationArgs& Args);

	/** Initialized a RDG buffer based on the description required. Only useful for gpudirect functionality */
	void InitializeGPUBuffer(const FIntPoint& InResolution, ERivermaxMediaSourcePixelFormat InSampleFormat);
	
	/** Returns RDG allocated buffer */
	TRefCountPtr<FRDGPooledBuffer> GetGPUBuffer() const;

	void SetBuffer(TRefCountPtr<FRDGPooledBuffer> NewBuffer);

	/** Returns an uninitialized buffer of size InBufferSize */
	void* RequestBuffer(uint32 InBufferSize);

	/** Returns the player that created this sample */
	TSharedPtr<FRivermaxMediaPlayer> GetPlayer() const;

	/** Returns the incoming sample format */
	ERivermaxMediaSourcePixelFormat GetInputFormat() const;

	/** Whether this samples is coming as srgb and needs to be converted to linear during buffer conversion */
	bool NeedsSRGBToLinearConversion() const;

private:

	/** Texture converted used to handle incoming 2110 formats and convert them to RGB textures the engine handles */
	TPimplPtr<FRivermaxMediaTextureSampleConverter> Converter;
	
	/** Pooled buffer used for gpudirect functionality. Received content will already be on GPU when received from NIC */
	TRefCountPtr<FRDGPooledBuffer> GPUBuffer;

	/** Player that created this sample */
	TWeakPtr<FRivermaxMediaPlayer> WeakPlayer;

	/** Player time for this sample to be evaluated at */
	FTimespan Time;

	/** Texture stride */
	uint32 Stride = 0;

	/** Texture dimensions */
	FIntPoint Dimension = FIntPoint::ZeroValue;

	/** Sample format. */
	EMediaTextureSampleFormat SampleFormat = EMediaTextureSampleFormat::CharBGRA;

	/** Format in the rivermax realm */
	ERivermaxMediaSourcePixelFormat InputFormat;

	/** Duration for which the sample is valid. */
	FTimespan Duration;

	/** Whether the sample is in sRGB space and requires an explicit conversion to linear */
	bool bIsInputSRGB = false;

	/** System memory buffer to be used by converter when not using gpudirect */
	TArray<uint8, TAlignedHeapAllocator<4096>> Buffer;
};

/** This is the mostly empty media samples that only carries a single sample per frame,
 *  that the player provides.
 */
class FRivermaxMediaTextureSamples : public IMediaSamples
{
public:
	FRivermaxMediaTextureSamples() = default;
	FRivermaxMediaTextureSamples(const FRivermaxMediaTextureSamples&) = delete;
	FRivermaxMediaTextureSamples& operator=(const FRivermaxMediaTextureSamples&) = delete;

public:

	//~ Begin IMediaSamples interface

	virtual bool FetchVideo(TRange<FTimespan> TimeRange, TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& OutSample) override;
	virtual bool PeekVideoSampleTime(FMediaTimeStamp& TimeStamp) override;
	virtual void FlushSamples() override;

	//~ End IMediaSamples interface

public:

	TSharedPtr<FRivermaxMediaTextureSample, ESPMode::ThreadSafe> CurrentSample;
};

}
