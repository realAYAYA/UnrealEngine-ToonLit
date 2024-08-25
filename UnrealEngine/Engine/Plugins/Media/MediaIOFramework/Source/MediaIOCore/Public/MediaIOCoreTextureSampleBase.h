// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaTextureSample.h"
#include "MediaIOCoreDefinitions.h"
#include "IMediaTextureSampleConverter.h"
#include "MediaObjectPool.h"


#include "ColorSpace.h"
#include "Misc/FrameRate.h"
#include "Templates/RefCounting.h"

class FMediaIOCorePlayerBase;
class FMediaIOCoreTextureSampleConverter;
class FRHITexture;

namespace UE::MediaIOCore
{
	struct FColorFormatArgs
	{
		FColorFormatArgs() = default;
		
		/** Bool constructor to allow backwards compatibility with previous method definitions of FMediaIOCoreTextureSampleBase::Initialize. */
		FColorFormatArgs(bool bIsSRGBInput)
		{
			if (bIsSRGBInput)
			{
				Encoding = UE::Color::EEncoding::sRGB;
				ColorSpace = UE::Color::EColorSpace::sRGB;
			}
			else
			{
				Encoding = UE::Color::EEncoding::Linear;
				ColorSpace = UE::Color::EColorSpace::sRGB;
			}
		}

		FColorFormatArgs(UE::Color::EEncoding InEncoding, UE::Color::EColorSpace InColorSpace)
			: Encoding(InEncoding)
			, ColorSpace(InColorSpace)
		{
		}
		

		/** Encoding of the texture. */
		UE::Color::EEncoding Encoding = UE::Color::EEncoding::Linear;

		/** Color space of the texture. */
		UE::Color::EColorSpace ColorSpace = UE::Color::EColorSpace::sRGB;
	};
}



/** Used to setup JITR data for a sample */
struct MEDIAIOCORE_API FMediaIOCoreSampleJITRConfigurationArgs
{
	/** Width of the sample in pixels */
	uint32 Width = 0;

	/** Height of the sample in pixels */
	uint32 Height = 0;

	/** Platform time fixed on the game thread for JITR sample picking */
	FTimespan Time;

	/** Engine timecode fixed on the game thread for JITR sample picking */
	FTimecode Timecode;

	/** Time offset evaluated on game thread */
	double EvaluationOffsetInSeconds = 0;

	/** Player that produced the sample */
	TSharedPtr<FMediaIOCorePlayerBase> Player;

	/** Sample converter to process this sample */
	TSharedPtr<FMediaIOCoreTextureSampleConverter> Converter;
};


/**
 * Implements the IMediaTextureSample/IMediaPoolable interface.
 */
