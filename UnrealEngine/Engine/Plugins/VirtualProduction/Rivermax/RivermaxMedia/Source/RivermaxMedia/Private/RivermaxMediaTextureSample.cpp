// Copyright Epic Games, Inc. All Rights Reserved.

#include "RivermaxMediaTextureSample.h"

#include "RivermaxMediaTextureSampleConverter.h"

FRivermaxMediaTextureSample::FRivermaxMediaTextureSample()
	: TextureConverter(MakeUnique<FRivermaxMediaTextureSampleConverter>())
{
	
}

const FMatrix& FRivermaxMediaTextureSample::GetYUVToRGBMatrix() const
{
	return MediaShaders::YuvToRgbRec709Scaled;
}

IMediaTextureSampleConverter* FRivermaxMediaTextureSample::GetMediaTextureSampleConverter()
{
	return TextureConverter.Get();
}

bool FRivermaxMediaTextureSample::IsOutputSrgb() const
{
	// We always do the sRGB to Linear conversion in shader if specified by the source
	// If true is returned here, MediaTextureResource will create (try) a sRGB texture which only works for 8 bits
	return false;
}

bool FRivermaxMediaTextureSample::ConfigureSample(uint32 InWidth, uint32 InHeight, uint32 InStride, ERivermaxMediaSourcePixelFormat InSampleFormat, FTimespan InTime, const FFrameRate& InFrameRate, const TOptional<FTimecode>& InTimecode, bool bInIsSRGBInput)
{
	EMediaTextureSampleFormat VideoSampleFormat;
	switch (InSampleFormat)
	{
		case ERivermaxMediaSourcePixelFormat::RGB_12bit:
			// Falls through
		case ERivermaxMediaSourcePixelFormat::RGB_16bit_Float:
		{
			VideoSampleFormat = EMediaTextureSampleFormat::FloatRGBA;
			break;
		}
		case ERivermaxMediaSourcePixelFormat::RGB_10bit:
			// Falls through
		case ERivermaxMediaSourcePixelFormat::YUV422_10bit:
		{
			VideoSampleFormat = EMediaTextureSampleFormat::CharBGR10A2;
			break;
		}
		case ERivermaxMediaSourcePixelFormat::YUV422_8bit:
			// Falls through
		case ERivermaxMediaSourcePixelFormat::RGB_8bit:
			// Falls through
		default:
		{
			VideoSampleFormat = EMediaTextureSampleFormat::CharBGRA;
			break;
		}
	}

	TextureConverter->Setup(InSampleFormat, AsShared(), bInIsSRGBInput);
	return Super::SetProperties(InStride, InWidth, InHeight, VideoSampleFormat, InTime, InFrameRate, InTimecode, bInIsSRGBInput);
}

