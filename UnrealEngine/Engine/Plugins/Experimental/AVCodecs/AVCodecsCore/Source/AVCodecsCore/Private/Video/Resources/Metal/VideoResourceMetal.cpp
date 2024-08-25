// Copyright Epic Games, Inc. All Rights Reserved.

#if AVCODECS_USE_METAL

#include "Video/Resources/Metal/VideoResourceMetal.h"

REGISTER_TYPEID(FVideoContextMetal);
REGISTER_TYPEID(FVideoResourceMetal);

static TAVResult<EVideoFormat> ConvertFormat(OSType Format)
{
	switch (Format)
	{
        case kCVPixelFormatType_32BGRA:
		return EVideoFormat::BGRA;
	case kCVPixelFormatType_ARGB2101010LEPacked:
		return EVideoFormat::ABGR10;
	default:
		return FAVResult(EAVResult::ErrorUnsupported, FString::Printf(TEXT("MTL::PixelFormat format %d is not supported"), Format), TEXT("Metal"));
	}
}

FVideoContextMetal::FVideoContextMetal(MTL::Device* Device)
	: Device(Device)
{
}

FVideoDescriptor FVideoResourceMetal::GetDescriptorFrom(TSharedRef<FAVDevice> const& Device, CVPixelBufferRef Raw)
{
    uint32_t Width = CVPixelBufferGetWidth(Raw);
    uint32_t Height = CVPixelBufferGetHeight(Raw);
    TAVResult<EVideoFormat> ConvertedFormat = ConvertFormat(CVPixelBufferGetPixelFormatType(Raw));
    
	return FVideoDescriptor(ConvertedFormat, Width, Height);
}

FVideoResourceMetal::FVideoResourceMetal(TSharedRef<FAVDevice> const& Device, CVPixelBufferRef Raw, FAVLayout const& Layout)
	: TVideoResource(Device, Layout, GetDescriptorFrom(Device, Raw))
	, Raw(Raw)
{
    if (Raw)
    {
        CFRetain(Raw);
    }
}

FVideoResourceMetal::~FVideoResourceMetal()
{
    if (Raw)
    {
        CFRelease(Raw);
        Raw = nullptr;
    }
}

FAVResult FVideoResourceMetal::Validate() const
{
	if (!Raw)
	{
		return FAVResult(EAVResult::ErrorInvalidState, TEXT("Raw resource is invalid"), TEXT("Metal"));
	}

	return EAVResult::Success;
}

#endif
