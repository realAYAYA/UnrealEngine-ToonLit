// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelCaptureInputFrameI420.h"
#include "PixelCaptureBufferFormat.h"

FPixelCaptureInputFrameI420::FPixelCaptureInputFrameI420(TSharedPtr<FPixelCaptureI420Buffer> Buffer)
	: I420Buffer(Buffer)
{
	Metadata.SourceTime = FPlatformTime::Cycles64();
}

int32 FPixelCaptureInputFrameI420::GetType() const
{
	return static_cast<int32>(PixelCaptureBufferFormat::FORMAT_I420);
}

int32 FPixelCaptureInputFrameI420::GetWidth() const
{
	return I420Buffer->GetWidth();
}

int32 FPixelCaptureInputFrameI420::GetHeight() const
{
	return I420Buffer->GetHeight();
}

TSharedPtr<FPixelCaptureI420Buffer> FPixelCaptureInputFrameI420::GetBuffer() const
{
	return I420Buffer;
}
