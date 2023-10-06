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

void FElectraPlayerVideoDecoderOutputApple::Initialize(CVImageBufferRef InImageBufferRef, TSharedPtr<FParamDict, ESPMode::ThreadSafe> InParamDict)
{
	FVideoDecoderOutputApple::Initialize(MoveTemp(InParamDict));

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
	SampleDim = GetOutputDim();
}

void FElectraPlayerVideoDecoderOutputApple::InitializeWithBuffer(const void* InBuffer, uint32 InSize, uint32 InStride, FIntPoint Dim, TSharedPtr<Electra::FParamDict, ESPMode::ThreadSafe> InParamDict)
{
	FVideoDecoderOutput::Initialize(MoveTemp(InParamDict));

	Buffer = MakeShared<TArray<uint8>, ESPMode::ThreadSafe>();
	Buffer->Append((uint8*)InBuffer, InSize);

	Stride = InStride;
	SampleDim = Dim;
}

void FElectraPlayerVideoDecoderOutputApple::InitializeWithBuffer(TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> InBuffer, uint32 InStride, FIntPoint Dim, TSharedPtr<Electra::FParamDict, ESPMode::ThreadSafe> InParamDict)
{
	FVideoDecoderOutput::Initialize(MoveTemp(InParamDict));

	Buffer = MoveTemp(InBuffer);

	Stride = InStride;
	SampleDim = Dim;
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

const TArray<uint8>& FElectraPlayerVideoDecoderOutputApple::GetBuffer() const
{
	if (Buffer.IsValid())
	{
		return *Buffer;
	}
	else
	{
		static TArray<uint8> Empty;
		return Empty;
	}
}

uint32 FElectraPlayerVideoDecoderOutputApple::GetStride() const
{
	return Stride;
}

FIntPoint FElectraPlayerVideoDecoderOutputApple::GetDim() const
{
	return SampleDim;
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