class MEDIAIOCORE_API FMediaIOCoreTextureSampleBase
	: public IMediaTextureSample
	, public IMediaPoolable
	, public IMediaTextureSampleColorConverter
{

public:
	FMediaIOCoreTextureSampleBase();

	// Note: We need to explicitly disable warnings on these constructors/operators for clang to be happy with deprecated variables
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FMediaIOCoreTextureSampleBase(const FMediaIOCoreTextureSampleBase&) = default;
	FMediaIOCoreTextureSampleBase(FMediaIOCoreTextureSampleBase&&) = default;
	FMediaIOCoreTextureSampleBase& operator=(const FMediaIOCoreTextureSampleBase&) = default;
	FMediaIOCoreTextureSampleBase& operator=(FMediaIOCoreTextureSampleBase&&) = default;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/**
	 * Initialize the sample.
	 *
	 * @param InVideoBuffer The video frame data.
	 * @param InBufferSize The size of the video buffer.
	 * @param InStride The number of channel of the video buffer.
	 * @param InWidth The sample rate of the video buffer.
	 * @param InHeight The sample rate of the video buffer.
	 * @param InSampleFormat The sample format of the video buffer.
	 * @param InTime The sample time (in the player's own clock).
	 * @param InFrameRate The framerate of the media that produce the sample.
	 * @param InTimecode The sample timecode if available.
	 * @param InColorFormatArgs Information about the texture color encoding and color space.
	 */
	bool Initialize(const void* InVideoBuffer, uint32 InBufferSize, uint32 InStride, uint32 InWidth, uint32 InHeight, EMediaTextureSampleFormat InSampleFormat, FTimespan InTime, const FFrameRate& InFrameRate, const TOptional<FTimecode>& InTimecode, const UE::MediaIOCore::FColorFormatArgs& InColorFormatArgs);

	/**
	 * Initialize the sample.
	 *
	 * @param InVideoBuffer The video frame data.
	 * @param InStride The number of channel of the video buffer.
	 * @param InWidth The sample rate of the video buffer.
	 * @param InHeight The sample rate of the video buffer.
	 * @param InSampleFormat The sample format of the video buffer.
	 * @param InTime The sample time (in the player's own clock).
	 * @param InFrameRate The framerate of the media that produce the sample.
	 * @param InTimecode The sample timecode if available.
	 * @param InColorFormatArgs Information about the texture color encoding and color space.
	 */
	bool Initialize(const TArray<uint8>& InVideoBuffer, uint32 InStride, uint32 InWidth, uint32 InHeight, EMediaTextureSampleFormat InSampleFormat, FTimespan InTime, const FFrameRate& InFrameRate, const TOptional<FTimecode>& InTimecode, const UE::MediaIOCore::FColorFormatArgs& InColorFormatArgs);

	/**
	 * Initialize the sample.
	 *
	 * @param InVideoBuffer The video frame data.
	 * @param InStride The number of channel of the video buffer.
	 * @param InWidth The sample rate of the video buffer.
	 * @param InHeight The sample rate of the video buffer.
	 * @param InSampleFormat The sample format of the video buffer.
	 * @param InTime The sample time (in the player's own clock).
	 * @param InFrameRate The framerate of the media that produce the sample.
	 * @param InTimecode The sample timecode if available.
	 * @param InColorFormatArgs Information about the texture color encoding and color space.
	 */
	bool Initialize(TArray<uint8>&& InVideoBuffer, uint32 InStride, uint32 InWidth, uint32 InHeight, EMediaTextureSampleFormat InSampleFormat, FTimespan InTime, const FFrameRate& InFrameRate, const TOptional<FTimecode>& InTimecode, const UE::MediaIOCore::FColorFormatArgs& InColorFormatArgs);

	/**
	 * Initialize the sample.
	 *
	 * @param InVideoBuffer The video frame data.
	 * @param InBufferSize The size of the video buffer.
	 */
	bool SetBuffer(const void* InVideoBuffer, uint32 InBufferSize);

	/**
	 * Set the sample buffer.
	 *
	 * @param InVideoBuffer The video frame data.
	 */
	bool SetBuffer(const TArray<uint8>& InVideoBuffer);

	/**
	 * Set the sample buffer.
	 *
	 * @param InVideoBuffer The video frame data.
	 */
	bool SetBuffer(TArray<uint8>&& InVideoBuffer);

	/**
	 * Set the sample properties.
	 *
	 * @param InStride The number of channel of the video buffer.
	 * @param InWidth The sample rate of the video buffer.
	 * @param InHeight The sample rate of the video buffer.
	 * @param InSampleFormat The sample format of the video buffer.
	 * @param InTime The sample time (in the player's own clock).
	 * @param InFrameRate The framerate of the media that produce the sample.
	 * @param InTimecode The sample timecode if available.
	 * @param InColorFormatArgs Information about the texture color encoding and color space.
	 */
	bool SetProperties(uint32 InStride, uint32 InWidth, uint32 InHeight, EMediaTextureSampleFormat InSampleFormat, FTimespan InTime, const FFrameRate& InFrameRate, const TOptional<FTimecode>& InTimecode, const UE::MediaIOCore::FColorFormatArgs& InColorFormatArgs);

	/**
	 * Initialize the sample with half it's original height and take only the odd or even line.
	 *
	 * @param bUseEvenLine Should use the Even or the Odd line from the video buffer.
	 * @param InVideoBuffer The video frame data.
	 * @param InStride The number of channel of the video buffer.
	 * @param InWidth The sample rate of the video buffer.
	 * @param InHeight The sample rate of the video buffer.
	 * @param InSampleFormat The sample format of the video buffer.
	 * @param InTime The sample time (in the player's own clock).
	 * @param InTimecode The sample timecode if available.
	 * @param InColorFormatArgs Information about the texture color encoding and color space.
	 */
	bool InitializeWithEvenOddLine(bool bUseEvenLine, const void* InVideoBuffer, uint32 InBufferSize, uint32 InStride, uint32 InWidth, uint32 InHeight, EMediaTextureSampleFormat InSampleFormat, FTimespan InTime, const FFrameRate& InFrameRate, const TOptional<FTimecode>& InTimecode, const UE::MediaIOCore::FColorFormatArgs& InColorFormatArgs);

	/**
	 * Set the sample buffer with half it's original height and take only the odd or even line.
	 *
	 * @param bUseEvenLine Should use the Even or the Odd line from the video buffer.
	 * @param InVideoBuffer The video frame data.
	 * @param InBufferSize The size of the video buffer.
	 * @param InStride The number of channel of the video buffer.
	 * @param InHeight The sample rate of the video buffer.
	 */
	bool SetBufferWithEvenOddLine(bool bUseEvenLine, const void* InVideoBuffer, uint32 InBufferSize, uint32 InStride, uint32 InHeight);

	/**
	 * Set the OCIO settings used for color conversion.
	 */
	void SetColorConversionSettings(TSharedPtr<struct FOpenColorIOColorConversionSettings> InColorConversionSettings)
	{
		ColorConversionSettings = InColorConversionSettings;
	}
	/**
	 * Request an uninitialized sample buffer.
	 * Should be used when the buffer could be filled by something else.
	 * SetProperties should still be called after.
	 *
	 * @param InBufferSize The size of the video buffer.
	 */
	virtual void* RequestBuffer(uint32 InBufferSize);

	/**
	 * Configure this sample for JITR
	 *
	 * @param Args JITR configuration parameters.
	 */
	virtual bool InitializeJITR(const FMediaIOCoreSampleJITRConfigurationArgs& Args);

	/** Marks this sample as one that is ready and awaiting for fast GPUDirect texture transfer */
	void SetAwaitingForGPUTransfer()
	{
		bIsAwaitingForGPUTransfer = true;
	}

	/** Returns whether it's ready for GPUDirect texture transfer */
	bool IsAwaitingForGPUTransfer() const
	{
		return bIsAwaitingForGPUTransfer;
	}

	/** Returns the player that created this sample */
	TSharedPtr<FMediaIOCorePlayerBase> GetPlayer() const;

	/** Returns time evaluated on the game thread for JITR */
	double GetEvaluationOffsetInSeconds() const
	{
		return EvaluationOffsetInSeconds;
	}

	/** Copies all neccessary data from a source sample to render JIT */
	virtual void CopyConfiguration(const TSharedPtr<FMediaIOCoreTextureSampleBase>& SourceSample);

public:
	//~ IMediaTextureSample interface

	virtual FIntPoint GetDim() const override
	{
		switch (GetFormat())
		{
		case EMediaTextureSampleFormat::CharAYUV:
		case EMediaTextureSampleFormat::CharNV12:
		case EMediaTextureSampleFormat::CharNV21:
		case EMediaTextureSampleFormat::CharUYVY:
		case EMediaTextureSampleFormat::CharYUY2:
		case EMediaTextureSampleFormat::CharYVYU:
			return FIntPoint(Width / 2, Height);
		case EMediaTextureSampleFormat::YUVv210:
			// Data for 6 output pixels is contained in 4 actual texture pixels
			// Padding aligned on 48 (16 and 6 at the same time)
			return FIntPoint(4 * ((((Width + 47) / 48) * 48) / 6), Height);
		default:
			return FIntPoint(Width, Height);
		}
	}

	virtual FTimespan GetDuration() const override
	{
		return Duration;
	}

	virtual EMediaTextureSampleFormat GetFormat() const override
	{
		return SampleFormat;
	}

	virtual FIntPoint GetOutputDim() const override
	{
		return FIntPoint(Width, Height);
	}

	virtual uint32 GetStride() const override
	{
		return Stride;
	}

	virtual FMediaTimeStamp GetTime() const override
	{
		return FMediaTimeStamp(Time);
	}

	virtual TOptional<FTimecode> GetTimecode() const override
	{
		return Timecode;
	}

	virtual bool IsCacheable() const override
	{
		return true;
	}

	virtual bool IsOutputSrgb() const override;
	
	virtual FMatrix44d GetGamutToXYZMatrix() const override;
	virtual FVector2d GetWhitePoint() const override;
	virtual FVector2d GetDisplayPrimaryRed() const override;
	virtual FVector2d GetDisplayPrimaryGreen() const override;
	virtual FVector2d GetDisplayPrimaryBlue() const override;
	virtual UE::Color::EEncoding GetEncodingType() const override;
	virtual float GetHDRNitsNormalizationFactor() const override;

	virtual const void* GetBuffer() override
	{
		// Don't return the buffer if we have a texture to force the media player to use the texture if available. 
		if (Texture)
		{
			return nullptr;
		}

		if (ExternalBuffer)
		{
			return ExternalBuffer;
		}

		return Buffer.GetData();
	}

	//~ IMediaTextureSampleColorConverter interface
	virtual bool ApplyColorConversion(FTexture2DRHIRef& InSrcTexture, FTexture2DRHIRef& InDstTexture) override;

	void* GetMutableBuffer()
	{
		if (ExternalBuffer)
		{
			return ExternalBuffer;
		}

		return Buffer.GetData();
	}

#if WITH_ENGINE
	virtual IMediaTextureSampleConverter* GetMediaTextureSampleConverter() override;
	virtual FRHITexture* GetTexture() const override;

	virtual IMediaTextureSampleColorConverter* GetMediaTextureSampleColorConverter() override;
#endif //WITH_ENGINE

	void SetBuffer(void* InBuffer)
	{
		ExternalBuffer = InBuffer;
	}

	void SetTexture(TRefCountPtr<FRHITexture> InRHITexture);
	void SetDestructionCallback(TFunction<void(TRefCountPtr<FRHITexture>)> InDestructionCallback);

private:
	/** Hold a texture to be used for gpu texture transfers. */
	TRefCountPtr<FRHITexture> Texture;

	/** Called when the sample is destroyed by its pool. */
	TFunction<void(TRefCountPtr<FRHITexture>)> DestructionCallback;

public:
	//~ IMediaPoolable interface

	virtual void ShutdownPoolable() override;

protected:
	virtual void FreeSample()
	{
		Buffer.Reset();
		ExternalBuffer = nullptr;
		CachedOCIOResources.Reset();
		Texture.SafeRelease();
		OriginalSample.Reset();
	}

	/**
	 * Get YUV to RGB conversion matrix
	 *
	 * @return MediaIOCore Yuv To Rgb matrix
	 */
	virtual const FMatrix& GetYUVToRGBMatrix() const override;

protected:
	/** Duration for which the sample is valid. */
	FTimespan Duration;

	/** Sample format. */
	EMediaTextureSampleFormat SampleFormat;

	/** Sample time. */
	FTimespan Time;

	/** Sample timecode. */
	TOptional<FTimecode> Timecode;

	/** Image dimensions */
	uint32 Stride = 0;
	uint32 Width  = 0;
	uint32 Height = 0;

	/** Pointer to raw pixels */
	TArray<uint8, TAlignedHeapAllocator<4096>> Buffer;
	void* ExternalBuffer = nullptr;

#if WITH_EDITORONLY_DATA
	UE_DEPRECATED(5.3, "Please use Encoding instead.")
	/** Whether the sample is in sRGB space and requires an explicit conversion to linear */
	bool bIsSRGBInput = false;
#endif

	/** Color encoding of the incoming texture. */
	UE::Color::EEncoding Encoding = UE::Color::EEncoding::Linear;

	/** Color space enum of the incoming texture. */
	UE::Color::EColorSpace ColorSpace = UE::Color::EColorSpace::sRGB;

	/** Color space structure of the incoming texture. Used for retrieving chromaticities. */
	UE::Color::FColorSpace ColorSpaceStruct = UE::Color::FColorSpace(UE::Color::EColorSpace::sRGB);

private:
	/** The player that created this sample */
	TWeakPtr<FMediaIOCorePlayerBase> Player;

	/** Custom converter that will be the one checking back with the player for just in time sample render purposes */
	TSharedPtr<FMediaIOCoreTextureSampleConverter> Converter;

	/**
	 * A reference to the original sample that was chosen during JITR. The idea of this member is to keep
	 * the original sample alive, prevent any of its resources from being released while this proxy sample is in use.
	 */
	TSharedPtr<FMediaIOCoreTextureSampleBase> OriginalSample;

	/** Whether this sample's texture data is awaiting to be transferred by GPUDirect */
	bool bIsAwaitingForGPUTransfer = false;

	/** Time offset evaluated on game thread for JITR */
	double EvaluationOffsetInSeconds = 0;

private:
	/** Settings used to apply an OCIO conversion to the sample. */
	TSharedPtr<struct FOpenColorIOColorConversionSettings> ColorConversionSettings;
	/** Cached rendering resources used for the color conversion pass when using OCIO. */
	TSharedPtr<struct FOpenColorIORenderPassResources> CachedOCIOResources;
};
