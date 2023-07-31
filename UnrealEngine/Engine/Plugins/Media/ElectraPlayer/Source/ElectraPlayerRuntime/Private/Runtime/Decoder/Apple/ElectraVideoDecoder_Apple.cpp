// Copyright Epic Games, Inc. All Rights Reserved.

#include "ElectraVideoDecoder_Apple.h"

#if PLATFORM_MAC || PLATFORM_IOS || PLATFORM_TVOS

namespace Electra
{

FElectraPlayerVideoDecoderOutputApple::FElectraPlayerVideoDecoderOutputApple()
	: ImageBufferRef(nullptr)
{
}

FElectraPlayerVideoDecoderOutputApple::~FElectraPlayerVideoDecoderOutputApple()
{
	if (ImageBufferRef)
	{
		CFRelease(ImageBufferRef);
	}
}

void FElectraPlayerVideoDecoderOutputApple::Initialize(CVImageBufferRef InImageBufferRef, FParamDict* InParamDict)
{
	FVideoDecoderOutputApple::Initialize(InParamDict);

	if (ImageBufferRef)
	{
		CFRelease(ImageBufferRef);
	}
	ImageBufferRef = InImageBufferRef;
	if (ImageBufferRef)
	{
		CFRetain(ImageBufferRef);

		Stride = CVPixelBufferGetBytesPerRow(ImageBufferRef);
	}
}

void FElectraPlayerVideoDecoderOutputApple::SetOwner(const TSharedPtr<IDecoderOutputOwner, ESPMode::ThreadSafe>& InOwningRenderer)
{
	OwningRenderer = InOwningRenderer;
}

void FElectraPlayerVideoDecoderOutputApple::ShutdownPoolable()
{
	// release image buffer (we currently realloc it every time anyway)
	if (ImageBufferRef)
	{
		CFRelease(ImageBufferRef);
		ImageBufferRef = nullptr;
	}

	TSharedPtr<IDecoderOutputOwner, ESPMode::ThreadSafe> lockedVideoRenderer = OwningRenderer.Pin();
	if (lockedVideoRenderer.IsValid())
	{
		lockedVideoRenderer->SampleReleasedToPool(this);
	}
}

uint32 FElectraPlayerVideoDecoderOutputApple::GetStride() const
{
	return Stride;
}

CVImageBufferRef FElectraPlayerVideoDecoderOutputApple::GetImageBuffer() const
{
	return ImageBufferRef;
}

} // namespace Electra

// -----------------------------------------------------------------------------------------------------------------------------

FVideoDecoderOutput* FElectraPlayerPlatformVideoDecoderOutputFactory::Create()
{
    return new Electra::FElectraPlayerVideoDecoderOutputApple();
}


#endif
