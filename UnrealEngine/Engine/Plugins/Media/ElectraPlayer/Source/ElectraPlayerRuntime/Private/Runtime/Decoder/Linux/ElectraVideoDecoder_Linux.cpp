// Copyright Epic Games, Inc. All Rights Reserved.

#include "ElectraVideoDecoder_Linux.h"

namespace Electra
{

FElectraPlayerVideoDecoderOutputLinux::FElectraPlayerVideoDecoderOutputLinux()
{
	Stride = 0;
	NumBits = 0;
	Buffer = MakeShared<TArray<uint8>, ESPMode::ThreadSafe>();
}

FElectraPlayerVideoDecoderOutputLinux::~FElectraPlayerVideoDecoderOutputLinux()
{
}

void FElectraPlayerVideoDecoderOutputLinux::SetDecodedImage(TSharedPtr<ILibavDecoderDecodedImage, ESPMode::ThreadSafe> InDecodedImage)
{
	DecodedImage = MoveTemp(InDecodedImage);
}

bool FElectraPlayerVideoDecoderOutputLinux::InitializeForBuffer(FIntPoint Dim, EPixelFormat PixFmt, int32 InNumBits, TSharedPtr<FParamDict, ESPMode::ThreadSafe> InParamDict, bool bAllocateBuffer)
{
	FVideoDecoderOutputLinux::Initialize(MoveTemp(InParamDict));
	NumBits = InNumBits;
	check(PixFmt == EPixelFormat::PF_NV12 || PixFmt == EPixelFormat::PF_P010);
	if (PixFmt != EPixelFormat::PF_NV12 && PixFmt != EPixelFormat::PF_P010)
	{
		return false;
	}

	// Round up to multiple of 2.
	Dim.X = ((Dim.X + 1) / 2) * 2;
	Dim.Y = ((Dim.Y + 1) / 2) * 2;

	if (bAllocateBuffer)
	{
		int32 AllocSize = (Dim.X * Dim.Y) * 3;
		if (NumBits <= 8)
		{
			AllocSize /= 2;
		}
		if (Buffer->Num() < AllocSize)
		{
			Buffer->SetNum(AllocSize);
		}
	}

	// Stride is the width of the buffer
	Stride = Dim.X;
	// The vertical sample dimension encompasses the height of the UV plane.
	SampleDim.X = Dim.X;
	SampleDim.Y = Dim.Y * 3 / 2;

	return true;
}

FIntPoint FElectraPlayerVideoDecoderOutputLinux::GetBufferDimensions() const
{
	return FIntPoint(SampleDim.X, SampleDim.Y * 2 / 3);
}

TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe>& FElectraPlayerVideoDecoderOutputLinux::GetMutableBuffer()
{
	return Buffer;
}

const TArray<uint8>& FElectraPlayerVideoDecoderOutputLinux::GetBuffer() const
{
	return *Buffer;
}

uint32 FElectraPlayerVideoDecoderOutputLinux::GetStride() const
{
	return Stride;
}


void FElectraPlayerVideoDecoderOutputLinux::SetOwner(const TSharedPtr<IDecoderOutputOwner, ESPMode::ThreadSafe>& InOwningRenderer)
{
	OwningRenderer = InOwningRenderer;
}

void FElectraPlayerVideoDecoderOutputLinux::ShutdownPoolable()
{
	// Release the decoded image.
	DecodedImage.Reset();

	TSharedPtr<IDecoderOutputOwner, ESPMode::ThreadSafe> lockedVideoRenderer = OwningRenderer.Pin();
	if (lockedVideoRenderer.IsValid())
	{
		lockedVideoRenderer->SampleReleasedToPool(this);
	}
}

}

// -----------------------------------------------------------------------------------------------------------------------------

FVideoDecoderOutput* FElectraPlayerPlatformVideoDecoderOutputFactory::Create()
{
    return new Electra::FElectraPlayerVideoDecoderOutputLinux();
}
